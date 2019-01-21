/*
 * $Revision: 1.1.1.1 $
 * $Author: lizhijie $
 * $Id: as_dev_io_transmit.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

#include "asstel.h"
#include "as_fxs_common.h"

/* get data from channel and write it into txb buffer : channel memory DMA address */
static void __as_io_channel_out_getbuf_chunk(struct as_dev_chan *chan, unsigned char *txb)
{
	/* Called with ss->lock held */
	/* Buffer we're using */
	unsigned char *buf;

	/* Old buffer number */
	int oldbuf;
	/* How many bytes we need to process */
	int bytes = AS_CHUNKSIZE, left;

	/* Let's pick something to transmit.  First source to
	   try is our write-out buffer.  Always check it first because
	   its our 'fast path' for whatever that's worth. */
	while(bytes) 
	{
#if AS_POLICY_ENBALE	
		if (( chan->outwritebuf > -1) && !chan->txdisable) 
#else
		if ( chan->outwritebuf > -1) 
#endif			
		{
		/* copy data from upper layer to the write chunk of channel for DMA*/
			buf= chan->writebuf[chan->outwritebuf];
			left = chan->writen[chan->outwritebuf] - chan->writeidx[chan->outwritebuf];
			if (left > bytes)
				left = bytes;

			memcpy(txb, buf + chan->writeidx[chan->outwritebuf], left);
			chan->writeidx[chan->outwritebuf]+=left;
			txb += left;
			bytes -= left;
#if 0//DEBUG
			printk("Channel %d output data\r\n", chan->channo);
#endif
			/* Check buffer status */
			if ( chan->writeidx[chan->outwritebuf] >= chan->writen[chan->outwritebuf]) 
			{/* We've reached the end of our buffer.  Go to the next. */
				oldbuf = chan->outwritebuf;
				/* Clear out write index and such */
				chan->writeidx[oldbuf] = 0;
				chan->writen[oldbuf] = 0;
				chan->outwritebuf = (chan->outwritebuf + 1) % chan->numbufs; /*next buffer in circular */
				if (chan->outwritebuf == chan->inwritebuf) 
				{
					/* Whoopsies, we're run out of buffers.  Mark ours
					as -1 and wait for the filler to notify us that
					there is something to write */
					chan->outwritebuf = -1;
#if AS_POLICY_ENBALE	
					/* If we're only supposed to start when full, disable the transmitter */
					if (chan->txbufpolicy == AS_POLICY_WHEN_FULL)
						chan->txdisable = 1;
#endif					
				}
				
				if (chan->inwritebuf < 0) 
				{
					/* The filler doesn't have a place to put data.  Now
					that we're done with this buffer, notify them. */
					chan->inwritebuf = oldbuf;
				}	
/* this line is added by lizhijie for the case of write block ,09.27 
	wakeup the thread sleep in file write operation
   move this line here from brace to preventing from current process block
   lizhijie 2004.12.24
*/
				wake_up_interruptible(&chan->writebufq);

			}
		}
		else 
		{/* Lastly we use silence on telephony channels */
			memset(txb, 0 , bytes);	
			bytes = 0;
		}
	}	
}

void __as_io_channel_out_transmit_chunk(struct as_dev_chan *chan, unsigned char *buf)
{
	int x;
#if 0	
	unsigned char silly[AS_CHUNKSIZE];
	
	/* Called with chan->lock locked */
	if (!buf)
	{
		memset(silly,  0 , sizeof(silly));
		buf = silly;
	}	
#endif

	__as_io_channel_out_getbuf_chunk(chan, buf);

	if(chan->gainalloc)
	{
		for (x=0;x<AS_CHUNKSIZE;x++) 
		{
			buf[x] = chan->txgain[buf[x]];
		}
	}	

}

int as_io_span_out_transmit(void *fxs)
{
	unsigned long flags;
	struct as_dev_chan  *chan;
	struct as_dev_span *span;
	int i;
	struct wcfxs *wc = (struct wcfxs *)fxs;
	span = wc->span;
	
	for (i=0;i<wc->cards;i++)
	{
		chan = wc->chans[i];
		
		spin_lock_irqsave(&chan->lock, flags);

		__as_io_channel_out_transmit_chunk(chan, chan->writechunk);

		/*end of this channel is not a slave of others */
		spin_unlock_irqrestore(&chan->lock, flags);
	}

	return 0;
}

