/*
 * $Id: ashfc_isr.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * Interrupt Service Routine for assist HFC-4S chip drivers
 * Li Zhijie, 2005.08.12
 */
#include <linux/delay.h>

#include "dchannel.h"
#include "bchannel.h"
#include "layer1.h"

#include "helper.h"

#if 0
#define SPIN_DEBUG
#define LOCK_STATISTIC
#endif
#include "hw_lock.h"
#include "ashfc.h"



/**************** output leds ***************/
void ashfc_leds(ashfc_t *hc)
{
	int i, state, active;
	dchannel_t *dch;
	int led[4];

	hc->ledcount += ashfc_poll;
	if (hc->ledcount > 4096)
		hc->ledcount -= 4096;

	if(hc->leds)
	{ /* HFC-4S OEM */
/* red blinking = PH_DEACTIVATE;  red steady = PH_ACTIVATE;  green flashing = fifo activity 
 * How to drive the LED blinking and in different color 
*/
		i = 0;
		while(i < 4)
		{
			state = 0;
			active = -1;
			if ((dch = hc->chan[(i<<2)|2].dch))
			{
				state = dch->ph_state;
				active = test_bit(HFC_CFG_NTMODE, &hc->chan[dch->channel].cfg)?NT_STATE_G3_ACTIVATED:TE_STATE_F7_ACTIVATED;
			}

			if (state)
			{
				if (state==active)
				{
					if (hc->activity[i])
					{
						led[i] = 1; 			/* led green */
						hc->activity[i] = 0;
					}
					else
						led[i] = 2; 			/* led red */
				}
				else if (hc->ledcount>>11) 	/* divided by 2048; deactivated, red blink */
					led[i] = 2; 				/* led red */
				else
					led[i] = 0; 				/* led off */
			}
			else
				led[i] = 0; 					/* led off */
			
			i++;
		}
		
/* use GPIO 8,9,10,11. when led[i]=0, output is not enabled, then this pin is in tristate:led off 
led[i]=1, GPIO pin output 1, led green; led[i]=2, GPIO pin output 0,led red */
		HFC_outb(hc, R_GPIO_EN1, ((led[0]>0)<<0) | ((led[1]>0)<<1) |((led[2]>0)<<2) | ((led[3]>0)<<3));
		HFC_outb(hc, R_GPIO_OUT1, ((led[0]&1)<<0) | ((led[1]&1)<<1) |((led[2]&1)<<2) | ((led[3]&1)<<3));
	}
}


static unsigned long _ashfc_read_w_coeff(ashfc_t *hc, int addr)
{
	unsigned char exponent;
	unsigned short w_float;
	unsigned long mantissa;

	HFC_outb_(hc, R_RAM_ADDR0, addr);
	HFC_outb_(hc, R_RAM_ADDR1, addr>>8);
#if 0
	HFC_outb_(hc, R_RAM_ADDR2, (addr>>16) | V_ADDR_INC);
#endif
	HFC_outb_(hc, R_RAM_ADDR2, (addr>>16) );

	w_float = HFC_inb_(hc, R_RAM_DATA);
#if CONFIG_HFCMULTI_PCIMEM
	w_float |= (HFC_inb_(hc, R_RAM_DATA) << 8);
#else
	w_float |= (HFC_getb(hc) << 8);
#endif

#if WITH_ASHFC_DEBUG_DATA
	if (ashfc_debug & ASHFC_DEBUG_DTMF)
		printk(" %04x", w_float);
#endif

	/* decode float (see chip doc) */
	mantissa = w_float & 0x0fff;
	if (w_float & 0x8000)
		mantissa |= 0xfffff000;

	exponent = (w_float>>12) & 0x7;
	if (exponent)
	{
		mantissa ^= 0x1000;
		mantissa <<= (exponent-1);
	}

	return mantissa;
}

