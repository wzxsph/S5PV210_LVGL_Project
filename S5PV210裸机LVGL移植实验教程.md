# **实验指导书：S5PV210裸机LVGL图形库移植实验 (Study210 + TFTP版)**

## **1. 实验背景与目标**

**实验类型：** 裸机实验

**适用平台：** Study210 实验箱 (S5PV210 Cortex-A8)
**运行方式：** U-boot 引导 + TFTP 网络下载运行

**实验目标：**

1. 熟练掌握 ARM 嵌入式平台的开发方法；
2. 了解软件移植的基本方法；
3. 了解图形编程的基本方法。

**背景知识：**

LVGL (Light and Versatile Graphics Library) 是一个免费的开源图形库，它提供创建嵌入式GUI所需的很多内容，具有易于使用的图形元素、精美的视觉效果和低内存占用。目前 LVGL 已经被移植到很多 MCU 和嵌入式 Linux 系统。本实验通过学习相关资料，将 LVGL 移植到课程所提供的实验箱的裸机环境中。

本实验使用的 LVGL 版本为 **v9.6.0-dev**，这是 LVGL 的最新主线版本，相较于 v8.x 系列在 API 层面有较大变更（如 Display、Tick 接口的重新设计），下文所有代码均基于 v9 API。

本课程使用的 S5PV210 带有 PowerVR SGX535 GPU，但是由于 GPU 编程超出本课程范畴，所以移植时**不要求支持GPU**，完全依赖 CPU 进行纯软件渲染，将图像数据直接输出到 LCD 的帧缓冲（Framebuffer）中。

**参考资料：**

\[1\] LVGL 官方文档：https://docs.lvgl.io/master/index.html

\[2\] 《x210v3 开发板裸机教程》中的**实例 21**

\[3\] 《嵌入式linux核心课程学习指导精华》

## **2. 实验准备**

### **2.1 提取基础底层工程**

本次移植的基础代码为 **template-framebuffer-gui** 工程（源自《x210v3 开发板裸机教程》实例 21 的改进版）。该工程已提供完整的底层驱动，包括：

| 模块 | 源文件 | 说明 |
|------|--------|------|
| 启动代码 | `source/startup/start.S` | 异常向量表、栈初始化、BSS清零 |
| 时钟 | `source/hardware/s5pv210-clk.c` | 系统时钟初始化 |
| 中断 | `source/hardware/s5pv210-irq.c` | VIC 中断控制器驱动 |
| 定时器 | `source/hardware/s5pv210-tick.c` | PWM Timer4，100Hz tick (jiffies) |
| 延时 | `source/hardware/s5pv210-tick-delay.c` | `mdelay()` / `udelay()` 实现 |
| 串口 | `source/hardware/s5pv210-serial.c` | UART 驱动 |
| LCD | `source/hardware/s5pv210-fb.c` | LCD控制器 + 双缓冲Framebuffer |
| 标准库 | `source/library/` | 自实现 malloc、string、stdio 等 |
| 图形库 | `source/gui/` | 简易GUI绘图函数（移植后可移除）|

**⚠️ 关键步骤：** 请先确保该工程能够成功编译，并通过 **TFTP** 将编译出的 `.bin` 文件下载到 Study210 的内存中运行测试，确认 LCD 能正常点亮并显示随机图形画面。

### **2.2 了解底层工程关键参数**

在开始移植前，请先确认以下关键硬件参数（从 template 工程中提取）：

**LCD 分辨率与颜色格式（来自 `s5pv210-fb.c`）：**

```c
static struct s5pv210fb_lcd vs070cxn_lcd = {
    .width              = 1024,
    .height             = 600,
    .bits_per_pixel     = 32,
    .bytes_per_pixel    = 4,
    .freq               = 60,
    .output             = S5PV210FB_OUTPUT_RGB,
    .rgb_mode           = S5PV210FB_MODE_BGR_P,
    .bpp_mode           = S5PV210FB_BPP_MODE_32BPP,
    .swap               = S5PV210FB_SWAP_WORD,
    // ...
};
```

