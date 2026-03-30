# S5PV210 Bare-metal LVGL Project

This repository provides a high-performance, modular bare-metal port of **LVGL v8.3** for the **Samsung S5PV210** (Cortex-A8) development board (e.g., X210/Smart210).

## 🚀 Key Features
- **Modern Architecture**: Clean separation of `arch`, `bsp`, `app`, and `porting` layers.
- **Optimized Display**: Custom FIMD (LCD) driver configured for **16-bit (RGB565)** color depth for high frame rates and lower memory bandwidth.
- **Precise Timing**: 1ms PWM4 system-tick integrated with LVGL's custom tick expression.
- **Cross-Platform Build**: Robust Makefile compatible with both Windows and Linux using `arm-none-eabi-gcc`.
- **Self-Contained**: LVGL v8.3 source is bundled directly for "clone and build" convenience.

## 🛠️ Build Requirements
- **Toolchain**: `arm-none-eabi-gcc` (Recommendation: version 10.3 or higher).
- **Build Tool**: `make` (Windows users can use Git Bash or MinGW-make).

## 🔨 How to Build
1. Clone the repository:
   ```bash
   git clone https://github.com/wzxsph/S5PV210_LVGL_Project.git
   cd S5PV210_LVGL_Project
   ```
2. Compile:
   ```bash
   make
   ```
3. Find your firmware in the `output/` directory:
   - `s5pv210_lvgl_demo.bin`: Raw binary for flashing.
   - `s5pv210_lvgl_demo.elf`: ELF file with debug symbols.

## 📼 Flashing and Running
### Method 1: USB Download (DNW)
1. Enter U-Boot on your board.
2. Execute `dnw 30000000` via serial terminal.
3. Use the **DNW tool** to send `output/s5pv210_lvgl_demo.bin`.
4. Run: `go 30000000`.

### Method 2: SD Card Boot
1. Add the S5PV210 boot header using `mkv210` tool.
2. Burn to SD card using an SD card burner tool.
3. Set your board boot switch to SD mode.

## 📁 Directory Structure
- `arch/`: CPU startup (`start.S`), clock, and interrupt handling.
- `bsp/`: Hardware drivers (LCD, UART, Timer).
- `app/`: Application logic (`main.c`) and UI code.
- `porting/`: LVGL display and input device bridges.
- `lvgl/`: LVGL v8.3 core source files.
- `include/`: Common headers and library support.

## 📝 Author
[wzxsph](https://github.com/wzxsph)
