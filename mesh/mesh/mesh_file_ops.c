/*
* $Id: mesh_file_ops.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
* device with minor is 0 is mesh mgr, other minor is the mesh_device, 
* eg. port device for every HW NIC
*/
#include "mesh.h"

static ssize_t swjtu_mesh_file_read(struct file *file, char *buf, size_t count, loff_t * off)
{
	mesh_inode	*dev = file->private_data;
	ssize_t		ret = 0;

	if (!dev)
		return(-ENODEV);
	down(&dev->inSema);

#if 0
	if (dev->index == SWJTU_DEVICE_MINOR_MGR)
	{/* core device */
		ret = _as_isdn_do_core_read(file, buf, count, off);
	}
	else
	{
		ret = _as_isdn_do_raw_read(file, buf, count, off);
	}
#endif	

	up(&dev->inSema);
	return(ret);
}

static ssize_t swjtu_mesh_file_write(struct file *file, const char *buf, size_t count, loff_t * off)
{
	mesh_inode	*dev = file->private_data;
	ssize_t		ret = 0;
//	u_long flags;

	if (!dev)
		return(-ENODEV);

	down(&dev->outSema);

	if (*off != file->f_pos)
	{
		return(-ESPIPE);
	}	

	if (!access_ok(VERIFY_WRITE, buf, count))
		return(-EFAULT);
	
	if (dev->index == SWJTU_DEVICE_MINOR_MGR )
	{
		if (count < 16)//AS_ISDN_HEADER_LEN)
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

#if 0
	if (dev->minor == SWJTU_DEVICE_MINOR_MGR)
		ret = _as_isdn_do_frame_write(file, buf, count, off);
	else
		ret = as_rawdev_do_write(file, buf, count, off);
#endif

	up(&dev->outSema);
	return(ret);
}

static loff_t  swjtu_mesh_file_llseek(struct file *file, loff_t offset, int orig)
{
	return -ESPIPE;
}

static int swjtu_mesh_file_close(struct inode *ino, struct file *filep)
{
	mesh_inode		*dev = filep->private_data;
	MESH_DEVICE	*port;

	dev->openMode &= ~filep->f_mode;

#if 0
	if (filep->f_mode & FMODE_READ)
	{
		test_and_clear_bit(FLG_AS_ISDNPORT_OPEN,  &dev->rport.Flag);
	}
	if (filep->f_mode & FMODE_WRITE)
	{
		test_and_clear_bit(FLG_AS_ISDNPORT_OPEN, &dev->wport.Flag);
	}
#endif

	filep->private_data = NULL;
	if (dev->index != SWJTU_DEVICE_MINOR_MGR )
	{
		port = (MESH_DEVICE *)dev;
#if 0		
		rawDev->eventinidx = 0;
		rawDev->eventoutidx = 0;

		if( rawDev->bch && rawDev->bch->file_ioctl)
		{
//			printk(KERN_ERR " minor(%d) is closing\n", dev->minor);
			(rawDev->bch->file_ioctl)( rawDev->bch, AS_ISDN_CTL_DEACTIVATE, 0);
		}
		
//		free_device(dev);
#endif
	}
	
	return 0;
}

static int swjtu_mesh_file_open(struct inode *ino, struct file *filep)
{
	u_int			minor = 	MINOR(ino->i_rdev);
	MESH_MGR 		*mgr = NULL;
	MESH_DEVICE 	*port = NULL;
	int 				mode;
	mesh_inode		*dev;

	if (minor != SWJTU_DEVICE_MINOR_MGR)
	{
		port = swjtu_get_port_from_mgr(minor);
		if ( port)
		{
			dev = &(port->dev);
			if (( dev->openMode & filep->f_mode) & (FMODE_READ | FMODE_WRITE))
			{
				MESH_WARN_INFO( " minor(%d) is busy\n", minor);
				return(-EBUSY);
			}
			else
			{
				filep->private_data = port;
				dev->openMode |= filep->f_mode & (FMODE_READ | FMODE_WRITE);
				mode = dev->openMode;
			}
		}
		else
		{
			MESH_DEBUG_INFO( MESH_DEBUG_MGR, "MESH device with minor %d is not exist\n", minor);
			return(-ENODEV);
		}	
	} 
	else //if (minor== SWJTU_DEVICE_MINOR_MGR) 
	{
		mgr = swjtu_get_mgr();
		filep->private_data = mgr;
		mgr->dev.openMode |= filep->f_mode & (FMODE_READ | FMODE_WRITE);
		mode = mgr->dev.openMode;
	}

#if 0	
	if (mode & FMODE_READ)
	{
//		printk(KERN_ERR " minor(%d) is READ\n", minor);
		skb_queue_purge( &(rport->queue) );
		rport->lock = SPIN_LOCK_UNLOCKED;
		rport->maxqlen = (minor==AS_ISDN_CORE_DEVICE)?DEFAULT_CORE_PORT_QUEUE_LENGTH:DEFAULT_RAW_POR_QUEUE_LENGTH;
		test_and_set_bit(FLG_AS_ISDNPORT_OPEN, &rport->Flag);
	}
	if (mode & FMODE_WRITE)
	{
//		printk(KERN_ERR " minor(%d) is WRITE\n", minor);
		skb_queue_purge(&(wport->queue) );
		wport->lock = SPIN_LOCK_UNLOCKED;
		wport->maxqlen = (minor==AS_ISDN_CORE_DEVICE)?DEFAULT_CORE_PORT_QUEUE_LENGTH:DEFAULT_RAW_POR_QUEUE_LENGTH;
		test_and_set_bit(FLG_AS_ISDNPORT_OPEN, &wport->Flag);
	}
#endif

	return(0);
}

static unsigned int swjtu_mesh_file_poll(struct file *file, poll_table * wait)
{
	unsigned int	mask = POLLERR;
	mesh_inode	*dev = file->private_data;
#if 0
	if (dev)
	{
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
#endif	
	return(mask);
}

static int  swjtu_mesh_file_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data)
{
	mesh_inode		*dev = file->private_data;
	ssize_t			ret = 0;

	if (!dev)
		return(-ENODEV);
	down(&dev->ioctrlSema);
	
	if(dev->index== SWJTU_DEVICE_MINOR_MGR )
	{
		MESH_MGR	*mgr = (MESH_MGR *)dev;
		if(mgr->do_ioctl)
			ret = mgr->do_ioctl(mgr, data, cmd);
		else
			ret = -ENODEV;
	}
	else
	{
		MESH_DEVICE *mdev;
		mdev = (MESH_DEVICE *)dev;
		mdev->do_ioctl(mdev, data, cmd );
	}
	
	up(&dev->ioctrlSema);
	
	return(ret);
}

struct file_operations swjtu_mesh_fops =
{
	llseek:		swjtu_mesh_file_llseek,
	read:		swjtu_mesh_file_read,
	write:		swjtu_mesh_file_write,
	poll:			swjtu_mesh_file_poll,
	ioctl:		swjtu_mesh_file_ioctl,
	open:		swjtu_mesh_file_open,
	release:		swjtu_mesh_file_close,
};

