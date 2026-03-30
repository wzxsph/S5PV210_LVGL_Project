#include <types.h>
#include <s5pv210-clk.h>
#include <s5pv210-irq.h>
#include <s5pv210-tick.h>
#include <s5pv210-serial.h>
#include <malloc.h>
#include "lcd.h"

#include "lvgl/lvgl.h"
#include "porting/lv_port_disp.h"
#include "porting/lv_port_indev.h"

static void hardware_init(void)
{
	malloc_init();

	s5pv210_clk_initial();
	s5pv210_irq_initial();
	s5pv210_tick_initial();
	s5pv210_serial_initial();
	s5pv210_fb_initial();
}

static void create_demo_ui(void)
{
	lv_obj_t * scr = lv_scr_act();

	lv_obj_t * label = lv_label_create(scr);
	lv_label_set_text(label, "Hello S5PV210 LVGL!");
    lv_obj_set_style_text_color(label, lv_color_hex(0xff0000), LV_PART_MAIN); /* Red color for test */
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

	/* 2. LVGL Init */
	lv_init();
	lv_port_disp_init();
	lv_port_indev_init();

	/* 3. Draw UI */
	create_demo_ui();

	/* 4. Main loop */
	while(1)
	{
		lv_timer_handler();
        /* Add a simple delay or WFI (Wait-For-Interrupt) if needed for CPU power saving */
        extern void mdelay(unsigned int);
        mdelay(5);
	}

	return 0;
}
