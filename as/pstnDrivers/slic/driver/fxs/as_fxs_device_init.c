/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_fxs_device_init.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/07/07 02:47:06  wangwei
 * 更改设备号从0-7
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.11  2004/12/31 09:36:42  fengshikui
 * 把  wake_up_interruptible(&chan->writebufq) 从286行移到238行
 *
 * Revision 1.10  2004/12/24 12:16:15  lizhijie
 * Cdebug the process block and Fedback cuurent is offVS: ----------------------------------------------------------------------
 *
 * Revision 1.9  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.8  2004/12/15 07:33:05  lizhijie
 * recommit
 *
 * Revision 1.7  2004/12/11 05:35:51  lizhijie
 * add Tiger320 debug info
 *
 * Revision 1.6  2004/12/09 09:54:41  lizhijie
 * debug a bug for multi-cards support
 *
 * Revision 1.5  2004/12/04 08:07:17  lizhijie
 * rename the channel name as "ASTDM-$card-$channel"
 *
 * Revision 1.4  2004/11/29 01:53:38  lizhijie
 * update some redudent code and register setting
 *
 * Revision 1.3  2004/11/26 12:33:31  lizhijie
 * add multi-card support
 *
 * Revision 1.2  2004/11/25 07:59:24  lizhijie
 * recommit all
 *
*/
#include "as_fxs_common.h"

extern int fxshonormode ;
extern int timingonly ;
extern struct fxo_mode  fxo_modes[] ;
extern int _opermode ;
//extern struct wcfxs *ifaces[];

//struct wcfxs asfxs;

/* for multi-card support , lzj */
struct wcfxs *wc_header = NULL;
int modules = 0;

#define AS_FXS_CHANNELS	4


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
}

static struct as_dev_chan *__as_create_channel(as_channel_type_t type ,int card_position, struct wcfxs *wc)
{
	struct as_dev_chan *chan;

	chan = kmalloc( sizeof(struct as_dev_chan), GFP_KERNEL);
	if(!chan)
	{
		printk("No memory available when create a channel\r\n");
		return NULL;
	}
	memset(chan, 0, sizeof(struct as_dev_chan) );
/*FXO device using FXS signing  as same as 1.9*/
	if(type==AS_CHANNEL_TYPE_FXO)
	{
		sprintf(chan->name, "FXO-%d-%d",wc->module, card_position );
		chan->flags = AS_CHAN_FLAG_AUDIO;			
		chan->sigcap = AS_SIG_FXSKS | AS_SIG_FXSLS | AS_SIG_FXSGS;
		chan->sig = AS_SIG_FXSGS;/*AS_SIG_FXOKS | AS_SIG_FXOLS | */
		chan->rxsig = AS_BBIT | ((AS_ABIT |AS_BBIT) << 8);
		chan->rxhooksig = AS_RXSIG_INITIAL;
	}
	else/*  FXS device use FXO signing*/
	{
		sprintf(chan->name,  "FXS-%d-%d",wc->module,  card_position);
		chan->flags =  AS_CHAN_FLAG_AUDIO;
		chan->sigcap = AS_SIG_FXOKS | AS_SIG_FXOLS | AS_SIG_FXOGS;
		chan->sig = AS_SIG_FXOGS;// |AS_SIG_FXSKS | AS_SIG_FXSLS;
		chan->rxsig = AS_BBIT | ((AS_ABIT |AS_BBIT) << 8);
		chan->rxhooksig = AS_RXSIG_INITIAL;
#if AS_PROSLIC_DSP
		chan->slic_play_tone = as_fxs_play_tone;
#endif
	}

	chan->chanpos = card_position + 1;
	chan->open = as_fxs_open;
	chan->close = as_fxs_close;
	chan->ioctl  = as_fxs_ioctl;
	chan->watchdog = as_fxs_watchdog;
	chan->hooksig = as_fxs_hooksig;

	chan->private = wc;//&asfxs;
	
	return chan;
}

static int __as_fxs_channel_register(struct wcfxs *wc, as_channel_type_t type, int card_position)
{
	int res = 0;
	struct as_dev_chan *chan;

	chan = __as_create_channel( type, card_position, wc);
	if( !chan)
		return -1;
	
	if( as_channel_register( chan) )
	{
			printk("Unable to register FXS channel %s with SPAN\n", chan->name );
			return -1;
	}
	printk("Registered FXS channel %s with SPAN\n", chan->name );

	wc->span = chan->span;
	wc->chans[card_position] = chan ;
	
	return res;
}

