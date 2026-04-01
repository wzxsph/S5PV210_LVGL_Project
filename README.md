# S5PV210 Bare-metal LVGL Project

> [!CAUTION]
> **本代码还未完成调试，正在进行中，请勿参考！**
> **This code is still under debugging and is NOT yet complete. DO NOT refer to it.**

This repository provides a high-performance, modular bare-metal port of **LVGL v8.3** for the **Samsung S5PV210** (Cortex-A8) development board.

## 🚀 Key Features
- **Modern Architecture**: Modular structure with `arch`, `bsp`, `app`, and `porting` layers.
- **16-bit Optimized**: FIMD driver configured for **RGB565** (16bpp) for maximum performance.
- **U-Boot Support**: Fine-tuned for jumping from U-Boot (addresses PLL/clock re-init issues).
- **Embedded Diagnostics**: Early serial output for hardware troubleshooting.

## 🛠️ Installation & Dependency Setup

The project source code does NOT include the LVGL library to keep the repository lightweight. You must set it up manually:

### 1. Download LVGL
Clone or download **LVGL v8.3** into the project root:

```bash
# From the project root
git clone -b release/v8.3 https://github.com/lvgl/lvgl.git
```

### 2. Project Structure
Ensure your directory looks like this:
```text
S5PV210_LVGL_Project/
├── app/          # UI Logic
├── arch/         # Startup and Clock
├── bsp/          # LCD, Serial, Timer Drivers
├── lvgl/         # <--- Place LVGL source here
├── porting/      # LVGL <-> Hardware Bridge
├── lv_conf.h     # LVGL Config
├── Makefile
└── s5pv210.lds
```

## 🔨 How to Build
1. **Toolchain**: Ensure `arm-none-eabi-gcc` is in your PATH.
2. **Compile**:
   ```bash
   make clean
   make
   ```
3. **Output**: Your firmware will be generated in `output/s5pv210_lvgl_demo.bin`.

## 📼 Flashing (TFTP Example)
If your board is running U-Boot:
1. Connect via Serial and Ethernet.
2. In U-Boot terminal:
   ```bash
   tftp 30000000 s5pv210_lvgl_demo.bin
   go 30000000
   ```

## 📝 Important Notes
- **Heap Size**: The internal heap is set to **4MB** (in `library/malloc/malloc.c`) to ensure fast boot and stability.
- **Clock init**: When booting from TFTP/U-Boot, clock initialization is skipped in `main.c` to prevent hardware hangs.

## Author
[wzxsph](https://github.com/wzxsph)