- **分辨率**：1024 × 600
- **颜色深度**：32bpp（XRGB8888）
- **RGB 模式**：BGR_P（蓝色在低位）
- **双缓冲**：`vram_front` 和 `vram_back`，静态分配 `u8_t vram[2][1024 * 600 * 4]`

**系统 Tick（来自 `s5pv210-tick.c`）：**

```c
volatile u32_t jiffies = 0;   // tick 计数器
static u32_t tick_hz = 0;     // tick 频率

// Timer4 每 10ms 中断一次，tick_hz = 100
// jiffies 每秒增加 100
```

- Tick 频率为 **100Hz**（每 10ms 一次中断），全局变量 `jiffies` 自增
- 可通过 `jiffies * 1000 / 100` 换算为毫秒

**内存链接起始地址（来自 `link.ld`）：**

```
MEMORY
{
    ram (rwx) : org = 0x30000000, len = 0x20000000  /* 512 MB */
}
```

- TEXT_BASE = **0x30000000**
- SVC 栈大小 = **0x8000**（32KB），足以支撑 LVGL

### **2.3 获取 LVGL 源码**

1. 访问 LVGL GitHub 仓库，获取 **v9.6.x** 版本源码。
2. 将整个 LVGL 源码目录放置在工程根目录下，命名为 `lvgl/`，与 `source/`、`include/` 同级。

工程目录结构应如下：

```
template-framebuffer-gui/
├── Makefile
├── link.ld
├── include/
│   ├── main.h
│   ├── hardware/           # 硬件驱动头文件
│   ├── library/            # 标准库头文件
│   └── graphic/            # 图形库头文件
├── source/
│   ├── main.c
│   ├── startup/start.S
│   ├── hardware/           # 硬件驱动源码
│   ├── library/            # 标准库实现
│   ├── graphic/            # 图形库实现
│   └── gui/                # 简易GUI（移植后可移除）
├── lvgl/                   # ← LVGL 源码（新增）
│   ├── lvgl.h
│   ├── lv_conf_template.h
│   ├── src/
│   │   ├── core/
│   │   ├── display/
│   │   ├── draw/
│   │   ├── font/
│   │   ├── indev/
│   │   ├── misc/
│   │   ├── tick/
│   │   ├── widgets/
│   │   └── ...
│   └── ...
└── lv_conf.h               # ← 配置文件（从模板复制，新增）
```

**⚠️ Study210 硬件差异特别提醒：** 根据《学习指导精华》，**Study210 和老款 x210v3 在 LCD 显示屏（引脚定义/时序）和触摸屏芯片上存在差异**。请在提取底层驱动时，务必确认您使用的是适配 **Study210** 版本的 LCD 初始化代码。如果使用了 x210v3 的代码，TFTP 运行后大概率会出现白屏或花屏。

## **3. 移植步骤**

### **步骤一：创建 lv_conf.h 配置文件**

1. 将 `lvgl/lv_conf_template.h` **复制** 到工程根目录，重命名为 `lv_conf.h`（与 `lvgl/` 目录同级）。
2. 打开 `lv_conf.h`，将文件开头的 `#if 0` 改为 `#if 1` 以启用配置。
3. 根据 S5PV210 实验箱的硬件参数修改核心配置项：

**颜色深度** — LCD 使用 32bpp XRGB8888：

```c
/** Color depth: 1 (I1), 8 (L8), 16 (RGB565), 24 (RGB888), 32 (XRGB8888) */
#define LV_COLOR_DEPTH 32
```

**内存管理** — 使用 LVGL 内建 malloc，分配足够的内存池：

```c
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    /** 为 LVGL 分配 256KB 内存池，Study210 DDR 内存充足 */
    #define LV_MEM_SIZE (256U * 1024U)
#endif
```

> **说明：** LVGL v9 不再使用 `LV_MEM_CUSTOM` 宏。`LV_USE_STDLIB_MALLOC` 设为 `LV_STDLIB_BUILTIN` 表示使用 LVGL 自带的内存分配器，无需依赖标准 C 库的 `malloc`。`LV_MEM_SIZE` 控制内存池大小。对于 Study210 的 512MB DDR，分配 256KB 甚至 1MB 均无问题。

