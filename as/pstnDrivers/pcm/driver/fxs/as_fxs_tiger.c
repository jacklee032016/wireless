/*
 * $Revision: 1.1.1.1 $
 * $Author: lizhijie $
 * $Id: as_fxs_tiger.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

/*
 * debug info for Tiger320 chip, added by lizhijie 2004.12.08
*/
#include "as_fxs_common.h"

#define WC_DMA_WRITE_CURRENT 	0x14
#define WC_DMA_READ_CURRENT 		0x24

char *as_fxs_tiger_info(struct wcfxs *wc)
{
	unsigned char x,y;
	unsigned int address;
	char *buf = NULL;
	int len = 0;
	
	buf = (char *)kmalloc(4096, GFP_KERNEL);
	if(!buf)
	{
		return NULL;
	}

	x=inb(wc->ioaddr + WC_CNTL);
	len += sprintf(buf + len, "\tTiger Control   Register  : %x \r\n", x );
	x=inb(wc->ioaddr + WC_OPER);
	len += sprintf(buf + len, "\tTiger Operation Register  : %x \r\n", x );

	address = inl( wc->ioaddr + WC_DMAWS);		/* Write start */
	len += sprintf(buf + len, "\tDMA WRITE Start     : %x \r\n", address );
	address = inl( wc->ioaddr + WC_DMAWI);		/* Write start */
	len += sprintf(buf + len, "\tDMA WRITE Interrupt : %x \r\n", address );
	address = inl( wc->ioaddr + WC_DMAWE);		/* Write start */
	len += sprintf(buf + len, "\tDMA WRITE End       : %x \r\n", address );
	address = inl( wc->ioaddr + WC_DMA_WRITE_CURRENT);		/* Write start */
	len += sprintf(buf + len, "\tDMA WRITE Current   : %x \r\n", address );
	
	address = inl( wc->ioaddr + WC_DMARS);		/* Write start */
	len += sprintf(buf + len, "\tDMA READ  Start     : %x \r\n", address );
	address = inl( wc->ioaddr + WC_DMARI);		/* Write start */
	len += sprintf(buf + len, "\tDMA READ  Interrupt : %x \r\n", address );
	address = inl( wc->ioaddr + WC_DMARE);		/* Write start */
	len += sprintf(buf + len, "\tDMA READ  End       : %x \r\n", address );
	address = inl( wc->ioaddr + WC_DMA_READ_CURRENT);		/* Write start */
	len += sprintf(buf + len, "\tDMA READ  Current   : %x \r\n\r\n", address );

	y = inb(wc->ioaddr + WC_INTSTAT);
	len += sprintf(buf + len, "\tInterrupt Status  : \r\n");
	len += sprintf(buf + len, "\t\tPCI Target Abort  : %x \r\n", y&0x20 );
	len += sprintf(buf + len, "\t\tPCI Master Abort  : %x \r\n", y&0x10 );
	len += sprintf(buf + len, "\t\tDMA READ End    : %x \r\n", y&0x08 );
	len += sprintf(buf + len, "\t\tDMA READ Int    : %x \r\n", y&0x04 );
	len += sprintf(buf + len, "\t\tDMA WRITE End : %x \r\n", y&0x02 );
	len += sprintf(buf + len, "\t\tDMA WRITE Int : %x \r\n", y&0x01 );
	
	len += sprintf(buf + len, "\tInterrupts statistics : \r\n");
	len += sprintf(buf + len, "\t\tREAD Interrupts  : %d \r\n", wc->read_interrupts);
	len += sprintf(buf + len, "\t\tREAD End  : %d \r\n", wc->read_ends);
	len += sprintf(buf + len, "\t\tWRITE Interrupts    : %d \r\n", wc->write_interrupts);
	len += sprintf(buf + len, "\t\tWRITE End    : %d \r\n", wc->write_ends);
	
	return buf;
}

