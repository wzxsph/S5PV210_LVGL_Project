#include "lv_port_disp.h"
#include <stdbool.h>
#include "lcd.h"

#define DISP_BUF_SIZE (LCD_WIDTH * LCD_HEIGHT / 10)

static lv_color_t buf_1[DISP_BUF_SIZE];
static lv_color_t buf_2[DISP_BUF_SIZE];

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    /* lcd_init() is called in s5pv210_fb_initial() during system startup */

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/

    static lv_disp_draw_buf_t draw_buf_dsc;
    lv_disp_draw_buf_init(&draw_buf_dsc, buf_1, buf_2, DISP_BUF_SIZE);   /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set the resolution of the display*/
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;

    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
}

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/
    int32_t x;
    int32_t y;
    
    /* 
     * TODO: Replace below CPU copy with S5PV210 DMA controller for "High Score" requirements
     * s5pv210_dma_transfer(dest_addr, (uint32_t)color_p, length); 
     */
    
    for(y = area->y1; y <= area->y2; y++) {
        for(x = area->x1; x <= area->x2; x++) {
            /* Put a pixel to the display. */
            *(pfb + y * LCD_WIDTH + x) = color_p->full;
            color_p++;
        }
    }

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
}
