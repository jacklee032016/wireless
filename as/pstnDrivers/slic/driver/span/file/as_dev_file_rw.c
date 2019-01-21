/*
 * $Author: lizhijie $
 * $Log: as_dev_file_rw.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/07/07 02:48:42  wangwei
 * no message
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/11/22 01:54:04  lizhijie
 * add some user module into CVS
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"

ssize_t as_file_read(struct file *file, char *usrbuf, size_t count, loff_t *ppos)
{
	int unit = UNIT(file);
	int amnt;
	int res, rv;
	int oldbuf,x;
	unsigned long flags;

	if (count < 0)
		return -EINVAL;
	
	struct as_dev_chan *chan = as_span_get_channel( unit );
	
	/* Make sure count never exceeds 65k, and make sure it's unsigned */
	count &= 0xffff;
	if (!chan) 
		return -EINVAL;
	if (count < 1)
		return -EINVAL;
	
	for(;;) 
	{
		/* grab the lock and only get status of channel : outreadbuf */
		spin_lock_irqsave(&chan->lock, flags);
#if 0
		if (chan->eventinidx != chan->eventoutidx) 
		{
			spin_unlock_irqrestore(&chan->lock, flags);
			return -AS_ELAST;
		}
#endif		
		res = chan->outreadbuf;
		if (chan->rxdisable)
			res = -1;
		
		spin_unlock_irqrestore(&chan->lock, flags);
		if (res >= 0) /* get the read result */
			break;
		
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		rv = as_schedule_delay(&chan->readbufq);
		
		if (rv) /* error ,return */
			return (rv);
	}
	
	amnt = count;
	if (chan->flags & AS_CHAN_FLAG_LINEAR) 
	{
		if (amnt > (chan->readn[chan->outreadbuf] << 1) ) 
			amnt = chan->readn[chan->outreadbuf] << 1;
		if (amnt) 
		{
			/* There seems to be a max stack size, so we have
			   to do this in smaller pieces */
			short lindata[512];
			int left = amnt >> 1; /* amnt is in bytes */
			int pos = 0;
			int pass;
			while(left) 
			{
				pass = left;
				if (pass > 512)
					pass = 512;
				for (x=0;x<pass;x++)
#if 0					
					lindata[x] = 0;//ZT_XLAW(chan->readbuf[chan->outreadbuf][x + pos], chan);
#endif				
					lindata[x] = AS_XLAW(chan->readbuf[chan->outreadbuf][x + pos], chan);
				if (copy_to_user(usrbuf + (pos << 1), lindata, pass << 1))
					return -EFAULT;
				left -= pass;
				pos += pass;
			}
		}
	}
	else 
	{
		if (amnt > chan->readn[chan->outreadbuf]) 
			amnt = chan->readn[chan->outreadbuf];
		if (amnt) 
		{
			if (copy_to_user(usrbuf, chan->readbuf[chan->outreadbuf], amnt))
				return -EFAULT;
		}
	}
	
	spin_lock_irqsave(&chan->lock, flags);
	chan->readidx[chan->outreadbuf] = 0;
	chan->readn[chan->outreadbuf] = 0;
	oldbuf = chan->outreadbuf;
	chan->outreadbuf = (chan->outreadbuf + 1) % chan->numbufs;
	
	if (chan->outreadbuf == chan->inreadbuf) 
	{
		/* Out of stuff */
		chan->outreadbuf = -1;
		if (chan->rxbufpolicy == AS_POLICY_WHEN_FULL)
			chan->rxdisable = 1;
	}
	
	if (chan->inreadbuf < 0) 
	{
		/* Notify interrupt handler that we have some space now */
		chan->inreadbuf = oldbuf;
	}
	spin_unlock_irqrestore(&chan->lock, flags);
	
	return amnt;
}


