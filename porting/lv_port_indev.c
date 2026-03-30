#include "lv_port_indev.h"

static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

lv_indev_t * indev_touchpad;

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    /*------------------
     * Touchpad
     * -----------------*/

    /*Initialize your touchpad if you have*/
    touchpad_init();

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /*Save the pressed coordinates and the state*/
    /* TODO: Implement I2C touch reading */
    bool touchpad_pressed = false; /* placeholder */

    if(touchpad_pressed) {
        data->state = LV_INDEV_STATE_PR;

        /*Set the last pressed coordinates*/
        // last_x = x;
        // last_y = y;
        data->point.x = last_x;
        data->point.y = last_y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
