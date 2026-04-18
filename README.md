# S5PV210 (X210) 裸机 LVGL v9 移植与界面开发

**北京理工大学 · 电子信息工程专业 · 嵌入式系统课程大作业**

本项目旨在 Samsung S5PV210 (ARM Cortex-A8) 处理器和 X210 开发板的**裸机环境**（无操作系统）下，从零开始移植并深度优化最新的 **LVGL v9.6.0-dev** 嵌入式图形图形库，实现高性能的 GUI 交互界面。

---

## 1. 项目与课程背景

作为北京理工大学电子信息工程专业的嵌入式系统课程大作业，本项目避开了直接使用 Linux 等成熟操作系统的捷径，选择了具有挑战性的裸机开发模式。这要求开发者深入理解底层硬件体系结构。

**核心挑战与学习目标：**
- 深入理解 ARM Cortex-A8 架构，掌握汇编指令、异常向量表（Data Abort、Undefined Instruction 等）及内核启动序列。
- 掌握存储器管理单元（MMU）和 Cache（L1/L2）的配置原理，为不同内存区域（代码区、堆栈、显存）配置对应的缓存策略（Cacheable / Non-cacheable）。
- 解析 LCD 控制器（FIMD）工作原理，自行编写驱动并解决屏幕防撕裂（Tearing）问题。
- 优化系统性能，利用 ARM NEON SIMD 协处理器及硬件浮点运算（VFP）加速图形渲染。

## 2. 硬件平台与开发环境

### 2.1 硬件配置 (X210 开发板)
| 组件 / 参数 | 说明 |
|------------|------|
| **SoC** | Samsung S5PV210 (ARM Cortex-A8, 主频 1GHz) |
| **内存 (RAM)** | 512MB DDR2 SDRAM (基地址: `0x30000000`) |
| **显示子系统** | 1024×600 RGB LCD 屏幕 (FIMD Window 2, 32bpp XRGB8888, 刷新率 60Hz) |
| **系统心跳** | PWM Timer 4 硬件定时器 (配置为 100Hz, 10ms/jiffies) |
| **输入设备** | 6 个 GPIO 物理按键 (UP/DOWN/LEFT/RIGHT/MENU/BACK) |
| **调试接口** | UART2 波特率 115200 baud |

### 2.2 交叉编译与调试环境
- **操作系统**：Windows 环境主导
- **编译器**：`arm-none-eabi-gcc` (开启 `-O3 -mcpu=cortex-a8 -mfpu=neon -ftree-vectorize -ffast-math` 极致优化)
- **构建工具**：GNU Make (支持 `make -j24` 并行编译)
- **烧录方式**：主机端运行自定义 Python TFTP 服务器，开发板端通过 U-Boot `tftp` 下载。并自研了 PowerShell 自动化烧录测试脚本。

---

## 3. 功能特性

1. **LVGL v9 官方演示无缝运行**：完美运行 LVGL 官方 `lv_demo_widgets()`，包含按钮、滑块、图表、复选框等精美动画控件。
2. **纯物理按键驱动的焦点导航**：将板载的 6 个 GPIO 按键映射至 LVGL 的虚拟键盘（Keypad InDev），实现无需触摸屏即可游刃有余的 UI 导航控制。
3. **极致的三级渲染防撕裂流水线**：使用 `PARTIAL` 模式结合 `shadow_fb`（影子帧缓冲），并结合 VSYNC（垂直同步）中断/轮询，在裸机下实现丝滑无撕裂的显示表现。
4. **MMU 分区缓存**：代码与普通内存启用 Write-Back/Write-Allocate 缓存，而帧缓冲区（VRAM）设为 Non-cacheable，不仅保证了高吞吐量的渲染，还防止了因缓存不一致导致的显示花屏问题。
5. **NEON / VFP 硬件加速**：在裸机 `start.S` 启动阶段即开启 Cortex-A8 核心的 NEON 与 VFP 协处理器，并充分利用 GCC 向量化选项，大幅缩短渲染延迟。

---

## 4. 工程架构深度解析（工程怎么写的）