static void __as_fxs_channel_unregister(struct wcfxs *wc)
{
	int i;
	int x;
	struct as_dev_chan  *chan;

	for(i=0; i< wc->cards; i++)
	{
		x = wc->cardslot[i];
		chan = wc->chans[x];
		as_channel_unregister( chan ) ;
	}
}


static int __wcfxs_hardware_init(struct wcfxs *wc)
{
	/* Hardware stuff */
	long oldjiffies;
	unsigned char ver;
	unsigned char x,y;
	int failed = 0;

	/* Signal Reset */
/* register in tiger internel register space can be addressed directly 
 * Tiger internel  register 0 is responsible for reset
 * internel register is not PCI Config Space register
*/	
	outb(0x01, wc->ioaddr + WC_CNTL); /* reset Tiger 320 */

	printk("Card with name %s , module %d is initting.....\r\n", wc->name, wc->module );

	/* Check Freshmaker chip */
	x=inb(wc->ioaddr + WC_CNTL);
	
	ver = as_fxs_getcreg(wc, WC_VER);
	failed = 0;
	if (ver != 0x59) 
	{
		printk("Freshmaker version: %0X on Card %s\n", ver, wc->name );
		for (x=0;x<255;x++) 
		{
			/* Test registers */
			if (ver >= 0x70) 
			{
				as_fxs_setcreg(wc, WC_CS, x);
				y = as_fxs_getcreg(wc, WC_CS);
			} 
			else 
			{
				as_fxs_setcreg(wc, WC_TEST, x);
				y = as_fxs_getcreg(wc, WC_TEST);
			}
			if (x != y) 
			{
				printk("%02x != %02x\n", x, y);
				failed++;
			}
		}
		if (!failed) 
		{
			printk("Freshmaker on card %s passed register test\n", wc->name );
		} 
		else 
		{
			printk("Freshmaker on card %s failed register test\n", wc->name );
			return -1;
		}
		/* Go to half-duty FSYNC */
		as_fxs_setcreg(wc, WC_SYNC, 0x01);
		y = as_fxs_getcreg(wc, WC_SYNC);
	} 
	else 
	{
		printk("No freshmaker chip\n");
	}

	/* Reset PCI Interface chip and registers (and serial) */
	outb(0x06, wc->ioaddr + WC_CNTL);
	/* Setup our proper outputs for when we switch for our "serial" port */
	wc->ios = BIT_CS | BIT_SCLK | BIT_SDI;

	outb(wc->ios, wc->ioaddr + WC_AUXD);

	/* Set all to outputs except AUX 5, which is an input */
	outb(0xdf, wc->ioaddr + WC_AUXC);

	/* Select alternate function for AUX0 */
/* refer page  25 */	
	outb(0x4, wc->ioaddr + WC_AUXFUNC);
	
	/* Wait 1/4 of a sec */
	oldjiffies = jiffies;
	while(jiffies - oldjiffies < (HZ / 4) + 1);

	/* Back to normal, with automatic DMA wrap around */
	outb(0x30 | 0x01, wc->ioaddr + WC_CNTL);
	
	/* Make sure serial port and DMA are out of reset */
	outb(inb(wc->ioaddr + WC_CNTL) & 0xf9, WC_CNTL);
	
	/* Configure serial port for MSB->LSB operation */
	outb(0xc1, wc->ioaddr + WC_SERCTL);

	/* Delay FSC by 0 so it's properly aligned */
	outb(0x0, wc->ioaddr + WC_FSCDELAY);

	/* Setup DMA Addresses */
#if 1
	outl(wc->readdma,                    	 wc->ioaddr + WC_DMARS);	/* Read start */
	outl(wc->readdma + AS_CHUNKSIZE * 4 - 4, 	 wc->ioaddr + WC_DMARI);	/* Middle (interrupt) */
	outl(wc->readdma + AS_CHUNKSIZE * 8 - 4, wc->ioaddr + WC_DMARE);	/* End */
	
	outl(wc->writedma,                    wc->ioaddr + WC_DMAWS);		/* Write start */
	outl(wc->writedma + AS_CHUNKSIZE * 4 - 4, wc->ioaddr + WC_DMAWI);		/* Middle (interrupt) */
	outl(wc->writedma + AS_CHUNKSIZE * 8 - 4, wc->ioaddr + WC_DMAWE);			/* End */
#endif
#if 0
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

	/* Wait 1/4 of a second more */
	oldjiffies = jiffies;
	while(jiffies - oldjiffies < (HZ / 4) + 1);

	for (x=0; x<NUM_CARDS ; x++) 
	{
		int sane=0,ret=0;
		int found = 0;
		as_channel_type_t is_fxs = AS_CHANNEL_TYPE_FXS ;

		/* Init with Auto Calibration */
		if (!(ret= as_fxs_init_proslic(wc, x, 0, 0, sane))) 
		{
			found = 1;
		} 
		else 
		{
			if(ret!=-2) 
			{
				sane=1;
				/* Init with Manual Calibration */
				if (!as_fxs_init_proslic(wc, x, 0, 1, sane)) 
				{
					found = 1;
				} 
				else 
				{
#if AS_WITH_FXO_MODULE
					printk(" FAILED FXS (%s)\n",  fxshonormode ? fxo_modes[_opermode].name : "FCC");
#endif
				} 
			}
			/* return -2 : this is a FXO channel */
#if AS_WITH_FXO_MODULE
			else if (!(ret = as_fxs_init_voicedaa(wc, x, 0, 0, sane))) 
			{
				found = 1;
				is_fxs = AS_CHANNEL_TYPE_FXO ;

			} 
#endif			
		}

		printk("\r\n\r\n==================Phone Hardware On Slot %d of card %s :", x, wc->name);
		if(found )
		{
			wc->cardflag |= (1 << x);
			__as_fxs_channel_register(wc,  is_fxs,  x /*wc->cards*/ );
			wc->cardslot[wc->cards] = x;
			wc->cards ++;
			printk(" Installed -- %s\n", is_fxs==AS_CHANNEL_TYPE_FXO?"FXO": "FXS" );
		
		}
		else
			printk("Not installed\n" );

	}


	if(! wc->cards )
	{
		printk("No modules found, device can not be initialized:%d\r\n", wc->cards );
		return -ENODEV;
	}
	
	/* Return error if nothing initialized okay. */
	if (!wc->cardflag && !timingonly)
	{
		return -1;
	}	
	as_fxs_setcreg(wc, WC_SYNC, (wc->cardflag << 1) | 0x1);

	return 0;
}


