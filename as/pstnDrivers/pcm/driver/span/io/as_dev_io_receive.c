/*
 * $Revision: 1.1.1.1 $
 * $Author: lizhijie $
 * $Id: as_dev_io_receive.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

#include "asstel.h"
#include "as_fxs_common.h"

static inline void __as_io_channel_in_putbuf_chunk(struct as_dev_chan *ms, unsigned char *rxb)
{
	/* Called with ss->lock held */
	/* Our receive buffer */
	unsigned char *buf;
	int oldbuf;
	int eof=0;
	int left;
	int bytes = AS_CHUNKSIZE;
#if 0	
	int abort=0;
#endif
	while(bytes) 
	{
#if 0	
		abort = 0;
#endif
		eof = 0;
		/* Next, figure out if we've got a buffer to receive into */
		if (ms->inreadbuf > -1) 
		{
			/* Read into the current buffer */
			buf = ms->readbuf[ms->inreadbuf];
			left = ms->blocksize - ms->readidx[ms->inreadbuf];
			if (left > bytes)
				left = bytes;
			memcpy(buf + ms->readidx[ms->inreadbuf], rxb, left);
			
			rxb += left;
			ms->readidx[ms->inreadbuf] += left;
			bytes -= left;
			/* End of frame is decided by block size of 'N' */
			eof = (ms->readidx[ms->inreadbuf] >= ms->blocksize);

			if (eof)  
			{/* Finished with this buffer, try another. */
				oldbuf = ms->inreadbuf;
				ms->readn[ms->inreadbuf] = ms->readidx[ms->inreadbuf];
#if 0//AS_DEBUG
				printk("EOF, len is %d in channel %d\n", ms->readn[ms->inreadbuf], ms->channo );
#endif
				{
					ms->inreadbuf = (ms->inreadbuf + 1) % ms->numbufs;
					if (ms->inreadbuf == ms->outreadbuf) 
					{
						/* Whoops, we're full, and have no where else
						to store into at the moment.  We'll drop it
						until there's a buffer available */
#if 0//AS_DEBUG
						printk("Out of storage space\n");
#endif
						ms->inreadbuf = -1;
#if 0
						/* Enable the receiver in case they've got POLICY_WHEN_FULL */
						ms->rxdisable = 0;
#endif
					}
					if (ms->outreadbuf < 0) 
					{ /* start out buffer if not already */
						ms->outreadbuf = oldbuf;
					}
/* In the very orignal driver, it was quite well known to me (Jim) that there
was a possibility that a channel sleeping on a receive block needed to
be potentially woken up EVERY time a buffer was filled, not just on the first
one, because if only done on the first one there is a slight timing potential
of missing the wakeup (between where it senses the (lack of) active condition
(with interrupts disabled) and where it does the sleep (interrupts enabled)
in the read or iomux call, etc). That is why the read and iomux calls start
with an infinite loop that gets broken out of upon an active condition,
otherwise keeps sleeping and looking. The part in this code got "optimized"
out in the later versions, and is put back now. */

#if AS_POLICY_ENBALE	
					if (!ms->rxdisable) 
#endif						
					{ /* if receiver enabled */
						/* Notify a blocked reader that there is data available
						to be read, unless we're waiting for it to be full */
#if 0//AS_DEBUG
						printk("Notifying reader data in block %d\n", oldbuf);
#endif
						wake_up_interruptible(&ms->readbufq);
						wake_up_interruptible(&ms->sel);
					}
				}
			}
#if 0
			if (abort) 
			{	/* Start over reading frame */
				ms->readidx[ms->inreadbuf] = 0;
			}
#endif			
		} 
		else /* No place to receive -- drop on the floor */
			break;
	}
}

static void __as_io_channel_in_receive_chunk(struct as_dev_chan *chan, unsigned char *buf)
{
	/* Receive chunk of audio -- called with chan->lock held */
	int x;
#if 0	
	char waste[AS_CHUNKSIZE];

	if (!buf) 
	{
		memset(waste, 0 , sizeof(waste));
		buf = waste;
	}
#endif

	if(chan->gainalloc)
	{
		for (x=0;x<AS_CHUNKSIZE;x++) 
		{
			buf[x] = chan->rxgain[buf[x]];
		}
	}	

	/* put it into the upper layer buffer pool */
	__as_io_channel_in_putbuf_chunk(chan, buf);
}

int as_io_span_in_receive(void *fxs)
{
	unsigned long flags;
	struct as_dev_chan  *chan;
	int i;
	/*added for multi-cards support , lizhijie 2004.11.25 */
	struct wcfxs *wc = (struct wcfxs *)fxs;

	for (i=0;i<wc->cards;i++)
	{
		chan = wc->chans[i];

		spin_lock_irqsave(&chan->lock, flags);
			
		/* Process a normal channel */
		__as_io_channel_in_receive_chunk(chan, chan->readchunk);

		spin_unlock_irqrestore(&chan->lock, flags);
	}

	return 0;
}

