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

/* 演示模式相关 */
static lv_obj_t * demo_screen = NULL;
static uint8_t current_demo = 0;
static const uint8_t TOTAL_DEMOS = 5;

/* 演示函数声明 */
static void create_demo_1(lv_obj_t * parent);
static void create_demo_2(lv_obj_t * parent);
static void create_demo_3(lv_obj_t * parent);
static void create_demo_4(lv_obj_t * parent);
static void create_demo_5(lv_obj_t * parent);
static void switch_demo(uint8_t demo_num);

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

/*-----------------------------------------------------
 * Demo 1: 4 彩色方块 (2x2 网格)
 *-----------------------------------------------------*/
static void create_demo_1(lv_obj_t * parent)
{
	lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
	lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

	#define RECT_W 280
	#define RECT_H 200
	#define GAP_X  ((1024 - RECT_W * 2) / 2)
	#define GAP_Y  ((600 - RECT_H * 2) / 2)

	/* 红色矩形 - 左上 */
	lv_obj_t * rect1 = lv_obj_create(parent);
	lv_obj_set_size(rect1, RECT_W, RECT_H);
	lv_obj_set_pos(rect1, GAP_X, GAP_Y);
	lv_obj_set_style_bg_color(rect1, lv_color_hex(0xFF0000), 0);
	lv_obj_set_style_bg_opa(rect1, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(rect1, 0, 0);

	/* 绿色矩形 - 右上 */
	lv_obj_t * rect2 = lv_obj_create(parent);
	lv_obj_set_size(rect2, RECT_W, RECT_H);
	lv_obj_set_pos(rect2, GAP_X + RECT_W, GAP_Y);
	lv_obj_set_style_bg_color(rect2, lv_color_hex(0x00FF00), 0);
	lv_obj_set_style_bg_opa(rect2, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(rect2, 0, 0);

	/* 蓝色矩形 - 左下 */
	lv_obj_t * rect3 = lv_obj_create(parent);
	lv_obj_set_size(rect3, RECT_W, RECT_H);
	lv_obj_set_pos(rect3, GAP_X, GAP_Y + RECT_H);
	lv_obj_set_style_bg_color(rect3, lv_color_hex(0x0000FF), 0);
	lv_obj_set_style_bg_opa(rect3, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(rect3, 0, 0);

	/* 黄色矩形 - 右下 */
	lv_obj_t * rect4 = lv_obj_create(parent);
	lv_obj_set_size(rect4, RECT_W, RECT_H);
	lv_obj_set_pos(rect4, GAP_X + RECT_W, GAP_Y + RECT_H);
	lv_obj_set_style_bg_color(rect4, lv_color_hex(0xFFFF00), 0);
	lv_obj_set_style_bg_opa(rect4, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(rect4, 0, 0);

	LV_LOG_USER("Demo 1: 4 colored rectangles");
}

/*-----------------------------------------------------
 * Demo 2: 彩虹色条 (水平渐变)
 *-----------------------------------------------------*/
static void create_demo_2(lv_obj_t * parent)
{
	lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
	lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

	/* 7个彩色条带代表彩虹色 */
	static const uint32_t colors[] = {
		0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF, 0x4B0082, 0x9400D3
	};
	uint32_t bar_height = 600 / 7;  /* 85 */

	for (int i = 0; i < 7; i++) {
		lv_obj_t * bar = lv_obj_create(parent);
		lv_obj_set_size(bar, 1024, bar_height);
		lv_obj_set_pos(bar, 0, i * bar_height);
		lv_obj_set_style_bg_color(bar, lv_color_hex(colors[i]), 0);
		lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
		lv_obj_set_style_radius(bar, 0, 0);
		lv_obj_set_style_border_width(bar, 0, 0);
	}

	LV_LOG_USER("Demo 2: Rainbow bars");
}

/*-----------------------------------------------------
 * Demo 3: 大圆形 + 小圆形群
 *-----------------------------------------------------*/
static void create_demo_3(lv_obj_t * parent)
{
	lv_obj_set_style_bg_color(parent, lv_color_hex(0x1a1a2e), 0);
	lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

	/* 中心大黄色圆形 */
	lv_obj_t * center = lv_obj_create(parent);
	lv_obj_set_size(center, 300, 300);
	lv_obj_set_pos(center, (1024 - 300) / 2, (600 - 300) / 2);
	lv_obj_set_style_bg_color(center, lv_color_hex(0xFFD700), 0);
	lv_obj_set_style_bg_opa(center, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(center, 150, 0);  /* 圆形 */

	/* 周围8个小彩色圆形 */
	static const uint32_t circle_colors[] = {
		0xFF6B6B, 0x4ECDC4, 0x45B7D1, 0x96CEB4, 0xFECE4E, 0xFF9F43, 0xEE5A24, 0x9B59B6
	};
	/* 预计算8个位置的坐标 (围绕中心 512,300) */
	static const int positions[8][2] = {
		{512 + 140, 300},      /* 0° - 右 */
		{512 + 99, 300 + 99},   /* 45° */
		{512, 300 + 140},      /* 90° - 下 */
		{512 - 99, 300 + 99},  /* 135° */
		{512 - 140, 300},      /* 180° - 左 */
		{512 - 99, 300 - 99},  /* 225° */
		{512, 300 - 140},      /* 270° - 上 */
		{512 + 99, 300 - 99}   /* 315° */
	};
	for (int i = 0; i < 8; i++) {
		int cx = positions[i][0];
		int cy = positions[i][1];

		lv_obj_t * circle = lv_obj_create(parent);
		lv_obj_set_size(circle, 60, 60);
		lv_obj_set_pos(circle, cx - 30, cy - 30);
		lv_obj_set_style_bg_color(circle, lv_color_hex(circle_colors[i]), 0);
		lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
		lv_obj_set_style_radius(circle, 30, 0);
	}

	LV_LOG_USER("Demo 3: Circles");
}

/*-----------------------------------------------------
 * Demo 4: 彩色小方块网格 (4x3)
 *-----------------------------------------------------*/
static void create_demo_4(lv_obj_t * parent)
{
	lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
	lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

	static const uint32_t colors[] = {
		0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00,
		0xFF00FF, 0x00FFFF, 0xFF8000, 0x8000FF,
		0x0080FF, 0x80FF00, 0xFF0080, 0x800080
	};
	uint32_t sq_size = 180;
	uint32_t gap = (1024 - 4 * sq_size) / 5;  /* 44 */
	uint32_t gap_y = (600 - 3 * sq_size) / 4;  /* 30 */

	for (int row = 0; row < 3; row++) {
		for (int col = 0; col < 4; col++) {
			lv_obj_t * sq = lv_obj_create(parent);
			lv_obj_set_size(sq, sq_size, sq_size);
			lv_obj_set_pos(sq, gap + col * (sq_size + gap), gap_y + row * (sq_size + gap_y));
			lv_obj_set_style_bg_color(sq, lv_color_hex(colors[row * 4 + col]), 0);
			lv_obj_set_style_bg_opa(sq, LV_OPA_COVER, 0);
			lv_obj_set_style_radius(sq, 10, 0);
		}
	}

	LV_LOG_USER("Demo 4: Color grid");
}

/*-----------------------------------------------------
 * Demo 5: 渐变背景 + 彩色形状
 *-----------------------------------------------------*/
static void create_demo_5(lv_obj_t * parent)
{
	lv_obj_set_style_bg_color(parent, lv_color_hex(0x404040), 0);
	lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

	/* 顶部渐变条 */
	lv_obj_t * top_bar = lv_obj_create(parent);
	lv_obj_set_size(top_bar, 1024, 80);
	lv_obj_set_pos(top_bar, 0, 0);
	lv_obj_set_style_bg_color(top_bar, lv_color_hex(0xFF4500), 0);
	lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);

	/* 底部渐变条 */
	lv_obj_t * bottom_bar = lv_obj_create(parent);
	lv_obj_set_size(bottom_bar, 1024, 80);
	lv_obj_set_pos(bottom_bar, 0, 520);
	lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x00CED1), 0);
	lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_COVER, 0);

	/* 中心白色圆形 */
	lv_obj_t * center = lv_obj_create(parent);
	lv_obj_set_size(center, 180, 180);
	lv_obj_set_pos(center, (1024 - 180) / 2, (600 - 180) / 2);
	lv_obj_set_style_bg_color(center, lv_color_hex(0xFFFFFF), 0);
	lv_obj_set_style_bg_opa(center, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(center, 90, 0);

	/* 左边青色矩形 */
	lv_obj_t * left = lv_obj_create(parent);
	lv_obj_set_size(left, 120, 150);
	lv_obj_set_pos(left, 80, 225);
	lv_obj_set_style_bg_color(left, lv_color_hex(0x00FFFF), 0);
	lv_obj_set_style_bg_opa(left, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(left, 0, 0);

	/* 右边粉红矩形 */
	lv_obj_t * right = lv_obj_create(parent);
	lv_obj_set_size(right, 120, 150);
	lv_obj_set_pos(right, 1024 - 80 - 120, 225);
	lv_obj_set_style_bg_color(right, lv_color_hex(0xFF69B4), 0);
	lv_obj_set_style_bg_opa(right, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(right, 0, 0);

	LV_LOG_USER("Demo 5: Gradient + shapes");
}

/*-----------------------------------------------------
 * 切换演示画面
 *-----------------------------------------------------*/
static void switch_demo(uint8_t demo_num)
{
	/* 删除旧屏幕 */
	if (demo_screen) {
		lv_obj_del(demo_screen);
		demo_screen = NULL;
	}

	/* 创建新屏幕 */
	demo_screen = lv_obj_create(NULL);
	lv_obj_set_size(demo_screen, 1024, 600);
	lv_scr_load(demo_screen);

	/* 根据demo编号创建内容 */
	switch (demo_num) {
		case 0: create_demo_1(demo_screen); break;
		case 1: create_demo_2(demo_screen); break;
		case 2: create_demo_3(demo_screen); break;
		case 3: create_demo_4(demo_screen); break;
		case 4: create_demo_5(demo_screen); break;
		default: create_demo_1(demo_screen); break;
	}

	current_demo = demo_num;
	debug_printf("[DEMO] Switched to demo %d/%d\r\n", demo_num + 1, TOTAL_DEMOS);
}

/*-----------------------------------------------------
 * 主循环中切换演示: 每N次loop切换一次
 *-----------------------------------------------------*/
#define DEMO_SWITCH_LOOPS 20  /* 约每20次loop切换 (实际~8秒@~400ms/loop) */

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
	debug_printf("[INIT]   SCTLR=0x%08X TTBR0=0x%08X DACR=0x%08X\r\n",
	             read_sctlr(), read_ttbr0(), read_dacr());
	debug_printf("[INIT]   MMU table: 0x%08X - 0x%08X\r\n",
	             (unsigned int)&__mmu_table_start, (unsigned int)&__mmu_table_end);
	debug_printf("[INIT]   FB NC window: 0x%08X - 0x%08X\r\n",
	             (unsigned int)&__fb_nocache_start, (unsigned int)&__fb_nocache_end);

	s5pv210_fb_initial();
	debug_printf("[INIT] s5pv210_fb_initial() done - LCD should be ON now\r\n");
	{
		struct surface_t * surface = s5pv210_screen_surface();
		debug_printf("[INIT] Framebuffer base: 0x%08X\r\n",
		             surface ? (unsigned int)surface->pixels : 0);
	}

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
	/* 终极防线：强制清零 BSS 段，防止未初始化全局变量导致死锁 */
	extern unsigned int __bss_start;
	extern unsigned int __bss_end;
	unsigned int * bss_ptr = (unsigned int *)&__bss_start;
	while(bss_ptr < (unsigned int *)&__bss_end) {
		*bss_ptr++ = 0;
	}

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

	/* 5. 创建演示画面 */
	debug_printf("\r\n[DEMO] Starting demo system (%d demos)...\r\n", TOTAL_DEMOS);

	/* 启动第一个演示 */
	switch_demo(0);

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

		/* 检查是否需要切换演示 */
		if (loop_count > 0 && loop_count % DEMO_SWITCH_LOOPS == 0) {
			uint8_t next_demo = (current_demo + 1) % TOTAL_DEMOS;
			debug_printf("[LOOP] Auto-switching demo %d -> %d at loop %lu\r\n",
			             current_demo + 1, next_demo + 1, (unsigned long)loop_count);
			switch_demo(next_demo);
		}

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
