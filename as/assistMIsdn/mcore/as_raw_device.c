#include <linux/module.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>
#include <linux/config.h>
#include <linux/init.h>

#include "core.h"

#include <linux/slab.h>

#include "bchannel.h"
#include "as_isdn_ctrl.h"

static LIST_HEAD(rawDeviceList);
static rwlock_t		rawDeviceLock = RW_LOCK_UNLOCKED;

/* enqueue an event on a B channel */
void __as_rawdev_qevent(as_isdn_rawdev_t *dev, int event)
{
	  /* if full, ignore */
	if (( dev->eventoutidx == 0) && (dev->eventinidx == (AS_ISDN_MAX_EVENTSIZE - 1))) 
		return;
	
	  /* if full, ignore */
	if ( dev->eventoutidx == (dev->eventinidx - 1)) 
		return;
	
	  /* save the event */
	dev->eventbuf[dev->eventinidx++] = event;
	  /* wrap the index, if necessary */
	if (dev->eventinidx >= AS_ISDN_MAX_EVENTSIZE) 
		dev->eventinidx = 0;
	  /* wake em all up */
//	if (chan->iomask & ZT_IOMUX_SIGEVENT) 
//		wake_up_interruptible(&dev->eventProcQ);
	
	wake_up_interruptible(&dev->rport.procq);
	wake_up_interruptible(&dev->wport.procq);
//	wake_up_interruptible(&dev->sel);
	return;
}

void as_rawdev_qevent_nolock( as_isdn_rawdev_t *dev, int event)
{
	__as_rawdev_qevent( dev, event);
}

void as_rawdev_qevent_lock( as_isdn_rawdev_t *dev, int event)
{
//	unsigned long flags;
//	spin_lock_irqsave(&dev->lock, flags);
	__as_rawdev_qevent(dev, event);
//	spin_unlock_irqrestore(&dev->lock, flags);
}

int as_rawdev_deqevent_lock( as_isdn_rawdev_t *dev )
{
//	unsigned long flags;
	int event = AS_EVENT_NONE;
	
//	spin_lock_irqsave(&dev->lock, flags);

	/* if some event in queue */
	if ( dev->eventinidx != dev->eventoutidx)
	{
		event = dev->eventbuf[dev->eventoutidx++];

		if ( dev->eventoutidx >= AS_ISDN_MAX_EVENTSIZE)
			dev->eventoutidx = 0;
   	}		
//	spin_unlock_irqrestore(&dev->lock, flags);

	return event;
}

/* construct minor device as the need of open file */
static as_isdn_rawdev_t *_as_rawdev_create(u_int minor)
{
	as_isdn_rawdev_t	*dev;
	u_long		flags;

	dev = kmalloc(sizeof(as_isdn_rawdev_t), GFP_KERNEL);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: dev(%d) %p\n",	__FUNCTION__, minor, dev); 
#endif

	if (dev)
	{
		memset(dev, 0, sizeof(as_isdn_device_t));
		dev->minor = minor;
		init_waitqueue_head(&dev->rport.procq);
		init_waitqueue_head(&dev->wport.procq);

		init_waitqueue_head(&dev->eventProcQ);
		
		skb_queue_head_init(&dev->rport.queue);
		skb_queue_head_init(&dev->wport.queue);
		
		init_MUTEX(&dev->in_sema);
		init_MUTEX(&dev->out_sema);
		init_MUTEX(&dev->ioctrl_sema);

#if 0		
		INIT_LIST_HEAD(&dev->dev.layerlist);
		INIT_LIST_HEAD(&dev->dev.stacklist);
		INIT_LIST_HEAD(&dev->dev.timerlist);
		INIT_LIST_HEAD(&dev->dev.entitylist);
#endif		
		write_lock_irqsave(&rawDeviceLock, flags);
		list_add_tail(&dev->list, &rawDeviceList);
		write_unlock_irqrestore(&rawDeviceLock, flags);
	}
	
	return(dev);
}


static int _as_rawdev_free(as_isdn_rawdev_t *dev)
{
//	struct list_head *item, *ni;
	u_long	flags;

	if (!dev)
		return(-ENODEV);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: dev(%d)\n", __FUNCTION__, dev->minor);
#endif

#if 0
	/* release related stuff */
	list_for_each_safe(item, ni, &dev->layerlist)
		del_layer(list_entry(item, devicelayer_t, list));

	list_for_each_safe(item, ni, &dev->stacklist)
		del_stack(list_entry(item, devicestack_t, list));
	
	list_for_each_safe(item, ni, &dev->timerlist)
		dev_free_timer(list_entry(item, as_isdn_timer_t, list));
#endif

	if (!skb_queue_empty(&dev->rport.queue))
		discard_queue(&dev->rport.queue);
	
	if (!skb_queue_empty(&dev->wport.queue))
		discard_queue(&dev->wport.queue);
	
	write_lock_irqsave(&rawDeviceLock, flags);
	list_del(&dev->list);
	write_unlock_irqrestore(&rawDeviceLock, flags);

#if 0
	if (!list_empty(&dev->dev.entitylist))
	{
		printk(KERN_WARNING "AS_ISDN %s: entitylist not empty\n", __FUNCTION__);
		list_for_each_safe(item, ni, &dev->dev.entitylist)
		{
			struct entity_item *ei = list_entry(item, struct entity_item, head);
			list_del(item);
			isdn_delete_entity(ei->entity);
			kfree(ei);
		}
	}
#endif

	kfree(dev);
	return(0);
}

