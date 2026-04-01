/**
 * @file lv_port_disp.c
 * LVGL Display Driver for S5PV210 (Study210) bare-metal framebuffer
 *
 * LCD: 1024x600, 32bpp ARGB8888, dual framebuffer via Window2
 */

#include "lv_port_disp.h"
#include "lvgl/lvgl.h"
#include <main.h>
#include <s5pv210-fb.h>
#include <string.h>

#define DISP_HOR_RES  1024
#define DISP_VER_RES  600

/* Draw buffer - 1/10 of screen size for partial rendering */
#define DISP_BUF_LINES  60
static lv_color_t draw_buf1[DISP_HOR_RES * DISP_BUF_LINES] __attribute__((aligned(4)));

static lv_disp_draw_buf_t draw_buf_dsc;
static lv_disp_drv_t disp_drv;

/**
 * Flush callback - copy rendered pixels to the framebuffer
 */
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    struct surface_t *surface = s5pv210_screen_surface();
    u32_t *fb = (u32_t *)surface->pixels;

    int32_t w = area->x2 - area->x1 + 1;
    int32_t y;

    for(y = area->y1; y <= area->y2; y++) {
        u32_t *dest = fb + (y * DISP_HOR_RES) + area->x1;
        memcpy(dest, color_p, w * sizeof(lv_color_t));
        color_p += w;
    }

    lv_disp_flush_ready(drv);
}

/**
 * Initialize the display driver for LVGL
 */
void lv_port_disp_init(void)
{
    /* Initialize draw buffer */
    lv_disp_draw_buf_init(&draw_buf_dsc, draw_buf1, NULL, DISP_HOR_RES * DISP_BUF_LINES);

    /* Initialize and register display driver */
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;

    /* Full-screen refresh not needed; partial rendering is more efficient */
    disp_drv.full_refresh = 0;

    lv_disp_drv_register(&disp_drv);
}
