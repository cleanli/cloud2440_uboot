/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <draw.h>
#include <netdev.h>
#include <asm/arch/s3c24x0_cpu.h>

DECLARE_GLOBAL_DATA_PTR;

#define FCLK_SPEED 1

#if FCLK_SPEED==0		/* Fout = 203MHz, Fin = 12MHz for Audio */
#define M_MDIV	0xC3
#define M_PDIV	0x4
#define M_SDIV	0x1
#elif FCLK_SPEED==1		/* Fout = 202.8MHz */
#define M_MDIV	0x7f
#define M_PDIV	0x2
#define M_SDIV	0x1
#endif

#define USB_CLOCK 1

#if USB_CLOCK==0
#define U_M_MDIV	0xA1
#define U_M_PDIV	0x3
#define U_M_SDIV	0x1
#elif USB_CLOCK==1
#define U_M_MDIV	0x38
#define U_M_PDIV	0x2
#define U_M_SDIV	0x2
#endif

// ADC
#define rADCCON    (*(volatile unsigned *)0x58000000) //ADC control
#define rADCTSC    (*(volatile unsigned *)0x58000004) //ADC touch screen control
#define rADCDLY    (*(volatile unsigned *)0x58000008) //ADC start or Interval Delay
#define rADCDAT0   (*(volatile unsigned *)0x5800000c) //ADC conversion data 0
#define rADCDAT1   (*(volatile unsigned *)0x58000010) //ADC conversion data 1

#define BIT_ADC        (0x1<<31)
#define BIT_SUB_TC     (0x1<<9)

// INTERRUPT
#define rSRCPND     (*(volatile unsigned *)0x4a000000) //Interrupt request status
#define rINTMOD     (*(volatile unsigned *)0x4a000004) //Interrupt mode control
#define rINTMSK     (*(volatile unsigned *)0x4a000008) //Interrupt mask control
#define rPRIORITY   (*(volatile unsigned *)0x4a00000a) //IRQ priority control
#define rINTPND     (*(volatile unsigned *)0x4a000010) //Interrupt request status
#define rINTOFFSET  (*(volatile unsigned *)0x4a000014) //Interruot request source offset
#define rSUBSRCPND  (*(volatile unsigned *)0x4a000018) //Sub source pending
#define rINTSUBMSK  (*(volatile unsigned *)0x4a00001c) //Interrupt sub mask

static void video_drawchars (int xx, int yy, char *s, int count);
void video_drawstring (int xx, int yy, char *s);
static int video_init (void);

static inline void delay (unsigned long loops)
{
	__asm__ volatile ("1:\n"
	  "subs %0, %1, #1\n"
	  "bne 1b":"=r" (loops):"0" (loops));
}

/*
 * Miscellaneous platform dependent initialisations
 */

int board_init (void)
{
	struct s3c24x0_clock_power * const clk_power =
					s3c24x0_get_base_clock_power();
	struct s3c24x0_gpio * const gpio = s3c24x0_get_base_gpio();

	/* to reduce PLL lock time, adjust the LOCKTIME register */
	clk_power->LOCKTIME = 0xFFFFFF;

	/* configure MPLL */
	clk_power->MPLLCON = ((M_MDIV << 12) + (M_PDIV << 4) + M_SDIV);

	/* some delay between MPLL and UPLL */
	delay (4000);

	/* configure UPLL */
	clk_power->UPLLCON = ((U_M_MDIV << 12) + (U_M_PDIV << 4) + U_M_SDIV);

	/* some delay between MPLL and UPLL */
	delay (8000);

	/* set up the I/O ports */
	/*
	gpio->GPACON = 0x007FFFFF;
	gpio->GPBCON = 0x00044555;
	gpio->GPBUP = 0x000007FF;
	*/
	gpio->GPCCON = 0xAAAAAAAA;
	gpio->GPCUP = 0x0000FFFF;
	gpio->GPDCON = 0xAAAAAAAA;
	gpio->GPDUP = 0x0000FFFF;
	gpio->GPECON = 0xAAAAAAAA;
	gpio->GPEUP = 0x0000FFFF;
	gpio->GPFCON = 0x00005500;
	gpio->GPFUP = 0x000000FF;
	gpio->GPGUP = 0x0000FFFF;
	gpio->GPGCON = 0x300;
	gpio->GPHCON = 0x002AFAAA;
	gpio->GPHUP = 0x000007FF;

	video_init();
	video_drawstring(15,5,"Welcome to use cloud2440 U-boot!"); 

    //Touch screen init
    rADCCON=(1<<14)+(49<<6);
    rADCTSC=0xd3;//wait pen down

	/* arch number of CLOUD2440-Board */
	gd->bd->bi_arch_number = MACH_TYPE_CLOUD2440;

	/* adress of boot parameters */
	gd->bd->bi_boot_params = 0x30000100;

	icache_enable();
	dcache_enable();

	return 0;
}

