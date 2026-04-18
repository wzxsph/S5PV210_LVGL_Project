# S5PV210 Bare-Metal LVGL Workspace

本仓库包含三个层次：
- 主工程：`template-framebuffer-gui/`（日常开发与烧录只关注这里）
- 参考工程：`template-watchdog-timer/`
- 上游 LVGL：`lvgl/`（通常不需要改）

## 分支说明

- **`withoutMMU`** - 当前开发分支，禁用MMU/Cache以确保显示正确，包含5个自动切换的Demo演示
- **`main`** - 主分支（MMU功能开发中）

## 推荐工作流（最简）

1. 编译主工程

```bash
cd template-framebuffer-gui
make clean && make
```

产物：`template-framebuffer-gui/output/template-framebuffer-gui.bin`

2. 一键下载并启动

```powershell
powershell -ExecutionPolicy Bypass -File auto_test.ps1 -SerialPort COM6
```

说明：根目录 `auto_test.ps1` 是统一入口，会转发到 `template-framebuffer-gui/auto_test.ps1`。

## 目录导航

- `template-framebuffer-gui/`: 主项目（代码、链接脚本、构建输出）
  - `source/main.c`: 入口程序，包含5个自动切换的Demo
  - `source/lv_port_disp.c`: LVGL显示驱动适配
  - `source/startup/`: ARM启动代码
  - `source/hardware/`: S5PV210硬件抽象层
- `template-framebuffer-gui/include/`: 公共头文件
- `template-framebuffer-gui/output/`: 构建产物（.elf/.bin/.map）
- `lvgl/`: LVGL v9 上游源码
- `doc/`: 进阶脚本与补充文档
- `README.md`: 当前总览
- `CLAUDE.md`: Claude AI 代理的工程说明

## 脚本约定

- 根目录脚本用于”快捷入口”：
	- `auto_test.ps1`
	- `tftp_server.py`
- 实际实现以 `template-framebuffer-gui/` 下脚本为准。

## 硬件参数（摘要）

- SoC: S5PV210 (ARM Cortex-A8, 1GHz)
- RAM: 512MB DDR2
- LCD: 1024x600, XRGB8888
- UART: UART2, 115200

## 5个自动演示Demo

系统启动后自动运行5个演示，每约8-10秒自动切换：

| Demo | 内容 | 描述 |
|------|------|------|
| 1 | 彩色矩形 | 4个大面积彩色矩形（红、绿、蓝、黄） |
| 2 | 彩虹条 | 7个水平彩色条带（红橙黄绿蓝靛紫） |
| 3 | 圆形组合 | 中心大圆 + 8个环绕小圆 |
| 4 | 彩色网格 | 4×3排列的彩色方块 |
| 5 | 渐变形状 | 顶部/底部渐变条 + 中心圆形 + 左右矩形 |

## 常见问题

- Python 不存在：安装 Python 并保证 `python` 在 PATH 中。
- 串口无输出：确认 `-SerialPort` 与开发板实际串口一致。
- TFTP 失败：确保开发板与 PC 同网段，且未被防火墙拦截 UDP 69。
