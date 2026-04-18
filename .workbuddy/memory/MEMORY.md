# MEMORY.md - S5PV210 LVGL v9 移植项目

## 项目基本信息
- **目标平台**: Samsung S5PV210 (ARM Cortex-A8, 1GHz, NEON)
- **LVGL版本**: v9.6.0-dev
- **LCD**: 1024×600, 32bpp XRGB8888 (vs070cxn_lcd)
- **工具链**: arm-none-eabi-gcc 15.2.1 (mingw-w64)
- **BSP**: 裸机（无RTOS），自定义精简C库
- **加载方式**: U-BOOT TFTP 加载到 0x30000000

## 已完成的修复（2026-04-09）
1. **LCD 像素格式修正**：BGR_P → RGB_P，关闭 WORD_SWAP，修正 rgba field 位置
   - 使 LCD 控制器像素格式与 LVGL XRGB8888 完全匹配
2. **disp_flush 性能优化**：逐像素复制 → 行级 memcpy
3. **lv_conf.h 优化**：关闭 BUILD_EXAMPLES/DEMOS、降低日志级别、关闭 TRACE

## 当前阻塞问题（2026-04-09）
- **lv_timer_handler() 卡死**：第一次调用即死循环，无论是否创建控件
- 原因定位到 lv_refr.c 的 `draw_buf_flush()` 中 `while(layer->draw_task_head)` 循环
- 可能是 LVGL v9.6.0-dev 软件渲染器在单线程裸机模式下的 bug
- 手动写 framebuffer 正常（RGB 条纹显示正常），LCD 硬件工作正常
- 启用 LV_USE_THEME_DEFAULT 时，lv_display_create() 内部也会卡死

## 项目结构
- `include/library/` — 自定义精简C库头文件（stddef.h, limits.h, malloc.h 等）
- `source/library/` — 自定义C库实现（malloc, string, stdio 等）
- `source/gui/` — 旧版 GUI 绘图函数（Bresenham 画线/圆等）
- `source/lv_port_disp.c` — LVGL 显示驱动移植
- `source/main.c` — 主程序入口
- `lvgl/` — LVGL v9.6.0-dev 源码
- `lv_conf.h` — LVGL 配置文件

## 自动测试
- `auto_test.ps1` — 自动 TFTP 烧录 + 串口输出捕获（COM6, 115200）
