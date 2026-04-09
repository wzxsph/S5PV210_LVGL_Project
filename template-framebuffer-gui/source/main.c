#include <main.h>
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <s5pv210-serial.h>
#include <s5pv210-serial-stdio.h>

/* 外部声明：显示接口初始化 */
extern void lv_port_disp_init(void);

/* 外部声明：毫秒时间获取函数 */
extern uint32_t get_system_time_ms(void);

#define DEBUG_UART_CH   2       /* 使用 UART2 进行调试输出 */
#define DEBUG_BAUD      B115200

static void debug_printf(const char * fmt, ...)
{
	va_list ap;
	char buf[256];
	int len;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (len > 0) {
		s5pv210_serial_write_string(DEBUG_UART_CH, buf);
	}
}

/* LVGL 日志回调函数 */
static void my_log_print_cb(lv_log_level_t level, const char * buf)
{
	const char * level_str;
	
	switch(level) {
		case LV_LOG_LEVEL_TRACE: level_str = "[TRACE]"; break;
		case LV_LOG_LEVEL_INFO:  level_str = "[INFO] "; break;
		case LV_LOG_LEVEL_WARN:  level_str = "[WARN] "; break;
		case LV_LOG_LEVEL_ERROR: level_str = "[ERROR]"; break;
		case LV_LOG_LEVEL_USER:  level_str = "[USER] "; break;
		default: level_str = "[LOG]  "; break;
	}
	
	debug_printf("%s %s", level_str, buf);
}

static void do_system_initial(void)
{
	debug_printf("[INIT] Starting system initialization...\r\n");

	malloc_init();
	debug_printf("[INIT] malloc_init() done\r\n");

	s5pv210_clk_initial();
	debug_printf("[INIT] s5pv210_clk_initial() done\r\n");

	s5pv210_irq_initial();
	debug_printf("[INIT] s5pv210_irq_initial() done\r\n");

	s5pv210_tick_initial();
	debug_printf("[INIT] s5pv210_tick_initial() done\r\n");

	s5pv210_tick_delay_initial();
	debug_printf("[INIT] s5pv210_tick_delay_initial() done\r\n");

	s5pv210_serial_initial();
	debug_printf("[INIT] s5pv210_serial_initial() done\r\n");

	/* 配置 UART2 用于调试输出（如果尚未配置） */
	s5pv210_serial_setup(DEBUG_UART_CH, DEBUG_BAUD, DATA_BITS_8, PARITY_NONE, STOP_BITS_1);
	debug_printf("[INIT] UART2 configured for debug @ 115200 baud\r\n");

	s5pv210_fb_initial();
	debug_printf("[INIT] s5pv210_fb_initial() done - LCD should be ON now\r\n");

	/* 额外验证：再次检查 framebuffer */
	{
		extern struct surface_t * s5pv210_screen_surface(void);
		struct surface_t * surf = s5pv210_screen_surface();
		debug_printf("[INIT] Post-init FB check: surf=0x%08X pixels=0x%08X\r\n",
		           (unsigned int)surf,
		           surf ? (unsigned int)(surf->pixels) : 0);

		if (surf && surf->pixels) {
			debug_printf("[INIT] Framebuffer is VALID!\r\n");
			/* 写入测试像素到 framebuffer 中心位置 */
			uint32_t * fb = (uint32_t *)(surf->pixels);
			int center_x = 512;  /* 1024/2 */
			int center_y = 300;  /* 600/2 */
			/* XRGB8888: 0xAARRGGBB -> 绿色 */
			fb[center_y * 1024 + center_x] = 0xFF00FF00;
			debug_printf("[INIT] Wrote test pixel GREEN at (%d,%d)\r\n", center_x, center_y);
		} else {
			debug_printf("[ERROR] Framebuffer is INVALID or NULL!\r\n");
		}
	}

	led_initial();
	beep_initial();
	key_initial();

	debug_printf("[INIT] System initialization COMPLETE!\r\n");
	debug_printf("[INIT] ============================================\r\n");
}

