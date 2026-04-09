# MEMORY.md - S5PV210 LVGL v9 移植项目

## 项目基本信息
- **目标平台**: Samsung S5PV210 (ARM Cortex-A8, 1GHz, NEON)
- **LVGL版本**: v9.6.0-dev
- **LCD**: 1024×600, 32bpp XRGB8888 (vs070cxn_lcd)
- **工具链**: arm-none-eabi-gcc 15.2.1 (mingw-w64)
- **BSP**: 裸机（无RTOS），自定义精简C库
- **加载方式**: U-BOOT TFTP 加载到 0x30000000

## 已识别的关键问题（2026-04-09 审阅）
1. **编译阻断**：自定义 stddef.h 与工具链 stdint.h/inttypes.h 冲突，size_t/wchar_t 未定义
2. **编译阻断**：limits.h INTMAX_MIN/MAX 重定义
3. **运行时风险**：LV_STDLIB_BUILTIN 模式下双重内存管理
4. **运行时风险**：渲染缓冲区 196KB 静态分配 + 1MB LVGL 内存池，BSS 段过大
5. **性能问题**：disp_flush 逐像素复制，应改用 memcpy
6. **配置冗余**：Makefile 编译了不适用的 Helium/NXP/Renesas 等模块

## 项目结构
- `include/library/` — 自定义精简C库头文件（stddef.h, limits.h, malloc.h 等）
- `source/library/` — 自定义C库实现（malloc, string, stdio 等）
- `source/gui/` — 旧版 GUI 绘图函数（Bresenham 画线/圆等）
- `source/lv_port_disp.c` — LVGL 显示驱动移植
- `source/main.c` — 主程序入口
- `lvgl/` — LVGL v9.6.0-dev 源码
- `lv_conf.h` — LVGL 配置文件
