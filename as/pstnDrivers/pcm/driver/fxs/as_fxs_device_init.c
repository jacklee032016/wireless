/*
 * $Revision: 1.1.1.1 $
 * $Author: lizhijie $
 * $Id: as_fxs_device_init.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

#include "as_fxs_common.h"

#define AS_DMA_BUFFER_SIZE	(AS_MAX_CHUNKSIZE * 2 * 2 * 2 * 4)

/* for multi-card support , lzj */
struct wcfxs *wc_header = NULL;
static int modules = 0;

static int as_fxs_watchdog(struct as_dev_chan *chan, int event)
{
	printk("TDM: Restarting DMA\n");
	as_fxs_restart_dma(chan->private);
	return 0;
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

static void as_fxs_reset_tdm(struct wcfxs *wc)
{
	/* Reset TDM */
	outb(0x0F,  wc->ioaddr + WC_CNTL);
}

void as_fxs_disable_interrupts(struct wcfxs *wc)	
{
	outb(0x00, wc->ioaddr + WC_MASK0);
	outb(0x00, wc->ioaddr + WC_MASK1);
}

static void __wcfxs_start_dma(struct wcfxs *wc)
{
/* Reset Master and TDM */
	outb(0x0f, wc->ioaddr + WC_CNTL);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);
	outb(0x01, wc->ioaddr + WC_CNTL);
	outb(0x01, wc->ioaddr + WC_OPER);
}

static void __wcfxs_enable_interrupts(struct wcfxs *wc)
{
	/* Enable interrupts (we care about all of them) */
	outb(0x3f, wc->ioaddr + WC_MASK0);

	/* No external interrupts */
	outb(0x00, wc->ioaddr + WC_MASK1);

	schedule_timeout(1);

	/* turn AUX2 into 0, eg. connect the line from PBX  */
	outb(0x00, wc->ioaddr + WC_AUXD); 

}

static struct as_dev_chan *__as_create_channel( int card_position, struct wcfxs *wc)
{
	struct as_dev_chan *chan;

	chan = kmalloc( sizeof(struct as_dev_chan), GFP_KERNEL);
	if(!chan)
	{
		printk("No memory available when create a channel\r\n");
		return NULL;
	}
	memset(chan, 0, sizeof(struct as_dev_chan) );

	sprintf(chan->name, "TDM-%d-%d", wc->module, card_position );

	chan->chanpos = card_position ;
	chan->watchdog = as_fxs_watchdog;

#if AS_POLICY_ENBALE	
	chan->txdisable = 1;
	chan->rxdisable = 1;
#endif

	chan->private = wc;
	
	return chan;
}

static int __as_fxs_channel_register(struct wcfxs *wc )
{
	int res = 0;
	int x;
	struct as_dev_chan *chan;

	for (x=0; x<NUM_CARDS ; x++) 
	{
		chan = __as_create_channel( x, wc);
		if( !chan)
			return -ENOMEM;

		if( as_channel_register( chan) )
		{
			printk("Unable to register FXS channel %s with SPAN\r\n", chan->name );
			return -1;
		}

		printk("\tRegistered FXS channel %s with SPAN\r\n", chan->name );

		wc->span = chan->span;
		wc->chans[wc->cards] = chan ;
		wc->cards ++;
	}
	return res;
}

static void __as_fxs_channel_unregister(struct wcfxs *wc)
{
	int i;
	struct as_dev_chan *chan;

	for(i=0; i< wc->cards; i++)
	{
		chan = wc->chans[i];
		as_channel_unregister( chan ) ;
		
		kfree(chan);
		chan = NULL;
	}
}

