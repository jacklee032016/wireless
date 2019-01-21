/*
 * $Revision: 1.1.1.1 $
 * $Log: as_dev_file_open.c,v $
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
 * $Id: as_dev_file_open.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

#include "asstel.h"

/* added support for ZarLink by lizhijie, 2005.05.22*/
#if ZARLINK_SUPPORT	
	#include "as_zl.h"
#endif


/*  implement the file operation of open and release(close) for i-node */
static int __as_initialize_channel(struct as_dev_chan *chan)
{
	int res;
	unsigned long flags;

	if ((res = as_channel_reallocbufs(chan, AS_DEFAULT_BLOCKSIZE, AS_DEFAULT_NUM_BUFS)))
		return res;

	spin_lock_irqsave(&chan->lock, flags);

#if AS_POLICY_ENBALE	
	chan->rxbufpolicy = AS_POLICY_IMMEDIATE;
	chan->txbufpolicy = AS_POLICY_IMMEDIATE;

	chan->txdisable = 0;
	chan->rxdisable = 0;
#endif
#if 0
	init_waitqueue_head(&chan->sel);
	init_waitqueue_head(&chan->readbufq);
	init_waitqueue_head(&chan->writebufq);
#endif

	as_channel_set_default_gain( chan);

	spin_unlock_irqrestore(&chan->lock, flags);

	return 0;
}


/* set all fields in this channel as the defaults */
void as_close_channel(struct as_dev_chan *chan)
{
	unsigned long flags;

	as_channel_reallocbufs(chan, 0, 0); 
	spin_lock_irqsave(&chan->lock, flags);

#if 0
	init_waitqueue_head(&chan->sel);
	init_waitqueue_head(&chan->readbufq);
	init_waitqueue_head(&chan->writebufq);
#endif

	as_channel_set_default_gain( chan);

#if AS_POLICY_ENBALE	
/* added 2005.03.03 */
	chan->txdisable = 1;
	chan->rxdisable = 1;
/* end of added */	
#endif

	spin_unlock_irqrestore(&chan->lock, flags);

}

int as_file_open(struct inode *inode, struct file *file)
{
	int res = 0;
	int unit = UNIT(file);
	struct as_dev_chan *chan = as_span_get_channel(unit);

	if (chan )  /*&& chan->sig) no signal used in Analog PCM   */
	{
		/* Make sure we're not already open, a net device, or a slave device */
		if (chan->flags & AS_CHAN_FLAG_OPEN) 
			res = -EBUSY;
		else 
		{
			/* Assume everything is going to be okay */
			res = __as_initialize_channel( chan );
			if (!res) 
			{
				chan->file = file;
				MOD_INC_USE_COUNT;
				
				chan->flags |= AS_CHAN_FLAG_OPEN;

/* added support for ZarLink by lizhijie, 2005.05.22*/
#if ZARLINK_SUPPORT	
				zl_ch_bypass_disable(chan->channo );
#endif
			} 
			else 
			{
				as_close_channel( chan );
			}
		}
	}
	else
		res = -ENXIO;
	
	return res;
}


int  as_file_release(struct inode *inode, struct file *file)
{
	int res=0;
	struct as_dev_chan *chan;
	int unit = UNIT(file);

	chan = as_span_get_channel(unit);
	
	if (chan) 
	{
		chan->flags &= ~ AS_CHAN_FLAG_OPEN;
		chan->file = NULL;
		as_close_channel(chan );

/* added support for ZarLink by lizhijie, 2005.05.22*/
#if ZARLINK_SUPPORT	
		zl_ch_bypass_enable(chan->channo );
#endif
	} 
	else
		res = -ENXIO;

	MOD_DEC_USE_COUNT;

	return res;
}

unsigned int as_file_poll(struct file *file, struct poll_table_struct *wait_table)
{
	int	ret = 0;
	unsigned long flags;

	int unit = UNIT(file);

	struct as_dev_chan *chan = as_span_get_channel(unit);

	  /* do the poll wait */
	if (chan) 
	{
		poll_wait(file, &chan->sel, wait_table);
		ret = 0; /* start with nothing to return */
		spin_lock_irqsave(&chan->lock, flags);
		   /* if at least 1 write buffer avail */
		if (chan->inwritebuf > -1) 
		{
			ret |= POLLOUT | POLLWRNORM;
		}
#if AS_POLICY_ENBALE	
		if ((chan->outreadbuf > -1) && !chan->rxdisable) 
#else
		if (chan->outreadbuf > -1)
#endif			
		{
			ret |= POLLIN | POLLRDNORM;
		}

		spin_unlock_irqrestore(&chan->lock, flags);
	}
	else
		ret = -EINVAL;

	return(ret);  /* return what we found */
}