int dram_init (void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}

#ifdef CONFIG_CMD_NET
int board_eth_init(bd_t *bis)
{
#ifdef CONFIG_DRIVER_DM9000
	return dm9000_initialize(bis);
#endif
}
#endif

/**************************video**********************************************/
#include <video_font.h>
#include <video_fb.h>
static u32 eorx, fgx, bgx;  /* color pats */
static u8* video_fb_address;
#define VIDEO_FB_ADRS 0x33000000
#define VIDEO_DATA_FORMAT GDF_16BIT_565RGB
#define VIDEO_PIXEL_SIZE 2	
#define VIDEO_LINE_LEN		(VIDEO_COLS*VIDEO_PIXEL_SIZE)
#define VIDEO_COLS 320	
#define VIDEO_ROWS 240
/* Macros */
//#define VIDEO_FB_LITTLE_ENDIAN
#define VIDEO_FB_16BPP_WORD_SWAP
#ifdef	VIDEO_FB_LITTLE_ENDIAN
#define SWAP16(x)	 ((((x) & 0x00ff) << 8) | ( (x) >> 8))
#define SWAP32(x)	 ((((x) & 0x000000ff) << 24) | (((x) & 0x0000ff00) << 8)|\
			  (((x) & 0x00ff0000) >>  8) | (((x) & 0xff000000) >> 24) )
#define SHORTSWAP32(x)	 ((((x) & 0x000000ff) <<  8) | (((x) & 0x0000ff00) >> 8)|\
			  (((x) & 0x00ff0000) <<  8) | (((x) & 0xff000000) >> 8) )
#else
#define SWAP16(x)	 (x)
#define SWAP32(x)	 (x)
#if defined(VIDEO_FB_16BPP_WORD_SWAP)
#define SHORTSWAP32(x)	 ( ((x) >> 16) | ((x) << 16) )
#else
#define SHORTSWAP32(x)	 (x)
#endif
#endif

static const int video_font_draw_table8[] = {
	    0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff,
	    0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
	    0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff,
	    0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff };

static const int video_font_draw_table15[] = {
	    0x00000000, 0x00007fff, 0x7fff0000, 0x7fff7fff };

static const int video_font_draw_table16[] = {
	    0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff };

static const int video_font_draw_table24[16][3] = {
	    { 0x00000000, 0x00000000, 0x00000000 },
	    { 0x00000000, 0x00000000, 0x00ffffff },
	    { 0x00000000, 0x0000ffff, 0xff000000 },
	    { 0x00000000, 0x0000ffff, 0xffffffff },
	    { 0x000000ff, 0xffff0000, 0x00000000 },
	    { 0x000000ff, 0xffff0000, 0x00ffffff },
	    { 0x000000ff, 0xffffffff, 0xff000000 },
	    { 0x000000ff, 0xffffffff, 0xffffffff },
	    { 0xffffff00, 0x00000000, 0x00000000 },
	    { 0xffffff00, 0x00000000, 0x00ffffff },
	    { 0xffffff00, 0x0000ffff, 0xff000000 },
	    { 0xffffff00, 0x0000ffff, 0xffffffff },
	    { 0xffffffff, 0xffff0000, 0x00000000 },
	    { 0xffffffff, 0xffff0000, 0x00ffffff },
	    { 0xffffffff, 0xffffffff, 0xff000000 },
	    { 0xffffffff, 0xffffffff, 0xffffffff } };

