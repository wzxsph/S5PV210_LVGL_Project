#include "lvgl/lvgl.h"
#include <s5pv210-fb.h>
#include <s5pv210-tick.h>
#include <graphic/surface.h>
#include <s5pv210-serial.h>
#include <stdio.h>
#include <string.h>

/* 前向声明 */
extern uint32_t get_system_time_ms(void);

/* LCD 物理分辨率 - 与 s5pv210-fb.c 中的 vs070cxn_lcd 一致 */
#define PANEL_HOR_RES  1024
#define PANEL_VER_RES  600

/* PARTIAL 模式渲染缓冲区行数 */
#define RENDER_BUF_LINES  120

/* 调试统计 */
uint32_t flush_count = 0;

/* 调试输出宏 */
#define DISP_DEBUG_CH   2       /* UART2 */

static void disp_debug(const char * fmt, ...)
{
	va_list ap;
	char buf[256];
	int len;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (len > 0) {
		s5pv210_serial_write_string(DISP_DEBUG_CH, buf);
	}
}

/*
 * 渲染缓冲区 — 位于 cached SDRAM (.bss 段, 0x30xxxxxx 区域)
 * LVGL 以 PARTIAL 模式将脏区域渲染到此 buffer，然后通过 flush_cb 传给驱动层。
 */
static uint8_t __attribute__((aligned(64)))
    render_buf[PANEL_HOR_RES * RENDER_BUF_LINES * 4];

/*
 * 影子帧缓冲 — 位于 cached SDRAM (.bss 段)
 *
 * 防撕裂核心：
 *   问题：PARTIAL 模式下 flush_cb 被多次调用（每条脏区域一次），
 *         如果每次都直接写 VRAM（non-cached），FIMD 在扫描同一块 VRAM，
 *         导致部分行显示新内容、部分行显示旧内容 = 严重撕裂。
 *
 *   方案：所有条带先写到 shadow_fb（cached → cached，极快），
 *         帧内最后一次 flush 时一次性将脏行区间从 shadow_fb 批量
 *         memcpy 到 VRAM（cached → uncached），撕裂窗口缩至最短。
 *
 *   代价：2.4 MB cached SDRAM（0x30xxxxxx），512 MB 总内存余量充足。
 */
static uint8_t __attribute__((aligned(64)))
    shadow_fb[PANEL_HOR_RES * PANEL_VER_RES * 4];

/* 每帧脏区 Y 范围追踪，仅拷贝实际更改的行到 VRAM */
static int32_t frame_y_min = PANEL_VER_RES;  /* 大于所有有效 y → "无脏区" */
static int32_t frame_y_max = -1;

