/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_fxs_isr.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.8  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.7  2004/12/15 07:33:05  lizhijie
 * recommit
 *
 * Revision 1.6  2004/12/14 08:45:21  lizhijie
 * add ARM channel exchange
 *
 * Revision 1.5  2004/11/29 01:53:38  lizhijie
 * update some redudent code and register setting
 *
 * Revision 1.4  2004/11/26 12:33:31  lizhijie
 * add multi-card support
 *
 * Revision 1.3  2004/11/25 07:59:24  lizhijie
 * recommit all
 *
*/

#include "as_fxs_common.h"
/* WCFXS Interrupt Service Routine */
extern int debug ;
extern int robust ;

static inline void __wcfxs_voicedaa_check_hook(struct wcfxs *wc, int card)
{
	unsigned char res;
	signed char b;
	int poopy = 0;
	
	/* Try to track issues that plague slot one FXO's */
	b = as_fxs_getreg(wc, card, 5);
	if ((b & 0x2) || !(b & 0x8)) 
	{
		/* Not good -- don't look at anything else */
		if (debug)
			printk("Poopy (%02x) on card %d!\n", b, card + 1); 
		poopy++;
	}
	b &= 0x9b;
	if (wc->mod.fxo.offhook[card]) 
	{
		if (b != 0x9)
			as_fxs_setreg(wc, card, 5, 0x9);
	} 
	else 
	{
		if (b != 0x8)
			as_fxs_setreg(wc, card, 5, 0x8);
	}
	if (poopy)
		return;

	if (!wc->mod.fxo.offhook[card]) 
	{
		res = as_fxs_getreg(wc, card, 5);
		if ((res & 0x60) && wc->mod.fxo.battery[card]) 
		{
			wc->mod.fxo.ringdebounce[card] += (AS_CHUNKSIZE * 4);
			if (wc->mod.fxo.ringdebounce[card] >= AS_CHUNKSIZE * 64) 
			{
				if (!wc->mod.fxo.wasringing[card]) 
				{
					wc->mod.fxo.wasringing[card] = 1;
					as_io_channel_hooksig( wc->chans[card], AS_RXSIG_RING);
					if (debug)
						printk("RING on %d!\n",  card + 1);
				}
				wc->mod.fxo.ringdebounce[card] = AS_CHUNKSIZE * 64;
			}
		} 
		else 
		{
			wc->mod.fxo.ringdebounce[card] -= AS_CHUNKSIZE;
			if (wc->mod.fxo.ringdebounce[card] <= 0) 
			{
				if (wc->mod.fxo.wasringing[card]) 
				{
					wc->mod.fxo.wasringing[card] =0;
					as_io_channel_hooksig( wc->chans[card], AS_RXSIG_OFFHOOK);
					if (debug)
						printk("NO RING on %d!\n",  card + 1);
				}
				wc->mod.fxo.ringdebounce[card] = 0;
			}
				
		}
	}

	b = as_fxs_getreg(wc, card, 29);
#if 0 
	{
		static int count = 0;
		if (!(count++ % 100)) {
			printk("Card %d: Voltage: %d\n", card + 1, b);
		}
	}
#endif	
	if (abs(b) < BATT_THRESH) 
	{
		wc->mod.fxo.nobatttimer[card]++;
#if 0
		if (wc->mod.fxo.battery[card])
			printk("Battery loss: %d (%d debounce)\n", b, wc->mod.fxo.battdebounce[card]);
#endif
		if (wc->mod.fxo.battery[card] && !wc->mod.fxo.battdebounce[card]) 
		{
			if (debug)
				printk("NO BATTERY on %d!\n",  card + 1);
			wc->mod.fxo.battery[card] =  0;
#ifdef	JAPAN
			if ((!wc->ohdebounce) && wc->offhook) 
			{
				as_io_channel_hooksig( wc->chans[card], AS_RXSIG_ONHOOK);
				if (debug)
					printk("Signalled On Hook\n");
#ifdef	ZERO_BATT_RING
				wc->onhook++;
#endif
			}
#else
			as_io_channel_hooksig( wc->chans[card], AS_RXSIG_ONHOOK);
#endif
			wc->mod.fxo.battdebounce[card] = BATT_DEBOUNCE;
		} else if (!wc->mod.fxo.battery[card])
			wc->mod.fxo.battdebounce[card] = BATT_DEBOUNCE;
	} else if (abs(b) > BATT_THRESH) {
		if (!wc->mod.fxo.battery[card] && !wc->mod.fxo.battdebounce[card]) {
			if (debug)
				printk("BATTERY on %d!\n",  card + 1);
#ifdef	ZERO_BATT_RING
			if (wc->onhook) 
			{
				wc->onhook = 0;
				as_io_channel_hooksig( wc->chans[card], AS_RXSIG_OFFHOOK);
				if (debug)
					printk("Signalled Off Hook\n");
			}
#else
			as_io_channel_hooksig( wc->chans[card], AS_RXSIG_OFFHOOK);
#endif
			wc->mod.fxo.battery[card] = 1;
			wc->mod.fxo.nobatttimer[card] = 0;
			wc->mod.fxo.battdebounce[card] = BATT_DEBOUNCE;
		} else if (wc->mod.fxo.battery[card])
			wc->mod.fxo.battdebounce[card] = BATT_DEBOUNCE;
	} else {
		/* It's something else... */
		wc->mod.fxo.battdebounce[card] = BATT_DEBOUNCE;
	}
	if (wc->mod.fxo.battdebounce[card])
		wc->mod.fxo.battdebounce[card]--;
	
}

