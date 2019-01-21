/*
 * $Author: lizhijie $
 * $Log: as_dev_chan_utils.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.3  2005/04/26 06:03:55  lizhijie
 * add version info for FXS devices
 *
 * Revision 1.2  2005/04/20 03:15:53  lizhijie
 * no message
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.3  2004/12/11 05:37:08  lizhijie
 * add Tiger320 debug info
 *
 * Revision 1.2  2004/11/29 01:59:11  lizhijie
 * proc file output
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"
#include "as_fxs_common.h" /* for proc file read the wcfxs info */

#include <linux/slab.h>

#if 0
static char *__as_sigstr(int sig)
{
	switch (sig) 
	{
		case AS_SIG_FXSLS:
			return "FXSLS";
		case AS_SIG_FXSKS:
			return "FXSKS";
		case AS_SIG_FXSGS:
			return "FXSGS";
		case AS_SIG_FXOLS:
			return "FXOLS";
		case AS_SIG_FXOKS:
			return "FXOKS";
		case AS_SIG_FXOGS:
			return "FXOGS";
		default:
			return "Unsupport";
	}
}
#endif

/* added for multi-cards support , lizhijie 2004.11.26 */
char *as_cards_debug_info(void *data)
{
	char *buf = NULL;
	int len = 0;
	int i= 0;
	struct wcfxs *wc = (struct wcfxs *)data;

		
	buf = (char *)kmalloc(4096, GFP_KERNEL);
	if(!buf)
	{
		return NULL;
	}
	
	if(!wc)
	{
		len += sprintf(buf + len, "\tCard is not provided\r\n");
		return buf;
	}		

	len += sprintf(buf + len, AS_VERSION_INFO("slic"));
	while(wc)
	{
		i++;
		len += sprintf(buf + len, "\tCard No. :%2d \tname : %s, \tmodules : %d \r\n", 
			i, wc->name, wc->module );
		wc =wc->next;
	}
	
	return buf;
}

#if AS_DEBUG_TIGER
/* debug card info for interrupt, lizhijie 2004.12.08 */
char *as_cards_debug_status_info(void *data)
{
	char *buf = NULL;
	int len = 0;
	int index = 0;
	struct wcfxs *wc = (struct wcfxs *)data;
	
	buf = (char *)kmalloc(4096, GFP_KERNEL);
	if(!buf)
	{
		return NULL;
	}

	while(wc)
	{
		char *tmp = NULL;
		index ++;
		len += sprintf(buf + len, "Card No. : %2d \tname : %s\r\n", index, wc->name );
		tmp = (wc->read_info)(wc);
		if(tmp)
		{
			len += sprintf(buf + len, "%s\r\n", tmp );
			kfree(tmp);
		}
		else
		{
			len += sprintf(buf + len, "Read card status info failed\r\n" );
		}

		wc = wc->next;
	}
	
	return buf;
}
#endif

