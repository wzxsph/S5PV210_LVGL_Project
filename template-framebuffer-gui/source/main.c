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

/* 调试探针函数（供 LVGL 内部调用） */
void my_debug_printf(const char * fmt, ...)
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

/* 简单的栈深度测试 - 递归调用 */
void test_stack_recursion(int depth)
{
	volatile char buf[64];  /* 用volatile防止优化 */
	buf[0] = depth & 0xFF;
	buf[63] = (depth >> 8) & 0xFF;
	(void)buf;  /* 防止未使用警告 */

	if (depth < 50) {
		test_stack_recursion(depth + 1);
	}
}

/* Data Abort 异常处理函数 - 由 start.S 调用
 * 当发生 Data Abort 时，CPU 会跳转到此函数
 * r0 = LR (Link Register，保存着发生异常时的返回地址)
 * 真正的崩溃PC是 LR - 8
 */
void my_data_abort_handler(unsigned int lr)
{
	/* 使用 UART2 (DEBUG_UART_CH) 打印调试信息 */
	s5pv210_serial_write_string(DEBUG_UART_CH, "\r\n\r\n===== DATA ABORT =====\r\n");

	/* 打印 LR 值，真正的崩溃地址是 LR - 8 */
	char msg[64];
	(void)snprintf(msg, sizeof(msg), "LR (return addr): 0x%08X\r\n", (unsigned int)lr);
	s5pv210_serial_write_string(DEBUG_UART_CH, msg);
	(void)snprintf(msg, sizeof(msg), "Real PC (LR-8): 0x%08X\r\n", (unsigned int)(lr - 8));
	s5pv210_serial_write_string(DEBUG_UART_CH, msg);

	s5pv210_serial_write_string(DEBUG_UART_CH, "======================\r\n");

	/* 死循环，保留现场供调试 */
	while(1);
}

/* Undefined Instruction 异常处理函数 - 由 start.S 调用
 * 当执行 NEON/VFP 指令但协处理器未开启时会触发此异常
 * r0 = LR (Link Register，保存着发生异常时的返回地址)
 * 真正的崩溃PC是 LR - 4
 */