/* init zap_span and zap_chan data struct and register it in zaptel driver */
static int __wcfxs_initialize(struct wcfxs *wc)
{
#if 0	
	wc->span.deflaw = AS_LAW_MULAW;

	wc->span.chans = wc->chans;
	wc->span.channels = wc->cards;
	wc->span.flags = AS_FLAG_RBS;
	init_waitqueue_head(&wc->span.maintq);

	wc->span.pvt = wc;
	return __as_fxs_channel_register(wc);
#endif	
	return 0;
}


int __devinit as_fxs_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct wcfxs *wc ;//&asfxs;
	struct wcfxs_desc *d = (struct wcfxs_desc *)ent->driver_data;
	int res;
	int x;
	int i = 1;

	trace
	/* for multi-card support , lizhijie 2004.11.25 */
	wc = (struct wcfxs *)kmalloc(sizeof(struct wcfxs), GFP_KERNEL);
	if(!wc)
	{
		printk("Critical mem failed for wcfxs\r\n");
		return -ENOMEM;
	}
	memset(wc, 0, sizeof(struct wcfxs));
	printk("wc address 0X%lx\r\n", (long)wc);
#if 1
	if(!wc_header)
	{
		wc_header = wc;
	}
	else
	{
		struct wcfxs *last = wc_header;
		struct wcfxs *next = last->next;
		while(next)
		{
			last = next;
			next =next->next;
			i++;
		}
		i++;
		last->next = wc;
	}
	wc->module = i;
	trace
#endif
#if 0
	if(!wc_header)
	{
		wc_header = wc;
	}
	else
	{
		struct wcfxs *last = wc_header;
		struct wcfxs *next = last->next;
		while(next)
		{
			last = next;
			next =next->next;
		}
		last->next = wc;
	}

	modules ++;
	wc->module = modules;
