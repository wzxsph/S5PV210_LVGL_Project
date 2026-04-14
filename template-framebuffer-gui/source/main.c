#include <main.h>
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <s5pv210-serial.h>
#include <s5pv210-serial-stdio.h>
#include <s5pv210-cp15.h>

/* 外部声明：显示接口初始化 */
extern void lv_port_disp_init(void);

/* 外部声明：毫秒时间获取函数 */
extern uint32_t get_system_time_ms(void);

/* 外部声明：flush 计数（来自 lv_port_disp.c） */
extern uint32_t flush_count;

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

	/* 注意：暂时禁用 Cache 和 MMU，因为没有正确的翻译表会导致显示异常 */
	/* TODO: 后续需要正确配置 MMU 翻译表后才能启用缓存以提升渲染性能 */
	debug_printf("[INIT] Cache and MMU disabled for display correctness\r\n");

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
	}

	/* 4. 初始化显示接口 */
	debug_printf("[DISP] Calling lv_port_disp_init()...\r\n");
	lv_port_disp_init();
	debug_printf("[DISP] lv_port_disp_init() returned!\r\n");

	/* 5. 测试1：先手动写 RGB 条纹验证 LCD 硬件 */
	debug_printf("[TEST1] Manual RGB stripe test to verify LCD hardware...\r\n");
	{
		extern struct surface_t * s5pv210_screen_surface(void);
		struct surface_t * surf = s5pv210_screen_surface();
		if (surf && surf->pixels) {
			uint32_t * fb = (uint32_t *)surf->pixels;
			for (int i = 0; i < 1024 * 200; i++) fb[i] = 0xFFFF0000;  /* Red */
			for (int i = 1024 * 200; i < 1024 * 400; i++) fb[i] = 0xFF00FF00;  /* Green */
			for (int i = 1024 * 400; i < 1024 * 600; i++) fb[i] = 0xFF0000FF;  /* Blue */
			debug_printf("[TEST1] RGB stripes drawn! Check LCD.\r\n");
		}
	}

	mdelay(2000);  /* 显示2秒让用户确认 */
	debug_printf("[TEST1] RGB stripe test done. Now testing LVGL rendering...\r\n");

	/* 6. 清屏 - 用黑色覆盖 RGB 条纹 */
	{
		extern struct surface_t * s5pv210_screen_surface(void);
		struct surface_t * surf = s5pv210_screen_surface();
		if (surf && surf->pixels) {
			uint32_t * fb = (uint32_t *)surf->pixels;
			for (int i = 0; i < 1024 * 600; i++) fb[i] = 0xFF000000;  /* Black */
			debug_printf("[TEST1] Screen cleared to black.\r\n");
		}
	}

	mdelay(500);

	/* 7. 测试2：创建一个简单的 LVGL 控件 */
	debug_printf("[TEST2] Creating LVGL label...\r\n");
	{
		lv_obj_t * label = lv_label_create(lv_screen_active());
		if (label) {
			lv_label_set_text(label, "Hello S5PV210!");
			lv_obj_center(label);
			lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
			debug_printf("[TEST2] Label created and centered.\r\n");
		} else {
			debug_printf("[TEST2] ERROR: Failed to create label!\r\n");
		}
	}

	/* 8. 强制刷新LVGL以渲染标签 */
	debug_printf("[TEST2] Forcing LVGL render...\r\n");
	lv_timer_handler();
	mdelay(100);
	s5pv210_screen_swap();
	s5pv210_screen_flush();
	debug_printf("[TEST2] LVGL render complete.\r\n");

	/* 8. 进入 LVGL 主循环 */
	debug_printf("[LOOP] Entering LVGL main loop with lv_timer_handler()...\r\n");
	debug_printf("============================================\r\n\r\n");

	/* 首次调用 lv_timer_handler 的调试包装 */
	{
		debug_printf("[LOOP] About to call lv_timer_handler() for the FIRST time...\r\n");
		uint32_t t0 = get_system_time_ms();
		uint32_t result = lv_timer_handler();
		uint32_t t1 = get_system_time_ms();
		debug_printf("[LOOP] FIRST lv_timer_handler() returned! result=%lu elapsed=%lums flush=%lu\r\n",
		             (unsigned long)result, (unsigned long)(t1 - t0), (unsigned long)flush_count);
	}

	while(1) {
		loop_count++;

		/* 调用 LVGL 定时器处理（包含渲染） */
		lv_timer_handler();

		/* 定期调试输出 */
		if ((get_system_time_ms() - last_debug_time) >= 3000) {
			last_debug_time = get_system_time_ms();
			debug_printf("[LOOP] tick=%u loops=%lu flush=%lu\r\n",
			             get_system_time_ms(),
			             (unsigned long)loop_count,
			             (unsigned long)flush_count);
		}

		mdelay(5);
	}

	return 0;
}