/************************** read dtmf coefficients **************************/
static void ashfc_dtmf(ashfc_t *hc)
{
	signed long coeff[16];
	int co, ch;
	bchannel_t *bch = NULL;
	int dtmf = 0;
	int addr;
#if 0
	struct sk_buff *skb;
#endif

#if WITH_ASHFC_DEBUG_DATA
	if (ashfc_debug & ASHFC_DEBUG_DTMF)
		printk(KERN_ERR  "%s : ISR-DTMF detection irq\n", hc->name);
#endif
	ch = 0;

	while(ch < 32)
	{/* only process enabled B-channels */
		if (!(bch = hc->chan[ch].bch))
		{
			ch++;
			continue;
		}
		if (!hc->created[hc->chan[ch].port])
		{
			ch++;
			continue;
		}
		if (bch->protocol != ISDN_PID_L1_B_64TRANS)
		{
			ch++;
			continue;
		}

#if WITH_ASHFC_DEBUG_DATA
		if (ashfc_debug & ASHFC_DEBUG_DTMF)
			printk(KERN_ERR  "%s : ISR-DTMF channel %d:", hc->name, ch);
#endif
		dtmf = 1;
		co = 0;		/* fre offset */

		while(co < 8)
		{
			/* read W(n-1) coefficient */
			addr = hc->DTMFbase + ((co<<7) | (ch<<2));		/* P.265. Equ 9.3 */
			/* store coefficient */
			coeff[co<<1] = _ashfc_read_w_coeff( hc, addr);

			coeff[(co<<1)|1] = _ashfc_read_w_coeff( hc, addr+1 );;
			co++;
		}/* end of frequency loop*/
#if WITH_ASHFC_DEBUG_DATA
		if (ashfc_debug & ASHFC_DEBUG_DTMF)
			printk("\n");
#endif

#if WITH_DSP_MODULE
	/* when DSP ot DTMF module is added in kernel, then PH_CONTROL_IND with dinfo is HW_HFC_COEFF pass upper layer */
		skb = create_link_skb(PH_CONTROL | INDICATION, HW_HFC_COEFF, sizeof(coeff), coeff, 0);
		if (!skb)
		{
			printk(KERN_WARNING "%s : ISR-DTMF No memory for coefficients skb\n", hc->name);
			ch++;
			continue;
		}
		skb_queue_tail(&hc->chan[ch].dtmfque, skb);
		/* schedule B channel BH */
		bch_sched_event(bch, B_DTMFREADY);
#endif
		
		ch++;
	}/* end of channel loop */

	/* restart DTMF processing */
	hc->dtmf = dtmf;
	if (dtmf)/* restart DTMF detect */
		HFC_outb_(hc, R_DTMF, hc->hw.r_dtmf | V_RST_DTMF);
}


