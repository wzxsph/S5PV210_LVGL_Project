#include "lvgl/lvgl.h"
#include <s5pv210-fb.h>
#include <s5pv210-tick.h>
#include <graphic/surface.h>
#include <s5pv210-serial.h>
#include <stdio.h>
#include <string.h>

/* 前向声明 */
extern uint32_t get_system_time_ms(void);

/* LCD 分辨率 - 与 s5pv210-fb.c 中的 vs070cxn_lcd 一致 */
#define MY_DISP_HOR_RES  1024
#define MY_DISP_VER_RES  600

/* 渲染缓冲区：分配 48 行的部分缓冲（约 192KB）*/
static uint8_t buf_1[MY_DISP_HOR_RES * 48 * 4];

/* 调试统计 */
uint32_t flush_count = 0;
static uint32_t last_flush_debug = 0;

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

	flush_count++;

	/* 调试：打印确认 flush_cb 被调用 */
	disp_debug("[FLUSH] #");
	disp_debug("%lu", (unsigned long)flush_count);
	disp_debug(" called\r\n");

	/* 获取 Framebuffer 首地址 */
	surface = s5pv210_screen_surface();

	if (!surface || !surface->pixels) {
		if (flush_count == 1 || (flush_count % 100) == 0) {
			disp_debug("[FLUSH] ERROR: Invalid framebuffer! surface=0x%08X pixels=0x%08X\r\n",
			           (unsigned int)surface,
			           surface ? (unsigned int)surface->pixels : 0);
		}
		lv_display_flush_ready(disp);
		return;
	}

	fb_base = (uint32_t *)surface->pixels;

	/* 计算刷新区域的参数 */
	int32_t w = area->x2 - area->x1 + 1;
	int32_t h = area->y2 - area->y1 + 1;
	uint32_t * dst = fb_base + area->y1 * MY_DISP_HOR_RES + area->x1;
	uint32_t * src = (uint32_t *)px_map;
	uint32_t dst_stride = MY_DISP_HOR_RES;  /* framebuffer 行步长（像素） */
	uint32_t src_stride = w;                 /* LVGL 渲染缓冲区行步长（像素） */

	/* 行级复制：每行用 memcpy 一次性拷贝 */
	for (int32_t y = 0; y < h; y++) {
		memcpy(dst, src, w * 4);
		dst += dst_stride;
		src += src_stride;
	}

	/* 首帧和定期调试输出 */
	if (flush_count <= 3 || (get_system_time_ms() - last_flush_debug) >= 5000) {
		last_flush_debug = get_system_time_ms();
		disp_debug("[FLUSH] #%lu area=(%d,%d)-(%d,%d) %dx%d pxmap=0x%08X fb=0x%08X\r\n",
		           (unsigned long)flush_count,
		           area->x1, area->y1, area->x2, area->y2,
		           w, h,
		           (unsigned int)px_map,
		           (unsigned int)fb_base);
	}

	/* 通知 LVGL 刷新完成 */
	lv_display_flush_ready(disp);
}

/*-----------------------------------------------------
 * 显示接口初始化
 *----------------------------------------------------*/