static inline void __wcfxs_proslic_check_hook(struct wcfxs *wc, int card)
{
	char res;
	int hook;

	/* For some reason we have to debounce the hook detector.  */
//printk("ProSlic check hook\r\n");
	res = as_fxs_getreg(wc, card, 68);
	hook = (res & 1); /* Loop Closure Detect has occurred */
	if (hook != wc->mod.fxs.lastrxhook[card]) 
	{/* Reset the debounce (must be multiple of 4ms : 4 cards in device ,lzj ) */
		wc->mod.fxs.debounce[card] = 8 * (4 * 8);
#if 1
		printk("Resetting debounce card %d hook %d, %d\n", card, hook, wc->mod.fxs.debounce[card]);
#endif
	} 
	else 
	{
		if (wc->mod.fxs.debounce[card] > 0) 
		{
			wc->mod.fxs.debounce[card]-= 4 * AS_CHUNKSIZE;
#if 1
			printk("Sustaining hook %d, %d\n", hook, wc->mod.fxs.debounce[card]);
#endif
			if (!wc->mod.fxs.debounce[card]) 
			{
#if 1
				printk("Counted down debounce, newhook: %d...\n", hook);
#endif
				wc->mod.fxs.debouncehook[card] = hook;
			}
			
			if (!wc->mod.fxs.oldrxhook[card] && wc->mod.fxs.debouncehook[card]) 
			{/* Off hook */
#if 0
				if (debug)
#endif				
					printk("wcfxs: Card %d Going off hook\n", card);
				as_io_channel_hooksig( wc->chans[card], AS_RXSIG_OFFHOOK);
				if (robust)
					as_fxs_init_proslic(wc, card, 1, 0, 1);
				wc->mod.fxs.oldrxhook[card] = 1;
			} 
			else if (wc->mod.fxs.oldrxhook[card] && !wc->mod.fxs.debouncehook[card]) 
			{/* On hook */
#if 0
				if (debug)
#endif				
					printk("wcfxs: Card %d Going on hook\n", card);
				as_io_channel_hooksig( wc->chans[card],  AS_RXSIG_ONHOOK);
				wc->mod.fxs.oldrxhook[card] = 0;
			}
		}
	}
	wc->mod.fxs.lastrxhook[card] = hook;

}


static inline void __wcfxs_proslic_recheck_sanity(struct wcfxs *wc, int card)
{
	int res;
	/* Check loopback */
	res = as_fxs_getreg(wc, card, 8);
	if (res) 
	{
		printk("Ouch, part reset, quickly restoring reality (%d)\n", card);
		as_fxs_init_proslic(wc, card, 1, 0, 1);
	} 
	else 
	{
		res = as_fxs_getreg(wc, card, 64);
		if (!res && (res != wc->mod.fxs.lasttxhook[card])) 
		{
			if (wc->mod.fxs.palarms[card]++ < MAX_ALARMS) 
			{
				printk("Power alarm on module %d, resetting!\n", card + 1);
				if (wc->mod.fxs.lasttxhook[card] == 4)
					wc->mod.fxs.lasttxhook[card] = 1;
				as_fxs_setreg(wc, card, 64, wc->mod.fxs.lasttxhook[card]);
			} 
			else 
			{
				if (wc->mod.fxs.palarms[card] == MAX_ALARMS)
					printk("Too many power alarms on card %d, NOT resetting!\n", card + 1);
			}
		}
	}
}

