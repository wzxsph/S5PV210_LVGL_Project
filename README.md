# S5PV210 Bare-Metal LVGL Workspace

本仓库包含三个层次：
- 主工程：`template-framebuffer-gui/`（日常开发与烧录只关注这里）
- 参考工程：`template-watchdog-timer/`
- 上游 LVGL：`lvgl/`（通常不需要改）

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
- `template-framebuffer-gui/source/`: 启动代码、硬件驱动、图形抽象、GUI 逻辑
- `template-framebuffer-gui/include/`: 公共头文件
- `template-framebuffer-gui/output/`: 构建产物（.elf/.bin/.map）
- `doc/`: 进阶脚本与补充文档
- `README.md`: 当前总览
- `CLAUDE.md`: 代码代理的工程说明

## 脚本约定

- 根目录脚本用于“快捷入口”：
	- `auto_test.ps1`
	- `tftp_server.py`
- 实际实现以 `template-framebuffer-gui/` 下脚本为准。

## 硬件参数（摘要）

- SoC: S5PV210 (ARM Cortex-A8, 1GHz)
- RAM: 512MB DDR2
- LCD: 1024x600, XRGB8888
- UART: UART2, 115200

## 常见问题

- Python 不存在：安装 Python 并保证 `python` 在 PATH 中。
- 串口无输出：确认 `-SerialPort` 与开发板实际串口一致。
- TFTP 失败：确保开发板与 PC 同网段，且未被防火墙拦截 UDP 69。