void lv_port_disp_init(void)
{
	lv_display_t * disp;

	disp_debug("\r\n[DISP_INIT] ======== Display Driver Initialization =======\r\n");
	disp_debug("[DISP_INIT] LCD Resolution: %dx%d\r\n", MY_DISP_HOR_RES, MY_DISP_VER_RES);
	disp_debug("[DISP_INIT] Color depth: 32bpp (XRGB8888)\r\n");
	disp_debug("[DISP_INIT] Buffer size: %d bytes (%.1f KB)\r\n",
	           sizeof(buf_1), sizeof(buf_1) / 1024.0);
	disp_debug("[DISP_INIT] Render mode: PARTIAL (48 lines)\r\n");

	/* 验证 LCD 是否已初始化 */
	{
		struct surface_t * surf = s5pv210_screen_surface();
		if (surf && surf->pixels) {
			disp_debug("[DISP_INIT] LCD Framebuffer validated: 0x%08X\r\n", (unsigned int)surf->pixels);
			disp_debug("[DISP_INIT] Surface: w=%d h=%d pitch=%d\r\n",
			           surf->w, surf->h, surf->pitch);
		} else {
			disp_debug("[DISP_INIT] WARNING: LCD surface not ready yet!\r\n");
			disp_debug("[DISP_INIT]          Will try again in flush_cb...\r\n");
		}
	}

	/* 1. 创建显示设备对象 */
	disp_debug("[DISP_INIT] Creating display object...\r\n");
	disp_debug("[DISP_INIT] About to call lv_display_create(%d, %d)...\r\n", MY_DISP_HOR_RES, MY_DISP_VER_RES);
	
	/* 检查LVGL内存状态 */
	{
		lv_mem_monitor_t mon;
		lv_mem_monitor(&mon);
		disp_debug("[DISP_INIT] Memory before lv_display_create:\r\n");
		disp_debug("[DISP_INIT]   Total: %zu bytes\r\n", mon.total_size);
		disp_debug("[DISP_INIT]   Free: %zu bytes\r\n", mon.free_size);
		disp_debug("[DISP_INIT]   Used: %zu bytes (%d%%)\r\n", mon.total_size - mon.free_size, mon.used_pct);
		disp_debug("[DISP_INIT]   Biggest free: %zu bytes\r\n", mon.free_biggest_size);
	}
	
	disp_debug("[DISP_INIT] Before lv_display_create()...\r\n");
	disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
	disp_debug("[DISP_INIT] After lv_display_create()...\r\n");

	disp_debug("[DISP_INIT] lv_display_create() returned: 0x%08X\r\n", (unsigned int)disp);

	if (!disp) {
		disp_debug("[DISP_INIT] FATAL: lv_display_create() returned NULL!\r\n");
		disp_debug("[DISP_INIT] Possible causes:\r\n");
		disp_debug("[DISP_INIT]   1. Insufficient memory (LV_MEM_SIZE too small)\r\n");
		disp_debug("[DISP_INIT]   2. Memory allocation failure\r\n");
		disp_debug("[DISP_INIT]   3. Internal LVGL error\r\n");
		disp_debug("[DISP_INIT] HALTING...\r\n");
		while(1);  /* 停止执行 */
	}
	disp_debug("[DISP_INIT] Display object created successfully: 0x%08X\r\n", (unsigned int)disp);

	/* 设置此 display 为默认 display */
	lv_display_set_default(disp);
	disp_debug("[DISP_INIT] Display set as default\r\n");

	/* 2. 设置颜色格式为 XRGB8888（与 LCD 32bpp 一致） */
	lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);
	disp_debug("[DISP_INIT] Color format set to XRGB8888\r\n");

	/* 3. 设置渲染缓冲区（单缓冲 + 部分渲染模式） */
	disp_debug("[DISP_INIT] Setting render buffers...\r\n");
	disp_debug("[DISP_INIT]   buf_1 size=%u bytes\r\n",
	           (unsigned int)sizeof(buf_1));
	
	lv_display_set_buffers(disp, buf_1, NULL, sizeof(buf_1),
	                       LV_DISPLAY_RENDER_MODE_PARTIAL);
	disp_debug("[DISP_INIT] Render buffers configured\r\n");

	/* 4. 注册刷屏回调函数 */
	lv_display_set_flush_cb(disp, disp_flush);
	disp_debug("[DISP_INIT] Flush callback registered\r\n");

	disp_debug("[DISP_INIT] ======== Display Init COMPLETE =======\r\n\r\n");
}

/*-----------------------------------------------------
 * 将 jiffies (100Hz) 换算为毫秒
 * 供 lv_tick_set_cb() 注册使用
 *----------------------------------------------------*/
uint32_t get_system_time_ms(void)
{
	return (uint32_t)(jiffies * 10);  /* 每个 jiffie = 10ms */
}
