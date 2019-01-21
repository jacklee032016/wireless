/*
* $Id: as_file_ops.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
*/

#include <linux/poll.h>

#include "core.h"

#include "bchannel.h"
#include "layer1.h"
#include "as_isdn_ctrl.h"

int  as_isdn_wdata_if(as_isdn_device_t *dev, struct sk_buff *skb);

static  __inline__ ssize_t  as_rawdev_do_write(struct file *file, const char *buf, size_t count, loff_t * off)
{
	as_isdn_rawdev_t	*dev = file->private_data;
	u_long			flags;
	struct sk_buff	*skb;
	struct sk_buff 	*header;

	 /* raw device */
//TRACE();
	skb = alloc_stack_skb(count, PORT_SKB_RESERVE);
	if (!skb)
	{
//			spin_unlock_irqrestore(&dev->wport.lock, flags);
		return(0);
	}

//		spin_unlock_irqrestore(&dev->wport.lock, flags);
	if (copy_from_user(skb_put(skb, count), buf, count))
	{
		dev_kfree_skb(skb);
		return(-EFAULT);
	}

	/* check precondition : wport.queue */	
	spin_lock_irqsave(&dev->wport.lock, flags);
	if(skb_queue_len(&dev->wport.queue) >= dev->wport.maxqlen)
	{
#if 1
			spin_unlock_irqrestore(&dev->wport.lock, flags);
			if (file->f_flags & O_NONBLOCK)
				return(-EAGAIN);
			printk(KERN_WARNING "wport is full for device %d\n", dev->minor);
			as_schedule_delay( &(dev->wport.procq));

			spin_lock_irqsave(&dev->wport.lock, flags);
#else			
		printk(KERN_WARNING "%s : queue in wport of device %d is fulled\n", __FUNCTION__ , dev->minor);
		header = skb_dequeue(&dev->wport.queue);
		dev_kfree_skb( header);
#endif		
	}
	skb_queue_tail(&dev->wport.queue, skb);

	spin_unlock_irqrestore(&dev->wport.lock, flags);
	
	if(!test_bit(BC_FLG_TX_BUSY, &dev->bch->Flag) )
	{
//		TRACE();
		bch_sched_event(dev->bch, B_XMTBUFREADY);
	}

//TRACE();
	
	return  count;
}


static __inline__ ssize_t _as_isdn_do_read(struct file *file, char *buf, size_t count, loff_t * off)
{
	as_isdn_device_t	*dev = file->private_data;
	size_t			len;
	u_long			flags;
	struct sk_buff	*skb;

	if (*off != file->f_pos)
		return(-ESPIPE);
	if (!access_ok(VERIFY_WRITE, buf, count))
		return(-EFAULT);
	if ((dev->minor == 0) && (count < AS_ISDN_HEADER_LEN))
	{
		printk(KERN_WARNING "AS_ISDN_read: count(%d) too small\n", count);
		return(-ENOSPC);
	}
	
#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR  "AS_ISDN_read: file(%d) %p max %d\n",dev->minor, file, count);
#endif

	spin_lock_irqsave(&dev->rport.lock, flags);
	
	while (skb_queue_empty(&dev->rport.queue))
	{
		spin_unlock_irqrestore(&dev->rport.lock, flags);
		if (file->f_flags & O_NONBLOCK)
			return(-EAGAIN);
#if 0		
		interruptible_sleep_on(&(dev->rport.procq));
		if (signal_pending(current))
			return(-ERESTARTSYS);
#else
		as_schedule_delay(&(dev->rport.procq));
#endif
		spin_lock_irqsave(&dev->rport.lock, flags);
	}
	len = 0;

	while ((skb = skb_dequeue(&dev->rport.queue)))
	{
		if (dev->minor == AS_ISDN_CORE_DEVICE)
		{/* core device */
			if ((skb->len + AS_ISDN_HEADER_LEN) > (count - len))
				goto nospace;
/*			
// jolly patch start
#warning note this: copy_*_user was called in locked context, so i unlock it. this workarround only works for single processor machines.
*/
			spin_unlock_irqrestore(&dev->rport.lock, flags);
			if (copy_to_user(buf, skb->cb, AS_ISDN_HEADER_LEN))
				goto efault;
			spin_lock_irqsave(&dev->rport.lock, flags);
// jolly patch stop
			len += AS_ISDN_HEADER_LEN;
			buf += AS_ISDN_HEADER_LEN;
		}
		else
		{/* raw device */
			if (skb->len > (count - len))
			{
nospace:
				skb_queue_head(&dev->rport.queue, skb);
				spin_unlock_irqrestore(&dev->rport.lock, flags);
				if (len)
					break;
				return(-ENOSPC);
			}
		}
		
		if (skb->len)
		{
/*		
// jolly patch start
#warning note this: copy_*_user was called in locked context, so i unlock it. this workarround only works for single processor machines.
*/
			spin_unlock_irqrestore(&dev->rport.lock, flags);
			if (copy_to_user(buf, skb->data, skb->len))
			{
efault:
				skb_queue_head(&dev->rport.queue, skb);
				return(-EFAULT);
			}
			spin_lock_irqsave(&dev->rport.lock, flags);
// jolly patch stop
			len += skb->len;
			buf += skb->len;
		}
		dev_kfree_skb(skb);
		if (test_bit(FLG_AS_ISDNPORT_ONEFRAME, &dev->rport.Flag))
			break;
	}
	*off += len;
	spin_unlock_irqrestore(&dev->rport.lock, flags);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR  "%s : file(%d) %d\n",__FUNCTION__,  dev->minor, len);