int main(int argc, char * argv[])
{
	uint32_t loop_count = 0;
	static uint32_t last_debug_time = 0;
	lv_obj_t * btn = NULL;
	lv_obj_t * label = NULL;

	debug_printf("\r\n");
	debug_printf("============================================\r\n");
	debug_printf("  S5PV210 LVGL v9 Application Starting...\r\n");
	debug_printf("  Build: %s %s\r\n", __DATE__, __TIME__);
	debug_printf("============================================\r\n");

	/* 1. 硬件底层初始化 */
	do_system_initial();

	mdelay(100);  /* 等待 LCD 稳定 */

	/* 2. 初始化 LVGL 核心 */
	debug_printf("[LVGL] Calling lv_init()...\r\n");
	lv_init();
	debug_printf("[LVGL] lv_init() SUCCESS!\r\n");
	
	/* 注册 LVGL 日志回调 */
	debug_printf("[LVGL] Registering log callback...\r\n");
	lv_log_register_print_cb(my_log_print_cb);
	debug_printf("[LVGL] Log callback registered\r\n");

	/* 3. 注册 Tick 回调 */
	debug_printf("[LVGL] Registering tick callback (jiffies->ms)...\r\n");
	lv_tick_set_cb(get_system_time_ms);
	debug_printf("[LVGL] Tick callback registered. Current time: %u ms\r\n", get_system_time_ms());

	/* 检查LVGL内存状态 */
	{
		lv_mem_monitor_t mon;
		lv_mem_monitor(&mon);
		debug_printf("[LVGL] Memory status:\r\n");
		debug_printf("[LVGL]   Total: %zu bytes\r\n", mon.total_size);
		debug_printf("[LVGL]   Free: %zu bytes\r\n", mon.free_size);
		debug_printf("[LVGL]   Used: %zu bytes (%d%%)\r\n", mon.total_size - mon.free_size, mon.used_pct);
		debug_printf("[LVGL]   Biggest free: %zu bytes\r\n", mon.free_biggest_size);
		debug_printf("[LVGL]   Frag: %d%%\r\n", mon.frag_pct);
	}

	/* 4. 初始化显示接口 */
	debug_printf("[DISP] Calling lv_port_disp_init()...\r\n");
	lv_port_disp_init();
	debug_printf("[DISP] lv_port_disp_init() returned!\r\n");

	/* 6. 创建测试 UI */
	debug_printf("[UI] Creating test button and label...\r\n");

	btn = lv_button_create(lv_screen_active());
	if (btn) {
		lv_obj_center(btn);
		lv_obj_set_size(btn, 300, 80);
		debug_printf("[UI] Button created at center (300x80)\r\n");

		label = lv_label_create(btn);
		if (label) {
			lv_label_set_text(label, "Hello Study210 LVGL!");
			lv_obj_center(label);
			debug_printf("[UI] Label created with text 'Hello Study210 LVGL!'\r\n");
		} else {
			debug_printf("[ERROR] Failed to create label!\r\n");
		}
	} else {
		debug_printf("[ERROR] Failed to create button!\r\n");
	}

	debug_printf("[UI] Test UI creation COMPLETE!\r\n");

	/* 5. 强制刷新一次屏幕 - 测试 flush_cb 是否工作 */
	debug_printf("[TEST] Skipping forced refresh to test main loop...\r\n");
	/* TEMPORARILY COMMENTED OUT FOR DEBUGGING */
	/* lv_refr_now(NULL); */
	/* mdelay(100); */
	debug_printf("[TEST] Skipped refresh.\r\n");

	debug_printf("============================================\r\n");
	debug_printf("  Entering main loop...\r\n");
	debug_printf("============================================\r\n\r\n");

	/* 7. LVGL 主循环 */
	while(1) {
		lv_timer_handler();     /* LVGL v9 的任务处理入口 */

		loop_count++;

		/* 每 5 秒输出一次调试状态 */
		if ((get_system_time_ms() - last_debug_time) >= 5000) {
			last_debug_time = get_system_time_ms();

			debug_printf("[LOOP] tick=%u loops=%lu time=%ums\r\n",
			             get_system_time_ms(),
			             (unsigned long)loop_count,
			             get_system_time_ms());

			/* 验证屏幕 surface 是否有效 */
			{
				extern struct surface_t * s5pv210_screen_surface(void);
				struct surface_t * surf = s5pv210_screen_surface();
				if (surf && surf->pixels) {
					debug_printf("[FB]   Framebuffer addr: 0x%08X\r\n", (unsigned int)surf->pixels);
				} else {
					debug_printf("[FB]   WARNING: Invalid framebuffer surface!\r\n");
				}
			}
		}

		mdelay(5);              /* 适当延时，降低 CPU 占用 */
	}

	return 0;
}
