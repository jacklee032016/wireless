/*
 * $Revision: 1.1.1.1 $
 * $Author: lizhijie $
 * $Id: as_fxs_isr.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

#include "as_fxs_common.h"
/* WCFXS Interrupt Service Routine */

static inline void __wcfxs_transmitprep(struct wcfxs *wc, unsigned char ints)
{
	volatile unsigned int *writechunk;
	int x;
	
	if (ints & 0x01) /* Write is at interrupt address.  Start writing from normal offset */
	{
		writechunk = wc->writechunk;
#if AS_DEBUG_TIGER
		wc->write_interrupts ++;
#endif
	}
	else if(ints & 0x02) /* Write reach at the end of buffer */
	{
		writechunk = wc->writechunk + AS_CHUNKSIZE;
#if AS_DEBUG_TIGER
		wc->write_ends++;
#endif
	}
	else
	{
		return;
	}
	
	/* put data from channel's buffer into channel's chunk */
	as_io_span_out_transmit( wc );

	/* put data from channel's chunk into Tiger's DMA write chunk */
	for (x=0;x<AS_CHUNKSIZE;x++) 
	{
		/* Send a sample, as a 32-bit word */
		writechunk[x] = 0;
#if  __ARM__
	/* following is test code for determine the position of every pcm slot ,lizhijie,2005.03.04 */
#if 0			
		if (wc->cardflag & (1 << 0))
		{
			writechunk[x] |= (wc->chans[0]->writechunk[x] );
			writechunk[x] |= (wc->chans[0]->writechunk[x] << 8);
			writechunk[x] |= (wc->chans[0]->writechunk[x] << 16);
			writechunk[x] |= (wc->chans[0]->writechunk[x] << 24);
		}	
#endif
//		if (wc->cardflag & (1 << 3))
			writechunk[x] |= (wc->chans[3]->writechunk[x] );
//		if (wc->cardflag & (1 << 2))
			writechunk[x] |= (wc->chans[2]->writechunk[x] << 8);
//		if (wc->cardflag & (1 << 1))
			writechunk[x] |= (wc->chans[1]->writechunk[x] << 16);
//		if (wc->cardflag & (1 << 0))
			writechunk[x] |= (wc->chans[0]->writechunk[x]<< 24);
#else
//		if (wc->cardflag & (1 << 3))
			writechunk[x] |= (wc->chans[3]->writechunk[x] << 24);
//		if (wc->cardflag & (1 << 2))
			writechunk[x] |= (wc->chans[2]->writechunk[x] << 16);
//		if (wc->cardflag & (1 << 1))
			writechunk[x] |= (wc->chans[1]->writechunk[x] << 8);
//		if (wc->cardflag & (1 << 0))
			writechunk[x] |= (wc->chans[0]->writechunk[x]);
#endif
	}

}

static inline void __wcfxs_receiveprep(struct wcfxs *wc, unsigned char ints)
{
	volatile unsigned int *readchunk;
	int x;

	if (ints & 0x08) /* Interrupt DMA read reach to the end of buffer */
	{
		readchunk = wc->readchunk + AS_CHUNKSIZE;
#if AS_DEBUG_TIGER
		wc->read_ends ++;
#endif
	}
	else if(ints & 0x04) 	/* Read is at interrupt address.  Valid data is available at normal offset */
	{
		readchunk = wc->readchunk;
#if AS_DEBUG_TIGER
		wc->read_interrupts ++;
#endif
	}
	else
		return;

	/* put received data into every channel from Tiger DMA memory chunk */
	for (x=0;x<AS_CHUNKSIZE;x++) 
	{
#if  __ARM__
//		if (wc->cardflag & (1 << 3))
			wc->chans[3]->readchunk[x] = (readchunk[x] ) & 0xff;
//		if (wc->cardflag & (1 << 2))
			wc->chans[2]->readchunk[x] = (readchunk[x] >> 8) & 0xff;
//		if (wc->cardflag & (1 << 1))
			wc->chans[1]->readchunk[x] = (readchunk[x] >> 16) & 0xff;
//		if (wc->cardflag & (1 << 0))
			wc->chans[0]->readchunk[x] = (readchunk[x]>> 24) & 0xff;
#else	
//		if (wc->cardflag & (1 << 3))
			wc->chans[3]->readchunk[x] = (readchunk[x] >> 24) & 0xff;
//		if (wc->cardflag & (1 << 2))
			wc->chans[2]->readchunk[x] = (readchunk[x] >> 16) & 0xff;
//		if (wc->cardflag & (1 << 1))
			wc->chans[1]->readchunk[x] = (readchunk[x] >> 8) & 0xff;
//		if (wc->cardflag & (1 << 0))
			wc->chans[0]->readchunk[x] = (readchunk[x]) & 0xff;
#endif
	}

	/* put data from channel's chunk into channel's buffer in order to read by user process  */
	as_io_span_in_receive( wc);
}

void as_fxs_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct wcfxs *wc = dev_id;
	unsigned char ints;
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

	if (ints & 0x0f) 
	{
		wc->intcount++;
		__wcfxs_receiveprep(wc, ints);
		__wcfxs_transmitprep(wc, ints);
	}

}