**操作系统** — 裸机环境，不使用 OS：

```c
#define LV_USE_OS   LV_OS_NONE
```

**渲染配置** — 使用纯软件渲染：

```c
#define LV_USE_DRAW_SW 1
```

**标准头文件路径** — 由于裸机工程自带标准库实现，需要确保以下路径配置正确（通常默认值即可，若编译报错则根据实际情况修改）：

```c
#define LV_STDINT_INCLUDE       <stdint.h>
#define LV_STDDEF_INCLUDE       <stddef.h>
#define LV_STDBOOL_INCLUDE      <stdbool.h>
#define LV_INTTYPES_INCLUDE     <inttypes.h>
#define LV_LIMITS_INCLUDE       <limits.h>
#define LV_STDARG_INCLUDE       <stdarg.h>
```

> **注意：** 裸机工程的 `include/library/` 下提供了 `stddef.h`、`limits.h`、`stdarg.h`、`types.h`等，但可能缺少 `stdint.h`、`stdbool.h`、`inttypes.h`。这些头文件由 arm-none-eabi-gcc 编译器自带提供。若编译时报找不到头文件，请检查编译器的系统 include 路径是否被正确搜索。

**字体配置** — 至少启用一个默认字体：

```c
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
```

**关闭不需要的功能** — 减小代码体积和编译时间：

```c
#define LV_USE_LOG          0       /* 可暂时关闭日志 */
#define LV_USE_FLOAT        0       /* 裸机通常不使用浮点 */
#define LV_USE_MATRIX       0
```

### **步骤二：提供系统心跳 (Tick)**

LVGL 需要一个毫秒级时间源来驱动动画和定时逻辑。在 **LVGL v9** 中，Tick 的提供方式是**注册一个回调函数**，通过 `lv_tick_set_cb()` 告知 LVGL 如何获取当前毫秒时间戳。

template 工程中 `s5pv210-tick.c` 已实现了 PWM Timer4 的 100Hz 中断，全局变量 `jiffies` 每 10ms 自增 1。我们只需编写一个换算函数，将 `jiffies` 转换为毫秒：

**在工程中新增（或在 main.c 中添加）一个时间获取函数：**

```c
#include <s5pv210-tick.h>

/* 将 jiffies (100Hz) 换算为毫秒 */
uint32_t get_system_time_ms(void)
{
    return (uint32_t)(jiffies * 10);  /* 每个 jiffie = 10ms */
}
```

> **原理说明：** `s5pv210-tick.c` 中配置 Timer4 的定时周期为 `pclk / (2 * 16 * 100)`，即 100Hz。`jiffies` 是 `volatile u32_t` 类型，在每次 Timer4 中断中自增。因此 `jiffies * 10` 即为运行毫秒数。该函数将在 `main()` 中通过 `lv_tick_set_cb()` 注册给 LVGL。

### **步骤三：移植显示驱动**

LVGL v9 的显示驱动移植不再使用 `lv_port_disp_template.c` 中的 v8 风格 API（`lv_disp_drv_t` 等已废弃）。取而代之的是更简洁的 v9 API：

1. `lv_display_create()` — 创建显示设备
2. `lv_display_set_flush_cb()` — 设置刷屏回调
3. `lv_display_set_buffers()` — 设置渲染缓冲区

**编写 `lv_port_disp.c`（在 source/ 目录下新建）：**