as_isdn_rawdev_t *as_rawdev_get_by_minor(int minor)
{
	as_isdn_rawdev_t	*dev;

	read_lock(&rawDeviceLock);
	list_for_each_entry(dev, &rawDeviceList, list)
	{
		if (dev->minor == minor)
		{
			read_unlock(&rawDeviceLock);
			return(dev);
		}
	}
	read_unlock(&rawDeviceLock);
	return(NULL);
}


static as_isdn_rawdev_t *_as_rawdev_get_free(void)
{
	as_isdn_rawdev_t	*dev;
	u_int		minor;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s:\n", __FUNCTION__);
#endif
	
	for (minor=AS_ISDN_MINOR_RAW_MIN; minor<=AS_ISDN_MINOR_RAW_MAX; minor++)
	{
		dev = as_rawdev_get_by_minor(minor);

#if AS_ISDN_DEBUG_CORE_DATA
		if (device_debug & DEBUG_MGR_FUNC)
			printk(KERN_ERR  "%s: dev(%d) %p\n",	__FUNCTION__, minor, dev); 
#endif

		if (!dev) 
		{
			dev = _as_rawdev_create(minor);
			if (!dev)
				return(NULL);
			
			/* rport is not used in new framework , lizhijie,2006.01.01 */
#if 0			
			printk(KERN_ERR "read port function for raw device %d is inited\n", minor );

			dev->rport.pif.func = as_rawdev_do_read_data_old;
			dev->rport.pif.fdata = dev;
#endif
			return(dev);
		}
	}
	return(NULL);
}


void as_rawdev_free_all(void )
{
	as_isdn_rawdev_t	*dev, *nd;
	
	if (!list_empty(&rawDeviceList))
	{
		printk(KERN_WARNING "AS ISDN: devices open on remove\n");
		list_for_each_entry_safe(dev, nd, &rawDeviceList, list)
		{
			_as_rawdev_free(dev);
		}
	}

	return ;
}

/* after registered, this channel is closed */
int as_isdn_register_bchannel(bchannel_t *bchan)
{
	as_isdn_rawdev_t *rawDev = NULL;
#if 0	
	unsigned long flags;
	char tempfile[AS_PROC_NAME_LENGTH];
	int i;
#endif
	int res = 0;
	
	if(!bchan)
		return -ENODEV;

	rawDev = _as_rawdev_get_free();
	if( !rawDev)
		return -ENOENT;

	bchan->dev = rawDev;
	rawDev->bch = bchan;
	
//	printk("ASSIST ISDN : %d channels are available\n", master_span.count );

#if 0
	sprintf(tempfile, "%s/%d", AS_PROC_DIR_NAME, bchan->channel+ AS_ISDN_RAW_DEVICE);
	as_proc_entries[bchan->channnel] = create_proc_read_entry(tempfile, 0444, NULL , __as_tel_proc_read_channel, (int *)(long)chan );
	
	/* added for multi-card suppport , lizhjie 2004.11.25 */
	if( (!master_span.private) &&  chan->private )
	{
		master_span.private = chan->private;
#if AS_DEBUG_TIGER
		sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, AS_PROC_CARD_NAME );
		as_proc_module_entry = create_proc_read_entry(tempfile, 0444, NULL , __as_tel_proc_read_module,  master_span.private );
#endif
	}
#endif

	return res;
}


void as_isdn_unregister_bchannel(bchannel_t *bchan)
{
	as_isdn_rawdev_t *rawDev = NULL;
#if 0
	unsigned long flags;
	char tempfile[AS_PROC_NAME_LENGTH];
#endif

	if(!bchan)
		return ;//-ENOENT;

	rawDev = bchan->dev;
	if(!rawDev)
		return ;//-ENODEV;
	
	bchan->dev = NULL;
	_as_rawdev_free( rawDev);
	
#if 0
	/* added for multi-cards support ,lizhijie 2004.11.25 */
	if(master_span.count ==0)
	{
#if AS_DEBUG_TIGER
		sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, AS_PROC_CARD_NAME );
		remove_proc_entry(tempfile, NULL);
#endif
		master_span.private = NULL;
	}
	
//	write_unlock_irqrestore(&chan_lock, flags);
#endif
	return;
}

EXPORT_SYMBOL(as_isdn_register_bchannel);
EXPORT_SYMBOL(as_isdn_unregister_bchannel);

