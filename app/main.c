#include <types.h>
#include <s5pv210-clk.h>
#include <s5pv210-irq.h>
#include <s5pv210-tick.h>
#include <s5pv210-serial.h>
#include <s5pv210-serial-stdio.h>
#include <malloc.h>
#include "lcd.h"

#include "lvgl/lvgl.h"
#include "porting/lv_port_disp.h"
#include "porting/lv_port_indev.h"

/* Map printf to serial output on UART channel 0 */
#define printf(...)  serial_printf(0, __VA_ARGS__)

static void hardware_init(void)
{
	/* 1. Serial FIRST so we can see debug output */
	s5pv210_serial_initial();
	printf("\r\n\r\n===== S5PV210 LVGL Project =====\r\n");
	printf("[SYS] Serial OK.\r\n");

	malloc_init();
	printf("[SYS] Malloc OK.\r\n");

	/*
	 * Query clock tree registers to populate s5pv210_clocks array.
	 * This is totally safe inside U-Boot because it ONLY READS registers.
	 * Without this, `clk_get_rate()` fails, which breaks UART and Timer4!
	 */
	s5pv210_clk_initial();
	printf("[SYS] Clock populated from U-Boot state.\r\n");

	s5pv210_irq_initial();
	printf("[SYS] IRQ OK.\r\n");

	s5pv210_tick_initial();
	printf("[SYS] Timer/Tick OK.\r\n");

	s5pv210_fb_initial();
	printf("[SYS] LCD/FB OK.\r\n");
}

static void create_demo_ui(void)
{
	lv_obj_t * scr = lv_scr_act();

	lv_obj_t * label = lv_label_create(scr);
	lv_label_set_text(label, "Hello S5PV210 LVGL!");
	lv_obj_set_style_text_color(label, lv_color_hex(0xff0000), LV_PART_MAIN);
	lv_obj_center(label);

	lv_obj_t * btn = lv_btn_create(scr);
	lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);
	lv_obj_set_size(btn, 120, 50);

	lv_obj_t * btn_label = lv_label_create(btn);
	lv_label_set_text(btn_label, "Click Me");
	lv_obj_center(btn_label);
}

int main(int argc, char * argv[])
{
	/* 1. Base Hardware Init */
	hardware_init();

	printf("[LVGL] lv_init...\r\n");
	/* 2. LVGL Init */
	lv_init();
	lv_port_disp_init();
	lv_port_indev_init();
	printf("[LVGL] Init done.\r\n");

	printf("[UI] Creating demo...\r\n");
	/* 3. Draw UI */
	create_demo_ui();
	printf("[UI] Demo created.\r\n");

	printf("[SYS] Entering main loop.\r\n");
	/* 4. Main loop */
	while(1)
	{
		lv_timer_handler();
		extern void mdelay(unsigned int);
		mdelay(5);
	}

	return 0;
}