```c
#include "lvgl/lvgl.h"
#include <s5pv210-fb.h>
#include <s5pv210-tick.h>

/* LCD 分辨率 - 与 s5pv210-fb.c 中的 vs070cxn_lcd 一致 */
#define MY_DISP_HOR_RES  1024
#define MY_DISP_VER_RES  600

/* 渲染缓冲区：分配 48 行的部分缓冲（约 192KB）*/
static uint8_t buf_1[MY_DISP_HOR_RES * 48 * 4];

/*-----------------------------------------------------
 * flush_cb: 将 LVGL 渲染好的像素数据写入 Framebuffer
 *----------------------------------------------------*/
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    /*
     * 获取 Framebuffer 首地址。
     * s5pv210_screen_surface() 返回当前前缓冲 surface，
     * 其 pixels 成员即为显存首地址。
     */
    struct surface_t * surface = s5pv210_screen_surface();
    uint32_t * fb_base = (uint32_t *)surface->pixels;

    int32_t x, y;
    uint32_t * color_p = (uint32_t *)px_map;

    for(y = area->y1; y <= area->y2; y++) {
        for(x = area->x1; x <= area->x2; x++) {
            fb_base[y * MY_DISP_HOR_RES + x] = *color_p;
            color_p++;
        }
    }

    /* 通知 LVGL 刷新完成 */
    lv_display_flush_ready(disp);
}

/*-----------------------------------------------------
 * 显示接口初始化
 *----------------------------------------------------*/
void lv_port_disp_init(void)
{
    /* 注意：s5pv210_fb_initial() 已在 do_system_initial() 中被调用，
     * LCD 硬件此时已完成初始化并点亮背光。此处无需重复初始化。
     */

    /* 1. 创建显示设备对象 */
    lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);

    /* 2. 设置颜色格式为 XRGB8888（与 LCD 32bpp 一致） */
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);

    /* 3. 设置渲染缓冲区（单缓冲 + 部分渲染模式） */
    lv_display_set_buffers(disp, buf_1, NULL, sizeof(buf_1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* 4. 注册刷屏回调函数 */
    lv_display_set_flush_cb(disp, disp_flush);
}
```

**关键 API 说明（对比 v8）：**

| LVGL v8 (旧) | LVGL v9 (新) | 说明 |
|---|---|---|
| `lv_disp_draw_buf_init()` | `lv_display_set_buffers()` | 设置渲染缓冲区 |
| `lv_disp_drv_init()` | `lv_display_create()` | 创建显示设备 |
| `lv_disp_drv_register()` | 不再需要 | 创建即注册 |
| `disp_drv.flush_cb` | `lv_display_set_flush_cb()` | 设置刷屏回调 |
| `lv_disp_flush_ready(&disp_drv)` | `lv_display_flush_ready(disp)` | 通知刷新完成 |
| `lv_color_t` 像素 | `uint8_t * px_map` 原始字节 | flush_cb 参数类型变更 |

### **步骤四：修改 Makefile**

需要在 Makefile 中添加 LVGL 源码的编译支持。LVGL v9 源码目录层次较深，需要将所有子目录加入编译。

**修改 INCDIRS（头文件包含路径）：**

在原有 `INCDIRS` 之后添加：

```makefile
# LVGL include paths
INCDIRS     += .                    \
               lvgl                 \
               lvgl/src
```

> **说明：** `lv_conf.h` 放在工程根目录（`.`），`lvgl.h` 在 `lvgl/` 目录，而 LVGL 内部头文件的相互引用基于 `lvgl/src/` 路径。将这三个路径加入 INCDIRS 可确保所有头文件都能被正确找到。

**修改 SRCDIRS（源文件目录）：**

在原有 `SRCDIRS` 之后添加 LVGL 的所有源码子目录：

