/*
* $Id: mesh_dev_sche.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "mesh.h"

static void _mesh_dev_watchdog(unsigned long arg)
{
	MESH_DEVICE *dev = (MESH_DEVICE *)arg;

	spin_lock(&dev->xmit_lock);
#if 0	
	if (dev->qdisc != &noop_qdisc)
#endif		
	{
		if (meshif_device_present(dev) && meshif_running(dev) &&
			meshif_carrier_ok(dev))
		{
			if (meshif_queue_stopped(dev) &&
			    (jiffies - dev->trans_start) > dev->watchdog_timeo)
			{
				printk(KERN_INFO "NETDEV WATCHDOG: %s: transmit timed out\n", dev->name);
				dev->tx_timeout(dev);
			}

			if (!mod_timer(&dev->watchdog_timer, jiffies + dev->watchdog_timeo))
				mesh_dev_hold(dev);
		}
	}
	spin_unlock(&dev->xmit_lock);

//	dev_put(dev);
}

static void _mesh_dev_watchdog_init(MESH_DEVICE *dev)
{
	init_timer(&dev->watchdog_timer);
	dev->watchdog_timer.data = (unsigned long)dev;
	dev->watchdog_timer.function = _mesh_dev_watchdog;
}

void __meshdev_watchdog_up(MESH_DEVICE *dev)
{
	if (dev->tx_timeout)
	{
		if (dev->watchdog_timeo <= 0)
			dev->watchdog_timeo = 5*HZ;
		if (!mod_timer(&dev->watchdog_timer, jiffies + dev->watchdog_timeo))
		{
			mesh_dev_hold(dev);
		}	
	}
}

static void mesh_dev_watchdog_up(MESH_DEVICE *dev)
{
	spin_lock_bh(&dev->xmit_lock);
	__meshdev_watchdog_up(dev);
	spin_unlock_bh(&dev->xmit_lock);
}

static void mesh_dev_watchdog_down(MESH_DEVICE *dev)
{
	spin_lock_bh(&dev->xmit_lock);
	if (del_timer(&dev->watchdog_timer))
	{
		__mesh_dev_put(dev);
	}	
	spin_unlock_bh(&dev->xmit_lock);
}

EXPORT_SYMBOL(softmesh_datas);
EXPORT_SYMBOL(__meshdev_watchdog_up);