static inline void __wcfxs_transmitprep(struct wcfxs *wc, unsigned char ints)
{
	volatile unsigned int *writechunk;
	int x;
	
	if (ints & 0x01) /* Write is at interrupt address.  Start writing from normal offset */
	{
		writechunk = wc->writechunk;
		wc->write_interrupts ++;
	}
	else if(ints & 0x02) /* Write reach at the end of buffer */
	{
		writechunk = wc->writechunk + AS_CHUNKSIZE;
		wc->write_ends++;
	}
	else
		return;

	/* Calculate Transmission */
#if 0	
	as_io_span_out_transmit( wc->span);
#endif
	as_io_span_out_transmit( wc );

	for (x=0;x<AS_CHUNKSIZE;x++) 
	{
		/* Send a sample, as a 32-bit word */
		writechunk[x] = 0;
#ifdef  __ARM__
		if (wc->cardflag & (1 << 3))
			writechunk[x] |= (wc->chans[3]->writechunk[x] );
		if (wc->cardflag & (1 << 2))
			writechunk[x] |= (wc->chans[2]->writechunk[x] << 8);
		if (wc->cardflag & (1 << 1))
			writechunk[x] |= (wc->chans[1]->writechunk[x] << 16);
		if (wc->cardflag & (1 << 0))
			writechunk[x] |= (wc->chans[0]->writechunk[x]<< 24);
#else
		if (wc->cardflag & (1 << 3))
			writechunk[x] |= (wc->chans[3]->writechunk[x] << 24);
		if (wc->cardflag & (1 << 2))
			writechunk[x] |= (wc->chans[2]->writechunk[x] << 16);
		if (wc->cardflag & (1 << 1))
			writechunk[x] |= (wc->chans[1]->writechunk[x] << 8);
		if (wc->cardflag & (1 << 0))
			writechunk[x] |= (wc->chans[0]->writechunk[x]);
#endif		
	}

}


static inline void __ring_check(struct wcfxs *wc, int card)
{
	int x;
	short sample;
	if (wc->modtype[card] != MOD_TYPE_FXO)
		return;
	
	wc->mod.fxo.pegtimer[card] += AS_CHUNKSIZE;
	for (x=0;x<AS_CHUNKSIZE;x++) 
	{
		/* Look for pegging to indicate ringing */
		sample = AS_XLAW(wc->chans[card]->readchunk[x], ( wc->chans[card] ) );
		if ((sample > 10000) && (wc->mod.fxo.peg[card] != 1)) 
		{
			if (debug > 1) 
				printk("High peg!\n");
			if ((wc->mod.fxo.pegtimer[card] < PEGTIME) && (wc->mod.fxo.pegtimer[card] > MINPEGTIME))
				wc->mod.fxo.pegcount[card]++;
			wc->mod.fxo.pegtimer[card] = 0;
			wc->mod.fxo.peg[card] = 1;
		}
		else if ((sample < -10000) && (wc->mod.fxo.peg[card] != -1)) 
		{
			if (debug > 1) 
				printk("Low peg!\n");
			if ((wc->mod.fxo.pegtimer[card] < (PEGTIME >> 2)) && (wc->mod.fxo.pegtimer[card] > (MINPEGTIME >> 2)))
				wc->mod.fxo.pegcount[card]++;
			wc->mod.fxo.pegtimer[card] = 0;
			wc->mod.fxo.peg[card] = -1;
		}
	}
	
	if (wc->mod.fxo.pegtimer[card] > PEGTIME) 
	{
		/* Reset pegcount if our timer expires */
		wc->mod.fxo.pegcount[card] = 0;
	}
	/* Decrement debouncer if appropriate */
	if (wc->mod.fxo.ringdebounce[card])
		wc->mod.fxo.ringdebounce[card]--;
	if (!wc->mod.fxo.offhook[card] && !wc->mod.fxo.ringdebounce[card]) 
	{
		if (!wc->mod.fxo.ring[card] && (wc->mod.fxo.pegcount[card] > PEGCOUNT)) 
		{
			/* It's ringing */
			if (debug)
				printk("RING on %d!\n",  card + 1);
			if (!wc->mod.fxo.offhook[card])
				as_io_channel_hooksig( wc->chans[card],  AS_RXSIG_RING);
			wc->mod.fxo.ring[card] = 1;
		}
		
		if (wc->mod.fxo.ring[card] && !wc->mod.fxo.pegcount[card]) 
		{
			/* No more ring */
			if (debug)
				printk("NO RING on %d!\n",  card + 1);
			as_io_channel_hooksig( wc->chans[card], AS_RXSIG_OFFHOOK);
			wc->mod.fxo.ring[card] = 0;
		}
	}
}

