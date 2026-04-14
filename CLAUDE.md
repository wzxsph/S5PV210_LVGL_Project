# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a bare-metal LVGL (Light and Versatile Graphics Library) port for the Samsung S5PV210 SoC (ARM Cortex-A8). The project runs without an operating system, directly on hardware with software rendering using ARM NEON SIMD optimizations.

## Build Commands

### Template Framebuffer GUI (Main Target)
```bash
cd template-framebuffer-gui
make clean && make
```
Output: `template-framebuffer-gui/output/template-framebuffer-gui.bin` (TFTP bootable)

### Cross-Compiler
The project uses `arm-none-eabi-gcc`. Set CROSS variable if needed:
```bash
make CROSS=arm-none-eabi-
```

### Build Artifacts
- ELF: `output/template-framebuffer-gui.elf`
- BIN: `output/template-framebuffer-gui.bin` (for TFTP boot)
- MAP: `output/template-framebuffer-gui.map`

## Architecture

### Bare-Metal Boot Flow
1. **BL0/BL1**: S5PV210 iROM boot ROM loads bootloader into iRAM
2. **Hardware Init**: Clock (APLL 1GHz, MPLL 667MHz), DRAM (512MB DDR2), MMU/Cache
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
- `lvgl/` - Standalone LVGL library (used as submodule reference)

### Memory Layout (link.ld)
- Load address: `0x30000000` (512MB SDRAM)
- Stack: 32KB SVC stack, 1KB each for FIQ/IRQ/ABT/UND

### Hardware Configuration
- **Display**: 1024x600 LCD via FIMD controller, XRGB8888 (32bpp)
- **Framebuffer**: 0x3E000000 (Non-cacheable, Write-Combining)
- **Timer**: PWM Timer 0 at 100Hz (10ms jiffies) for LVGL tick
- **UART**: UART2 at 115200 baud for debug output

### LVGL Configuration (lv_conf.h)
- `LV_COLOR_DEPTH 32` - XRGB8888
- `LV_USE_OS LV_OS_NONE` - No RTOS
- `LV_USE_DRAW_SW 1` - Software rendering (no GPU)
- `LV_DRAW_SW_DRAW_UNIT_CNT 1` - Single-threaded rendering
- `LV_MEM_SIZE (1024U * 1024U)` - 1MB heap for LVGL

### Display Driver (lv_port_disp.c)
- Uses partial rendering buffer (48 lines = ~192KB)
- `flush_cb` copies rendered regions to physical framebuffer via `memcpy`
- Framebuffer at `s5pv210_screen_surface()->pixels`

### Debug Output
Debug messages sent via UART2 at 115200 baud. Key log sources:
- `main.c` - System init, LVGL state
- `lv_port_disp.c` - Flush callbacks, display init

## Key Files
- `link.ld` - Linker script defining memory layout
- `lv_conf.h` - LVGL master configuration
- `source/main.c` - Application entry point
- `source/lv_port_disp.c` - Display driver adapter
- `include/hardware/s5pv210-fb.h` - Framebuffer HAL
- `include/hardware/s5pv210-tick.h` - Timer/hw-time HAL
