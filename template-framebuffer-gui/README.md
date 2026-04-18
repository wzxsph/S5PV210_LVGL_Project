# S5PV210 LVGL Framebuffer GUI

裸机 LVGL v9.6.0-dev 移植项目，运行于 Samsung S5PV210 (ARM Cortex-A8) X210 开发板。无操作系统，使用 ARM NEON SIMD 加速的软件渲染，通过 TFTP 引导。

## 功能特性

- **LVGL Widgets Demo**：运行 LVGL 官方 `lv_demo_widgets()`，包含按钮、滑块、图表、复选框等丰富控件
- **GPIO 按键导航**：6 个板载按键映射为 LVGL 键盘输入（PREV/NEXT/ENTER/ESC/LEFT/RIGHT）
- **高性能渲染**：PARTIAL 模式 + 影子帧缓冲 (shadow FB) 防撕裂，首帧渲染 ~280ms
- **MMU 分区缓存**：SDRAM 启用 Write-Back/Write-Allocate 缓存，帧缓冲区 Non-cacheable
- **VFP/NEON 硬件加速**：启动阶段即开启 Cortex-A8 NEON 协处理器
- **实时性能监控**：右下角 FPS/CPU 显示，左下角内存使用显示
- **调试输出**：UART2 @ 115200 baud 实时打印系统状态

## 硬件配置

| 参数 | 值 |
|------|-----|
| SoC | Samsung S5PV210 (ARM Cortex-A8, 1GHz) |
| RAM | 512MB DDR2 SDRAM |
| 显示 | 1024×600 LCD via FIMD Window 2, XRGB8888 (32bpp), 60Hz |
| 帧缓冲 | 0x3E000000 (Non-cacheable, 6MB 区间) |
| 定时器 | PWM Timer 0, 100Hz (10ms jiffies) |
| 调试串口 | UART2 @ 115200 baud |
| 按键 | 6 个 GPIO 按键 (UP/DOWN/LEFT/RIGHT/MENU/BACK) |

## 快速开始

### 编译

```bash
cd template-framebuffer-gui
make clean && make
```

产物位于 `output/template-framebuffer-gui.bin`（约 1038KB）。

交叉编译器：`arm-none-eabi-gcc`，编译选项 `-O3 -mcpu=cortex-a8 -mfpu=neon -ftree-vectorize -ffast-math`。

### TFTP 烧录运行

```powershell
powershell -ExecutionPolicy Bypass -File auto_test.ps1
```

脚本自动完成：启动本地 TFTP 服务器 → U-Boot 下载 → `go 0x30000000` 启动 → 串口捕获输出。

### 手动 U-Boot 引导

```
tftp 0x30000000 template-framebuffer-gui.bin
go 0x30000000
```

## 按键映射

| 物理按键 | GPIO | LVGL 键码 | 功能 |
|---------|------|-----------|------|
| UP | GPH2_0 | `LV_KEY_PREV` | 上一个控件 |
| DOWN | GPH0_3 | `LV_KEY_NEXT` | 下一个控件 |
| LEFT | GPH0_2 | `LV_KEY_LEFT` | 控件内左调 (如滑块减小) |
| RIGHT | GPH2_1 | `LV_KEY_RIGHT` | 控件内右调 (如滑块增大) |
| MENU | GPH2_3 | `LV_KEY_ENTER` | 选中 / 进入编辑模式 |
| BACK | GPH2_2 | `LV_KEY_ESC` | 取消 / 退出编辑模式 |

## 项目结构

```
template-framebuffer-gui/
├── source/
│   ├── main.c              # 应用入口，系统初始化，LVGL 主循环
│   ├── lv_port_disp.c      # 显示驱动 (PARTIAL + shadow FB 防撕裂)
│   ├── lv_port_indev.c     # 输入驱动 (GPIO 按键 → LVGL 键盘)
│   ├── startup/            # 启动代码 (start.S: MMU/VFP/NEON 初始化)
│   ├── hardware/           # S5PV210 HAL (clk, fb, irq, serial, tick)
│   ├── arm/                # ARM 汇编优化 (memcpy.S 等)
│   ├── graphic/            # 帧缓冲 surface 抽象层
│   └── library/            # 自定义 stdlib (string, stdio, math, malloc)
├── include/                # 公共头文件
├── lvgl/                   # LVGL v9.6.0-dev 源码树
├── lv_conf.h               # LVGL 主配置文件
├── link.ld                 # 链接脚本 (内存布局定义)
├── Makefile                # 构建系统
├── auto_test.ps1           # 自动 TFTP 烧录 + 串口监控脚本
├── tftp_server.py          # Python TFTP 服务器
├── output/                 # 构建产物 (.elf, .bin, .map)
└── test_reports/           # 测试记录
```

## 架构

### 启动流程

```
BL0/BL1 (iROM)
  → start.S: MMU 页表配置, VFP/NEON 开启, 栈初始化
    → main(): 时钟/DRAM/LCD/IRQ/Timer 初始化
      → lv_init() → lv_port_disp_init() → lv_port_indev_init()
        → lv_demo_widgets()
          → while(1) { lv_timer_handler(); }
```

