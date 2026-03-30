#include "lcd.h"
#include <io.h>
#include <s5pv210/reg-gpio.h>
#include <s5pv210/reg-lcd.h>
#include <s5pv210/reg-others.h>
#include <s5pv210-tick-delay.h>

#define FB_ADDR 0x23000000

volatile unsigned short *pfb = (volatile unsigned short *)FB_ADDR;

void lcd_init(void)
{
	/* Configure GPF0,1,2,3 for LCD */
	writel(S5PV210_GPF0CON, 0x22222222);
	writel(S5PV210_GPF0DRV, 0xffffffff);
	writel(S5PV210_GPF0PUD, 0x0);
	
	writel(S5PV210_GPF1CON, 0x22222222);
	writel(S5PV210_GPF1DRV, 0xffffffff);
	writel(S5PV210_GPF1PUD, 0x0);
	
	writel(S5PV210_GPF2CON, 0x22222222);
	writel(S5PV210_GPF2DRV, 0xffffffff);
	writel(S5PV210_GPF2PUD, 0x0);
	
	writel(S5PV210_GPF3CON, (readl(S5PV210_GPF3CON) & ~(0xffff<<0)) | (0x2222<<0));
	writel(S5PV210_GPF3DRV, (readl(S5PV210_GPF3DRV) & ~(0xff<<0)) | (0xff<<0));
	writel(S5PV210_GPF3PUD, (readl(S5PV210_GPF3PUD) & ~(0xff<<0)) | (0x00<<0));

	/* Backlight configuration (GPD0_0 & GPF3_5) */
	writel(S5PV210_GPD0CON, (readl(S5PV210_GPD0CON) & ~(0xf<<0)) | (0x1<<0));
	writel(S5PV210_GPD0PUD, (readl(S5PV210_GPD0PUD) & ~(0x3<<0)) | (0x2<<0));
	writel(S5PV210_GPD0DAT, (readl(S5PV210_GPD0DAT) & ~(0x1<<0)) | (0x1<<0));

	writel(S5PV210_GPF3CON, (readl(S5PV210_GPF3CON) & ~(0xf<<20)) | (0x1<<20));
	writel(S5PV210_GPF3PUD, (readl(S5PV210_GPF3PUD) & ~(0x3<<10)) | (0x2<<10));
	writel(S5PV210_GPF3DAT, (readl(S5PV210_GPF3DAT) & ~(0x1<<5)) | (0x0<<5));

	mdelay(10);
}

void lcd_backlight(unsigned char brightness)
{
	if(brightness) {
		writel(S5PV210_GPF3DAT, (readl(S5PV210_GPF3DAT) & ~(0x1<<5)) | (0x1<<5));
		writel(S5PV210_GPD0DAT, (readl(S5PV210_GPD0DAT) & ~(0x1<<0)) | (0x0<<0));
	} else {
		writel(S5PV210_GPF3DAT, (readl(S5PV210_GPF3DAT) & ~(0x1<<5)) | (0x0<<5));
		writel(S5PV210_GPD0DAT, (readl(S5PV210_GPD0DAT) & ~(0x1<<0)) | (0x1<<0));
	}
}

void s5pv210_fb_initial(void)
{
	unsigned int cfg;

	lcd_init();

	/* VIDCON0: Set clock prescaler and RGB interface */
	cfg = (4 << 6) | (1 << 4); /* CLKVAL = 4, RGB output */
	writel(S5PV210_VIDCON0, cfg);

	/* VIDCON1: Invert VSYNC and HSYNC */
	cfg = (1 << 6) | (1 << 5); 
	writel(S5PV210_VIDCON1, cfg);

	/* Timings */
	writel(S5PV210_VIDTCON0, (VBPD << 16) | (VFPD << 8) | (VSPW << 0));
	writel(S5PV210_VIDTCON1, (HBPD << 16) | (HFPD << 8) | (HSPW << 0));
	writel(S5PV210_VIDTCON2, ((LCD_HEIGHT - 1) << 11) | ((LCD_WIDTH - 1) << 0));

	/* WINCON0: 16BPP RGB565 (0x5) */
	writel(S5PV210_WINCON0, (0x5 << 2));

	/* Window 0 Size and Position */
	writel(S5PV210_VIDOSD0A, (0 << 11) | (0 << 0));
	writel(S5PV210_VIDOSD0B, ((LCD_WIDTH - 1) << 11) | ((LCD_HEIGHT - 1) << 0));
	writel(S5PV210_VIDOSD0C, (LCD_WIDTH * LCD_HEIGHT)); /* Size in words/half words depending on bitdepth */

	/* Buffer addresses */
	writel(S5PV210_VIDW00ADD0B0, FB_ADDR);
	writel(S5PV210_VIDW00ADD1B0, FB_ADDR + FB_SIZE);

	/* Enable Window 0 */
	cfg = readl(S5PV210_WINCON0);
	cfg |= (1 << 0);
	writel(S5PV210_WINCON0, cfg);

	/* Enable global LCD display */
	cfg = readl(S5PV210_VIDCON0);
	cfg |= (3 << 0); /* ENVID = 1, ENVID_F = 1 */
	writel(S5PV210_VIDCON0, cfg);

	/* Display path selection -> 0x2 */
	writel(S5PV210_DISPLAY_CONTROL, (readl(S5PV210_DISPLAY_CONTROL) & ~(0x3<<0)) | (0x2<<0));

	/* Ensure SHADOWCON does not block */
	cfg = readl(S5PV210_SHADOWCON);
	cfg |= (1 << 2);
	writel(S5PV210_SHADOWCON, cfg);

	/* Wait for stable */
	lcd_backlight(1);
}