static const int video_font_draw_table32[16][4] = {
	    { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
	    { 0x00000000, 0x00000000, 0x00000000, 0x00ffffff },
	    { 0x00000000, 0x00000000, 0x00ffffff, 0x00000000 },
	    { 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff },
	    { 0x00000000, 0x00ffffff, 0x00000000, 0x00000000 },
	    { 0x00000000, 0x00ffffff, 0x00000000, 0x00ffffff },
	    { 0x00000000, 0x00ffffff, 0x00ffffff, 0x00000000 },
	    { 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff },
	    { 0x00ffffff, 0x00000000, 0x00000000, 0x00000000 },
	    { 0x00ffffff, 0x00000000, 0x00000000, 0x00ffffff },
	    { 0x00ffffff, 0x00000000, 0x00ffffff, 0x00000000 },
	    { 0x00ffffff, 0x00000000, 0x00ffffff, 0x00ffffff },
	    { 0x00ffffff, 0x00ffffff, 0x00000000, 0x00000000 },
	    { 0x00ffffff, 0x00ffffff, 0x00000000, 0x00ffffff },
	    { 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00000000 },
	    { 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff } };

void set_draw_color(u32 fgcol, u32 bgcol)
{
	unsigned char color8;

	/* Init drawing pats */
	switch (VIDEO_DATA_FORMAT) {
	case GDF__8BIT_332RGB:
		color8 = ((fgcol & 0xe0) |
			  ((fgcol >> 3) & 0x1c) | fgcol >> 6);
		fgx = (color8 << 24) | (color8 << 16) | (color8 << 8) | color8;
		color8 = ((bgcol & 0xe0) |
			  ((bgcol >> 3) & 0x1c) | bgcol >> 6);
		bgx = (color8 << 24) | (color8 << 16) | (color8 << 8) | color8;
		break;
	case GDF_15BIT_555RGB:
		fgx = (((fgcol >> 3) << 26) |
		       ((fgcol >> 3) << 21) | ((fgcol >> 3) << 16) |
		       ((fgcol >> 3) << 10) | ((fgcol >> 3) << 5) |
		       (fgcol >> 3));
		bgx = (((bgcol >> 3) << 26) |
		       ((bgcol >> 3) << 21) | ((bgcol >> 3) << 16) |
		       ((bgcol >> 3) << 10) | ((bgcol >> 3) << 5) |
		       (bgcol >> 3));
		break;
	case GDF_16BIT_565RGB:
		fgx = (((fgcol >> 3) << 27) |
		       ((fgcol >> 2) << 21) | ((fgcol >> 3) << 16) |
		       ((fgcol >> 3) << 11) | ((fgcol >> 2) << 5) |
		       (fgcol >> 3));
		bgx = (((bgcol >> 3) << 27) |
		       ((bgcol >> 2) << 21) | ((bgcol >> 3) << 16) |
		       ((bgcol >> 3) << 11) | ((bgcol >> 2) << 5) |
		       (bgcol >> 3));
		break;
	case GDF_32BIT_X888RGB:
		fgx = (fgcol << 16) | (fgcol << 8) | fgcol;
		bgx = (bgcol << 16) | (bgcol << 8) | bgcol;
		break;
	case GDF_24BIT_888RGB:
		fgx = (fgcol << 24) | (fgcol << 16) |
			(fgcol << 8) | fgcol;
		bgx = (bgcol << 24) | (bgcol << 16) |
			(bgcol << 8) | bgcol;
		break;
	}
	eorx = fgx ^ bgx;

}
static int video_init (void)
{
	/*init framebuffer*/
	*(volatile unsigned int *)0x4d000060 = 0xf82;
	*(volatile unsigned int *)0x4d00001c = 0x140;
	*(volatile unsigned int *)0x4d000018 = 0x12c00;
	*(volatile unsigned int *)0x4d000014 = 0x19800000;
	*(volatile unsigned int *)0x4d000010 = 0x14b09;
	*(volatile unsigned int *)0x4d00000c = 0x2b;
	*(volatile unsigned int *)0x4d000008 = 0xa13f00;
	*(volatile unsigned int *)0x4d000004 = 0x33bc14f;
	*(volatile unsigned int *)0x4d000000 = 0x3180778;
	*(volatile unsigned int *)0x4d000000 = 0x3180779;

	video_fb_address = (void *) VIDEO_FB_ADRS;

	set_draw_color(CONSOLE_FG_COL, CONSOLE_BG_COL);  
	clear_screen();

	return 0;
}


/*****************************************************************************/

static void video_drawchars (int xx, int yy, char *s, int count)
{
	u8 *cdat, *dest, *dest0;
	int rows, offset, c;

	offset = yy * VIDEO_LINE_LEN + xx * VIDEO_PIXEL_SIZE;
	dest0 = video_fb_address + offset;

	switch (VIDEO_DATA_FORMAT) {
	case GDF__8BIT_INDEX:
	case GDF__8BIT_332RGB:
		while (count--) {
			c = *s;
			cdat = video_fontdata + c * VIDEO_FONT_HEIGHT;
			for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
			     rows--;
			     dest += VIDEO_LINE_LEN) {
				u8 bits = *cdat++;

				((u32 *) dest)[0] = (video_font_draw_table8[bits >> 4] & eorx) ^ bgx;
				((u32 *) dest)[1] = (video_font_draw_table8[bits & 15] & eorx) ^ bgx;
			}
			dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			s++;
		}
		break;

	case GDF_15BIT_555RGB:
		while (count--) {
			c = *s;
			cdat = video_fontdata + c * VIDEO_FONT_HEIGHT;
			for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
			     rows--;
			     dest += VIDEO_LINE_LEN) {
				u8 bits = *cdat++;

				((u32 *) dest)[0] = SHORTSWAP32 ((video_font_draw_table15 [bits >> 6] & eorx) ^ bgx);
				((u32 *) dest)[1] = SHORTSWAP32 ((video_font_draw_table15 [bits >> 4 & 3] & eorx) ^ bgx);
				((u32 *) dest)[2] = SHORTSWAP32 ((video_font_draw_table15 [bits >> 2 & 3] & eorx) ^ bgx);
				((u32 *) dest)[3] = SHORTSWAP32 ((video_font_draw_table15 [bits & 3] & eorx) ^ bgx);
			}
			dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			s++;
		}
		break;

	case GDF_16BIT_565RGB:
		while (count--) {
			c = *s;
			cdat = video_fontdata + c * VIDEO_FONT_HEIGHT;
			for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
			     rows--;
			     dest += VIDEO_LINE_LEN) {
				u8 bits = *cdat++;

				((u32 *) dest)[0] = SHORTSWAP32 ((video_font_draw_table16 [bits >> 6] & eorx) ^ bgx);
				((u32 *) dest)[1] = SHORTSWAP32 ((video_font_draw_table16 [bits >> 4 & 3] & eorx) ^ bgx);
				((u32 *) dest)[2] = SHORTSWAP32 ((video_font_draw_table16 [bits >> 2 & 3] & eorx) ^ bgx);
				((u32 *) dest)[3] = SHORTSWAP32 ((video_font_draw_table16 [bits & 3] & eorx) ^ bgx);
			}
			dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			s++;
		}
		break;

	case GDF_32BIT_X888RGB:
		while (count--) {
			c = *s;
			cdat = video_fontdata + c * VIDEO_FONT_HEIGHT;
			for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
			     rows--;
			     dest += VIDEO_LINE_LEN) {
				u8 bits = *cdat++;

				((u32 *) dest)[0] = SWAP32 ((video_font_draw_table32 [bits >> 4][0] & eorx) ^ bgx);
				((u32 *) dest)[1] = SWAP32 ((video_font_draw_table32 [bits >> 4][1] & eorx) ^ bgx);
				((u32 *) dest)[2] = SWAP32 ((video_font_draw_table32 [bits >> 4][2] & eorx) ^ bgx);
				((u32 *) dest)[3] = SWAP32 ((video_font_draw_table32 [bits >> 4][3] & eorx) ^ bgx);
				((u32 *) dest)[4] = SWAP32 ((video_font_draw_table32 [bits & 15][0] & eorx) ^ bgx);
				((u32 *) dest)[5] = SWAP32 ((video_font_draw_table32 [bits & 15][1] & eorx) ^ bgx);
				((u32 *) dest)[6] = SWAP32 ((video_font_draw_table32 [bits & 15][2] & eorx) ^ bgx);
				((u32 *) dest)[7] = SWAP32 ((video_font_draw_table32 [bits & 15][3] & eorx) ^ bgx);
			}
			dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			s++;
		}
		break;

	case GDF_24BIT_888RGB:
		while (count--) {
			c = *s;
			cdat = video_fontdata + c * VIDEO_FONT_HEIGHT;
			for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
			     rows--;
			     dest += VIDEO_LINE_LEN) {
				u8 bits = *cdat++;

				((u32 *) dest)[0] = (video_font_draw_table24[bits >> 4][0] & eorx) ^ bgx;
				((u32 *) dest)[1] = (video_font_draw_table24[bits >> 4][1] & eorx) ^ bgx;
				((u32 *) dest)[2] = (video_font_draw_table24[bits >> 4][2] & eorx) ^ bgx;
				((u32 *) dest)[3] = (video_font_draw_table24[bits & 15][0] & eorx) ^ bgx;
				((u32 *) dest)[4] = (video_font_draw_table24[bits & 15][1] & eorx) ^ bgx;
				((u32 *) dest)[5] = (video_font_draw_table24[bits & 15][2] & eorx) ^ bgx;
			}
			dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			s++;
		}
		break;
	}
}

