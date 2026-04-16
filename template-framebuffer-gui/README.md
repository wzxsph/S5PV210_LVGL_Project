# S5PV210 LVGL Framebuffer GUI

裸机 LVGL v9.6.0-dev 移植项目，运行于 Samsung S5PV210 (ARM Cortex-A8) 开发板，无操作系统，使用软件渲染。

## 功能特性

- **5个自动切换的演示demo**：彩色矩形、彩虹条、圆形组合、彩色网格、渐变形状
- **全屏渲染**：1024×600 分辨率，XRGB8888 色彩格式
- **双缓冲机制**：FIMD 控制器自动切换前后缓冲区
- **调试输出**：UART2 115200 波特率

## 硬件配置

| 参数 | 值 |
|------|-----|
| SoC | Samsung S5PV210 (ARM Cortex-A8, 1GHz) |
| RAM | 512MB DDR2 |
| 显示 | 1024×600 LCD via FIMD |
| 帧缓冲 | 0x3E000000 |
| 调试串口 | UART2 @ 115200 baud |

## 快速开始

### 编译

```bash
cd template-framebuffer-gui
make clean && make
```

产物位于 `output/template-framebuffer-gui.bin`

### 烧录运行

```powershell
powershell -ExecutionPolicy Bypass -File auto_test.ps1
```

默认使用 COM6，波特率 115200。超时时间 180 秒。

## Demo 演示

系统启动后自动运行 5 个演示，每约 8-10 秒自动切换：

| Demo | 内容 | 描述 |
|------|------|------|
| 1 | 彩色矩形 | 4个大面积彩色矩形（红、绿、蓝、黄） |
| 2 | 彩虹条 | 7个水平彩色条带（红橙黄绿蓝靛紫） |
| 3 | 圆形组合 | 中心大圆 + 8个环绕小圆 |
| 4 | 彩色网格 | 4×3排列的彩色方块 |
| 5 | 渐变形状 | 顶部/底部渐变条 + 中心圆形 + 左右矩形 |

## 项目结构

```
template-framebuffer-gui/
├── source/
│   ├── main.c              # 入口，demo系统
│   ├── lv_port_disp.c      # 显示驱动适配
│   ├── startup/             # 启动代码 (start.S)
│   ├── hardware/            # 硬件抽象层 (clk, fb, irq, serial, tick)
│   ├── graphic/             # 帧缓冲抽象
│   ├── library/             # 标准库实现
│   └── gui/                 # GUI应用代码
├── include/                 # 公共头文件
├── lvgl/                    # LVGL v9 源码
├── output/                  # 构建产物
├── link.ld                  # 链接脚本
└── auto_test.ps1            # 自动烧录脚本
```

## 架构

### 启动流程
1. **BL0/BL1**: S5PV210 iROM 从 NAND/NOR 加载到 iRAM
2. **系统初始化**: 时钟、DRAM、LCD控制器
3. **LVGL 初始化**: `lv_init()` → 显示驱动 → 创建demo
4. **主循环**: `lv_timer_handler()` + 演示自动切换

### 渲染模式
- **渲染缓冲区**: 单全屏缓冲 (1024×600×4 = 2.4MB)
- **显示驱动**: `flush_cb` 通过 `memcpy` 复制像素到物理帧缓冲
- **缓冲区切换**: `screen_swap()` / `screen_flush()` 管理前后缓冲

## LVGL 配置

| 配置项 | 值 |
|--------|-----|
| 色彩深度 | 32 (XRGB8888) |
| 操作系统 | 无 (LV_OS_NONE) |
| 渲染方式 | 软件渲染 (SW) |
| 内存池 | 1MB |

## 调试

调试信息通过 UART2 输出，关键日志源：

- `source/main.c` - 系统初始化、demo切换
- `source/lv_port_disp.c` - 刷新回调、显示初始化

串口输出示例：
```
[LOOP] Auto-switching demo 1 -> 2 at loop 20
[DEMO] Switched to demo 2/5
```

## 依赖

- [arm-none-eabi-gcc](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm) 交叉编译工具链
- Python 3 (用于 TFTP 服务器)
- Windows PowerShell (用于烧录脚本)

## 分支说明

- `withoutMMU` - 当前开发分支，禁用MMU/Cache以确保显示正确
- `main` - 主分支（MMU功能开发中）

## 许可

本项目为个人学习用途，基于 LVGL v9 开源协议。