```makefile
# LVGL source directories
SRCDIRS     += lvgl/src                                     \
               lvgl/src/core                                \
               lvgl/src/display                             \
               lvgl/src/draw                                \
               lvgl/src/draw/sw                             \
               lvgl/src/draw/sw/blend                       \
               lvgl/src/font                                \
               lvgl/src/font/fmt_txt                        \
               lvgl/src/indev                               \
               lvgl/src/layouts                             \
               lvgl/src/layouts/flex                        \
               lvgl/src/layouts/grid                        \
               lvgl/src/libs/bin_decoder                    \
               lvgl/src/misc                                \
               lvgl/src/misc/cache                          \
               lvgl/src/osal                                \
               lvgl/src/others                              \
               lvgl/src/stdlib                              \
               lvgl/src/stdlib/builtin                      \
               lvgl/src/themes                              \
               lvgl/src/themes/default                      \
               lvgl/src/themes/simple                       \
               lvgl/src/tick                                \
               lvgl/src/widgets/animimage                   \
               lvgl/src/widgets/arc                         \
               lvgl/src/widgets/bar                         \
               lvgl/src/widgets/button                      \
               lvgl/src/widgets/buttonmatrix                \
               lvgl/src/widgets/canvas                      \
               lvgl/src/widgets/checkbox                    \
               lvgl/src/widgets/dropdown                    \
               lvgl/src/widgets/image                       \
               lvgl/src/widgets/imagebutton                 \
               lvgl/src/widgets/keyboard                    \
               lvgl/src/widgets/label                       \
               lvgl/src/widgets/led                         \
               lvgl/src/widgets/line                        \
               lvgl/src/widgets/list                        \
               lvgl/src/widgets/menu                        \
               lvgl/src/widgets/msgbox                      \
               lvgl/src/widgets/objx_templ                  \
               lvgl/src/widgets/roller                      \
               lvgl/src/widgets/scale                       \
               lvgl/src/widgets/slider                      \
               lvgl/src/widgets/span                        \
               lvgl/src/widgets/spinbox                     \
               lvgl/src/widgets/spinner                     \
               lvgl/src/widgets/switch                      \
               lvgl/src/widgets/table                       \
               lvgl/src/widgets/tabview                     \
               lvgl/src/widgets/textarea                    \
               lvgl/src/widgets/tileview                    \
               lvgl/src/widgets/win
```

> **💡 提示：** 如果编译出现"找不到源文件"错误，说明某些源码子目录未被包含。请检查 `lvgl/src/` 下实际存在的子目录，并确保都被加入 `SRCDIRS`。LVGL v9 可能包含更多子目录（如 `debugging/`、`others/` 子目录），按需添加。

**同步更新 .obj 目录的创建** — Makefile 末尾的 Windows 目录创建段需要添加对应的 `.obj\lvgl\...` 目录。由于 LVGL 子目录众多，建议改为通用的自动创建方式。若使用 Linux/MSYS2 编译环境，`$(shell $(MKDIR) $(X_OBJDIRS) $(X_OUT))` 会自动处理。

**关闭不必要的编译警告（可选）:**

由于 LVGL 源码使用了一些 GCC 特性和类型转换，裸机环境中可能会产生大量警告。可在 `CFLAGS` 中添加：

```makefile
CFLAGS      := -g -ggdb -Wall -O3 -Wno-unused-function -Wno-unused-variable
```

### **步骤五：解决头文件和类型冲突**

由于裸机工程自带了一套简化的标准库，可能与 LVGL 期望的标准 C 类型定义冲突。常见问题及解决方案：

**1. 缺少 `stdint.h` / `stdbool.h` / `inttypes.h`**

LVGL 依赖标准 C99 类型（`uint8_t`、`int32_t`、`bool` 等）。裸机工程的 `types.h` 定义了自己的类型（`u8_t`、`s32_t` 等），但未定义标准名称。

**解决方案：** `arm-none-eabi-gcc` 编译器自带这些标准头文件，通常能自动找到。如果编译报错，可以在 `lv_conf.h` 中将这些 include 指向编译器自带版本（默认配置通常可用），或者在工程的 `include/library/` 下创建兼容层：

```c
/* include/library/stdint.h (如果编译器未自动提供) */
#ifndef __STDINT_H__
#define __STDINT_H__
#include <types.h>
typedef s8_t   int8_t;
typedef u8_t   uint8_t;
typedef s16_t  int16_t;
typedef u16_t  uint16_t;
typedef s32_t  int32_t;
typedef u32_t  uint32_t;
typedef s64_t  int64_t;
typedef u64_t  uint64_t;
#endif
```

```c
/* include/library/stdbool.h (如果编译器未自动提供) */
#ifndef __STDBOOL_H__
#define __STDBOOL_H__
#ifndef __cplusplus
typedef int bool;
#define true  1
#define false 0
#endif
#endif
```

**2. `size_t` 重复定义**

裸机 `types.h` 和编译器内置 `stddef.h` 都定义了 `size_t`。可在裸机 `types.h` 中添加条件编译保护：

```c
#ifndef _SIZE_T_DEFINED
typedef unsigned int size_t;
#define _SIZE_T_DEFINED
#endif
```