void my_undef_handler(unsigned int lr)
{
	/* 使用 UART2 (DEBUG_UART_CH) 打印调试信息 */
	s5pv210_serial_write_string(DEBUG_UART_CH, "\r\n\r\n===== UNDEFINED INSTRUCTION =====\r\n");

	/* 打印 LR 值，真正的崩溃地址是 LR - 4 */
	char msg[64];
	(void)snprintf(msg, sizeof(msg), "LR (return addr): 0x%08X\r\n", (unsigned int)lr);
	s5pv210_serial_write_string(DEBUG_UART_CH, msg);
	(void)snprintf(msg, sizeof(msg), "Real PC (LR-4): 0x%08X\r\n", (unsigned int)(lr - 4));
	s5pv210_serial_write_string(DEBUG_UART_CH, msg);

	s5pv210_serial_write_string(DEBUG_UART_CH, " Likely caused by NEON/VFP instruction without FPU enabled\r\n");
	s5pv210_serial_write_string(DEBUG_UART_CH, " Check start.S: need to enable CP10/CP11 (FPU) in CP15\r\n");

	s5pv210_serial_write_string(DEBUG_UART_CH, "======================\r\n");

	/* 死循环，保留现场供调试 */
	while(1);
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

/*-----------------------------------------------------
 * 直接Framebuffer测试（不依赖LVGL）
 *-----------------------------------------------------*/
static void rgb_test_direct_framebuffer(void)
{
	extern struct surface_t * s5pv210_screen_surface(void);
	extern void s5pv210_screen_swap(void);
	extern void s5pv210_screen_flush(void);

	struct surface_t * surf = s5pv210_screen_surface();
	debug_printf("\r\n[RGB_TEST] ======== Direct Framebuffer Test =======\r\n");
	debug_printf("[RGB_TEST] surface @ 0x%08X\r\n", (unsigned int)surf);
	debug_printf("[RGB_TEST] pixels @ 0x%08X\r\n", (unsigned int)surf->pixels);

	/* ARM Demo 风格：swap -> 写入 -> flush */
	debug_printf("[RGB_TEST] Calling s5pv210_screen_swap()...\r\n");
	s5pv210_screen_swap();

	surf = s5pv210_screen_surface();
	uint32_t * fb = (uint32_t *)surf->pixels;
	debug_printf("[RGB_TEST] After swap: pixels @ 0x%08X\r\n", (unsigned int)fb);

	/* 绘制 RGB 条纹 */
	debug_printf("[RGB_TEST] Drawing RGB stripes...\r\n");
	for (int i = 0; i < 1024 * 200; i++) fb[i] = 0xFFFF0000;  /* Red */
	for (int i = 1024 * 200; i < 1024 * 400; i++) fb[i] = 0xFF00FF00;  /* Green */
	for (int i = 1024 * 400; i < 1024 * 600; i++) fb[i] = 0xFF0000FF;  /* Blue */
	debug_printf("[RGB_TEST] RGB stripes drawn to buffer\r\n");

	/* 更新 FIMD 寄存器 */
	debug_printf("[RGB_TEST] Calling s5pv210_screen_flush()...\r\n");
	s5pv210_screen_flush();
	debug_printf("[RGB_TEST] Flush complete\r\n");

	/* 再swap一次，显示刚才绘制的缓冲区 */
	debug_printf("[RGB_TEST] Calling s5pv210_screen_swap() to display...\r\n");
	s5pv210_screen_swap();

	debug_printf("[RGB_TEST] ======== RGB Test Complete =======\r\n");
	debug_printf("[RGB_TEST] LCD should show RGB stripes now!\r\n");

	/* 保留RGB条纹3秒让用户确认 */
	mdelay(3000);
	debug_printf("[RGB_TEST] Continuing to LVGL init...\r\n");
}

int main(int argc, char * argv[])
{
	uint32_t loop_count = 0;
	static uint32_t last_debug_time = 0;
	uint32_t t0, t1, result;

	debug_printf("\r\n");
	debug_printf("============================================\r\n");
	debug_printf("  S5PV210 LVGL v9 Application Starting...\r\n");
	debug_printf("  Build: %s %s\r\n", __DATE__, __TIME__);
	debug_printf("============================================\r\n");

	/* 1. 硬件底层初始化 */
	do_system_initial();

	mdelay(100);  /* 等待 LCD 稳定 */

	/* ========== 直接Framebuffer测试（不依赖LVGL）========== */
	rgb_test_direct_framebuffer();

	/* ========== LVGL 初始化和测试 ========== */

	/* 2. 初始化 LVGL 核心 */
	debug_printf("\r\n[LVGL] Calling lv_init()...\r\n");
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
	}

	/* 4. 初始化显示接口 */
	debug_printf("\r\n[DISP] Calling lv_port_disp_init()...\r\n");
	lv_port_disp_init();
	debug_printf("[DISP] lv_port_disp_init() returned!\r\n");

	/* 5. 创建screen和obj（先于lv_timer_handler调用） */
	debug_printf("\r\n[UI] Creating screen and object...\r\n");
	lv_obj_t * scr = lv_scr_act();
	debug_printf("[UI] Active screen: %p\r\n", (void *)scr);

	/* 创建一个完全扁平的 obj，无圆角无阴影 */
	lv_obj_t * obj = lv_obj_create(scr);
	lv_obj_set_size(obj, 20, 10);  /* 32x32 快速测试模式下的小尺寸 */
	lv_obj_set_pos(obj, 2, 2);     /* 32x32 可见范围内 */
	/* 移除所有圆角和阴影 */
	lv_obj_set_style_radius(obj, 0, 0);
	lv_obj_set_style_shadow_width(obj, 0, 0);
	lv_obj_set_style_border_width(obj, 0, 0);
	debug_printf("[UI] Flat object created at (%d, %d)\r\n", 2, 2);

	/* 强制刷新布局 */
	debug_printf("[UI] Calling lv_obj_update_layout on screen...\r\n");
	lv_obj_update_layout(scr);
	debug_printf("[UI] lv_obj_update_layout returned\r\n");

	/* 6. 调用 lv_timer_handler (有了UI对象后) */
	debug_printf("\r\n[TEST] Calling lv_timer_handler() to render UI...\r\n");
	flush_count = 0;
	debug_printf("[TEST] About to call lv_timer_handler()...\r\n");

	/* 测试栈深度 - 调用一个简单的递归函数 */
	debug_printf("[TEST] Testing stack depth with simple recursion...\r\n");
	extern void test_stack_recursion(int depth);
	test_stack_recursion(0);

	debug_printf("[TEST] Stack test complete, calling lv_timer_handler()...\r\n");

	t0 = get_system_time_ms();
	debug_printf("[TEST] Before lv_timer_handler() call, flush_count=%lu\r\n", (unsigned long)flush_count);
	result = lv_timer_handler();
	t1 = get_system_time_ms();
	debug_printf("[TEST] lv_timer_handler() RETURNED! result=%lu elapsed=%lu ms flush=%lu\r\n",
	             (unsigned long)result, (unsigned long)(t1 - t0), (unsigned long)flush_count);

	/* 7. 进入主循环 */
	debug_printf("\r\n[LOOP] Entering main loop...\r\n");
	debug_printf("============================================\r\n\r\n");

	while(1) {
		loop_count++;
		debug_printf("[LOOP] About to call lv_timer_handler(), loop=%lu\r\n", (unsigned long)loop_count);
		lv_timer_handler();
		debug_printf("[LOOP] lv_timer_handler() returned, loop=%lu\r\n", (unsigned long)loop_count);

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