ssize_t as_file_write(struct file *file, const char *usrbuf, size_t count, loff_t *ppos)
{
	int unit = UNIT(file);
	int res, amnt, oldbuf, rv,x;
	unsigned long flags;
	if (count < 0)
		return -EINVAL;
	struct as_dev_chan *chan = as_span_get_channel(unit) ;
	
	/* Make sure count never exceeds 65k, and make sure it's unsigned */
	count &= 0xffff;
	if (!chan) 
		return -EINVAL;
	if (count < 1)
		return -EINVAL;

	/* check the write buffer are available, otherwise wait some time later and */	
	for(;;) 
	{
		spin_lock_irqsave(&chan->lock, flags);

		if (chan->curtone ) 
		{
			chan->curtone = NULL;
			chan->tonep = 0;
			chan->dialing = 0;
			chan->txdialbuf[0] = '\0';
		}

#if 0		
		if (chan->eventinidx != chan->eventoutidx) 
		{/* some event is pending now */
			spin_unlock_irqrestore(&chan->lock, flags);
			return -AS_ELAST;
		}
#endif		
		res = chan->inwritebuf;
		spin_unlock_irqrestore(&chan->lock, flags);
		if (res >= 0) 
			break;
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		/* Wait for something to be available */
		rv = as_schedule_delay(&chan->writebufq);
		if (rv)/*some error, then return */
			return rv;
	}

	amnt = count;
	if (chan->flags & AS_CHAN_FLAG_LINEAR) 
	{
		if (amnt > (chan->blocksize << 1))
			amnt = chan->blocksize << 1;
	} 
	else 
	{
		if (amnt > chan->blocksize)
			amnt = chan->blocksize;
	}

#if CONFIG_AS_DEBUG
	printk("__as_chan_write(unit: %d, inwritebuf: %d, outwritebuf: %d amnt: %d\n", 
		unit, chan->inwritebuf, chan->outwritebuf, amnt);
#endif

	if (amnt) 
	{
		if (chan->flags & AS_CHAN_FLAG_LINEAR) 
		{
		/* this flags indicate that data transfered by user space is in the format of 
		 * 16 bits linear code ?????
		*/
			/* There seems to be a max stack size, so we have
			   to do this in smaller pieces */
			short lindata[512];
			int left = amnt >> 1; /* amnt is in bytes */
			int pos = 0;
			int pass;
			while(left) 
			{
				pass = left;
				if (pass > 512)
					pass = 512;
				if (copy_from_user(lindata, usrbuf + (pos << 1), pass << 1))
					return -EFAULT;
				left -= pass;
				for (x=0;x<pass;x++)
				{
#if 0
					chan->writebuf[chan->inwritebuf][x + pos] = 0; ZT_LIN2X(lindata[x], chan);
#endif
					chan->writebuf[chan->inwritebuf][x + pos] = AS_LIN2X(lindata[x], chan);
				}	
				pos += pass;
			}
			chan->writen[chan->inwritebuf] = amnt >> 1;
		}
		else 
		{
			copy_from_user(chan->writebuf[chan->inwritebuf], usrbuf, amnt);
			chan->writen[chan->inwritebuf] = amnt;
		}
		
		chan->writeidx[chan->inwritebuf] = 0;

		oldbuf = chan->inwritebuf;
		spin_lock_irqsave(&chan->lock, flags);
		chan->inwritebuf = (chan->inwritebuf + 1) % chan->numbufs;
		if (chan->inwritebuf == chan->outwritebuf) 
		{
			/* Don't stomp on the transmitter, just wait for them to 
			   wake us up */
			chan->inwritebuf = -1;
			/* Make sure the transmitter is transmitting in case of POLICY_WHEN_FULL */
			chan->txdisable = 0;
		}
		if (chan->outwritebuf < 0) 
		{
			/* Okay, the interrupt handler has been waiting for us.  Give them a buffer */
			chan->outwritebuf = oldbuf;
		}
		spin_unlock_irqrestore(&chan->lock, flags);
	}
	
	return amnt;
}

