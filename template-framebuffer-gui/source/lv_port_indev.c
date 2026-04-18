/*
 * lv_port_indev.c - LVGL input device driver for X210 board GPIO keys
 *
 * Hardware key mapping (X210 board, matching template-key-with-led):
 *   POWER -> GPH0_1 (EINT1)   -> unused
 *   LEFT  -> GPH0_2 (EINT2)   -> LV_KEY_LEFT
 *   DOWN  -> GPH0_3 (EINT3)   -> LV_KEY_NEXT  (navigate to next widget)
 *   UP    -> GPH2_0 (KP_COL0) -> LV_KEY_PREV  (navigate to previous widget)
 *   RIGHT -> GPH2_1 (KP_COL1) -> LV_KEY_RIGHT
 *   BACK  -> GPH2_2 (KP_COL2) -> LV_KEY_ESC   (exit edit mode / go back)
 *   MENU  -> GPH2_3 (KP_COL3) -> LV_KEY_ENTER (select / enter edit mode)
 *
 * LVGL keypad navigation model:
 *   NEXT/PREV  - move focus between widgets in the group
 *   ENTER      - activate widget / enter edit mode
 *   ESC        - deactivate widget / exit edit mode
 *   LEFT/RIGHT - in-widget control (slider value, tab switch, etc.)
 *
 * Uses get_key_status() polling — LVGL handles edge detection internally.
 */

#include "lvgl/lvgl.h"
#include <types.h>
#include <hw-key.h>

static lv_indev_t * keypad_indev = NULL;
static lv_group_t * keypad_group = NULL;

/* Last reported key code (LVGL always needs a valid key) */
static uint32_t last_key = LV_KEY_ENTER;

static void keypad_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    (void)indev;

    u32_t key_status = 0;

    if (!get_key_status(&key_status)) {
        /* Debounce failed — report released with last key */
        data->key = last_key;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (key_status != 0) {
        /* A key is pressed — map to LVGL key code (one key at a time) */
        if (key_status & KEY_NAME_UP) {
            last_key = LV_KEY_PREV;
        } else if (key_status & KEY_NAME_DOWN) {
            last_key = LV_KEY_NEXT;
        } else if (key_status & KEY_NAME_LEFT) {
            last_key = LV_KEY_LEFT;
        } else if (key_status & KEY_NAME_RIGHT) {
            last_key = LV_KEY_RIGHT;
        } else if (key_status & KEY_NAME_MENU) {
            last_key = LV_KEY_ENTER;
        } else if (key_status & KEY_NAME_BACK) {
            last_key = LV_KEY_ESC;
        } else {
            /* POWER or unknown — ignore */
            data->key = last_key;
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        /* No key pressed */
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->key = last_key;
}

void lv_port_indev_init(void)
{
    /* Create keypad input device */
    keypad_indev = lv_indev_create();
    lv_indev_set_type(keypad_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keypad_indev, keypad_read_cb);

    /* Create a group for keypad navigation and set as default.
     * LVGL v9 auto-adds widgets with group_def=TRUE to the default group,
     * so widgets created by lv_demo_widgets() will be navigable. */
    keypad_group = lv_group_create();
    lv_indev_set_group(keypad_indev, keypad_group);
    lv_group_set_default(keypad_group);
}
