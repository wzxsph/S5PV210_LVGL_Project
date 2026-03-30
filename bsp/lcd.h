#ifndef __LCD_H__
#define __LCD_H__

#include <types.h>

/* Screen Resolution */
#if 1
#define LCD_WIDTH  1024
#define LCD_HEIGHT 600
/* Timing for 1024x600 */
#define VSPW       (3 - 1)
#define VBPD       (20 - 1)
#define VFPD       (12 - 1)
#define HSPW       (20 - 1)
#define HBPD       (140 - 1)
#define HFPD       (160 - 1)
#else
#define LCD_WIDTH  800
#define LCD_HEIGHT 480
/* Timing for 800x480 */
#define VSPW       (3 - 1)
#define VBPD       (13 - 1)
#define VFPD       (22 - 1)
#define HSPW       (20 - 1)
#define HBPD       (26 - 1)
#define HFPD       (210 - 1)
#endif

/* Total Frame Buffer Size in Bytes for RGB565 (2 Bytes per Pixel) */
#define FB_SIZE (LCD_WIDTH * LCD_HEIGHT * 2)

/* LCD Frame Buffer Address (Double buffered if needed) */
extern volatile unsigned short *pfb;

void lcd_init(void);
void lcd_backlight(unsigned char brightness);
void s5pv210_fb_initial(void);

#endif
