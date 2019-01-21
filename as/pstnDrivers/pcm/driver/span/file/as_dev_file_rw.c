/*
 * $Revision: 1.1.1.1 $
 * $Log: as_dev_file_rw.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/06/27 06:01:38  lizhijie
 * no message
 *
 * $Author: lizhijie $
 * $Id: as_dev_file_rw.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

#include "asstel.h"

static ssize_t __as_channel_read(struct file *file, char *usrbuf, size_t count, int unit)
{
	int amnt;
	int res, rv;
	int oldbuf;
	unsigned long flags;
	struct as_dev_chan *chan = as_span_get_channel( unit );
	if (!chan) 
		return -EINVAL;
	
/* Make sure count never exceeds 65k, and make sure it's unsigned */
	count &= 0xffff;
	if (count < 1)
		return -EINVAL;
	
	for(;;) 
	{
/* grab the lock and only get status of channel : outreadbuf */
		spin_lock_irqsave(&chan->lock, flags);

		res = chan->outreadbuf;
		
#if AS_POLICY_ENBALE	
		if (chan->rxdisable)
			res = -1;
#endif		
		spin_unlock_irqrestore(&chan->lock, flags);
		if (res >= 0) /* get the read result */
			break;
		
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		/* block this process */
		rv = as_schedule_delay(&chan->readbufq);

		/* this process is scheduled to run again */
		if (rv) /* error ,return */
			return (rv);
	}
	
	amnt = count;

	/* copy the data from a buffer block which is pointed by chan->outreadbuf into user space */
	if (amnt > chan->readn[chan->outreadbuf]) 
		amnt = chan->readn[chan->outreadbuf];
	if (amnt) 
	{
		if (copy_to_user(usrbuf, chan->readbuf[chan->outreadbuf], amnt))
			return -EFAULT;
	}

	/* modify the info of buffer block pointed by outreadbuf */
	spin_lock_irqsave(&chan->lock, flags);
	chan->readidx[chan->outreadbuf] = 0;
	chan->readn[chan->outreadbuf] = 0;

	/* repoint the outreadbuf to a usable buffer block */
	oldbuf = chan->outreadbuf;
	chan->outreadbuf = (chan->outreadbuf + 1) % chan->numbufs;
	
	if (chan->outreadbuf == chan->inreadbuf) 
	{
		/* Out of stuff */
		chan->outreadbuf = -1;
#if AS_POLICY_ENBALE	
		if (chan->rxbufpolicy == AS_POLICY_WHEN_FULL)
			chan->rxdisable = 1;
#endif		
	}
	
	if (chan->inreadbuf < 0) 
	{
		/* Notify interrupt handler that we have some space now */
		chan->inreadbuf = oldbuf;
	}
	spin_unlock_irqrestore(&chan->lock, flags);
	
	return amnt;
}

ssize_t as_file_read(struct file *file, char *usrbuf, size_t count, loff_t *ppos)
{
	int unit = UNIT(file);

	if (count < 0)
		return -EINVAL;

	return __as_channel_read(file, usrbuf, count, unit);
}

/* copy data from user space into channel's write buffers */
static ssize_t __as_chan_write(struct file *file, const char *usrbuf, size_t count, int unit)
{
	unsigned long flags;
	int res, amnt, oldbuf, rv; 
	struct as_dev_chan *chan = as_span_get_channel(unit) ;
	if (!chan) 
		return -EINVAL;
	
	/* Make sure count never exceeds 65k, and make sure it's unsigned */
	count &= 0xffff;
	if (count < 1)
		return -EINVAL;

/* check the write buffer are available, otherwise wait some time later and */	
	for(;;) 
	{
		spin_lock_irqsave(&chan->lock, flags);

		res = chan->inwritebuf;
//		if (chan->txdisable)
//			res = -1;
		spin_unlock_irqrestore(&chan->lock, flags);
		if (res >= 0) 
			break;
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		/* Wait for something to be available, user process is blocked */
		rv = as_schedule_delay(&chan->writebufq);

		if (rv)/*some error, then return */
			return rv;
	}

	amnt = count;
	if (amnt > chan->blocksize)
		amnt = chan->blocksize;
#if CONFIG_AS_DEBUG
	printk("__as_chan_write(unit: %d, inwritebuf: %d, outwritebuf: %d amnt: %d\n", 
		unit, chan->inwritebuf, chan->outwritebuf, amnt);
#endif

	if (amnt) 
	{	/* copy data from user process into buffer block pointed by inwritebuf */
		copy_from_user(chan->writebuf[chan->inwritebuf], usrbuf, amnt);

		/* modify the info about this buffer block : byte number and index  */
		chan->writen[chan->inwritebuf] = amnt;
		chan->writeidx[chan->inwritebuf] = 0;

		oldbuf = chan->inwritebuf;
		spin_lock_irqsave(&chan->lock, flags);
		/* next position for copy data from user space */
		chan->inwritebuf = (chan->inwritebuf + 1) % chan->numbufs;
		if (chan->inwritebuf == chan->outwritebuf) 
		{
			/* Don't stomp on the transmitter, just wait for them to wake us up */
			chan->inwritebuf = -1;
			/* Make sure the transmitter is transmitting in case of POLICY_WHEN_FULL */
//			chan->txdisable = 0;
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


ssize_t as_file_write(struct file *file, const char *usrbuf, size_t count, loff_t *ppos)
{
	int unit = UNIT(file);
	
	if (count < 0)
		return -EINVAL;

	return __as_chan_write(file, usrbuf, count, unit);
}