char *as_channel_debug_info(struct as_dev_chan *chan)
{
	char *buf = NULL;
	int len = 0;
	
	buf = (char *)kmalloc(4096, GFP_KERNEL);
	if(!buf)
	{
		return NULL;
	}
	
	if(!chan)
	{
		len += sprintf(buf + len, "\tThe channel provided is null");
		return buf;
	}		
	
	if ( chan->name)
	{
		struct wcfxs *wc = (struct wcfxs *)chan->private;
		len += sprintf(buf + len, "\tChannel No. : %2d \tname : %s, \tdevice : %s ", 
			chan->channo, chan->name, wc->name );
	}
#if 0	
	if (chan->sig) 
		len += sprintf(buf + len, "Signal type : %s ", __as_sigstr(chan->sig) );
#endif	
	if ((chan->flags & AS_CHAN_FLAG_OPEN) ) 
		len += sprintf(buf + len, "(In use) ");

#if 0	
	len += sprintf(buf + len, "Buffer Info : Blocks : %d  BlockSize %d\r\n" , chan->numbufs, chan->blocksize);
	len += sprintf(buf + len, "Read Buffer Info : In-Index : %d  Out-Index %d\r\n" , chan->inreadbuf, chan->outreadbuf);
	len += sprintf(buf + len, "Write Buffer Info : In-Index : %d  Out-Index %d\r\n" , chan->inwritebuf, chan->outwritebuf);
	len += sprintf(buf + len, "\n");

	len += sprintf(buf + len,"Dump of Astel Channel %d (%s,%d,%d):\n\n",chan->channo,
				chan->name,chan->channo,chan->chanpos);
	len += sprintf(buf + len,"flags: %x hex, writechunk: %08lx, readchunk: %08lx\n",
				chan->flags, (long) chan->writechunk, (long) chan->readchunk);
	len += sprintf(buf + len,"rxgain: %08lx, txgain: %08lx, gainalloc: %d\n",
				(long) chan->rxgain, (long)chan->txgain, chan->gainalloc);
	len += sprintf(buf + len,"span: %08lx, sig: %x hex, sigcap: %x hex\n",
				(long)chan->span, chan->sig, chan->sigcap);
	len += sprintf(buf + len,"inreadbuf: %d, outreadbuf: %d, inwritebuf: %d, outwritebuf: %d\n",
				chan->inreadbuf, chan->outreadbuf, chan->inwritebuf, chan->outwritebuf);
	len += sprintf(buf + len,"blocksize: %d, numbufs: %d, txbufpolicy: %d, txbufpolicy: %d\n",
				chan->blocksize, chan->numbufs, chan->txbufpolicy, chan->rxbufpolicy);
	len += sprintf(buf + len,"txdisable: %d, rxdisable: %d, iomask: %d\n",
				chan->txdisable, chan->rxdisable, chan->iomask);
//	len += sprintf(buf + len,"curzone: %08lx, tonezone: %d, curtone: %08lx, tonep: %d\n",
//				(long) chan->curzone, chan->tonezone, (long) chan->curtone, chan->tonep);
//	len += sprintf(buf + len,"digitmode: %d, txdialbuf: %s, dialing: %d, aftdialtimer: %d, cadpos. %d\n",
//				chan->digitmode, chan->txdialbuf, chan->dialing,
//					chan->afterdialingtimer, chan->cadencepos);
//	len += sprintf(buf + len,"confna: %d, confn: %d, confmode: %d, confmute: %d\n",
//				chan->confna, chan->_confn, chan->confmode, chan->confmute);
//	len += sprintf(buf + len,"ec: %08lx, echocancel: %d, deflaw: %d, xlaw: %08lx\n",
//				(long) chan->ec, chan->echocancel, chan->deflaw, (long) chan->xlaw);
//	len += sprintf(buf + len,"echostate: %02x, echotimer: %d, echolastupdate: %d\n",
//				(int) chan->echostate, chan->echotimer, chan->echolastupdate);
	len += sprintf(buf + len,"itimer: %d, otimer: %d, ringdebtimer: %d\n\n",
				chan->itimer,chan->otimer,chan->ringdebtimer);
#endif

	return buf;
}


/* realloc the buffer for a channel 
 *  paramters : j : buffer size  numbufs : number of buffer 
*/
int  as_channel_reallocbufs(struct as_dev_chan *ss, int size, int numbufs)
{
	unsigned char *newbuf, *oldbuf;
	unsigned long flags;
	int x;
	/* Check numbufs */
	if (numbufs < 2)
		numbufs = 2;
	if (numbufs > AS_MAX_NUM_BUFS)
		numbufs = AS_MAX_NUM_BUFS;

	/* We need to allocate our buffers now */
	if (size ) 
	{
		newbuf = kmalloc(size * 2 * numbufs, GFP_KERNEL);
		if (!newbuf) 
			return (-ENOMEM);
	} 
	else
		newbuf = NULL;
	  /* Now that we've allocated our new buffer, we can safely
	     move things around... */
	spin_lock_irqsave(&ss->lock, flags);
	ss->blocksize = size ; /* set the blocksize */
	oldbuf = ss->readbuf[0]; /* Keep track of the old buffer */
	ss->readbuf[0] = NULL;
	
	if (newbuf) 
	{
		for (x=0;x<numbufs;x++) 
		{
			ss->readbuf[x] = newbuf + x * size ;
			ss->writebuf[x] = newbuf + (numbufs + x) * size;
		}
	} 
	else 
	{
		for (x=0;x<numbufs;x++) 
		{
			ss->readbuf[x] = NULL;
			ss->writebuf[x] = NULL;
		}
	}
	
	/* Mark all buffers as empty */
	for (x=0;x<numbufs;x++) 
		ss->writen[x] = 
		ss->writeidx[x]=
		ss->readn[x]=
		ss->readidx[x] = 0;
	
	/* Keep track of where our data goes (if it goes
	   anywhere at all) */
	if (newbuf) 
	{
		ss->inreadbuf = 0;
		ss->inwritebuf = 0;
	} 
	else 
	{
		ss->inreadbuf = -1;
		ss->inwritebuf = -1;
	}
	
	ss->outreadbuf = -1;
	ss->outwritebuf = -1;
	ss->numbufs = numbufs;
	if (ss->txbufpolicy == AS_POLICY_WHEN_FULL)
		ss->txdisable = 1;
	else
		ss->txdisable = 0;

	if (ss->rxbufpolicy == AS_POLICY_WHEN_FULL)
		ss->rxdisable = 1;
	else
		ss->rxdisable = 0;

	spin_unlock_irqrestore(&ss->lock, flags);
	if (oldbuf)
		kfree(oldbuf);
	
	return 0;
}

