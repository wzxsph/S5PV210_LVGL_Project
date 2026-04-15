# S5PV210 LVGL Bare-Metal GUI

Samsung S5PV210 (ARM Cortex-A8) 无操作系统 LVGL 图形库移植。

## 硬件配置

- **SoC**: Samsung S5PV210 @ 1GHz (ARM Cortex-A8 + NEON SIMD)
- **内存**: 512MB DDR2 @ 667MHz
- **显示**: 1024x600 LCD, XRGB8888 (32bpp)
- **调试**: UART2 @ 115200 baud

## 快速开始

### 1. 编译

```bash
cd template-framebuffer-gui
make clean && make
```

输出: `output/template-framebuffer-gui.bin`

### 2. 配置网络 (TFTP 服务器)

TFTP 服务器地址需与 S5PV210 开发板在同一网段。

编辑 `tftp_server.py` 第 8 行，确认 `ROOT_DIR` 指向 output 目录：

```python
ROOT_DIR = "C:\\Users\\<你的用户名>\\Desktop\\maybe\\LVGL_baseS5PV210_20260407\\template-framebuffer-gui\\output"
```

确保 Windows PC IP 为 `192.168.1.x` 网段（脚本会自动扫描），或手动指定。

### 3. 配置串口

编辑 `auto_test.ps1` 第 2 行：

```powershell
[string]$SerialPort = "COM6"  # 修改为你的串口号
```

### 4. 运行测试

```powershell
powershell -ExecutionPolicy Bypass -File auto_test.ps1
```

脚本流程：
1. 启动 TFTP 服务器
2. 连接串口
3. 发送 `tftp 0x30000000 template-framebuffer-gui.bin`
4. 传输完成后发送 `go 0x30000000` 启动程序

## 目录结构

```
template-framebuffer-gui/
├── source/              # 应用代码
│   ├── startup/          # 启动汇编
│   ├── hardware/        # S5PV210 HAL
│   ├── graphic/         # 帧缓冲 + 软件渲染
│   ├── library/         # 标准库实现
│   └── gui/             # LVGL 应用
├── lvgl/                # LVGL v9 源码
├── include/             # 公共头文件
├── output/              # 编译输出
├── link.ld              # 链接脚本
└── lv_conf.h            # LVGL 配置
```

## 内存布局

| 区域 | 地址 |
|------|------|
| SDRAM | 0x30000000 (512MB) |
| 帧缓冲 | 0x3E000000 |
| 栈 (SVC) | 32KB |

## 调试输出

程序启动后通过 UART2 输出调试信息，波特率 115200。
