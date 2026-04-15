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

/*
 * 调试开关：
 * 1 = 使用 32x32 FULL 快速验证（区分真实死锁 vs 全屏渲染过慢）
 * 0 = 使用面板全分辨率 FULL（原始配置）
 */
#define LVGL_FAST_PROBE_MODE 1

#if LVGL_FAST_PROBE_MODE
#define LVGL_HOR_RES  32
#define LVGL_VER_RES  32
static uint8_t buf_1[LVGL_HOR_RES * LVGL_VER_RES * 4] __attribute__((aligned(64)));
#else
#define LVGL_HOR_RES  PANEL_HOR_RES
#define LVGL_VER_RES  PANEL_VER_RES
/* 渲染缓冲区：全屏缓冲
 * 64 字节对齐是 LVGL v9 SW 渲染器的强制要求 */
static uint8_t buf_1[LVGL_HOR_RES * LVGL_VER_RES * 4] __attribute__((aligned(64)));
#endif

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

/*-----------------------------------------------------
 * flush_cb: 将 LVGL 渲染好的像素数据写入 Framebuffer
 *
 * S5PV210 LCD 控制器已配置为 RGB_P 模式 + WORD SWAP，
 * 像素格式为 XRGB8888（字节序：B[0] G[1] R[2] X[3]），
 * 与 LVGL 的 LV_COLOR_FORMAT_XRGB8888 完全匹配，
 * 因此可以直接 memcpy，无需逐像素转换。
 *----------------------------------------------------*/
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
	struct surface_t * surface;
	uint32_t * fb_base;
	uint32_t t0, t1;

	flush_count++;
	t0 = get_system_time_ms();

	/* 调试：打印 flush_cb 进入点 */
	disp_debug("\r\n[FLUSH] #%lu ENTRY\r\n", (unsigned long)flush_count);
	disp_debug("[FLUSH]   area=(%d,%d)-(%d,%d) px_map=0x%08X\r\n",
	           area->x1, area->y1, area->x2, area->y2,
	           (unsigned int)px_map);

	/* 获取 Framebuffer 首地址 */
	surface = s5pv210_screen_surface();
	if (!surface || !surface->pixels) {
		disp_debug("[FLUSH]   ERROR: Invalid framebuffer!\r\n");
		lv_display_flush_ready(disp);
		return;
	}

	fb_base = (uint32_t *)surface->pixels;

	/* 计算刷新区域的参数 */
	int32_t w = area->x2 - area->x1 + 1;
	int32_t h = area->y2 - area->y1 + 1;
	uint32_t * dst = fb_base + area->y1 * MY_DISP_HOR_RES + area->x1;
	uint32_t * src = (uint32_t *)px_map;
	uint32_t dst_stride = PANEL_HOR_RES;
	uint32_t src_stride = w;

	disp_debug("[FLUSH]   w=%d h=%d\r\n", w, h);

	/* ARM Demo 风格的缓冲区切换：
	 * 1. screen_swap() 切换到 back buffer
	 * 2. memcpy 将渲染数据写入 back buffer
	 * 3. screen_flush() 更新 FIMD 寄存器
	 */
	s5pv210_screen_swap();

	/* 重新获取 surface->pixels（现在是新的 back buffer）*/
	surface = s5pv210_screen_surface();
	fb_base = (uint32_t *)surface->pixels;
	dst = fb_base + area->y1 * MY_DISP_HOR_RES + area->x1;

	/* 行级复制：每行用 memcpy 一次性拷贝 */
	for (int32_t y = 0; y < h; y++) {
		memcpy(dst, src, w * 4);
		dst += dst_stride;
		src += src_stride;
	}

	/* 更新 FIMD 缓冲区地址 */
	s5pv210_screen_flush();

	/* 通知 LVGL 刷新完成 */
	lv_display_flush_ready(disp);

	t1 = get_system_time_ms();
	disp_debug("[FLUSH] #%lu EXIT (elapsed=%u ms)\r\n", (unsigned long)flush_count, t1 - t0);
}

/*-----------------------------------------------------
 * 显示接口初始化
 *----------------------------------------------------*/
void lv_port_disp_init(void)
{
	lv_display_t * disp;

	disp_debug("\r\n[DISP_INIT] ======== Display Driver Initialization =======\r\n");
	disp_debug("[DISP_INIT] Panel Resolution: %dx%d\r\n", PANEL_HOR_RES, PANEL_VER_RES);
	disp_debug("[DISP_INIT] LVGL Resolution: %dx%d\r\n", LVGL_HOR_RES, LVGL_VER_RES);
	disp_debug("[DISP_INIT] Buffer size: %d bytes\r\n", (unsigned int)sizeof(buf_1));
#if LVGL_FAST_PROBE_MODE
	disp_debug("[DISP_INIT] Render mode: FULL (32x32 quick probe)\r\n");
#else
	disp_debug("[DISP_INIT] Render mode: FULL (single full-screen buffer)\r\n");
#endif

	/* 1. 创建显示设备对象 */
	disp = lv_display_create(LVGL_HOR_RES, LVGL_VER_RES);
	if (!disp) {
		disp_debug("[DISP_INIT] FATAL: lv_display_create() returned NULL!\r\n");
		while(1);
	}
	disp_debug("[DISP_INIT] Display created\r\n");

	/* 设置此 display 为默认 display */
	lv_display_set_default(disp);

	/* 2. 设置颜色格式为 XRGB8888（与 LCD 32bpp 一致） */
	lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);

	/* 3. 设置渲染缓冲区（单缓冲 + 全屏渲染模式） */
	lv_display_set_buffers(disp, buf_1, NULL, sizeof(buf_1),
	                       LV_DISPLAY_RENDER_MODE_FULL);
	disp_debug("[DISP_INIT] Buffers configured\r\n");

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