#endif

	return(len);
}

static __inline__ ssize_t  _as_isdn_do_frame_write(struct file *file, const char *buf, size_t count, loff_t * off)
{
	as_isdn_device_t	*dev = file->private_data;
	size_t			len =0;
	u_long			flags = 0;
	struct sk_buff	*skb;
	as_isdn_head_t	head;

//	TRACE();
	
	if (dev->minor == AS_ISDN_CORE_DEVICE)
	{/* core device */
//		TRACE();

		/* check precondition : wport.queue */	
		spin_lock_irqsave(&dev->wport.lock, flags);
		while (skb_queue_len(&dev->wport.queue) >= dev->wport.maxqlen)
		{
			spin_unlock_irqrestore(&dev->wport.lock, flags);
			if (file->f_flags & O_NONBLOCK)
				return(-EAGAIN);
			printk(KERN_WARNING "wport is full for device %d\n", dev->minor);
#if 0		
			interruptible_sleep_on(&(dev->wport.procq));
			if (signal_pending(current))
				return(-ERESTARTSYS);
#else
			as_schedule_delay( &(dev->wport.procq));
#endif
			spin_lock_irqsave(&dev->wport.lock, flags);
		}
		spin_unlock_irqrestore(&dev->wport.lock, flags);


		len = count;
		while (len >= AS_ISDN_HEADER_LEN)
		{
/*		
// jolly patch start
#warning note this: copy_*_user was called in locked context, so i unlock it. this workarround only works for single processor machines.
*/
//			spin_unlock_irqrestore(&dev->rport.lock, flags);
			if (copy_from_user(&head.addr, buf, AS_ISDN_HEADER_LEN))
			{
				return(-EFAULT);
			}
			spin_lock_irqsave(&dev->rport.lock, flags);
// jolly patch stop
			if (head.len > 0)
				skb = alloc_stack_skb((head.len > PORT_SKB_MINIMUM) ? head.len : PORT_SKB_MINIMUM, PORT_SKB_RESERVE);
			else
				skb = alloc_stack_skb(PORT_SKB_MINIMUM, PORT_SKB_RESERVE);
			if (!skb)
				break;
			
			memcpy(skb->cb, &head.addr, AS_ISDN_HEADER_LEN);
			len -= AS_ISDN_HEADER_LEN;
			buf += AS_ISDN_HEADER_LEN;

			if (head.len > 0)
			{
				if (head.len > len)
				{
					/* since header is complete we can handle this later */
/*					
// jolly patch start
#warning note this: copy_*_user was called in locked context, so i unlock it. this workarround only works for single processor machines.
*/
					spin_unlock_irqrestore(&dev->rport.lock, flags);
					if (copy_from_user(skb_put(skb, len), buf, len))
					{
						dev_kfree_skb(skb);
						return(-EFAULT);
					}
					spin_lock_irqsave(&dev->rport.lock, flags);
// jolly patch stop
					len = 0;
				}
				else
				{
/*				
// jolly patch start
#warning note this: copy_*_user was called in locked context, so i unlock it. this workarround only works for single processor machines.
*/
					spin_unlock_irqrestore(&dev->rport.lock, flags);
					if (copy_from_user(skb_put(skb, head.len), buf, head.len))
					{
						dev_kfree_skb(skb);
						return(-EFAULT);
					}
					spin_lock_irqsave(&dev->rport.lock, flags);
// jolly patch stop
					len -= head.len;
					buf += head.len;
				}
			}
			/* added in the tail of wport.queue */
			skb_queue_tail(&dev->wport.queue, skb);
		}

		
		if (len)
			printk(KERN_WARNING "%s: incomplete frame data (%d/%d)\n", __FUNCTION__, len, count);
		if (test_and_set_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag))
		{
			printk(KERN_WARNING "%s FLG_AS_ISDNPORT_BUSY for this device \n", __FUNCTION__);
			spin_unlock_irqrestore(&dev->wport.lock, flags);
			return(count-len);
		}

		/* process from begin of wport->queue */
//		spin_unlock_irqrestore(&dev->wport.lock, flags);
		while ((skb = skb_dequeue(&dev->wport.queue)))
		{
			if ( as_isdn_wdata_if(dev, skb))
				dev_kfree_skb(skb);
			wake_up(&dev->wport.procq);
		}
		test_and_clear_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag);
		return(count - len);
	}

	return 0;
}