/******************** fill fifo as much as possible *********************************/
/* called by write thread and BH */
void ashfc_tx(ashfc_t *hc, int ch, dchannel_t *dch, bchannel_t *bch)
{
	int i, ii, temp;
	int Zspace, z1, z2;
	int Fspace, f1, f2;
	unsigned char *d, *dd, *buf = NULL;
	int *len = NULL, *idx = NULL; /* = NULL, to make GCC happy */
	int txpending, slot_tx;
	int hdlc = 0;
	int isBCh = 0;

//	printk(KERN_ERR  "%s: channel(%x)\n", __FUNCTION__, ch);
	/* get skb, fifo & mode */
	if (dch)
	{
		buf = dch->tx_buf;
		len = &dch->tx_len;
		idx = &dch->tx_idx;
		hdlc = 1;
	}
	if (bch)
	{
		buf = bch->tx_buf;
		len = &bch->tx_len;
		idx = &bch->tx_idx;
		if (bch->protocol == ISDN_PID_L1_B_64HDLC)
			hdlc = 1;
		else
			isBCh = 0x80;
	}
	txpending = hc->chan[ch].txpending;
	slot_tx = hc->chan[ch].slot_tx;
#if 0
	if ((!(*len)) && txpending!= ASHFC_TXPENDING_YES)
#endif
	if ((!(*len)) && txpending!= ASHFC_TXPENDING_SPLLOOP)
		return; /* no data */

//printk("ashfc_debug: data: len = %d, txpending = %d!!!!\n", *len, txpending);
	/* lets see how much data we will have left in buffer */
	HFC_outb_(hc, R_FIFO, ((ch<<1)|isBCh) );
	HFC_wait_(hc);
	if (txpending == ASHFC_TXPENDING_SPLLOOP )
	{
		/* reset fifo */
		HFC_outb_(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait_(hc);
		HFC_outb(hc, A_SUBCH_CFG, 0);
		txpending = ASHFC_TXPENDING_YES;
	}
next_frame:
	if (hdlc)
	{
		f1 = HFC_inb_(hc, A_F1);
		f2 = HFC_inb_(hc, A_F2);
		while (f2 != (temp=HFC_inb_(hc, A_F2)))
		{
#if WITH_ASHFC_DEBUG_DATA
			if (ashfc_debug & ASHFC_DEBUG_FIFO)
				printk(KERN_ERR  "%s : ISR-TX reread f2 because %d!=%d\n", hc->name, temp, f2);
#endif
			f2 = temp; /* repeat until F2 is equal */
		}
		Fspace = f2-f1-1;
		if (Fspace < 0)
			Fspace += hc->Flen;
		/* Old FIFO handling doesn't give us the current Z2 read
		 * pointer, so we cannot send the next frame before the fifo
		 * is empty. It makes no difference except for a slightly
		 * lower performance.
		 */
		if (test_bit(HFC_CHIP_REVISION0, &hc->chip))
		{
			if (f1 != f2)
				Fspace = 0;
			else
				Fspace = 1;
		}
		/* one frame only for ST D-channels, to allow resending */
		if (hc->type!=1 && dch)
		{
			if (f1 != f2)
				Fspace = 0;
		}
		/* F-counter full condition */
		if (Fspace == 0)
			return;
	}
	
	z1 = HFC_inw_(hc, A_Z1) - hc->Zmin;
	z2 = HFC_inw_(hc, A_Z2) - hc->Zmin;
	while(z2 != (temp=(HFC_inw_(hc, A_Z2) - hc->Zmin)))
	{
#if WITH_ASHFC_DEBUG_DATA
		if (ashfc_debug & ASHFC_DEBUG_FIFO)
			printk(KERN_ERR  "%s : ISR-TX reread z2 because %d!=%d\n", hc->name, temp, z2);
#endif
		z2 = temp; /* repeat unti Z2 is equal */
	}
	Zspace = z2-z1-1;
	if (Zspace < 0)
		Zspace += hc->Zlen;
	/* buffer too full, there must be at least one more byte for 0-volume */
	if (Zspace < 4) /* just to be safe */
		return;

	/* if no data */
	if (!(*len))
	{
		if (z1 == z2)
		{ /* empty */
			/* if done with FIFO audio data during PCM connection */
			if (!hdlc && txpending && slot_tx>=0)
			{
#if WITH_ASHFC_DEBUG_DATA
				if (ashfc_debug & ASHFC_DEBUG_MODE)
					printk(KERN_ERR  "%s : ISR-TX reconnecting PCM due to no more FIFO data: channel %d slot_tx %d\n", hc->name, ch, slot_tx);
#endif

				/* connect slot */
				/* Tx FIFO selected currently */
#if 0				
				HFC_outb(hc, A_CON_HDLC, 0xc0 | 0x00 | V_HDLC_TRP | V_IFF);
#endif
				HFC_outb(hc, A_CON_HDLC, TX_FIFO_CONN_ST2PCM | 0x00 | V_HDLC_TRP | V_IFF);
				/* select Rx FIFO for same D/B channel */
				HFC_outb_(hc, R_FIFO, ((ch<<1) | 1|isBCh) );
				HFC_wait_(hc);
#if 0				
				HFC_outb(hc, A_CON_HDLC, 0xc0 | 0x00 | V_HDLC_TRP | V_IFF);
#endif
				HFC_outb(hc, A_CON_HDLC, TX_FIFO_CONN_ST2PCM| 0x00 | V_HDLC_TRP | V_IFF);
				/* reselect the Tx FIFO */
				HFC_outb_(hc, R_FIFO, ((ch<<1)|isBCh) );
				HFC_wait_(hc);
			}
			txpending = hc->chan[ch].txpending = ASHFC_TXPENDING_NO;
		}
		return; /* no data */
	}

	/* if audio data */
	if (!hdlc && !txpending && slot_tx>=0)
	{
#if WITH_ASHFC_DEBUG_DATA
		if (ashfc_debug & ASHFC_DEBUG_MODE)
			printk(KERN_ERR  "%s : ISR-TX disconnecting PCM due to FIFO data: channel %d slot_tx %d\n", hc->name, ch, slot_tx);
#endif
		/* disconnect slot */
		/* use the Tx FIFO selected currently */
#if 0		
		HFC_outb(hc, A_CON_HDLC, 0x80 | 0x00 | V_HDLC_TRP | V_IFF);
#endif
		HFC_outb(hc, A_CON_HDLC, TX_FIFO_CONN_FIFO2ST| 0x00 | V_HDLC_TRP | V_IFF);
		/* select the Rx FIFO of same D/B channel */
		HFC_outb_(hc, R_FIFO, ((ch<<1) |1|isBCh) );
		HFC_wait_(hc);
#if 0		
		HFC_outb(hc, A_CON_HDLC, 0x80 | 0x00 | V_HDLC_TRP | V_IFF);
#endif
		HFC_outb(hc, A_CON_HDLC, TX_FIFO_CONN_FIFO2ST|0x00 | V_HDLC_TRP | V_IFF);
		/* reselect the Tx FIFO */
		HFC_outb_(hc, R_FIFO, ((ch<<1)|isBCh) );
		HFC_wait_(hc);
	}
	txpending = hc->chan[ch].txpending = ASHFC_TXPENDING_YES;

	/* show activity */
	hc->activity[hc->chan[ch].port] = 1;

	/* fill fifo to what we have left */
	i = *idx;
	ii = *len; 
	d = buf + i;
	if (ii-i > Zspace)
		ii = Zspace+i;

#if WITH_ASHFC_DEBUG_DATA
	if (ashfc_debug & ASHFC_DEBUG_FIFO)
	{
		printk(KERN_ERR  "%s : ISR-TX fifo(%d) has %d bytes space left (z1=%04x, z2=%04x) sending %d of %d bytes %s on port %d\n",
			hc->name, ch, Zspace, z1, z2, ii-i, (*len)-i, hdlc?"HDLC":"TRANS", hc->chan[ch].port);
	}
#endif

	#ifndef CONFIG_HFCMULTI_PCIMEM
		#if 0
		HFC_set(hc, A_FIFO_DATA0);
		#endif
	#endif


#if WITH_ASHFC_DEBUG_DCHANNEL
	if (ashfc_debug & ASHFC_DEBUG_FIFO && dch)
		printk("Tx L2 Frame \t\"" );
#endif

#if 0
	dd = d + ((ii-i)&0xfffc);
	i += (ii-i) & 0xfffc;

	while(d != dd)
	{
#if CONFIG_HFCMULTI_PCIMEM
		HFC_outl_(hc, A_FIFO_DATA0, *((unsigned long *)d));
#else
		HFC_putl(hc, *((unsigned long *)d));
#endif

#if WITH_ASHFC_DEBUG_DCHANNEL
		if (ashfc_debug & ASHFC_DEBUG_FIFO && dch)
			printk("%02x %02x %02x %02x ", d[0], d[1], d[2], d[3]);
#endif
		d+=4;
	}
#endif	
	dd = d + (ii-i);

	while(d != dd)
	{
#if CONFIG_HFCMULTI_PCIMEM
		HFC_outb_(hc, A_FIFO_DATA0, *d);
#else
	#if 0
		HFC_putb(hc, *d);
	#else
		HFC_outb(hc, A_FIFO_DATA0, *d);
	#endif
#endif

#if WITH_ASHFC_DEBUG_DCHANNEL
		if (ashfc_debug & ASHFC_DEBUG_FIFO && dch)
			printk("%02x ", *d );
#endif
		d++;
	}

#if WITH_ASHFC_DEBUG_DCHANNEL
	if (ashfc_debug & ASHFC_DEBUG_FIFO && dch)
		printk("\" on port %d\n", hc->chan[ch].port);
#endif

	*idx = ii;

	/* if some data has not been written to FIFO utile now, then return; and it is transfered by ISR */
	if (ii != *len)
	{
		/* NOTE: fifo is started by the calling function */
		return;
	}

	/* if all data has been written */
	if (hdlc)
	{
		/* increment f-counter */
		HFC_outb_(hc, R_INC_RES_FIFO, V_INC_F);
		HFC_wait_(hc);
	}

	if (dch)
	{
		/* check for next frame */
		if (test_and_clear_bit(FLG_TX_NEXT, &dch->DFlags))
		{
			if (dch->next_skb)
			{
				dch->tx_idx = 0;
				dch->tx_len = dch->next_skb->len;
				memcpy(dch->tx_buf, dch->next_skb->data, dch->tx_len);
				dchannel_sched_event(dch, D_XMTBUFREADY);
				goto next_frame;
			}
			else
				printk(KERN_WARNING "%s : ISR-TX irq TX_NEXT without skb (dch ch=%d) on port %d\n", hc->name, ch, hc->chan[ch].port);
		}
		test_and_clear_bit(FLG_TX_BUSY, &dch->DFlags);
		dch->tx_idx = dch->tx_len = 0;
	}
	
	if (bch)
	{
		bch_sched_event(bch, B_XMTBUFREADY);
#if 0
		printk(KERN_WARNING "%s : ISR-TX end, scheduler BH for next (bch ch=%d)\n", hc->name, ch);
		/* check for next frame */
		if (test_and_clear_bit(BC_FLG_TX_NEXT, &bch->Flag))
		{
			if (bch->next_skb)
			{
				bch->tx_idx = 0;
				bch->tx_len = bch->next_skb->len;
				memcpy(bch->tx_buf, bch->next_skb->data, bch->tx_len);
				
				/* Add following ,lizhijie,2005.12.21 */
				dev_kfree_skb( bch->next_skb);
				bch->next_skb = NULL;
				/* end of added */
				bch_sched_event(bch, B_XMTBUFREADY);
				goto next_frame;
			}
			else
				printk(KERN_WARNING "%s : ISR-TX irq TX_NEXT without skb (bch ch=%d)\n", hc->name, ch);
		}
#endif

		test_and_clear_bit(BC_FLG_TX_BUSY, &bch->Flag);
		bch->tx_idx = bch->tx_len = 0;
	}
	/* now we have no more data, so in case of transparent,
	 * we set the last byte in fifo to 'ashfc_silence' in case we will get
	 * no more data at all. this prevents sending an undefined value.
	 */
	if (!hdlc)
		HFC_outb_(hc, A_FIFO_DATA0_NOINC, ashfc_silence);
}



/*************** empty fifo ***************/
void ashfc_rx(ashfc_t *hc, int ch, dchannel_t *dch, bchannel_t *bch)
{
	int ii, temp;
	int Zsize, z1, z2 = 0; /* = 0, to make GCC happy */
	int f1 = 0, f2 = 0; /* = 0, to make GCC happy */
	unsigned char *d, *dd, *buf = NULL;
	int *idx = NULL, max = 0; /* = 0, to make GCC happy */
	int hdlc = 0;
	struct sk_buff *skb = NULL;
	int isBCh = 0;			/*added for B channel, 09.07 */

	static int bTotal = 0;

	/* get skb, fifo & mode */
	if (dch)
	{
		buf = hc->chan[ch].rx_buf;
		idx = &hc->chan[ch].rx_idx;
		max = MAX_DFRAME_LEN_L1;
		hdlc = 1;
	}
	if (bch)
	{
		buf = bch->rx_buf;
		idx = &bch->rx_idx;
		max = MAX_DATA_MEM;
		if (bch->protocol == ISDN_PID_L1_B_64HDLC)
			hdlc = 1;
		else
			isBCh = 0x80;
	}

	/* lets see how much data we received */
	HFC_outb_(hc, R_FIFO, ((ch<<1)|1|isBCh));
	HFC_wait_(hc);
next_frame:
#if 0
	/* set Z2(F1) */
	HFC_outb_(hc, R_RAM_SZ, hc->hw.r_ram_sz & ~V_FZ_MD);
#endif
	if (hdlc)
	{
		f1 = HFC_inb_(hc, A_F1);
		while (f1 != (temp=HFC_inb_(hc, A_F1)))
		{
#if WITH_ASHFC_DEBUG_DATA
			if (ashfc_debug & ASHFC_DEBUG_FIFO)
				printk(KERN_ERR  "%s : ISR-RX reread f1 because %d!=%d\n", hc->name, temp, f1);
#endif
			f1 = temp; /* repeat until F1 is equal */
		}
		f2 = HFC_inb_(hc, A_F2);
	}

	z1 = HFC_inw_(hc, A_Z1) - hc->Zmin;
	while(z1 != (temp=(HFC_inw_(hc, A_Z1) - hc->Zmin)))
	{
#if WITH_ASHFC_DEBUG_DATA
		if (ashfc_debug & ASHFC_DEBUG_FIFO)
			printk(KERN_ERR  "%s : ISR-RX reread z1 because %d!=%d\n", hc->name, temp, z2);
#endif
		z1 = temp; /* repeat unti Z1 is equal */
	}
	z2 = HFC_inw_(hc, A_Z2) - hc->Zmin;
	
	Zsize = z1-z2;
	if (hdlc && f1!=f2) /* complete hdlc frame */
		Zsize++;
	if (Zsize < 0)
		Zsize += hc->Zlen;
	/* if buffer is empty */
	if (Zsize <= 0)
		return;
	
	/* show activity */
	hc->activity[hc->chan[ch].port] = 1;

	/* empty fifo with what we have */
	if (hdlc)
	{/* hdlc mode */
#if WITH_ASHFC_DEBUG_DATA
		if (ashfc_debug & ASHFC_DEBUG_FIFO)
			printk(KERN_ERR  "%s : ISR-RX fifo(%d) reading %d bytes (z1=%04x, z2=%04x) \n\t\tHDLC %s (f1=%d, f2=%d) got=%d on port %d\n",
				hc->name, ch, Zsize, z1, z2, (f1==f2)?"fragment":"COMPLETE", f1, f2, Zsize+*idx, hc->chan[ch].port);
#endif
		/* HDLC */
		ii = Zsize;
		if ((ii + *idx) > max)
		{/* idx is start point. overflow then reset this FIFO */
#if WITH_ASHFC_DEBUG_DATA
			if (ashfc_debug & ASHFC_DEBUG_FIFO)
				printk(KERN_ERR  "%s : ISR-RX hdlc-frame too large on port %d.\n", hc->name, hc->chan[ch].port);
#endif
			*idx = 0;
			HFC_outb_(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait_(hc);
			return;
		}
		
		d = buf + *idx;

#if 0
	/* 32 bit only work in PC big-endian */
	#ifndef CONFIG_HFCMULTI_PCIMEM
		#if 0
		HFC_set(hc, A_FIFO_DATA0);
		#endif
	#endif
		dd = d + (ii&0xfffc);		/* 32 bit address, lowest 2 bits are not used */
		while(d != dd)
		{/* read 31 bits data as integer */
	#if CONFIG_HFCMULTI_PCIMEM
			*((unsigned long *)d) = HFC_inl_(hc, A_FIFO_DATA0);
	#else
		#if 0
			*((unsigned long *)d) = HFC_getl(hc);
		#else
			*((unsigned long *)d) = HFC_inl(hc, A_FIFO_DATA0);
		#endif
	#endif
			d+=4;
		}
		dd = d + (ii&0x0003);
#else
		dd = d + ii;/* for 8 bits access */
#endif
		while(d != dd)
		{/* read left bytes less than 4 */
#if CONFIG_HFCMULTI_PCIMEM
			*d++ = HFC_inb_(hc, A_FIFO_DATA0);
#else
	#if 0
			*d++ = HFC_getb(hc);
	#else
			*d++ = HFC_inb(hc, A_FIFO_DATA0);
	#endif
#endif
		}

		*idx += ii;
		if (f1 != f2)
		{
			/* increment Z2,F2-counter */
			HFC_outb_(hc, R_INC_RES_FIFO, V_INC_F);
			HFC_wait_(hc);
			/* check size */
			if (*idx < 4)
			{
#if WITH_ASHFC_DEBUG_DATA
				if (ashfc_debug & ASHFC_DEBUG_FIFO)
					printk(KERN_ERR  "%s : ISR-RX Frame below minimum size\n", hc->name);
#endif
				return;
			}
			/* there is at least one complete frame, check crc */
			if (buf[(*idx)-1])
			{
#if WITH_ASHFC_DEBUG_DATA
				if (ashfc_debug & ASHFC_DEBUG_CRC)
					printk(KERN_ERR  "%s : ISR-RX CRC-error\n", hc->name);
#endif
				return;
			} 

			if (!(skb = alloc_stack_skb((*idx)-3, (bch)?bch->up_headerlen:dch->up_headerlen)))
			{
				printk(KERN_ERR  "%s : ISR-RX No mem for skb\n", hc->name);
				return;
			}
			memcpy(skb_put(skb, (*idx)-3), buf, (*idx)-3);

#if WITH_ASHFC_DEBUG_DCHANNEL
			if (ashfc_debug & ASHFC_DEBUG_FIFO && dch)
			{
				temp = 0;
				printk("Rx L2 Frame is \"");
				while(temp < (*idx)-3)
					printk("%02x ", skb->data[temp++]);
				printk("\" on port %d\n", hc->chan[ch].port);
			}
#endif

			if (dch)
			{
#if WITH_ASHFC_DEBUG_DATA
				if (ashfc_debug & ASHFC_DEBUG_FIFO)
					printk(KERN_ERR  "%s : ISR-RX sending D-channel frame to user space.\n", hc->name); 
#endif
				/* schedule D-channel event */
				skb_queue_tail(&dch->rqueue, skb);
				dchannel_sched_event(dch, D_RCVBUFREADY);
			}
			if (bch)
			{
				/* schedule B-channel event */
				skb_queue_tail(&bch->rqueue, skb);
				bch_sched_event(bch, B_RCVBUFREADY);
			}
			*idx = 0;
			goto next_frame;
		}
		/* there is an incomplete frame */
	} 
	else
	{/* transparent */
		ii = Zsize;
		if (ii > MAX_DATA_MEM)
			ii = MAX_DATA_MEM;
		if (!(skb = alloc_stack_skb(ii, bch->up_headerlen)))
		{
			printk(KERN_ERR  "%s : ISR-RX No mem for skb\n", hc->name);
			HFC_outb_(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait_(hc);
			return;
		}
		bTotal += ii;
		d = skb_put(skb, ii);
		
#if  WITH_ASHFC_DEBUG_DATA
		printk(KERN_ERR  "%s : BChannel %d RX %d bytes, total %d bytes\n",hc->name,bch->channel, ii, bTotal);
		if (ashfc_debug & ASHFC_DEBUG_FIFO)
			printk(KERN_ERR  "%s : ISR-RX fifo(%d) reading %d bytes (z1=%04x, z2=%04x) TRANS\n",	hc->name, ch, ii, z1, z2);

		printk(KERN_ERR  "%s : ISR-RX fifo(%d) reading %d bytes (z1=%04x, z2=%04x) TRANS\n",	hc->name, ch, ii, z1, z2);
		printk(KERN_ERR  "\n" );
#endif

#if 0
	#ifndef CONFIG_HFCMULTI_PCIMEM
		#if 0
		HFC_set(hc, A_FIFO_DATA0);
		#endif
	#endif
		dd = d + (ii&0xfffc);
		while(d != dd)
		{
	#if CONFIG_HFCMULTI_PCIMEM
			*((unsigned long *)d) = HFC_inl_(hc, A_FIFO_DATA0);
	#else
		#if 0
			*((unsigned long *)d) = HFC_getl(hc);
		#else
			*((unsigned long *)d) = HFC_inl(hc, A_FIFO_DATA0);
#if 0//WITH_ASHFC_DEBUG_DATA
			printk(  "0x%2x 0x%2x 0x%2x 0x%2x ", *(d), *(d+1) , *(d+2), *(d+3)   );
#endif
		#endif
	#endif
			d+=4;
		}

		dd = d + (ii&0x0003);
#endif

		dd = d + ii;
		while(d != dd)
		{
	#if CONFIG_HFCMULTI_PCIMEM
			*d++ = HFC_inb_(hc, A_FIFO_DATA0);
	#else
		#if 0
			*d++ = HFC_getb(hc);
		#else
			*d++ = HFC_inb(hc, A_FIFO_DATA0);
#if WITH_ASHFC_DEBUG_DATA
			printk(  "0x%2x ", *(d-1) );
#endif
		#endif
	#endif
		}
#if WITH_ASHFC_DEBUG_DATA
		printk( "\n" );
#endif
		/* 12.20 */
		if (f1 != f2)
		{
			/* increment Z2,F2-counter */
			HFC_outb_(hc, R_INC_RES_FIFO, V_INC_F);
			HFC_wait_(hc);
		}	
		
		if (dch)
		{/* schedule D-channel event : D channel maybe not used in this mode */
			skb_queue_tail(&dch->rqueue, skb);
			dchannel_sched_event(dch, D_RCVBUFREADY);
		}
		
		if (bch)
		{/* schedule B-channel event */
//			printk(KERN_ERR " skb-length =%d for this Bchanne\n" , skb->len);
			skb_queue_tail(&bch->rqueue, skb);
			bch_sched_event(bch, B_RCVBUFREADY);
		}
	}
}

/********************** Interrupt handler **********************/
irqreturn_t ashfc_interrupt(int intno, void *dev_id, struct pt_regs *regs)
{
	ashfc_t *hc = dev_id;
	bchannel_t *bch;
	dchannel_t *dch;
	u_long flags;
	u_char r_irq_statech, status, r_irq_misc, r_irq_oview, r_irq_fifo_bl;
	int ch;
	int i, j;

	spin_lock_irqsave(&hc->lock.lock, flags);
#ifdef SPIN_DEBUG
	hc->lock.spin_adr = (void *)0x3001;
#endif

	if (!hc)
	{
		printk(KERN_WARNING "HFC-4S : ISR Spurious interrupt!\n");
		irq_notforus:
#ifdef SPIN_DEBUG
		hc->lock.spin_adr = NULL;
#endif
		spin_unlock_irqrestore(&hc->lock.lock, flags);
		//return  (IRQ_NONE);
		return ;
	}

	status = HFC_inb_(hc, R_STATUS);
	r_irq_statech = HFC_inb_(hc, R_IRQ_STATECH);
	if (!r_irq_statech && !(status & (V_DTMF_STA | V_LOST_STA | V_EXT_IRQSTA | V_MISC_IRQSTA | V_FR_IRQSTA)))
	{/* irq is not for us */
		goto irq_notforus;
	}
	
	hc->irqcnt++;
	if (test_and_set_bit(STATE_FLAG_BUSY, &hc->lock.state))
	{
		printk(KERN_ERR "%s : ISR STATE_FLAG_BUSY allready active, should never happen state:%lx\n", hc->name,  hc->lock.state);
#ifdef SPIN_DEBUG
		printk(KERN_ERR "%s : ISR previous lock:%p\n", hc->name, hc->lock.busy_adr);
#endif
#ifdef LOCK_STATISTIC
		hc->lock.irq_fail++;
#endif
	}
	else
	{
#ifdef LOCK_STATISTIC
		hc->lock.irq_ok++;
#endif
#ifdef SPIN_DEBUG
		hc->lock.busy_adr = ashfc_interrupt;
#endif
	}

	test_and_set_bit(STATE_FLAG_INIRQ, &hc->lock.state);
#ifdef SPIN_DEBUG
	hc->lock.spin_adr= NULL;
#endif
	spin_unlock_irqrestore(&hc->lock.lock, flags);

	/* S/T interrupt */
	if (r_irq_statech)
	{/* state machine */
#if WITH_ASHFC_DEBUG_STATE
		printk(KERN_ERR  "%s : ISR-S/T irq\n", hc->name);
#endif
		ch = 0;
		while(ch < 32)
		{
			if ((dch = hc->chan[ch].dch))
			{
				if (r_irq_statech & 1)
				{/* IRQ in this ST interface  */
					HFC_outb_(hc, R_ST_SEL, hc->chan[ch].port);
					dch->ph_state = HFC_inb(hc, A_ST_RD_STATE) & 0x0f;
					if (dch->ph_state == (test_bit(HFC_CFG_NTMODE, &hc->chan[dch->channel].cfg)?NT_STATE_G3_ACTIVATED:TE_STATE_F7_ACTIVATED))
					{/* when physical state of dch is active (NT is in G3 or TE is in F7) */
						HFC_outb_(hc, R_FIFO, (ch<<1)|1);
						HFC_wait_(hc);
						HFC_outb_(hc, R_INC_RES_FIFO, V_RES_F);	/*reset selected FIFO(F/Z/A_CH_MSK)*/
						HFC_wait_(hc);
					}

					dchannel_sched_event(dch, D_L1STATECHANGE);

#if WITH_ASHFC_DEBUG_STATE
					if (ashfc_debug & ASHFC_DEBUG_STATE)
						printk(KERN_ERR  "%s : ISR-S/T newstate %x port %d\n", hc->name, dch->ph_state, hc->chan[ch].port);
#endif

				}
				r_irq_statech >>= 1;
			}
			ch++;
		}
	}
	
	if (status & V_EXT_IRQSTA)
	{/* external IRQ */
	}
	if (status & V_LOST_STA)
	{/* LOST IRQ */
		printk(KERN_ERR  "%s : ISR-LOST irq\n", hc->name);
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_LOST); /* clear irq! */
	}
	
	if (status & V_MISC_IRQSTA)
	{/* misc IRQ */
//		printk(KERN_ERR  "%s : ISR-MISC irq\n", hc->name);
		r_irq_misc = HFC_inb_(hc, R_IRQ_MISC);
#if 0		
		if (r_irq_misc & V_STA_IRQ)
		{
			if (hc->type == 1)
			{/* state machine */
				dch = hc->chan[16].dch;
				dch->ph_state = HFC_inb_(hc, R_E1_RD_STA) & 0x7;
				dchannel_sched_event(dch, D_L1STATECHANGE);
				if (ashfc_debug & ASHFC_DEBUG_STATE)
					printk(KERN_ERR  "%s: E1 newstate %x\n", __FUNCTION__, dch->ph_state);
			}
		}
#endif

		if (r_irq_misc & V_TI_IRQ)
		{/* -> timer IRQ */
//			printk(KERN_ERR  "%s : ISR-MISC-TIMER irq\n", hc->name);
			ch = 0;
//			while(ch < 32)	/* lizhijie, 2005.12.21 */
			while(ch < 16)
			{
				if ((dch = hc->chan[ch].dch))
				{
					if (hc->created[hc->chan[ch].port])
					{
//						printk(KERN_ERR  "%s : ISR-MISC-TIMER irq for TX/RX\n", hc->name);
						ashfc_tx(hc, ch, dch, NULL);
					/* fifo is started when switching to rx-fifo */
						ashfc_rx(hc, ch, dch, NULL);
						if (hc->chan[ch].nt_timer > -1)
						{
							if (!(--hc->chan[ch].nt_timer))
							{/* shedule D channel BH */
								dchannel_sched_event(dch, D_L1STATECHANGE);

#if WITH_ASHFC_DEBUG_STATE
								if (ashfc_debug & ASHFC_DEBUG_STATE)
									printk(KERN_ERR  "%s : ISR-TIMER nt_timer at state %x\n", hc->name, dch->ph_state);
#endif
							}
						}
					}
				}
	
				if ((bch = hc->chan[ch].bch))
				if (hc->created[hc->chan[ch].port])
				{
					if (bch->protocol != ISDN_PID_NONE)
					{
						ashfc_tx(hc, ch, NULL, bch);
#if 1
						/* fifo is started when switching to rx-fifo */
						ashfc_rx(hc, ch, NULL, bch);
#endif
					}
				}

				ch++;
			}
			
			if (hc->leds)
				ashfc_leds(hc);
		}

		if (r_irq_misc & V_DTMF_IRQ)
		{/* -> DTMF IRQ */
//			ashfc_dtmf(hc);
		}
	}
	
	if (status & V_FR_IRQSTA)
	{/* FIFO IRQ */
#if WITH_ASHFC_DEBUG_DATA
		printk(KERN_ERR  "%s : ISR-FIFO irq\n", hc->name);
#endif
		r_irq_oview = HFC_inb_(hc, R_IRQ_OVIEW);
		i = 0;

//		while(i < 8)
		/* HFC-4S, only 4 port, so only use 4 block of overview register, lizhijie,2005.12.21 */
		while(i < 4)
		{/* IRQ on FIFO block 0--7 */
			if (r_irq_oview & (1 << i))
			{
				r_irq_fifo_bl = HFC_inb_(hc, R_IRQ_FIFO_BL0 + i);
				j = 0;
				while(j < 8)
				{
					ch = (i<<2) + (j>>1);		/* eg. i*4+j/2 */
					if (r_irq_fifo_bl & (1 << j))
					{/* bit 0,2,4,6 for Tx */
						if ((dch = hc->chan[ch].dch))
						if (hc->created[hc->chan[ch].port])
						{/* when D channel */
#if WITH_ASHFC_DEBUG_DATA
							printk(KERN_ERR  "%s : ISR-FIFO irq on port %d\n", hc->name, hc->chan[ch].port);
#endif

							ashfc_tx(hc, ch, dch, NULL);
							/* start fifo */
//							HFC_outb_(hc, R_FIFO, 0);	/* ?? */
							HFC_outb_(hc, R_FIFO, ch);	/* ?? */
							HFC_wait_(hc);
						}
#if 0						
						/* when B channel */
						if ((bch = hc->chan[ch].bch))
						if (hc->created[hc->chan[ch].port])
						{
//						printk(KERN_ERR"B CHAN TX in FIFO IRQ\n\n ");
							if (bch->protocol != ISDN_PID_NONE)
							{
								ashfc_tx(hc, ch, NULL, bch);
								/* start fifo */
								HFC_outb_(hc, R_FIFO, 0|0x80);
//								HFC_outb_(hc, R_FIFO, 0 );
								HFC_wait_(hc);
							}
						}
#endif						
					}

					/* bit 1,3,5,7 for Rx */
					j++;
					if (r_irq_fifo_bl & (1 << j))
					{
						if ((dch = hc->chan[ch].dch))
						if (hc->created[hc->chan[ch].port])
						{/* D channel */
							ashfc_rx(hc, ch, dch, NULL);
						}

#if 0						
						if ((bch = hc->chan[ch].bch))
						if (hc->created[hc->chan[ch].port])
						{/* B channel */
							if (bch->protocol != ISDN_PID_NONE)
							{
//						printk(KERN_ERR"B CHAN RX in FIFO IRQ\n\n ");
								ashfc_rx(hc, ch, NULL, bch);
								HFC_outb_(hc, R_FIFO, ch |1|0x80);
//								HFC_outb_(hc, R_FIFO, 0|1 );
								HFC_wait_(hc);
							}
						}
#endif						
					}
					j++;
				}
			}
			i++;
		}
	}

	spin_lock_irqsave(&hc->lock.lock, flags);
#ifdef SPIN_DEBUG
	hc->lock.spin_adr = (void *)0x3002;
#endif
	if (!test_and_clear_bit(STATE_FLAG_INIRQ, &hc->lock.state))
	{
	}
	if (!test_and_clear_bit(STATE_FLAG_BUSY, &hc->lock.state))
	{
		printk(KERN_ERR "%s : ISR STATE_FLAG_BUSY not locked state(%lx)\n", hc->name, hc->lock.state);
	}
#ifdef SPIN_DEBUG
	hc->lock.busy_adr = NULL;
	hc->lock.spin_adr = NULL;
#endif
	spin_unlock_irqrestore(&hc->lock.lock, flags);
	return IRQ_HANDLED;
}