/*****************************************************************************/

void video_drawstring (int xx, int yy, char *s)
{
	//printf("lcd printing:%s\n",s);
	//getc();
	video_drawchars (xx, yy, s, strlen ((char *)s));
}

void clear_screen()
{
	u32 *pp;
	for(pp = (u32*)0x33000000; (u32)pp < 0x33025800;pp++)
		*pp = bgx;

}

void lcd_printf(int x, int y, const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[128];

	va_start(args, fmt);

	i = vsprintf(printbuffer, fmt, args);
	va_end(args);

	/* Print the string */
	video_drawstring (x, y, printbuffer);
}

#define GPIO_F_DATA (*(unsigned int*)0x56000054)
#define GPIO_G_DATA (*(unsigned int*)0x56000064)
int get_keypress()
{
	int key = 0;
	if((GPIO_F_DATA & 0xf) != 0xf || ((GPIO_G_DATA & 0x808) != 0x808)){
		udelay(50000);
		if((GPIO_F_DATA & 0x1) == 0)
			key = UP_KEY;
		if((GPIO_F_DATA & 0x2) == 0)
			key = DOWN_KEY;
		if((GPIO_F_DATA & 0x4) == 0)
			key = LEFT_KEY;
		if((GPIO_F_DATA & 0x8) == 0)
			key = LEFT_KEY;
		if((GPIO_G_DATA & 0x8) == 0)
			key = OK_KEY;
		if((GPIO_G_DATA & 0x800) == 0)
			key = CANCEL_KEY;

	}
	return key;

}