## **4. 主程序集成与测试**

修改 `source/main.c`，集成 LVGL 主循环：

```c
#include <main.h>
#include "lvgl/lvgl.h"

/* 外部声明：显示接口初始化 */
extern void lv_port_disp_init(void);

/* 外部声明：毫秒时间获取函数 */
extern uint32_t get_system_time_ms(void);

static void do_system_initial(void)
{
    malloc_init();

    s5pv210_clk_initial();
    s5pv210_irq_initial();
    s5pv210_tick_initial();
    s5pv210_tick_delay_initial();
    s5pv210_serial_initial();
    s5pv210_fb_initial();      /* LCD 硬件初始化 + 背光开启 */

    led_initial();
    beep_initial();
    key_initial();
}

int main(int argc, char * argv[])
{
    /* 1. 硬件底层初始化（保留原有的全部初始化流程） */
    do_system_initial();

    /* 2. 初始化 LVGL 核心 */
    lv_init();

    /* 3. 注册 Tick 回调 — 告知 LVGL 如何获取毫秒级时间戳 */
    lv_tick_set_cb(get_system_time_ms);

    /* 4. 初始化并注册显示接口 */
    lv_port_disp_init();

    /* 5. 创建一个测试用的按钮和标签 */
    lv_obj_t * btn = lv_button_create(lv_screen_active());
    lv_obj_center(btn);
    lv_obj_set_size(btn, 300, 80);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Hello Study210 LVGL!");
    lv_obj_center(label);

    /* 6. LVGL 任务轮询主循环 */
    while(1) {
        lv_timer_handler();     /* LVGL v9 的任务处理入口 */
        mdelay(5);              /* 适当延时，降低 CPU 占用 */
    }

    return 0;
}
```

**v8 → v9 API 变更要点：**

| LVGL v8 (旧) | LVGL v9 (新) | 说明 |
|---|---|---|
| `lv_task_handler()` | `lv_timer_handler()` | 主循环任务处理 |
| `lv_scr_act()` | `lv_screen_active()` | 获取当前活动屏幕 |
| `lv_btn_create()` | `lv_button_create()` | 按钮创建函数 |
| `LV_TICK_CUSTOM` 宏配置 | `lv_tick_set_cb()` 运行时注册 | Tick 提供方式 |

## **5. 编译与 TFTP 运行指南**

### **5.1 编译工程**

在 Linux 虚拟机或 Windows MSYS2 / WSL 环境下执行编译。建议的编译工具链路径配置（以 Windows 为例）：

```
编译器路径：d:\compiler\bin 和 d:\compiler\arm-2013.05\bin
工具链前缀：arm-none-eabi-
```

```bash
# 进入工程目录
cd template-framebuffer-gui

# 清理旧的编译产物
make clean

# 编译
make

# 编译成功后，输出文件位于：
# output/template-framebuffer-gui.bin
```

> **⚠️ 首次编译 LVGL 源码较慢**（数百个 .c 文件），请耐心等待。后续修改只需增量编译。

### **5.2 TFTP 下载与运行**

1. **准备 TFTP 环境：**
   - 将编译好的 `output/template-framebuffer-gui.bin` 复制到电脑端 TFTP 服务器软件（如 Tftpd64）的指定根目录下。
   - 确保电脑与 Study210 开发板通过网线连接，并且关闭了电脑的防火墙。

2. **U-boot 配置与下载：** 通过串口终端（SecureCRT / Xshell）连接开发板，开启电源，**在倒计时结束前按回车键，进入 U-boot 命令行**。
   - 配置网络 IP（如果之前未配置过）：

     ```
     setenv ipaddr 192.168.1.10    # 开发板IP
     setenv serverip 192.168.1.100 # 电脑IP（需同网段）
     saveenv
     ```

   - 测试网络连通性：

     ```
     ping 192.168.1.100
     ```

   - 下载固件到内存（链接地址为 **0x30000000**，与 `link.ld` 一致）：

     ```
     tftp 0x30000000 template-framebuffer-gui.bin
     ```

