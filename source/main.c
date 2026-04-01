/**
 * @file main.c
 * LVGL on S5PV210 (Study210) Bare-Metal Demo
 *
 * Demonstrates LVGL porting with:
 * - System dashboard with live uptime
 * - Interactive controls (slider, buttons, switch)
 * - Key navigation via hardware buttons
 */

#include <main.h>
#include "lvgl/lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

/*--- Forward declarations ---*/
static void do_system_initial(void);
static void create_ui_real(void);

/*--- UI state ---*/
static lv_obj_t *label_uptime;
static lv_obj_t *label_slider_val;
static lv_obj_t *label_status;
static u32_t btn_press_count = 0;

/*===========================================================
 * System initialization
 *===========================================================*/
static void do_system_initial(void)
{
    malloc_init();

    s5pv210_clk_initial();
    s5pv210_irq_initial();
    s5pv210_tick_initial();
    s5pv210_tick_delay_initial();
    s5pv210_serial_initial();
    s5pv210_fb_initial();

    led_initial();
    beep_initial();
    key_initial();
}

/*===========================================================
 * UI Event Callbacks
 *===========================================================*/

/* Slider value changed callback */
static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", (int)val);
    lv_label_set_text(label_slider_val, buf);
}

/* Button click callback */
static void btn_event_cb(lv_event_t *e)
{
    btn_press_count++;
    char buf[48];
    snprintf(buf, sizeof(buf), "Button pressed: %d times", (int)btn_press_count);
    lv_label_set_text(label_status, buf);
}

/* Switch toggle callback */
static void switch_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    if(lv_obj_has_state(sw, LV_STATE_CHECKED)) {
        lv_label_set_text(label_status, "LED: ON");
        led_set_status(LED_NAME_LED1, LED_STATUS_ON);
    } else {
        lv_label_set_text(label_status, "LED: OFF");
        led_set_status(LED_NAME_LED1, LED_STATUS_OFF);
    }
}

/*===========================================================
 * Uptime and progress bar update - called from main loop
 *===========================================================*/
static lv_obj_t *g_bar = NULL;
static int g_progress = 0;


/*
 * Separate timer callback as a proper function
 */
static void uptime_timer_cb(lv_timer_t *timer)
{
    u32_t uptime_sec = jiffies / 100;
    u32_t hours = uptime_sec / 3600;
    u32_t mins = (uptime_sec % 3600) / 60;
    u32_t secs = uptime_sec % 60;

    char buf[32];
    if(hours > 0)
        snprintf(buf, sizeof(buf), "Up:   %dh%02dm%02ds", (int)hours, (int)mins, (int)secs);
    else if(mins > 0)
        snprintf(buf, sizeof(buf), "Up:   %dm%02ds", (int)mins, (int)secs);
    else
        snprintf(buf, sizeof(buf), "Up:   %ds", (int)secs);

    if(label_uptime)
        lv_label_set_text(label_uptime, buf);

    /* Animate progress bar */
    if(g_bar) {
        g_progress += 2;
        if(g_progress > 100) g_progress = 0;
        lv_bar_set_value(g_bar, g_progress, LV_ANIM_ON);
    }
}

/*===========================================================
 * Main entry point
 *===========================================================*/
int main(int argc, char * argv[])
{
    do_system_initial();

    /* Initialize LVGL */
    lv_init();

    /* Initialize display and input drivers */
    lv_port_disp_init();
    lv_port_indev_init();

    /* Create the UI */
    create_ui_real();

    /* Main loop */
    while(1) {
        lv_timer_handler();
        mdelay(5);
    }

    return 0;
}

/*===========================================================
 * Actual UI creation (clean version without lambda issues)
 *===========================================================*/
