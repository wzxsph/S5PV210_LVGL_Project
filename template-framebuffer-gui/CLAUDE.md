# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bare-metal LVGL (Light and Versatile Graphics Library) v9.6.0-dev port for Samsung S5PV210 (ARM Cortex-A8) on the X210 board. Runs without an OS, using software rendering with ARM NEON SIMD optimizations. Output is a TFTP-bootable binary loaded at 0x30000000 (512MB SDRAM).

## Build Commands

### Template Framebuffer GUI (Main Target)
```bash
cd template-framebuffer-gui
make clean && make
```
Output: `template-framebuffer-gui/output/template-framebuffer-gui.bin` (TFTP bootable)

### Cross-Compiler
Uses `arm-none-eabi-gcc`. Path configured via CROSS variable:
```bash
make CROSS=arm-none-eabi-
```

### Build Artifacts
- ELF: `output/template-framebuffer-gui.elf`
- BIN: `output/template-framebuffer-gui.bin` (for TFTP boot)
- MAP: `output/template-framebuffer-gui.map`

### Note on Windows Host
The Makefile uses bash-compatible mkdir syntax. On Windows, if `make dirs` fails, run:
```bash
mkdir -p .obj output
make
```

## Architecture

### Bare-Metal Boot Flow
1. **BL0/BL1**: S5PV210 iROM boot ROM loads bootloader into iRAM
2. **Hardware Init**: Clock (APLL 1GHz, MPLL 667MHz), DRAM (512MB DDR2)
3. **LVGL Init**: `lv_init()` → display driver → input driver
4. **Main Loop**: `lv_timer_handler()` + `wfi` (Wait For Interrupt)

### Key Directories
- `template-framebuffer-gui/source/` - Application code
  - `startup/` - Boot assembly (start.S, vectors.S)
  - `hardware/` - S5PV210 HAL (clk, fb, irq, serial, tick, cp15)
  - `graphic/` - Framebuffer surface abstraction + software blit
  - `library/` - Custom stdlib (string, stdio, math, malloc)
  - `gui/` - LVGL GUI application code
- `template-framebuffer-gui/lvgl/` - LVGL v9 source tree
- `template-framebuffer-gui/include/` - Public headers
- `lvgl/` - Standalone LVGL library reference

### Memory Layout (link.ld)
- Load address: `0x30000000` (512MB SDRAM)
- Stack: 32KB SVC stack, 1KB each for FIQ/IRQ/ABT/UND
- Heap: Starts after BSS, used by LVGL's builtin malloc

### Hardware Configuration
- **Display**: 1024x600 LCD via FIMD controller, XRGB8888 (32bpp)
- **Framebuffer**: 0x3E000000 (Non-cacheable, Write-Combining)
- **Timer**: PWM Timer 0 at 100Hz (10ms jiffies) for LVGL tick
- **UART**: UART2 at 115200 baud for debug output

### LVGL Configuration (lv_conf.h)
- `LV_COLOR_DEPTH 32` - XRGB8888
- `LV_USE_OS LV_OS_NONE` - No RTOS, bare-metal
- `LV_USE_DRAW_SW 1` - Software rendering (no GPU)
- `LV_DRAW_SW_DRAW_UNIT_CNT 1` - Single-threaded rendering
- `LV_MEM_SIZE (1024U * 1024U)` - 1MB heap for LVGL

## Deployment

### TFTP Boot (auto_test.ps1)
```powershell
powershell -ExecutionPolicy Bypass -File auto_test.ps1
```
This script:
1. Starts local TFTP server (Python)
2. Downloads binary via U-Boot: `tftp 0x30000000 template-framebuffer-gui.bin`
3. Boots: `go 0x30000000`
4. Captures serial output at 115200 baud on COM6

### Manual TFTP Boot (via U-Boot)
```
tftp 0x30000000 template-framebuffer-gui.bin
go 0x30000000
```

## Debug Output

Debug messages sent via UART2 at 115200 baud. Key log sources:
- `main.c` - System init, LVGL state
- `lv_port_disp.c` - Flush callbacks, display init

LVGL log levels: TRACE, INFO, USER, WARN, ERROR. Use `LV_LOG_USER()` for user-level probe messages.

## Key Files
- `link.ld` - Linker script defining memory layout
- `lv_conf.h` - LVGL master configuration
- `source/main.c` - Application entry point
- `source/lv_port_disp.c` - Display driver adapter
- `include/hardware/s5pv210-fb.h` - Framebuffer HAL
- `include/hardware/s5pv210-tick.h` - Timer/hw-time HAL
- `lvgl/src/core/lv_refr.c` - LVGL rendering pipeline (refresh logic)
- `lvgl/src/draw/sw/lv_draw_sw.c` - SW rendering unit dispatcher
