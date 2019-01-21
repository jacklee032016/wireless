/*
 * $Revision: 1.1.1.1 $
 * $Log: as_dev_file_ctl.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/06/27 06:02:08  lizhijie
 * no message
 *
 * $Author: lizhijie $
 * $Id: as_dev_file_ctl.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

#include "asstel.h"

static void  __as_channel_ctl_flush(struct as_dev_chan *chan,  int mode)
{
	int j;
	unsigned long flags;
	
	spin_lock_irqsave(&chan->lock, flags);
	if (mode & AS_FLUSH_READ)  /* if for read (input) */
	{
	  /* initialize read buffers and pointers */
		chan->inreadbuf = 0;
		chan->outreadbuf = -1;
		for (j=0;j<chan->numbufs;j++) 
		{
				/* Do we need this? */
			chan->readn[j] = 0;
			chan->readidx[j] = 0;
		}
		wake_up_interruptible(&chan->readbufq);  /* wake_up_interruptible waiting on read */
		wake_up_interruptible(&chan->sel); /* wake_up_interruptible waiting on select */
   	}
			
	if (mode & AS_FLUSH_WRITE) /* if for write (output) */
	{
		chan->outwritebuf = -1;
		chan->inwritebuf = 0;
		for (j=0;j<chan->numbufs;j++) 
		{
				/* Do we need this? */
			chan->writen[j] = 0;
			chan->writeidx[j] = 0;
		}
		wake_up_interruptible(&chan->writebufq); /* wake_up_interruptible waiting on write */
		wake_up_interruptible(&chan->sel);  /* wake_up_interruptible waiting on select */
	}
	
	spin_unlock_irqrestore(&chan->lock, flags);

}

static int __as_channel_ctl_set_gain(struct as_dev_chan *chan, unsigned long data)
{
	int  res;
	struct as_gains gain;
	
	if ( !chan ) 
		return(-EINVAL);
	
	if (copy_from_user(&gain,(struct as_gains *) data,sizeof(gain)))
		return -EIO;
		
	res = as_channel_set_gain( chan, &gain);	
	return res;
}

static int __as_channel_ctl_common(struct inode *node, struct file *file, unsigned int cmd, unsigned long data, int unit)
{
	struct as_dev_chan *chan = as_span_get_channel(unit);
	if (!chan) 
		return -EINVAL;

	switch(cmd) 
	{
		case AS_CTL_SETGAINS:  /* set gain stuff */
			return __as_channel_ctl_set_gain( chan, data);
			break;
		
		default:
			return -ENOTTY;
	}

	return 0;
}

int  as_file_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data)
{
	int i;
	int unit = UNIT(file);

	struct as_dev_chan *chan =as_span_get_channel(unit);
	if (!chan)
		return -EINVAL;

	switch(cmd) 
	{
		/*case 7 */	
		case AS_CTL_FLUSH:  /* flush input buffer, output buffer, and/or event queue */
		{
			get_user(i,(int *)data);  /* get param */
			__as_channel_ctl_flush( chan, i);
			break;

		}
		
		default:
		{/* Check for common ioctl's and private ones */
			return  __as_channel_ctl_common(inode, file, cmd, data, unit);
		}	
	}
	return 0;
}