static ssize_t as_isdn_file_read(struct file *file, char *buf, size_t count, loff_t * off)
{
	as_isdn_device_t	*dev = file->private_data;
	ssize_t		ret;

	if (!dev)
		return(-ENODEV);
	down(&dev->in_sema);
	ret = _as_isdn_do_read(file, buf, count, off);
//	ret = _do_isdn_read_one_by_one(file, buf, count, off);
	up(&dev->in_sema);
	return(ret);
}

static ssize_t as_isdn_file_write(struct file *file, const char *buf, size_t count, loff_t * off)
{
	as_isdn_device_t	*dev = file->private_data;
	ssize_t		ret;
	u_long flags;

	if (!dev)
		return(-ENODEV);
	down(&dev->out_sema);

	if (*off != file->f_pos)
	{
		return(-ESPIPE);
	}	

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR  "AS_ISDN_write: file(%d) %p count %d queue(%d)\n", dev->minor, file, count, skb_queue_len(&dev->wport.queue));
#endif

//	TRACE();
	if (!access_ok(VERIFY_WRITE, buf, count))
		return(-EFAULT);
	
	if (dev->minor == 0)
	{
		if (count < AS_ISDN_HEADER_LEN)
			return(-EINVAL);
	}

#if 0
	/* Core device and raw device with different process */
	/* check precondition : wport.queue */	
	spin_lock_irqsave(&dev->wport.lock, flags);
	while (skb_queue_len(&dev->wport.queue) >= dev->wport.maxqlen)
	{
		spin_unlock_irqrestore(&dev->wport.lock, flags);
		if (file->f_flags & O_NONBLOCK)
			return(-EAGAIN);
		printk(KERN_WARNING "wport is full for device %d\n", dev->minor);
#if 0		
		interruptible_sleep_on(&(dev->wport.procq));
		if (signal_pending(current))
			return(-ERESTARTSYS);
#else
		as_schedule_delay( &(dev->wport.procq));
#endif
		spin_lock_irqsave(&dev->wport.lock, flags);
	}
	spin_unlock_irqrestore(&dev->wport.lock, flags);
#endif

	if (dev->minor == AS_ISDN_CORE_DEVICE)
		ret = _as_isdn_do_frame_write(file, buf, count, off);
	else
		ret = as_rawdev_do_write(file, buf, count, off);
	
	up(&dev->out_sema);
	return(ret);
}

static loff_t  as_isdn_file_llseek(struct file *file, loff_t offset, int orig)
{
	return -ESPIPE;
}

static int as_isdn_file_close(struct inode *ino, struct file *filep)
{
	as_isdn_device_t *dev = filep->private_data;
	as_isdn_rawdev_t  *rawDev;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR  "AS_ISDN: AS_ISDN_close %p %p\n", filep, filep->private_data);
#endif

	dev->open_mode &= ~filep->f_mode;

	if (filep->f_mode & FMODE_READ)
	{
		test_and_clear_bit(FLG_AS_ISDNPORT_OPEN,  &dev->rport.Flag);
	}
	if (filep->f_mode & FMODE_WRITE)
	{
		test_and_clear_bit(FLG_AS_ISDNPORT_OPEN, &dev->wport.Flag);
	}

	filep->private_data = NULL;
	if (dev->minor)
	{
		rawDev = (as_isdn_rawdev_t *)dev;
		rawDev->eventinidx = 0;
		rawDev->eventoutidx = 0;
//		free_device(dev);
	}
	
//	printk(KERN_WARNING "AS_ISDN: No private data while closing device\n");

	return 0;
}