本工程具备极高的模块化程度，自底向上构建了完整的裸机运行支持。

### 4.1 目录结构与职责
```text
template-framebuffer-gui/
├── source/
│   ├── startup/            # 汇编级启动代码 (start.S/memset.S等)，栈与协处理器初始化
│   ├── hardware/           # SoC 级板级支持包 (BSP) (clk, fb, irq, serial, tick 等)
│   ├── library/            # 极简版 C 标准库替换 (string.h, malloc 等 TLSF 算法实现)
│   ├── lv_port_disp.c      # LVGL 显示接口对接：实现防撕裂刷屏逻辑
│   ├── lv_port_indev.c     # LVGL 输入接口对接：GPIO状态轮询与键码转换
│   └── main.c              # OS 主循环：负责硬件统筹初始化、延时节流控制与 LVGL Task 驱动
├── include/                # API 暴露：包含所有的全局结构体与宏定义
├── lvgl/                   # LVGL v9 原生源码树 (未做侵入式修改)
├── lv_conf.h               # LVGL 核心配置文件 (分配 4MB LVGL 堆，开启性能监控)
├── link.ld                 # 核心连接器脚本：定义内存段(.text, .data, .bss)、异常栈与显存绝对位置
├── auto_test.ps1           # 上位机自动化测试驱动角本：杀死旧进程、启 TFTP、监听串口等
└── tftp_server.py          # 定制的 Python TFTP 服务端：支持多网卡环境下的自动 IP 嗅探
```

### 4.2 系统启动序列 (Boot Flow)
1. **BL0/BL1/U-Boot 引导**：处理器上电，执行内部 iROM 与 iRAM 段，加载 U-Boot，通过 tftp 将本程序的 `template-framebuffer-gui.bin` 下载到 `0x30000000` 并跳转（`go 0x30000000`）。
2. **汇编启动 (`start.S`)**：
    - 切入 SVC 模式，关闭不严格的对齐检查。
    - 初始化堆栈：为 FIQ/IRQ/Abort/SVC 分别分配物理栈顶。
    - **使能 NEON & VFP**：修改 CP15 协处理器寄存器，开启浮点。
    - 清理 BSS 段。
    - **建立 MMU 页表**：调用 `mmu_init` 构建 16KB 页表，将外设、显存配置为不带 Cache，将 SDRAM 配置为带 Cache，开启 MMU。
3. **C 语言入口 (`main.c`)**：
    - 初始化硬件：Clock 主频提速、UART2 串口、PWM Timer4 定时器、IRQ 软中断、LCD FIMD 以及 Beep/LED/Key。
    - 初始化 LVGL：调用 `lv_init()`, 挂载显卡、挂载按键，启动 Widget Demo。
    - **非阻塞主循环**：死循环中运行 `lv_timer_handler()` 并结合 `jiffies` 作空闲节流（非阻塞睡眠），防止 100% CPU 长期空转死锁。

### 4.3 显示防撕裂技术 (PARTIAL + Shadow FB)
裸机 UI 最大的痛点是操作显存带来的撕裂（Tearing）现象。这套架构引入了**三级缓冲**策略：
1. **渲染缓冲 (render_buf)**：分配了一块仅约 491KB（120行）位于 Cache 域的缓冲区。LVGL 绘图时只需在这块极速内存中进行。
2. **影子缓冲 (shadow_fb)**：全屏尺寸 (2.4MB)，位于 Cache 域。每当 `render_buf` 渲染完一块条带（flush_cb）时，利用 `memcpy` 并入此缓冲区。这步属于 RAM 内部数据转移，耗时极短。
3. **真实显存 (vram_fb)**：全屏尺寸 (2.4MB)，由于由 FIMD 扫描必须被配置为 Non-cacheable。当一帧所有脏区域都合并至 `shadow_fb` 并触发 `lv_display_flush_is_last()` 后，**软件主动轮询 LCD 的 VSYNC 信号**，在消隐期一次性刷入真实显存。
*该机制极大限度缓解了大量细碎绘制导致的显存总线拥堵，并完全消除了半帧撕裂。*