#define TSLESS 0x54
#define TSMAX 0x3AA
u32 transfer_to_xy_ord(u32 in, u32 max)
{
    u32 ret;
    ret = max * (in - TSLESS) /(u32) (TSMAX - TSLESS);
    return ret;
}

u32 get_touch_xy()
{
    u32 x, y, ret=0xffffffff;
    static int touch_adc_trigged = 0;

    BDDGX(rADCCON);
    //enable adc clk
    //rADCCON=(1<<14)+(ADCPRS<<6);
    if(touch_adc_trigged != 0){
        BDDGL;
        if(!(rADCCON & 0x1) && (rADCCON & 0x8000)){
            //get data
            x=(rADCDAT0&0x3ff);
            y=(rADCDAT1&0x3ff);
            //clear int flag
            rSUBSRCPND|=BIT_SUB_TC;
            rSRCPND = BIT_ADC;
            rINTPND = BIT_ADC;
            rADCTSC=0x1d3;//wait pen up

            BDDGL;
            x = transfer_to_xy_ord(x, 320);
            y = transfer_to_xy_ord(y, 240);
            BDDGL;
            y = 240 - y;
            printf("touch screen %d %d\n", x, y);
            ret = y<<16 | x;
            touch_adc_trigged = 0;
        }
    }
    else{
        BDDGL;
        if(!(rADCDAT0&0x8000)){
            rADCTSC=(1<<3)|(1<<2);
            rADCDLY=40000;

            //start ADC
            rADCCON|=0x1;

            touch_adc_trigged = 1;
        }
    }
    BDDGL;
    //rADCCON=(0<<14);
    //rINTSUBMSK=~(BIT_SUB_TC);
    //rINTMSK=~(BIT_ADC);
    return ret;
}
/*****************************************************************************/
#if 1
#define __REGb(x)       (*(volatile unsigned char *)(x))
#define __REGi(x)       (*(volatile unsigned int *)(x))
#define NF_BASE         0x4e000000
#define USCON0 *(volatile unsigned long *)0x50000010
#define UTXH0 *(volatile unsigned long *)0x50000020
#define URXH0 *(volatile unsigned long *)0x50000024