static int __wcfxs_hardware_init(struct wcfxs *wc)
{
	unsigned char  x, y;
	
	/* first, put AUX2 in 1 */
	outb(0x04, wc->ioaddr + WC_AUXD); 
	/* second, put AUX2 into OUTPUT */
	outb(0x04, wc->ioaddr + WC_AUXC); 

#if 0
	/* Signal Reset */
/* register in tiger internel register space can be addressed directly 
 * Tiger internel  register 0 is responsible for reset
 * internel register is not PCI Config Space register */	
	outb(0x01, wc->ioaddr + WC_CNTL); /* reset Tiger 320 */

	printk("Card with name %s , module %d is initting.....\r\n", wc->name, wc->module );

	/* Reset PCI Interface chip and registers (and serial) */
	outb(0x06, wc->ioaddr + WC_CNTL);


	/* Back to normal, with automatic DMA wrap around */
	outb(0x30 | 0x01, wc->ioaddr + WC_CNTL);
	
	/* Make sure serial port and DMA are out of reset */
	outb(inb(wc->ioaddr + WC_CNTL) & 0xf9, WC_CNTL);
#endif

	outb(0x30 | 0x01, wc->ioaddr + WC_CNTL);
	/* Configure PCM bus for MSB->LSB operation */
	outb(0xc1, wc->ioaddr + WC_SERCTL);

	/* Delay Frame Sync by 0 so it's properly aligned */
	outb(0x0, wc->ioaddr + WC_FSCDELAY);

	/* Setup DMA Addresses */
#if 0
	outl(wc->writedma,                    wc->ioaddr + WC_DMAWS);		/* Write start */
	outl(wc->writedma + AS_CHUNKSIZE * 4 - 4, wc->ioaddr + WC_DMAWI);		/* Middle (interrupt) */
	outl(wc->writedma + AS_CHUNKSIZE * 8 - 4, wc->ioaddr + WC_DMAWE);			/* End */
	
	outl(wc->readdma,                    	 wc->ioaddr + WC_DMARS);	/* Read start */
	outl(wc->readdma + AS_CHUNKSIZE * 4 - 4, 	 wc->ioaddr + WC_DMARI);	/* Middle (interrupt) */
	outl(wc->readdma + AS_CHUNKSIZE * 8 - 4, wc->ioaddr + WC_DMARE);	/* End */
#endif
#if 1
	for(y=0; y<4; y++)
	{
		int bits;
		bits = y*8;
		x = (wc->writedma >>bits )&0xff;
		outb(x, wc->ioaddr + WC_DMAWS + y);		/* Write start */
		x = ((wc->writedma + AS_CHUNKSIZE * 4 - 4)>>bits) &0xff;
		outb(x, wc->ioaddr + WC_DMAWI + y );		/* Middle (interrupt) */
		x = ((wc->writedma + AS_CHUNKSIZE * 8 - 4)>>bits) &0xff;
		outb(x, wc->ioaddr + WC_DMAWE + y );		/* End */

		x = (wc->readdma >>bits )&0xff;
		outb(x, wc->ioaddr + WC_DMARS + y);		/* Read start */
		x = ((wc->readdma + AS_CHUNKSIZE * 4 - 4)>>bits) &0xff;
		outb(x, wc->ioaddr + WC_DMARI + y );		/* Middle (interrupt) */
		x = ((wc->readdma + AS_CHUNKSIZE * 8 - 4)>>bits) &0xff;
		outb(x, wc->ioaddr + WC_DMARE + y );		/* End */
		
//	outl(wc->readdma,                    	 wc->ioaddr + WC_DMARS);	/* Read start */
//	outl(wc->readdma + AS_CHUNKSIZE * 4 - 4, 	 wc->ioaddr + WC_DMARI);	/* Middle (interrupt) */
//	outl(wc->readdma + AS_CHUNKSIZE * 8 - 4, wc->ioaddr + WC_DMARE);	/* End */
	}
#endif	

	/* Clear interrupts */
	outb(0xff, wc->ioaddr + WC_INTSTAT);

//		printk("\r\n\r\n==================Phone Hardware On Slot %d of card %s :", x, wc->name);
	__as_fxs_channel_register(wc );
	
	if(! wc->cards )
	{
		printk("No modules found, device can not be initialized:%d\r\n", wc->cards );
		return -ENODEV;
	}

	return 0;
}

int __devinit as_fxs_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int res =1;
	struct wcfxs *wc =NULL;
	struct wcfxs *last, *next;

	/* for multi-card support , lizhijie 2004.11.25 */
	wc = (struct wcfxs *)kmalloc(sizeof(struct wcfxs), GFP_KERNEL);
	if(!wc)
	{
		printk("Critical mem failed for wcfxs\r\n");
		return -ENOMEM;
	}
	memset(wc, 0, sizeof(struct wcfxs));
	
	if(!wc_header)
	{
		wc_header = wc;
	}
	else
	{
		last = wc_header;
		next = last->next;;
		while(next)
		{
			last = next;
			next =next->next;
		}
		last->next = wc;
	}

	modules ++;
	wc->module = modules;
	sprintf(wc->name, "ASTDM-%d", wc->module);

