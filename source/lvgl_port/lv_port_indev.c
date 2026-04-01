/**
 * @file lv_port_indev.c
 * LVGL Input Device Driver for S5PV210 (Study210) bare-metal
 *
 * Keys: POWER, LEFT, DOWN, UP, RIGHT, BACK, MENU
 * Mapped to LVGL keypad input device for group navigation
 */

#include "lv_port_indev.h"
#include "lvgl/lvgl.h"
#include <main.h>
#include <hw-key.h>

static lv_indev_drv_t indev_drv;
static lv_group_t *indev_group;

/**
 * Keypad read callback
 * Maps hardware keys to LVGL key codes
 */
static void keypad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static u32_t last_key = 0;
    u32_t keyup = 0, keydown = 0;

    data->state = LV_INDEV_STATE_RELEASED;

    if(get_key_event(&keyup, &keydown)) {
        if(keydown) {
            data->state = LV_INDEV_STATE_PRESSED;

            if(keydown & KEY_NAME_UP)
                data->key = LV_KEY_UP;
            else if(keydown & KEY_NAME_DOWN)
                data->key = LV_KEY_DOWN;
            else if(keydown & KEY_NAME_LEFT)
                data->key = LV_KEY_LEFT;
            else if(keydown & KEY_NAME_RIGHT)
                data->key = LV_KEY_RIGHT;
            else if(keydown & KEY_NAME_MENU)
                data->key = LV_KEY_ENTER;
            else if(keydown & KEY_NAME_BACK)
                data->key = LV_KEY_ESC;
            else if(keydown & KEY_NAME_POWER)
                data->key = LV_KEY_HOME;

            last_key = data->key;
        }
        if(keyup) {
            data->state = LV_INDEV_STATE_RELEASED;
            data->key = last_key;
        }
    } else {
        /* No key event - check if any key is still held */
        u32_t key_status = 0;
        if(get_key_status(&key_status) && key_status) {
            data->state = LV_INDEV_STATE_PRESSED;

            if(key_status & KEY_NAME_UP)
                data->key = LV_KEY_UP;
            else if(key_status & KEY_NAME_DOWN)
                data->key = LV_KEY_DOWN;
            else if(key_status & KEY_NAME_LEFT)
                data->key = LV_KEY_LEFT;
            else if(key_status & KEY_NAME_RIGHT)
                data->key = LV_KEY_RIGHT;
            else if(key_status & KEY_NAME_MENU)
                data->key = LV_KEY_ENTER;
            else if(key_status & KEY_NAME_BACK)
                data->key = LV_KEY_ESC;
            else if(key_status & KEY_NAME_POWER)
                data->key = LV_KEY_HOME;

            last_key = data->key;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
            data->key = last_key;
        }
    }
}

/**
 * Initialize the input device driver
 */
void lv_port_indev_init(void)
{
    /* Create a group for key navigation */
    indev_group = lv_group_create();
    lv_group_set_default(indev_group);

    /* Register keypad input device */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;

    lv_indev_t *indev = lv_indev_drv_register(&indev_drv);

    /* Assign the group to the keypad */
    lv_indev_set_group(indev, indev_group);
}

/**
 * Get the input device group for adding widgets to navigate
 */
lv_group_t * lv_port_indev_get_group(void)
{
    return indev_group;
}