#define NFCONF          __REGi(NF_BASE + 0x0)
#define NFCONT          __REGi(NF_BASE + 0x4)
#define NFCMD           __REGb(NF_BASE + 0x8)
#define NFADDR          __REGb(NF_BASE + 0xC)
#define NFDATA          __REGb(NF_BASE + 0x10)
#define NFSTAT          __REGb(NF_BASE + 0x20)

#define NAND_CHIP_ENABLE  (NFCONT &= ~(1<<1))
#define NAND_CHIP_DISABLE (NFCONT |=  (1<<1))
#define NAND_CLEAR_RB     (NFSTAT |=  (1<<2))
#define NAND_DETECT_RB    { while(! (NFSTAT&(1<<2)) );}

#define NAND_SECTOR_SIZE        512
#define NAND_BLOCK_MASK         (NAND_SECTOR_SIZE - 1)

/*
void s3c2440_serial_send_byte(unsigned char c)
{
        while(!(USCON0 & 0x2));
        UTXH0 = c;
}

void ps(char*s)
{
	while(*s)
		s3c2440_serial_send_byte(*s++);
}
*/

void nand_reset()
{
	uint tmp = 10;
	NFCONF = (7<<12)|(7<<8)|(7<<4)|(0<<0);
	NFCONT = (1<<4)|(0<<1)|(1<<0);
	NFSTAT = 0x4;
	NFCMD = 0xff;
	while(tmp--);
        NAND_CHIP_DISABLE;
}