static int as_isdn_file_open(struct inode *ino, struct file *filep)
{
	u_int		minor = 	MINOR(ino->i_rdev);
	as_isdn_rawdev_t 	*rawDev = NULL;
	as_isdn_device_t 	*dev = NULL;
	int 				mode;
	as_isdn_port_t 	*rport , *wport;

//	TRACE();
#if AS_ISDN_DEBUG_CORE_INIT
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR "file_open in: minor(%d) %p %p mode(%x)\n",	minor, filep, filep->private_data, filep->f_mode);
#endif

	if (minor)
	{
		rawDev = as_rawdev_get_by_minor(minor);
		if (rawDev)
		{
			if ((rawDev->open_mode & filep->f_mode) & (FMODE_READ | FMODE_WRITE))
			{
				return(-EBUSY);
			}
			else
			{
				filep->private_data = rawDev;
				rawDev->open_mode |= filep->f_mode & (FMODE_READ | FMODE_WRITE);
				mode = rawDev->open_mode;
				rport = &rawDev->rport;
				wport = &rawDev->wport;
			}
		}
		else
			return(-ENODEV);
	} 
	else if (minor== 0) /* mainDev*/
	{
		dev = &mainDev;
		filep->private_data = dev;
		dev->open_mode |= filep->f_mode & (FMODE_READ | FMODE_WRITE);
		mode = dev->open_mode;
		rport = &dev->rport;
		wport = &dev->wport;
	}
	else
		return(-ENODEV);
	
	if (mode & FMODE_READ)
	{
		rport->lock = SPIN_LOCK_UNLOCKED;
		rport->maxqlen = (minor==0)?DEFAULT_CORE_PORT_QUEUE_LENGTH:DEFAULT_RAW_POR_QUEUE_LENGTH;
		test_and_set_bit(FLG_AS_ISDNPORT_OPEN, &rport->Flag);
	}
	if (mode & FMODE_WRITE)
	{
		wport->lock = SPIN_LOCK_UNLOCKED;
		wport->maxqlen = (minor==0)?DEFAULT_CORE_PORT_QUEUE_LENGTH:DEFAULT_RAW_POR_QUEUE_LENGTH;
		test_and_set_bit(FLG_AS_ISDNPORT_OPEN, &wport->Flag);
	}
	
#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR  "file_open out: %p %p\n", filep, filep->private_data);
#endif

	return(0);
}

static unsigned int as_isdn_file_poll(struct file *file, poll_table * wait)
{
	unsigned int	mask = POLLERR;
	/* as_isdn_rawdev_t has same fields in the first few fields */
	as_isdn_device_t	*dev = file->private_data;
	as_isdn_port_t	*rport = (file->f_mode & FMODE_READ) ? &dev->rport : NULL;
	as_isdn_port_t	*wport = (file->f_mode & FMODE_WRITE) ? &dev->wport : NULL;

	if (dev)
	{

#if AS_ISDN_DEBUG_CORE_DATA
		if (device_debug & DEBUG_DEV_OP)
			printk(KERN_ERR  "AS_ISDN_poll in: file(%d) %p\n", dev->minor, file);
#endif
		
		if (rport)
		{
			poll_wait(file, &rport->procq, wait);
			mask = 0;
			if (!skb_queue_empty(&rport->queue))
				mask |= (POLLIN | POLLRDNORM);
		}
		
		if (wport)
		{
			poll_wait(file, &wport->procq, wait);
			if (mask == POLLERR)
				mask = 0;
			if (skb_queue_len(&wport->queue) < wport->maxqlen)
				mask |= (POLLOUT | POLLWRNORM);
		}
	}
	
#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR  "AS_ISDN_poll out: file %p mask %x\n", file, mask);
#endif

	return(mask);
}

static int  as_isdn_file_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data)
{
	as_isdn_rawdev_t	*dev = file->private_data;
#if 0	
	as_isdn_port_t	*rport = (file->f_mode & FMODE_READ) ? &dev->rport : NULL;
	as_isdn_port_t	*wport = (file->f_mode & FMODE_WRITE) ? &dev->wport : NULL;
#endif
	ssize_t		ret = 0;

	if (!dev)
		return(-ENODEV);
	
	if(dev->minor==0)
		return (-EPERM);
	
	down(&dev->ioctrl_sema);

	if( dev->bch && dev->bch->file_ioctl)
	{
//		printk(KERN_ERR "B Channel %d is registered in raw device %d\n" ,dev->bch->channel, dev->minor);
		ret = (dev->bch->file_ioctl)(dev->bch, cmd, data);
	}
	else
	{
		printk(KERN_ERR "No B Channel is registered in raw device %d\n" ,dev->minor);
		ret =-ENODEV;
	}	
	
	up(&dev->ioctrl_sema);
	
	return(ret);
}

struct file_operations as_isdn_fops =
{
	llseek:		as_isdn_file_llseek,
	read:		as_isdn_file_read,
	write:		as_isdn_file_write,
	poll:		as_isdn_file_poll,
	ioctl:		as_isdn_file_ioctl,
	open:		as_isdn_file_open,
	release:		as_isdn_file_close,
};