void as_channel_rbs_sethook(struct as_dev_chan *chan, int txsig, int txstate, int timeout)
{
	/* if no span, return doing nothing */
	if (!chan->span) 
		return;
	
	if (!chan->span->flags & AS_FLAG_RBS) 
	{
		printk("as_rbs: Tried to set RBS hook state on non-RBS channel %s\n", chan->name);
		return;
	}
	
	if ((txsig > 3) || (txsig < 0)) 
	{
		printk("as_rbs: Tried to set RBS hook state %d (> 3) on  channel %s\n", txsig, chan->name);
		return;
	}
#if 0	
	if (!chan->span->rbsbits && !chan->span->hooksig) 
	{
		printk("as_rbs: Tried to set RBS hook state %d on channel %s while span %s lacks rbsbits or hooksig function\n",
			txsig, chan->name, chan->span->name);
		return;
	}
#endif	
	chan->txstate = txstate;
/* wcfxs : this is the only case that should be happened */	
	if (chan->hooksig) 
	{
		printk("sethook with  hooksig provided by low layer driver\r\n");
		chan->txhooksig = txsig;
		chan->hooksig(chan, txsig);
		chan->otimer = timeout * 8;			/* Otimer is timer in samples */
		return;
	} 
	
	printk("as_rbs: Don't know RBS signalling type %d on channel %s\n", chan->sig, chan->name);
}


/* when channel is entering into ON-HOOK state, this function is called 
 * such as registered a channel., set on-hook by ioctl   lzj
*/
int as_channel_hangup(struct as_dev_chan *chan)
{
	int x,res=0;

	/* Can't hangup pseudo channels */
	if (!chan->span)
		return 0;
	/* Can't hang up a clear channel */
	if (chan->flags & AS_CHAN_FLAG_CLEAR)
		return -EINVAL;

	chan->kewlonhook = 0;


	if ( (chan->sig == AS_SIG_FXSLS) 
		| (chan->sig == AS_SIG_FXSKS) 
		||(chan->sig == AS_SIG_FXSGS) ) 
			chan->ringdebtimer = AS_RING_DEBOUNCE_TIME;

	if (chan->span->flags & AS_FLAG_RBS) 
	{
		printk("hangup with RBS span\r\n");
		if ((chan->sig == AS_SIG_FXOKS) && (chan->txstate != AS_TXSTATE_ONHOOK)) 
		{
			/* Do RBS signalling on the channel's behalf */
			as_channel_rbs_sethook(chan, AS_TXSIG_KEWL, AS_TXSTATE_KEWL, AS_KEWLTIME);
		} 
		else
			as_channel_rbs_sethook(chan, AS_TXSIG_ONHOOK, AS_TXSTATE_ONHOOK, 0);
	} 
	else 
	{
		printk("hangup with the handler provided by low layer driver\r\n");
		/* Let the driver hang up the line if it wants to  */
		if (chan->span->sethook)
		/* ZT_ONHOOK is not processed by handler provided by low layer driver */	
			res = chan->span->sethook(chan, AS_ONHOOK);
	}
	/* if not registered yet, just return here */
	if (!(chan->flags & AS_CHAN_FLAG_REGISTERED)) 
		return res;

	/* Mark all buffers as empty */
	for (x = 0;x < chan->numbufs;x++) 
	{
		chan->writen[x] = chan->writeidx[x]= chan->readn[x]= chan->readidx[x] = 0;
	}	
	
	if (chan->readbuf[0]) 
	{
		chan->inreadbuf = 0;
		chan->inwritebuf = 0;
	} 
	else 
	{
		chan->inreadbuf = -1;
		chan->inwritebuf = -1;
	}
	
	chan->outreadbuf = -1;
	chan->outwritebuf = -1;
	chan->dialing = 0;
	chan->afterdialingtimer = 0;
#if  AS_PROSLIC_DSP

#else
	chan->curtone = NULL;
#endif
	chan->cadencepos = 0;
	chan->txdialbuf[0] = 0;
	
	return res;
}

