/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_fxs.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/11/25 07:59:24  lizhijie
 * recommit all
 *
*/
#include "as_fxs_common.h"

extern int debug ;

static inline void __write_8bits(struct wcfxs *wc, unsigned char bits)
{
	/* Drop chip select */
	int x;
#if 0
	unsigned long delay;
	int static pulse_sum = 0;
#endif	
	wc->ios |= BIT_SCLK;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	wc->ios &= ~BIT_CS;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	for (x=0;x<8;x++) 
	{
#if 0	
			delay = 0;
			while (  delay < 800000 )
			      delay++;
#endif

		/* Send out each bit, MSB first, drop SCLK as we do so */
		if (bits & 0x80)/* read operation */
			wc->ios |= BIT_SDI;
		else
			wc->ios &= ~BIT_SDI;
		wc->ios &= ~BIT_SCLK;
		outb(wc->ios, wc->ioaddr + WC_AUXD);
		/* Now raise SCLK high again and repeat */
		wc->ios |= BIT_SCLK;
		outb(wc->ios, wc->ioaddr + WC_AUXD);
#if 0		
			delay = 0;
			while (  delay < 800000 )
			      delay++;
			pulse_sum++;
		printk("Pulse No. %d, total %d\r\n", x, pulse_sum);
#endif			

		bits <<= 1;
	}
	/* Finally raise CS back high again */
	wc->ios |= BIT_CS;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	
}

static inline void __reset_spi(struct wcfxs *wc)
{
	/* Drop chip select and clock once and raise and clock once */
	wc->ios |= BIT_SCLK;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	wc->ios &= ~BIT_CS;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	wc->ios |= BIT_SDI;
	wc->ios &= ~BIT_SCLK;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	/* Now raise SCLK high again and repeat */
	wc->ios |= BIT_SCLK;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	/* Finally raise CS back high again */
	wc->ios |= BIT_CS;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	/* Clock again */
	wc->ios &= ~BIT_SCLK;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	/* Now raise SCLK high again and repeat */
	wc->ios |= BIT_SCLK;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	
}

static inline unsigned char __read_8bits(struct wcfxs *wc)
{
	unsigned char res=0, c;
	int x;
	wc->ios |= BIT_SCLK;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	/* Drop chip select */
	wc->ios &= ~BIT_CS;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	for (x=0;x<8;x++) 
	{
		res <<= 1;
		/* Get SCLK */
		wc->ios &= ~BIT_SCLK;
		outb(wc->ios, wc->ioaddr + WC_AUXD);
		/* Read back the value */
		c = inb(wc->ioaddr + WC_AUXR);
		if (c & BIT_SDO)
			res |= 1;
		/* Now raise SCLK high again */
		wc->ios |= BIT_SCLK;
		outb(wc->ios, wc->ioaddr + WC_AUXD);
	}
	/* Finally raise CS back high again */
	wc->ios |= BIT_CS;
	outb(wc->ios, wc->ioaddr + WC_AUXD);
	wc->ios &= ~BIT_SCLK;
	outb(wc->ios, wc->ioaddr + WC_AUXD);

	/* And return our result */
	return res;
}

/* set/read the register in PIB address space, which is byte addressed,
 * so, every register address must be aligned with word border
 * refer  <tiger> page 10 and page 13 
*/
void as_fxs_setcreg(struct wcfxs *wc, unsigned char reg, unsigned char val)
{
	outb(val, wc->ioaddr + WC_REGBASE + ((reg & 0xf) << 2));
}

/* get control registers of preperial device connected to the PIB bus of Tiger 320  */
unsigned char as_fxs_getcreg(struct wcfxs *wc, unsigned char reg)
{
	return inb(wc->ioaddr + WC_REGBASE + ((reg & 0xf) << 2));
}

inline void __fxs_setcard(struct wcfxs *wc, int card)
{
	if (wc->curcard != card) 
	{
		as_fxs_setcreg(wc, WC_CS, (1 << card));
		wc->curcard = card;
	}
}

void as_fxs_setreg_nolock(struct wcfxs *wc, int card, unsigned char reg, unsigned char value)
{
	__fxs_setcard(wc, card);
#if AS_WITH_FXO_MODULE
	if (wc->modtype[card] == MOD_TYPE_FXO) 
	{
		__write_8bits(wc, 0x20);
		__write_8bits(wc, reg & 0x7f);
	} 
	else 
	{
#endif	
		__write_8bits(wc, reg & 0x7f);
#if AS_WITH_FXO_MODULE
	}
#endif	
	__write_8bits(wc, value);
}

unsigned char as_fxs_getreg_nolock(struct wcfxs *wc, int card, unsigned char reg)
{
	__fxs_setcard(wc, card);
	if (wc->modtype[card] == MOD_TYPE_FXO) 
	{
		__write_8bits(wc, 0x60);
		__write_8bits(wc, reg & 0x7f);
	} 
	else 
	{
		__write_8bits(wc, reg | 0x80);
	}
	return __read_8bits(wc);
}

void as_reset_spi(struct wcfxs *wc, int card)
{
	unsigned long flags;
	spin_lock_irqsave(&wc->lock, flags);
	__fxs_setcard(wc, card);
	__reset_spi(wc);
	__reset_spi(wc);
	spin_unlock_irqrestore(&wc->lock, flags);
}

void as_fxs_setreg(struct wcfxs *wc, int card, unsigned char reg, unsigned char value)
{
	unsigned long flags;
	
	spin_lock_irqsave(&wc->lock, flags);
	as_fxs_setreg_nolock(wc, card, reg, value);
	spin_unlock_irqrestore(&wc->lock, flags);
}

unsigned char as_fxs_getreg(struct wcfxs *wc, int card, unsigned char reg)
{
	unsigned long flags;
	unsigned char res;
	
	spin_lock_irqsave(&wc->lock, flags);
	res = as_fxs_getreg_nolock(wc, card, reg);
	spin_unlock_irqrestore(&wc->lock, flags);
	
	return res;
}

void as_fxs_restart_dma(struct wcfxs *wc)
{
/* Reset Master and TDM */
	outb(0x01, wc->ioaddr + WC_CNTL);
	outb(0x01, wc->ioaddr + WC_OPER);
}

void as_fxs_stop_dma(struct wcfxs *wc)
{
	outb(0x00, wc->ioaddr + WC_OPER);
}

void as_fxs_reset_tdm(struct wcfxs *wc)
{
	/* Reset TDM */
	outb(0x0f, wc->ioaddr + WC_CNTL);
}

void as_fxs_disable_interrupts(struct wcfxs *wc)	
{
	outb(0x00, wc->ioaddr + WC_MASK0);
	outb(0x00, wc->ioaddr + WC_MASK1);
}