#endif	
#if AS_DEBUG_TIGER
	wc->read_info = as_fxs_tiger_info;
#endif

trace
	sprintf(wc->name, "ASTDM-%d", wc->module);
		
#if 1//AS_DEBUG
	printk("*************************************\r\n\tInterface with name %s on card %s\r\n", d->name, wc->name );
#endif
		
	if (pci_enable_device(pdev)) 
	{
		res = -EIO;
	} 
	else 
	{
		spin_lock_init(&wc->lock);
		
		wc->curcard = -1;
		wc->cards = 0;
		wc->ioaddr = pci_resource_start(pdev, 0);
		wc->dev = pdev;
		wc->flags = d->flags;
			/* Keep track of whether we need to free the region */
		if (request_region(wc->ioaddr, 0xff, wc->name /*"asfxs"*/ )) 
				wc->freeregion = 1;

		/* Allocate enough memory for two zt chunks, receive and transmit.  Each sample uses
		   32 bits.  Allocate an extra set just for control too */
		wc->writechunk = (int *)pci_alloc_consistent(pdev, AS_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, &wc->writedma);
		if (!wc->writechunk) 
		{
			printk("wcfxs: Unable to allocate DMA-able memory\n");
			if (wc->freeregion)
				release_region(wc->ioaddr, 0xff);
			kfree(wc);/* adde for multi-cards support */
			return -ENOMEM;
		}

		wc->readchunk = wc->writechunk + AS_MAX_CHUNKSIZE * 2;	/* in doublewords */
		wc->readdma = wc->writedma + AS_MAX_CHUNKSIZE * 8;		/* in bytes */

		if ( __wcfxs_initialize(wc)) 
		{
			printk("wcfxs: Unable to intialize FXS\n");
				/* Set Reset Low */
			x=inb(wc->ioaddr + WC_CNTL);
			outb((~0x1)&x, wc->ioaddr + WC_CNTL);
				/* Free Resources */
			free_irq(pdev->irq, wc);
			if (wc->freeregion)
				release_region(wc->ioaddr, 0xff);
			pci_free_consistent(pdev, AS_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)wc->writechunk, wc->writedma);
			kfree(wc);/* adde for multi-cards support */
			return -EIO;
		}

		/* Enable bus mastering */
		pci_set_master(pdev);

		pci_set_drvdata(pdev, wc);

		if (request_irq(pdev->irq, as_fxs_interrupt, SA_SHIRQ, /*"astdm"*/wc->name, wc)) 
		{
			printk("asfxs: Unable to request IRQ %d\n", pdev->irq);
			if (wc->freeregion)
				release_region(wc->ioaddr, 0xff);
			pci_free_consistent(pdev, AS_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)wc->writechunk, wc->writedma);
			pci_set_drvdata(pdev, NULL);
			kfree(wc);/* adde for multi-cards support */
			return -EIO;
		}

		if ( __wcfxs_hardware_init(wc)) 
		{
			unsigned char x;
			/* Set Reset Low */

			x=inb(wc->ioaddr + WC_CNTL);
			outb((~0x1)&x, wc->ioaddr + WC_CNTL);
			/* Free Resources */
			free_irq(pdev->irq, wc);
			if (wc->freeregion)
				release_region(wc->ioaddr, 0xff);
			pci_free_consistent(pdev, AS_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)wc->writechunk, wc->writedma);
			pci_set_drvdata(pdev, NULL);
			__as_fxs_channel_unregister( wc);
			kfree(wc);/* adde for multi-cards support */
			return -EIO;
		}

		__wcfxs_enable_interrupts(wc);
			/* Initialize Write/Buffers to all blank data */
		memset((void *)wc->writechunk,0, AS_MAX_CHUNKSIZE * 2 * 2 * 4);

			/* Start DMA */
		__wcfxs_start_dma(wc);

		printk("Found a Asisst TDM Card %s:  (%d modules)\n", wc->name, wc->cards);
			
		for(i=0; i< wc->cards; i++)
		{/* line feed when startup */
			x = wc->cardslot[i];
			as_fxs_setreg( wc, x, 64, 0x1);
		}

		
		res = 1;
	}

	printk("Card %s with %d channels initialized\r\n*********************************\r\n\r\n", wc->name, wc->cards);
	return res;
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
			wc = NULL;
		}
		else
			wc->dead = 1;
	}
}