static inline void __wcfxs_receiveprep(struct wcfxs *wc, unsigned char ints)
{
	volatile unsigned int *readchunk;
	int x;
	int i;

	if (ints & 0x08) /* Interrupt DMA read reach to the end of buffer */
	{
		readchunk = wc->readchunk + AS_CHUNKSIZE;
		wc->read_ends ++;
	}
	else if(ints & 0x04) 	/* Read is at interrupt address.  Valid data is available at normal offset */
	{
		readchunk = wc->readchunk;
		wc->read_interrupts ++;
	}
	else
		return;

	for (x=0;x<AS_CHUNKSIZE;x++) 
	{
#ifdef  __ARM__
		if (wc->cardflag & (1 << 3))
			wc->chans[3]->readchunk[x] = (readchunk[x] ) & 0xff;
		if (wc->cardflag & (1 << 2))
			wc->chans[2]->readchunk[x] = (readchunk[x] >> 8) & 0xff;
		if (wc->cardflag & (1 << 1))
			wc->chans[1]->readchunk[x] = (readchunk[x] >> 16) & 0xff;
		if (wc->cardflag & (1 << 0))
			wc->chans[0]->readchunk[x] = (readchunk[x]>> 24) & 0xff;
#else	
		if (wc->cardflag & (1 << 3))
			wc->chans[3]->readchunk[x] = (readchunk[x] >> 24) & 0xff;
		if (wc->cardflag & (1 << 2))
			wc->chans[2]->readchunk[x] = (readchunk[x] >> 16) & 0xff;
		if (wc->cardflag & (1 << 1))
			wc->chans[1]->readchunk[x] = (readchunk[x] >> 8) & 0xff;
		if (wc->cardflag & (1 << 0))
			wc->chans[0]->readchunk[x] = (readchunk[x]) & 0xff;
#endif
	}

	for (i=0;i<wc->cards;i++)
	{
		x = wc->cardslot[i];
		__ring_check(wc, x);
	}	
	/* XXX We're wasting 8 taps.  We should get closer :( */
	for (x=0;x<wc->cards;x++) 
	{
		if (wc->cardflag & (1 << x))
			as_ec_chunk( wc->chans[x], wc->chans[x]->readchunk, wc->chans[x]->writechunk);
	}

#if 0
	as_io_span_in_receive( wc->span);
#endif
	as_io_span_in_receive( wc);
}

void as_fxs_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct wcfxs *wc = dev_id;
	unsigned char ints;
	int x;
	int i;
/* refer tiger page 23 */
	ints = inb(wc->ioaddr + WC_INTSTAT);
	outb(ints, wc->ioaddr + WC_INTSTAT);

	if (!ints)
		return;

/* PCI master abort */
	if (ints & 0x10) 
	{
		/* Stop DMA, wait for watchdog */
		printk("TDM PCI Master abort\n");
		as_fxs_stop_dma(wc);
		return;
	}
	if (ints & 0x20)
	{
		printk("PCI Target abort\n");
		return;
	}

	for (i=0; i<wc->cards; i++) 
	{
		x = wc->cardslot[i];
		as_fxs_get_dtmf(wc, x );

		if (/*(x < wc->cards) &&*/ (wc->cardflag & (1 << x)) &&
			(wc->modtype[x] == MOD_TYPE_FXS)) 
		{
			if (wc->mod.fxs.lasttxhook[x] == 0x4)  /*register 64 is 0x04: ringing */
			{
				/* RINGing, prepare for OHT */
				wc->mod.fxs.ohttimer[x] = OHT_TIMER << 3;
				wc->mod.fxs.idletxhookstate[x] = 0x2;	/* OHT mode when idle */
			} 
			else 
			{
				if (wc->mod.fxs.ohttimer[x]) 
				{
					wc->mod.fxs.ohttimer[x]-= AS_CHUNKSIZE;
					if (!wc->mod.fxs.ohttimer[x]) 
					{
						wc->mod.fxs.idletxhookstate[x] = 0x1;	/* Switch to active */
						if (wc->mod.fxs.lasttxhook[x] == 0x2) 
						{
							/* Apply the change if appropriate */
							wc->mod.fxs.lasttxhook[x] = 0x1;
							as_fxs_setreg(wc, x, 64, wc->mod.fxs.lasttxhook[x]);
						}
					}
				}
			}
		}
	}

	if (ints & 0x0f) 
	{
		wc->intcount++;
		i = wc->intcount % wc->cards;/* x is the card number ??? :lzj */
		x = wc->cardslot[i];
		if (/*(x < wc->cards) && */(wc->cardflag & (1 << x))) 
		{
			if (wc->modtype[x] == MOD_TYPE_FXS) 
			{
				__wcfxs_proslic_check_hook(wc, x);
				if (!(wc->intcount & 0xfc))
					__wcfxs_proslic_recheck_sanity(wc, x);
			} 
			else if (wc->modtype[x] == MOD_TYPE_FXO) 
			{
				__wcfxs_voicedaa_check_hook(wc, x);
			}
		}
		if (!(wc->intcount % 10000)) 
		{/* followinf code should be checked , lizhijie 2004.11.13 */
			/* Accept an alarm once per 10 seconds */
			for (i=0;i<wc->cards;i++) 
			{
				x = wc->cardslot[i];
				if (wc->modtype[x] == MOD_TYPE_FXS) 
				{
					if (wc->mod.fxs.palarms[x])
						wc->mod.fxs.palarms[x]--;
				}
			}	
		}
		__wcfxs_receiveprep(wc, ints);
		__wcfxs_transmitprep(wc, ints);
	}

}