/* low level nand read function */
int ll_nand_read(unsigned char *buf, unsigned long start_addr, int size)
{
        int i, j;
	
	printf("Copy Command:membuf=%x, nandaddr=%x, size=%x\r\n", buf, start_addr, size);
        if ((start_addr & NAND_BLOCK_MASK) || (size & NAND_BLOCK_MASK)) {
                return -1;      /* invalid alignment */
        }
	if(!(NFSTAT&0x1)){
		printf("nand flash may have some problem, quit!\r\n");
		return -1;
	}

        NAND_CHIP_ENABLE;

        for(i=start_addr; i < (start_addr + size);) {
                /* READ0 */
                NAND_CLEAR_RB;
                NFCMD = 0;

                /* Write Address */
                NFADDR = i & 0xff;
                NFADDR = (i >> 9) & 0xff;
                NFADDR = (i >> 17) & 0xff;
                NFADDR = (i >> 25) & 0xff;

                NAND_DETECT_RB;

                for(j=0; j < NAND_SECTOR_SIZE; j++, i++) {
                        *buf = (NFDATA & 0xff);
                        buf++;
                }
		if(!((i>>9) & 0x3f))
			printf(">");
        }
        NAND_CHIP_DISABLE;
        return 0;
}

#define ERASE_BLOCK_ADDR_MASK (512 * 32 -1)
int ll_nand_erase(uint addr)
{
        printf("Erase Command:addr=%x\r\n", addr);
	if(addr & ERASE_BLOCK_ADDR_MASK){
		printf("erase addr not correct!\r\n");
		return -1;
	}	
        if(!(NFSTAT&0x1)){
                printf("nand flash may have some problem, quit!\r\n");
                return -1;
        }
/*
	if(is_marked_bad_block(addr)){
                printf("block %x is bad block, quit!\r\n",addr);
                return -1;
	}
*/
        NAND_CHIP_ENABLE;
        NAND_CLEAR_RB;
	NFCMD = 0x60;
        NFADDR = (addr >> 9) & 0xff;
        NFADDR = (addr >> 17) & 0xff;
        NFADDR = (addr >> 25) & 0xff;
	NFCMD = 0xd0;
	while(!(NFSTAT & 0x1)){
#ifdef NAND_DEBUG
		printf("%x\r\n", NFSTAT);
#endif	
	}
	NFCMD = 0x70;
	if(NFDATA & 0x1){
		printf("erase failed! may get bad.\r\n");
        	NAND_CHIP_DISABLE;
		return -1;
	}
	printf("erase successfully! \r\n");
        NAND_CHIP_DISABLE;
	return 0;
}

int ll_nand_write(unsigned char *buf, unsigned long start_addr, int size)
{
        uint i, j;

        printf("Write Command:membuf=%x, nandaddr=%x, size=%x\r\n", buf, start_addr, size);
        if ((start_addr & NAND_BLOCK_MASK) || (size & NAND_BLOCK_MASK)) {
                return -1;      /* invalid alignment */
        }
        if(!(NFSTAT&0x1)){
                printf("nand flash may have some problem, quit!\r\n");
                return -1;
        }

        NAND_CHIP_ENABLE;

        for(i=start_addr; i < (start_addr + size);) {
                /* READ0 */
                NAND_CLEAR_RB;
                NFCMD = 0x80;

                /* Write Address */
                NFADDR = i & 0xff;
                NFADDR = (i >> 9) & 0xff;
                NFADDR = (i >> 17) & 0xff;
                NFADDR = (i >> 25) & 0xff;


                for(j=0; j < NAND_SECTOR_SIZE; j++, i++) {
                        NFDATA = *buf++;
                }
		NFCMD = 0x10;
        	NAND_DETECT_RB;
		
		while(!NFSTAT&0x1);
		NFCMD = 0x70;
		if(NFDATA & 0x1){
			printf("current block(%x)program failed! may get bad.\r\n", (i-512)&~ERASE_BLOCK_ADDR_MASK);
        		NAND_CHIP_DISABLE;
			return -1;
		}	
		
                if(!((i>>9) & 0x3f))
                        printf("<");
        }
        NAND_CHIP_DISABLE;
        return 0;
}
#endif