static void create_ui_real(void)
{
    lv_group_t *g = lv_port_indev_get_group();

    /* ========== Style setup ========== */
    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_title, lv_color_hex(0x00B4D8));

    static lv_style_t style_card;
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x1E293B));
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_pad_all(&style_card, 20);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, lv_color_hex(0x334155));

    /* ========== Title bar ========== */
    lv_obj_t *title_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(title_bar, 1024, 60);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_hor(title_bar, 20, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, LV_SYMBOL_HOME "  LVGL on Study210");
    lv_obj_add_style(title_label, &style_title, 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *subtitle = lv_label_create(title_bar);
    lv_label_set_text(subtitle, "S5PV210 Bare-Metal Demo");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_align(subtitle, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ========== Card 1: System Info ========== */
    lv_obj_t *card1 = lv_obj_create(lv_scr_act());
    lv_obj_add_style(card1, &style_card, 0);
    lv_obj_set_size(card1, 300, 220);
    lv_obj_align(card1, LV_ALIGN_TOP_LEFT, 20, 75);
    lv_obj_clear_flag(card1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card1_title = lv_label_create(card1);
    lv_label_set_text(card1_title, LV_SYMBOL_SETTINGS "  System Info");
    lv_obj_set_style_text_font(card1_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(card1_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(card1_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *sep1 = lv_obj_create(card1);
    lv_obj_set_size(sep1, 260, 1);
    lv_obj_set_style_bg_color(sep1, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_opa(sep1, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_set_style_radius(sep1, 0, 0);
    lv_obj_set_style_pad_all(sep1, 0, 0);
    lv_obj_align(sep1, LV_ALIGN_TOP_LEFT, 0, 30);

    lv_obj_t *info1 = lv_label_create(card1);
    lv_label_set_text(info1, "CPU:  S5PV210 Cortex-A8");
    lv_obj_set_style_text_color(info1, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(info1, LV_ALIGN_TOP_LEFT, 0, 40);

    lv_obj_t *info2 = lv_label_create(card1);
    lv_label_set_text(info2, "LCD:  1024x600 @32bpp");
    lv_obj_set_style_text_color(info2, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(info2, LV_ALIGN_TOP_LEFT, 0, 65);

    lv_obj_t *info3 = lv_label_create(card1);
    lv_label_set_text(info3, "RAM:  512MB DDR2");
    lv_obj_set_style_text_color(info3, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(info3, LV_ALIGN_TOP_LEFT, 0, 90);

    lv_obj_t *info4 = lv_label_create(card1);
    lv_label_set_text(info4, "Tick: 100Hz");
    lv_obj_set_style_text_color(info4, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(info4, LV_ALIGN_TOP_LEFT, 0, 115);

    label_uptime = lv_label_create(card1);
    lv_label_set_text(label_uptime, "Up:   0s");
    lv_obj_set_style_text_color(label_uptime, lv_color_hex(0x4ADE80), 0);
    lv_obj_align(label_uptime, LV_ALIGN_TOP_LEFT, 0, 140);

    /* ========== Card 2: Controls ========== */
    lv_obj_t *card2 = lv_obj_create(lv_scr_act());
    lv_obj_add_style(card2, &style_card, 0);
    lv_obj_set_size(card2, 340, 220);
    lv_obj_align(card2, LV_ALIGN_TOP_MID, 0, 75);
    lv_obj_clear_flag(card2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card2_title = lv_label_create(card2);
    lv_label_set_text(card2_title, LV_SYMBOL_EDIT "  Controls");
    lv_obj_set_style_text_font(card2_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(card2_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(card2_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *sep2 = lv_obj_create(card2);
    lv_obj_set_size(sep2, 300, 1);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_opa(sep2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_radius(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);
    lv_obj_align(sep2, LV_ALIGN_TOP_LEFT, 0, 30);

    /* Slider */
    lv_obj_t *slider_label = lv_label_create(card2);
    lv_label_set_text(slider_label, "Brightness:");
    lv_obj_set_style_text_color(slider_label, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(slider_label, LV_ALIGN_TOP_LEFT, 0, 45);

    lv_obj_t *slider = lv_slider_create(card2);
    lv_obj_set_size(slider, 200, 10);
    lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 0, 75);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, slider);

    label_slider_val = lv_label_create(card2);
    lv_label_set_text(label_slider_val, "50%");
    lv_obj_set_style_text_color(label_slider_val, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(label_slider_val, LV_ALIGN_TOP_LEFT, 215, 68);

    /* Button */
    lv_obj_t *btn = lv_btn_create(card2);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, 105);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2563EB), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1D4ED8), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, btn);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, LV_SYMBOL_OK " Press Me");
    lv_obj_center(btn_label);

    /* Switch */
    lv_obj_t *sw_label = lv_label_create(card2);
    lv_label_set_text(sw_label, "LED:");
    lv_obj_set_style_text_color(sw_label, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(sw_label, LV_ALIGN_TOP_LEFT, 150, 115);

    lv_obj_t *sw = lv_switch_create(card2);
    lv_obj_align(sw, LV_ALIGN_TOP_LEFT, 195, 110);
    lv_obj_add_event_cb(sw, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, sw);

    /* Status */
    label_status = lv_label_create(card2);
    lv_label_set_text(label_status, "Use keys to navigate");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_12, 0);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* ========== Card 3: Indicators ========== */
    lv_obj_t *card3 = lv_obj_create(lv_scr_act());
    lv_obj_add_style(card3, &style_card, 0);
    lv_obj_set_size(card3, 300, 220);
    lv_obj_align(card3, LV_ALIGN_TOP_RIGHT, -20, 75);
    lv_obj_clear_flag(card3, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card3_title = lv_label_create(card3);
    lv_label_set_text(card3_title, LV_SYMBOL_EYE_OPEN "  Indicators");
    lv_obj_set_style_text_font(card3_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(card3_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(card3_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *sep3 = lv_obj_create(card3);
    lv_obj_set_size(sep3, 260, 1);
    lv_obj_set_style_bg_color(sep3, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_opa(sep3, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep3, 0, 0);
    lv_obj_set_style_radius(sep3, 0, 0);
    lv_obj_set_style_pad_all(sep3, 0, 0);
    lv_obj_align(sep3, LV_ALIGN_TOP_LEFT, 0, 30);

    /* Progress bar */
    lv_obj_t *bar_label = lv_label_create(card3);
    lv_label_set_text(bar_label, "Progress:");
    lv_obj_set_style_text_color(bar_label, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(bar_label, LV_ALIGN_TOP_LEFT, 0, 40);

    g_bar = lv_bar_create(card3);
    lv_obj_set_size(g_bar, 240, 15);
    lv_obj_align(g_bar, LV_ALIGN_TOP_LEFT, 0, 65);
    lv_bar_set_value(g_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x22C55E), LV_PART_INDICATOR);

    /* Spinner */
    lv_obj_t *spinner = lv_spinner_create(card3, 1000, 60);
    lv_obj_set_size(spinner, 50, 50);
    lv_obj_align(spinner, LV_ALIGN_TOP_LEFT, 15, 100);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x334155), 0);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x3B82F6), LV_PART_INDICATOR);

    lv_obj_t *spin_label = lv_label_create(card3);
    lv_label_set_text(spin_label, "Processing...");
    lv_obj_set_style_text_color(spin_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(spin_label, LV_ALIGN_TOP_LEFT, 80, 115);

    /* LED indicators */
    lv_obj_t *led_g = lv_led_create(card3);
    lv_obj_set_size(led_g, 20, 20);
    lv_obj_align(led_g, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_led_set_color(led_g, lv_color_hex(0x22C55E));
    lv_led_on(led_g);

    lv_obj_t *led_g_lbl = lv_label_create(card3);
    lv_label_set_text(led_g_lbl, "OK");
    lv_obj_set_style_text_color(led_g_lbl, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(led_g_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align_to(led_g_lbl, led_g, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    lv_obj_t *led_y = lv_led_create(card3);
    lv_obj_set_size(led_y, 20, 20);
    lv_obj_align(led_y, LV_ALIGN_BOTTOM_LEFT, 60, 0);
    lv_led_set_color(led_y, lv_color_hex(0xFBBF24));
    lv_led_set_brightness(led_y, 150);

    lv_obj_t *led_y_lbl = lv_label_create(card3);
    lv_label_set_text(led_y_lbl, "WARN");
    lv_obj_set_style_text_color(led_y_lbl, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(led_y_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align_to(led_y_lbl, led_y, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    lv_obj_t *led_r = lv_led_create(card3);
    lv_obj_set_size(led_r, 20, 20);
    lv_obj_align(led_r, LV_ALIGN_BOTTOM_LEFT, 140, 0);
    lv_led_set_color(led_r, lv_color_hex(0xEF4444));
    lv_led_off(led_r);

    lv_obj_t *led_r_lbl = lv_label_create(card3);
    lv_label_set_text(led_r_lbl, "ERR");
    lv_obj_set_style_text_color(led_r_lbl, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(led_r_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align_to(led_r_lbl, led_r, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    /* ========== Bottom key hint bar ========== */
    lv_obj_t *hint_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(hint_bar, 1024, 40);
    lv_obj_align(hint_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(hint_bar, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(hint_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hint_bar, 0, 0);
    lv_obj_set_style_radius(hint_bar, 0, 0);
    lv_obj_clear_flag(hint_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hint = lv_label_create(hint_bar);
    lv_label_set_text(hint,
        LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Navigate   "
        LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust   "
        "MENU=Enter   BACK=Esc");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_center(hint);

    /* ========== Set dark background ========== */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0F172A), 0);

    /* ========== Create uptime update timer ========== */
    lv_timer_create(uptime_timer_cb, 1000, NULL);
}
