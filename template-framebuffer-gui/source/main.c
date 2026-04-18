#include <main.h>
#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <stdio.h>
#include <s5pv210-serial.h>
#include <s5pv210-serial-stdio.h>
#include <s5pv210-cp15.h>

/* 外部声明：显示接口初始化 */
extern void lv_port_disp_init(void);

/* 外部声明：输入设备初始化 */
extern void lv_port_indev_init(void);

/* 外部声明：毫秒时间获取函数 */
extern uint32_t get_system_time_ms(void);

/* 外部声明：flush 计数（来自 lv_port_disp.c） */
extern uint32_t flush_count;

#define DEBUG_UART_CH   2       /* 使用 UART2 进行调试输出 */
#define DEBUG_BAUD      B115200

extern unsigned char __mmu_table_start;
extern unsigned char __mmu_table_end;
extern unsigned char __fb_nocache_start;
extern unsigned char __fb_nocache_end;

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

static u32_t read_sctlr(void)
{
	u32_t value;

	__asm__ __volatile__(
		"mrc p15, 0, %0, c1, c0, 0"
		: "=r" (value)
		:
		: "memory");

	return value;
}

static u32_t read_ttbr0(void)
{
	u32_t value;

	__asm__ __volatile__(
		"mrc p15, 0, %0, c2, c0, 0"
		: "=r" (value)
		:
		: "memory");

	return value;
}

static u32_t read_dacr(void)
{
	u32_t value;

	__asm__ __volatile__(
		"mrc p15, 0, %0, c3, c0, 0"
		: "=r" (value)
		:
		: "memory");

	return value;
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

	/* 读取 DFAR (Data Fault Address Register) - c6,c0,0 */
	unsigned int dfar;
	__asm__ volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(dfar));
	(void)snprintf(msg, sizeof(msg), "DFAR (fault addr): 0x%08X\r\n", dfar);
	s5pv210_serial_write_string(DEBUG_UART_CH, msg);

	/* 读取当前 SP */
	unsigned int sp_val;
	__asm__ volatile("mov %0, sp" : "=r"(sp_val));
	(void)snprintf(msg, sizeof(msg), "SP at abort: 0x%08X\r\n", sp_val);
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
	debug_printf("[INIT] Cache and MMU enabled for LVGL performance\r\n");

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
	debug_printf("[INIT] MMU/cache enabled by start.S\r\n");
	debug_printf("[INIT]   SCTLR=0x%08x TTBR0=0x%08x DACR=0x%08x\r\n",
	             read_sctlr(), read_ttbr0(), read_dacr());
	debug_printf("[INIT]   MMU table: 0x%08x - 0x%08x\r\n",
	             (unsigned int)&__mmu_table_start, (unsigned int)&__mmu_table_end);
	debug_printf("[INIT]   FB NC window: 0x%08x - 0x%08x\r\n",
	             (unsigned int)&__fb_nocache_start, (unsigned int)&__fb_nocache_end);

	s5pv210_fb_initial();
	debug_printf("[INIT] s5pv210_fb_initial() done - LCD should be ON now\r\n");
	{
		struct surface_t * surface = s5pv210_screen_surface();
		debug_printf("[INIT] Framebuffer base: 0x%08x\r\n",
		             surface ? (unsigned int)surface->pixels : 0);
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
	/* 注释掉RGB测试，避免干扰LVGL状态
	rgb_test_direct_framebuffer();
	*/

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

	/* 5. 初始化输入设备（GPIO 按键 → LVGL 键盘） */
	debug_printf("[INDEV] Calling lv_port_indev_init()...\r\n");
	lv_port_indev_init();
	debug_printf("[INDEV] lv_port_indev_init() done\r\n");

	/* 6. 启动 LVGL 官方 Widgets Demo */
	debug_printf("\r\n[DEMO] Starting lv_demo_widgets()...\r\n");
	lv_demo_widgets();
	debug_printf("[DEMO] lv_demo_widgets() created\r\n");

	/* 7. 首次渲染 */
	flush_count = 0;
	t0 = get_system_time_ms();
	result = lv_timer_handler();
	t1 = get_system_time_ms();
	debug_printf("[DEMO] First render: %lu ms, flush=%lu\r\n",
	             (unsigned long)(t1 - t0), (unsigned long)flush_count);

	/* 8. 进入主循环 */
	debug_printf("\r\n[LOOP] Entering main loop...\r\n");
	debug_printf("============================================\r\n\r\n");

	while(1) {
		loop_count++;
		lv_timer_handler();

		if ((get_system_time_ms() - last_debug_time) >= 5000) {
			last_debug_time = get_system_time_ms();
			debug_printf("[LOOP] tick=%u loops=%lu flush=%lu\r\n",
			             get_system_time_ms(),
			             (unsigned long)loop_count,
			             (unsigned long)flush_count);
		}
	}

	return 0;
}