#if AS_DEBUG_TIGER
	wc->read_info = as_fxs_tiger_info;
#endif

#if 1//AS_DEBUG
	printk("\r\n*************************************Interface card %s\r\n", wc->name );
#endif
		
#if 1
	if (pci_enable_device(pdev)) 
	{
		printk("\r\n*************************************Interface card Error %s\r\n", wc->name );
		return -EIO;
	} 

	spin_lock_init(&wc->lock);
		
	wc->cards = 0;
	wc->ioaddr = pci_resource_start(pdev, 0);
	wc->dev = pdev;
		
	/* Keep track of whether we need to free the region */
	if (request_region(wc->ioaddr, 0xff, wc->name /*"asfxs"*/ )) 
		wc->freeregion = 1;

	/* Allocate enough memory for two as chunks, receive and transmit.  Each sample uses
	   32 bits.  Allocate an extra set just for control too */
	wc->writechunk = (int *)pci_alloc_consistent(pdev, AS_DMA_BUFFER_SIZE, &wc->writedma);
	if (!wc->writechunk) 
	{
		printk("wcfxs: Unable to allocate DMA-able memory\n");
		goto Error;
	}

	/* Initialize Write/Buffers to all blank data */
	memset((void *)wc->writechunk, 0, AS_DMA_BUFFER_SIZE );
			
	wc->readchunk = wc->writechunk + AS_MAX_CHUNKSIZE * 2;	/* in doublewords */
	wc->readdma = wc->writedma + AS_MAX_CHUNKSIZE * 8;		/* in bytes */

	trace
	if (request_irq(pdev->irq, as_fxs_interrupt, SA_SHIRQ, /*"astdm"*/wc->name, wc)) 
	{
		printk("asfxs: Unable to request IRQ %d\n", pdev->irq);
		goto Error;
	}

	trace
	if ( __wcfxs_hardware_init(wc)) 
	{
#if 0	
		unsigned char x;
		/* Set Reset Low */
		x=inb(wc->ioaddr + WC_CNTL);
		outb((~0x1)&x, wc->ioaddr + WC_CNTL);
#endif		
		/* Free Resources */
		free_irq(pdev->irq, wc);

		__as_fxs_channel_unregister( wc);
		goto Error;
	}

	/* Start DMA */
	__wcfxs_start_dma(wc);

	/* Enable bus mastering */
	pci_set_master(pdev);

	pci_set_drvdata(pdev, wc);

	__wcfxs_enable_interrupts(wc);

	printk("Found a Asisst TDM Card %s:  (%d Channels)\n", wc->name, wc->cards);
			
	res = 1;
#endif

	printk("Card %s with %d channels initialized!!!\r\n***********************************************************\r\n\r\n", wc->name, wc->cards);
	return res;
	
Error:
	pci_set_drvdata(pdev, NULL);
	
	if (wc->writechunk)
		pci_free_consistent(pdev, AS_DMA_BUFFER_SIZE, (void *)wc->writechunk, wc->writedma);
	if (wc->freeregion)
		release_region(wc->ioaddr, 0xff);
	kfree(wc);/* adde for multi-cards support */
	return -EIO;
}

void as_fxs_release(struct wcfxs *wc)
{
	__as_fxs_channel_unregister( wc);
	if (wc->freeregion)
		release_region(wc->ioaddr, 0xff);
	printk("release the FXS card\n");
}

void __devexit as_fxs_remove_one(struct pci_dev *pdev)
{
	struct wcfxs *wc = pci_get_drvdata(pdev);
	if (wc) 
	{
		/* Stop any DMA */
		as_fxs_stop_dma(wc);
		as_fxs_reset_tdm(wc);

		/* In case hardware is still there */
		as_fxs_disable_interrupts(wc);
		
		/* Immediately free resources */
		pci_free_consistent(pdev, AS_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)wc->writechunk, wc->writedma);
		free_irq(pdev->irq, wc);

		/* Reset PCI chip and registers */
		outb(0x0e, wc->ioaddr + WC_CNTL);

		/* Release span, possibly delayed */
		if (!wc->usecount)
		{
			as_fxs_release(wc);
			kfree(wc);/* adde for multi-cards support */
		}
		else
			wc->dead = 1;
	}
}
