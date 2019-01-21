/*
 * $Author: lizhijie $
 * $Log: as_dev_event_queue.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:46  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:04  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:51  fengshikui
 * ĞŞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:09  fengshikui
 * ĞŞ¸Ä°æ
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"

/* enqueue an event on a channel */
void __as_channel_qevent(struct as_dev_chan *chan, int event)
{
	  /* if full, ignore */
	if ((chan->eventoutidx == 0) && (chan->eventinidx == (AS_MAX_EVENTSIZE - 1))) 
		return;
	
	  /* if full, ignore */
	if (chan->eventoutidx == (chan->eventinidx - 1)) 
		return;
	
	  /* save the event */
	chan->eventbuf[chan->eventinidx++] = event;
	  /* wrap the index, if necessary */
	if (chan->eventinidx >= AS_MAX_EVENTSIZE) 
		chan->eventinidx = 0;
	  /* wake em all up */
//	if (chan->iomask & ZT_IOMUX_SIGEVENT) 
//		wake_up_interruptible(&chan->eventbufq);
	
	wake_up_interruptible(&chan->readbufq);
	wake_up_interruptible(&chan->writebufq);
	wake_up_interruptible(&chan->sel);
	return;
}

void as_channel_qevent_nolock(struct as_dev_chan *chan, int event)
{
	__as_channel_qevent(chan, event);
}

void as_channel_qevent_lock(struct as_dev_chan *chan, int event)
{
	unsigned long flags;
	spin_lock_irqsave(&chan->lock, flags);
	__as_channel_qevent(chan, event);
	spin_unlock_irqrestore(&chan->lock, flags);
}

int as_channel_deqevent_lock(struct as_dev_chan *chan )
{
	unsigned long flags;
	int event = AS_EVENT_NONE;
	
	spin_lock_irqsave(&chan->lock, flags);

	/* if some event in queue */
	if (chan->eventinidx != chan->eventoutidx)
	{
		event = chan->eventbuf[chan->eventoutidx++];

		if (chan->eventoutidx >= AS_MAX_EVENTSIZE)
			chan->eventoutidx = 0;
   	}		
	spin_unlock_irqrestore(&chan->lock, flags);

	return event;
}


