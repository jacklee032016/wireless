/*
 * $Author: lizhijie $
 * $Log: as_dev_file_poll.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/07/07 02:48:33  wangwei
 * no message
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:45  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:04  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:51  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:09  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"

/* device poll routine */

unsigned int as_file_poll(struct file *file, struct poll_table_struct *wait_table)
{
	int unit = UNIT(file);
	int	ret = 0;
	unsigned long flags;
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
		if ((chan->outreadbuf > -1) && !chan->rxdisable) 
		{
			ret |= POLLIN | POLLRDNORM;
		}
		if (chan->eventoutidx != chan->eventinidx)
		{/* Indicate an exception */
			ret |= POLLPRI;
		}
		spin_unlock_irqrestore(&chan->lock, flags);
	}
	else
		ret = -EINVAL;

	return(ret);  /* return what we found */

}