### 内存布局

| 区间 | 地址范围 | MMU 属性 | 用途 |
|------|---------|---------|------|
| 代码/数据/BSS | 0x30000000+ | Cached (WB/WA) | 程序、LVGL 堆、render_buf、shadow_fb |
| MMU 页表 | 0x30580000 | Cached | 16KB 一级页表 |
| 栈 | BSS 之后 | Cached | SVC 32KB, IRQ 4KB, FIQ/ABT/UND 各 1KB |
| 帧缓冲 | 0x3E000000-0x3E5FFFFF | Non-cacheable | 2×2.4MB VRAM (FIMD 扫描) |

### 渲染架构 (PARTIAL + Shadow FB 防撕裂)

```
┌─────────────┐     ┌────────────┐     ┌────────────┐
│  LVGL 渲染   │────→│ render_buf │────→│ shadow_fb  │
│  (SW + NEON) │     │ 491KB      │     │ 2.4MB      │
│              │     │ cached RAM │     │ cached RAM │
└─────────────┘     └────────────┘     └────────────┘
                                              │
                                    VSYNC ────┤ (帧内最后一块时)
                                              │
                                              ▼
                                       ┌────────────┐
                                       │    VRAM    │
                                       │ 2.4MB      │
                                       │ non-cached │──→ FIMD → LCD
                                       └────────────┘
```

**三级缓冲流水线**：
1. **render_buf** (491KB, cached)：LVGL PARTIAL 模式逐条带渲染，每条带 120 行
2. **shadow_fb** (2.4MB, cached)：每条带完成后立即合并到影子帧缓冲（cached→cached 极快）
3. **VRAM** (2.4MB, non-cacheable)：帧内最后一条带时，等待 VSYNC 后一次性 memcpy 脏行区间

**关键优化**：
- LVGL 全程渲染在 cached RAM 中（~2-5ns/pixel），避免直接写 non-cacheable VRAM（~50-100ns/pixel）
- 脏区 Y 范围追踪：仅拷贝每帧实际更改的行到 VRAM，减少 uncached 写入量
- VSYNC 寄存器轮询（VIDINTCON1[1]）：可靠同步，20ms 超时保底

## LVGL 配置摘要

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `LV_COLOR_DEPTH` | 32 | XRGB8888，与 LCD 一致 |
| `LV_USE_OS` | `LV_OS_NONE` | 裸机，无 RTOS |
| `LV_MEM_SIZE` | 4MB | LVGL 内置堆 |
| `LV_DEF_REFR_PERIOD` | 16ms | 刷新周期 (~60Hz 上限) |
| `LV_DRAW_SW_DRAW_UNIT_CNT` | 1 | 单线程软件渲染 |
| `LV_USE_DRAW_SW_ASM` | `NEON` | ARM NEON SIMD 加速 |
| `LV_USE_FLOAT` | 1 | 使用 VFP 浮点 |
| `LV_DRAW_BUF_ALIGN` | 64 | 缓存行对齐 |
| `LV_OBJ_STYLE_CACHE` | 1 | 样式属性查找加速 |
| `LV_DRAW_SW_SHADOW_CACHE_SIZE` | 64 | 阴影计算缓存 |
| `LV_USE_PERF_MONITOR` | 1 | 右下角 FPS/CPU 显示 |
| `LV_USE_MEM_MONITOR` | 1 | 左下角内存使用显示 |
| `LV_USE_DEMO_WIDGETS` | 1 | 官方 Widgets Demo |

## 调试

调试信息通过 UART2 输出，关键日志源：

| 日志前缀 | 来源 | 内容 |
|----------|------|------|
| `[INIT]` | `main.c` | 系统初始化各阶段 |
| `[LVGL]` | `main.c` | LVGL 核心初始化、内存状态 |
| `[DISP_INIT]` | `lv_port_disp.c` | 显示驱动配置、缓冲区地址 |
| `[INDEV]` | `main.c` | 输入设备初始化 |
| `[DEMO]` | `main.c` | Demo 创建和首帧渲染耗时 |
| `[LOOP]` | `main.c` | 每 5 秒打印 tick/loops/flush 统计 |

串口输出示例：
```
[DISP_INIT] Render mode: PARTIAL (120-line cached + shadow FB)
[DISP_INIT] Render buffer: 0x30103800 (491520 bytes, 120 lines)
[DISP_INIT] Shadow FB:     0x3017b800 (2457600 bytes, anti-tear)
[DEMO] First render: 280 ms, flush=5
[LOOP] tick=5000 loops=4707855 flush=34
```

## 依赖

- [arm-none-eabi-gcc](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm) 交叉编译工具链
- Python 3（用于 `tftp_server.py`）
- Windows PowerShell（用于 `auto_test.ps1`）

## 许可

本项目为个人学习用途。LVGL v9 基于 MIT 许可证。