3. **运行验证：**

   ```
   go 0x30000000
   ```

   **预期现象：** LCD 屏幕点亮，屏幕中央显示一个包含 "Hello Study210 LVGL!" 文字的按钮。

## **6. 常见问题排查 (Troubleshooting)**

### **6.1 编译相关问题**

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| `fatal error: stdint.h: No such file` | 编译器系统 include 路径未正确搜索 | 检查 `arm-none-eabi-gcc` 安装路径，或按步骤五创建兼容头文件 |
| `error: redefinition of 'size_t'` | 裸机 `types.h` 与 `stddef.h` 重复定义 | 在裸机 `types.h` 中添加 `_SIZE_T_DEFINED` 条件编译保护 |
| `undefined reference to 'lv_...'` | LVGL 源码子目录未加入 Makefile 的 SRCDIRS | 检查并补充缺少的 `lvgl/src/xxx` 目录 |
| 链接时 `.bss` 段过大导致 go 命令后挂死 | LVGL 静态缓冲区 + FB双缓冲 太大 | 减小 `LV_MEM_SIZE` 或缩小渲染缓冲区行数 |
| 大量编译告警 | LVGL 源码风格与 `-Wall` 不完全兼容 | 在 CFLAGS 中添加 `-Wno-unused-function -Wno-unused-variable` 等 |

### **6.2 运行相关问题**

1. **TFTP 下载超时 (T T T T...)：**
   - 检查网线连接，确认电脑和开发板在同一网段（如 192.168.1.x）。
   - **强烈建议关闭 Windows 防火墙**，并确保 Tftpd64 软件中选择的"Server interfaces"是与开发板连接的那张网卡。

2. **LCD 白屏 / 花屏：**
   - 确认使用的是 Study210 版本的 LCD 初始化代码，而非 x210v3 版本。
   - 检查 `s5pv210-fb.c` 中的 LCD 时序参数是否与您的屏幕型号匹配。

3. **画面有撕裂感或只显示一半：**
   - S5PV210 默认可能会开启 D-Cache。CPU 写入 Framebuffer 的数据可能滞留在 Cache 中，导致 LCD 控制器读出旧数据。解决办法是：在 `disp_flush` 末尾调用汇编指令清空 D-Cache，或在 MMU 配置中将 Framebuffer 页表属性设置为 Non-cacheable。

4. **运行后反复重启 / 跑飞：**
   - 检查 `link.ld` 中的 `org = 0x30000000` 与 `tftp` / `go` 命令使用的地址是否完全一致。
   - 确认 SVC 栈空间足够（默认为 0x8000 = 32KB，通常足以支撑 LVGL）。
   - 若 BSS 段过大（LVGL 静态变量 + 双缓冲 vram），`start.S` 中的 BSS 清零可能耗时过长或溢出。可尝试减小 `LV_MEM_SIZE` 或将 Framebuffer vram 的 `__attribute__((section))` 放到独立段。

5. **LVGL 动画不动 / 按钮无反应：**
   - 检查 Tick 是否正常工作：在 `timer_interrupt` 中切换 LED 来验证 Timer4 是否在正常中断。
   - 确认 `lv_tick_set_cb()` 已被调用，且 `get_system_time_ms()` 返回单调递增的毫秒值。
   - 确认主循环中 `lv_timer_handler()` 被持续调用。

## **7. 进阶扩展（选做）**

完成基本移植后，可继续进行以下扩展实验：

1. **触摸屏输入** — 使用 LVGL v9 的 `lv_indev_create()` / `lv_indev_set_read_cb()` API 接入 Study210 的电阻触摸屏，实现触控交互。
2. **LVGL Demos** — 启用 `lvgl/demos/` 中的演示程序（如 `lv_demo_widgets`），在 `lv_conf.h` 中开启对应宏即可。
3. **双缓冲优化** — 利用 template 工程已有的双缓冲机制（`s5pv210_screen_swap()` / `s5pv210_screen_flush()`），将 LVGL 配置为 `LV_DISPLAY_RENDER_MODE_DIRECT` 模式以提升刷新效率。
4. **自定义字体** — 使用 LVGL 的在线字体转换工具，生成包含中文字符的 `.c` 字体文件，实现中文界面显示。