/*-----------------------------------------------------
 * flush_cb — PARTIAL 模式 + 影子帧缓冲防撕裂
 *
 * 调用流程（每帧可被调用多次，每次一条脏区域）：
 *   1. 将 LVGL 渲染好的条带从 render_buf → shadow_fb (cached → cached)
 *   2. 追踪脏区 Y 范围
 *   3. 最后一条时: 批量 shadow_fb → VRAM (cached → uncached)
 *
 * 注意 px_map stride:
 *   LVGL v9 PARTIAL 模式下 draw buffer 被 reshape 为脏区域宽度
 *   (lv_refr.c:900: layer->buf_area = *area_p),
 *   所以 px_map 的行步幅 = area_width * bpp，不是 display_width * bpp。
 *----------------------------------------------------*/
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
	flush_count++;

	int32_t w = area->x2 - area->x1 + 1;
	int32_t h = area->y2 - area->y1 + 1;

	/* Bounds check: clamp to panel dimensions to prevent out-of-bounds memcpy */
	if (area->x1 < 0 || area->y1 < 0 ||
	    area->x2 >= PANEL_HOR_RES || area->y2 >= PANEL_VER_RES ||
	    w <= 0 || h <= 0) {
		lv_display_flush_ready(disp);
		return;
	}

	/*
	 * 行步幅 (stride)：
	 *   shadow_fb / VRAM: PANEL_HOR_RES * 4 = 4096 (全屏宽)
	 *   px_map:           w * 4              (脏区域宽度)
	 */
	uint32_t vram_stride = PANEL_HOR_RES * 4;
	uint32_t src_stride  = (uint32_t)w * 4;

	/* Step 1: 条带 → shadow_fb  (cached → cached，极快) */
	if (w == PANEL_HOR_RES) {
		/* 全宽条带：src_stride == vram_stride，单次 bulk copy */
		memcpy(shadow_fb + (uint32_t)area->y1 * vram_stride,
		       px_map,
		       (uint32_t)h * vram_stride);
	} else {
		/* 非全宽：逐行 copy（stride 转换） */
		int32_t y;
		for (y = 0; y < h; y++) {
			memcpy(shadow_fb + (uint32_t)(area->y1 + y) * vram_stride
			                 + (uint32_t)area->x1 * 4,
			       px_map + (uint32_t)y * src_stride,
			       src_stride);
		}
	}

	/* Step 2: 追踪帧内脏区 Y 范围 */
	if (area->y1 < frame_y_min) frame_y_min = area->y1;
	if (area->y2 > frame_y_max) frame_y_max = area->y2;

	/* Step 3: 帧内最后一块 → 批量写 VRAM */
	if (lv_display_flush_is_last(disp)) {
		struct surface_t * surface = s5pv210_screen_surface();
		uint8_t * vram = (uint8_t *)surface->pixels;

		if (frame_y_min <= frame_y_max) {
			/* 等待 VSYNC 以减少撕裂（带超时，不会死锁） */
			s5pv210_screen_wait_vsync();

			uint32_t off = (uint32_t)frame_y_min * vram_stride;
			uint32_t len = (uint32_t)(frame_y_max - frame_y_min + 1) * vram_stride;
			memcpy(vram + off, shadow_fb + off, len);
		}

		/* 重置脏区追踪 */
		frame_y_min = PANEL_VER_RES;
		frame_y_max = -1;
	}

	lv_display_flush_ready(disp);
}

/*-----------------------------------------------------
 * 显示接口初始化
 *----------------------------------------------------*/
void lv_port_disp_init(void)
{
	lv_display_t * disp;
	uint32_t buf_size = sizeof(render_buf);

	disp_debug("\r\n[DISP_INIT] ======== Display Driver Initialization =======\r\n");
	disp_debug("[DISP_INIT] Panel Resolution: %dx%d\r\n", PANEL_HOR_RES, PANEL_VER_RES);
	disp_debug("[DISP_INIT] Render mode: PARTIAL (%d-line cached + shadow FB)\r\n",
	           RENDER_BUF_LINES);

	/* 1. 创建显示设备对象 */
	disp = lv_display_create(PANEL_HOR_RES, PANEL_VER_RES);
	if (!disp) {
		disp_debug("[DISP_INIT] FATAL: lv_display_create() returned NULL!\r\n");
		while(1);
	}
	disp_debug("[DISP_INIT] Display created\r\n");

	/* 设置此 display 为默认 display */
	lv_display_set_default(disp);

	/* 2. 设置颜色格式为 XRGB8888（与 LCD 32bpp 一致） */
	lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);

	/* 3. PARTIAL 模式 — 渲染缓冲区位于 cached SDRAM
	 *    LVGL 渲染到 render_buf (cached)，flush_cb 复制到 shadow_fb → VRAM */
	lv_display_set_buffers(disp, render_buf, NULL, buf_size,
	                       LV_DISPLAY_RENDER_MODE_PARTIAL);
	disp_debug("[DISP_INIT] Render buffer: 0x%08x (%u bytes, %d lines)\r\n",
	           (unsigned int)(uintptr_t)render_buf, buf_size, RENDER_BUF_LINES);
	disp_debug("[DISP_INIT] Shadow FB:     0x%08x (%u bytes, anti-tear)\r\n",
	           (unsigned int)(uintptr_t)shadow_fb,
	           (unsigned int)sizeof(shadow_fb));

	/* 4. 注册刷屏回调函数 */
	lv_display_set_flush_cb(disp, disp_flush);
	disp_debug("[DISP_INIT] Flush callback registered\r\n");

	disp_debug("[DISP_INIT] ======== Display Init COMPLETE =======\r\n\r\n");
}

/*-----------------------------------------------------
 * 将 jiffies (100Hz) 换算为毫秒
 *----------------------------------------------------*/
uint32_t get_system_time_ms(void)
{
	return (uint32_t)(jiffies * 10);  /* 每个 jiffie = 10ms */
}