---

## 5. 项目怎么用（快速部署指南）

### 5.1 环境准备
1. 安装 **ARM 裸机 GCC 工具链** (`arm-none-eabi-gcc`) 并添加至系统 PATH。
2. 安装 **Python 3** (运行 TFTP 脚本需要)。
3. 开发板连接串口线（COM2 波特率 115200），连接网线与宿主机在同一局域网内。

### 5.2 编译生成固件
打开终端，进入工程根目录：
```bash
# 清理并全速编译
make clean && make -j24
```
> 若使用 Windows，推荐安装 MSYS2 / MinGW 提供的 `make`。
构建成功后，在 `output/` 目录会生成主固件 `template-framebuffer-gui.bin` 和相关映射文件 `.map` / `.elf`。

### 5.3 自动化烧录测试 (强烈推荐)
Windows 平台提供了一键启动并自动测试的 PowerShell 脚本：
```powershell
powershell -ExecutionPolicy Bypass -File auto_test.ps1
```
**该脚本会：**
1. 自动启动并配置后台 TFTP 服务器（基于 `tftp_server.py`）。
2. 打开串口交互，提取当前网络 IP ，并抓取开发板 U-Boot 提示符。
3. 自动向 U-Boot 发送 `tftp` 及 `go` 指令完成程序拉取和启动。
4. 随后在控制台无缝监控程序运行串口日志输出。

### 5.4 手动烧录方式
若在串口工具中看到 `x210 # ` 的 U-Boot 提示符，可手动输入：
```text
tftp 0x30000000 template-framebuffer-gui.bin
go 0x30000000
```

### 5.5 控件交互键盘映射
系统抛弃了单一的无聊展示，引入了基于开发板物理矩阵按键的 UI 操作逻辑。通过操作按键，能在各个炫酷 LVGL 控件间跳转、交互、拉动进度条等。
| S5PV210 按键 | GPIO Pin | LVGL 映射码 | 功能描述 |
|------------|----------|-------------|----------|
| **UP**     | GPH2_0   | `LV_KEY_PREV` | 焦点切换：跳转至**上一个**可交互控件 |
| **DOWN**   | GPH0_3   | `LV_KEY_NEXT` | 焦点切换：跳转至**下一个**可交互控件 |
| **LEFT**   | GPH0_2   | `LV_KEY_LEFT` | 操作反馈：控件内**减小**值 (如进度条左滑、切换标签页) |
| **RIGHT**  | GPH2_1   | `LV_KEY_RIGHT`| 操作反馈：控件内**增大**值 (如进度条右滑) |
| **MENU**   | GPH2_3   | `LV_KEY_ENTER`| 动作触发：等同触摸**点击 / 进入修改模式** |
| **BACK**   | GPH2_2   | `LV_KEY_ESC`  | 动作触发：等同**返回 / 退出修改模式** |

---

## 6. 异常与调试信息
本系统内置了非常完备的**异常转储**及运行期心跳监测机制。如遇代码越界操作，绝不会陷入神秘死机：
- 若访问非法地址，串口会直接打印 `===== DATA ABORT =====` 以及真实崩溃点 PC 寄存器、引发页缺失的物理地址。
- 心跳监控：每 5 秒钟会在串口上报当前系统时间 `jiffies`、主循环空转 `loops` 数以及总完成的渲染帧数 `flush`，帮助开发者判定是否因高负载阻塞。
  
**正常启动日志示例：**
```text
[LVGL] Memory status:
[LVGL]   Total: 4194304 bytes
[LVGL]   Free: 4165504 bytes
...
[LOOP] tick=5000 loops=4707855 flush=34
```

## 7. 结语

这个项目完美结合了底层 ARM 汇编知识、系统架构原理与高级 GUI 图形渲染架构。通过将原本繁复且依赖操作系统的 LVGL 生态强行适配到纯粹的裸机系统之上，让每一个字节的变动、每一条指令的流动都做到心中有数。这是嵌入式学习道路上极佳的实践体验！感谢阅读！# S5PV210 LVGL Framebuffer GUI

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
