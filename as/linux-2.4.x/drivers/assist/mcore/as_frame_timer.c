/*
* $Id: as_frame_timer.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
*/
#include <linux/stddef.h>
#include <linux/timer.h>
#include "core.h"

static as_isdn_timer_t *get_devtimer(as_isdn_device_t *dev, int id)
{
	as_isdn_timer_t	*ht;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_TIMER)
		printk(KERN_ERR  "%s: dev:%p id:%x\n", __FUNCTION__, dev, id);
#endif
	list_for_each_entry(ht, &dev->timerlist, list)
	{
		if (ht->id == id)
			return(ht);
	}
	return(NULL);
}


void dev_expire_timer(as_isdn_timer_t *ht)
{
	struct sk_buff	*skb;
	as_isdn_head_t	*hp;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_TIMER)
		printk(KERN_ERR  "%s: timer(%x)\n", __FUNCTION__, ht->id);
#endif

	if (test_and_clear_bit(FLG_MGR_TIMER_RUNING, &ht->Flags))
	{
		skb = alloc_stack_skb(16, 0);
		if (!skb)
		{
			printk(KERN_WARNING "%s: timer(%x) no skb\n", __FUNCTION__, ht->id);
			return;
		}
		hp = AS_ISDN_HEAD_P(skb);
		hp->dinfo = 0;
		hp->prim = MGR_TIMER | INDICATION;
		hp->addr = ht->id;
		hp->len = 0;
		if (as_isdn_rdata_frame(ht->dev, skb))
			dev_kfree_skb(skb);
	}
	else
		printk(KERN_WARNING "%s: timer(%x) not active\n", __FUNCTION__, ht->id);
}

int dev_init_timer(as_isdn_device_t *dev, u_int id)
{
	as_isdn_timer_t	*ht;

	ht = get_devtimer(dev, id);
	if (!ht)
	{
		ht = kmalloc(sizeof(as_isdn_timer_t), GFP_ATOMIC);
		if (!ht)
			return(-ENOMEM);
		ht->dev = dev;
		ht->id = id;
		ht->tl.data = (long) ht;
		ht->tl.function = (void *) dev_expire_timer;
		init_timer(&ht->tl);
		list_add_tail(&ht->list, &dev->timerlist);
		
#if AS_ISDN_DEBUG_CORE_DATA
		if (device_debug & DEBUG_DEV_TIMER)
			printk(KERN_ERR  "%s: new(%x)\n", __FUNCTION__, ht->id);
#endif		
	}

#if AS_ISDN_DEBUG_CORE_DATA
	else if (device_debug & DEBUG_DEV_TIMER)
		printk(KERN_ERR  "%s: old(%x)\n", __FUNCTION__, ht->id);
#endif

	if (timer_pending(&ht->tl))
	{
		printk(KERN_WARNING "%s: timer(%x) pending\n", __FUNCTION__, ht->id);
		del_timer(&ht->tl);
	}
	init_timer(&ht->tl);
	test_and_set_bit(FLG_MGR_TIMER_INIT, &ht->Flags);
	return(0);
}

int dev_add_timer(as_isdn_device_t *dev, as_isdn_head_t *hp)
{
	as_isdn_timer_t	*ht;

	ht = get_devtimer(dev, hp->addr);
	if (!ht)
	{
		printk(KERN_WARNING "%s: no timer(%x)\n", __FUNCTION__, 	hp->addr);
		return(-ENODEV);
	}
	if (timer_pending(&ht->tl))
	{
		printk(KERN_WARNING "%s: timer(%x) pending\n",	__FUNCTION__, ht->id);
		return(-EBUSY);
	}
	if (hp->dinfo < 10)
	{
		printk(KERN_WARNING "%s: timer(%x): %d ms too short\n",	__FUNCTION__, ht->id, hp->dinfo);
		return(-EINVAL);
	}
	
#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_TIMER)
		printk(KERN_ERR  "%s: timer(%x) %d ms\n",	__FUNCTION__, ht->id, hp->dinfo);
#endif

	init_timer(&ht->tl);
	ht->tl.expires = jiffies + (hp->dinfo * HZ) / 1000;
	test_and_set_bit(FLG_MGR_TIMER_RUNING, &ht->Flags);
	add_timer(&ht->tl);
	return(0);
}

/* delete this timer, not free it */
int dev_del_timer(as_isdn_device_t *dev, u_int id)
{
	as_isdn_timer_t	*ht;

	ht = get_devtimer(dev, id);
	if (!ht)
	{
		printk(KERN_WARNING "%s: no timer(%x)\n", __FUNCTION__, 	id);
		return(-ENODEV);
	}
	
#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_TIMER)
		printk(KERN_ERR  "%s: timer(%x)\n", __FUNCTION__, ht->id);
#endif
	
	del_timer(&ht->tl);
	if (!test_and_clear_bit(FLG_MGR_TIMER_RUNING, &ht->Flags))
		printk(KERN_WARNING "%s: timer(%x) not running\n", __FUNCTION__, ht->id);
	return(0);
}

void dev_free_timer(as_isdn_timer_t *ht)
{
#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_TIMER)
		printk(KERN_ERR  "%s: timer(%x)\n", __FUNCTION__, ht->id);
#endif

	del_timer(&ht->tl);
	list_del(&ht->list);
	kfree(ht);
}

int dev_remove_timer(as_isdn_device_t *dev, u_int id)
{
	as_isdn_timer_t	*ht;

	ht = get_devtimer(dev, id);
	if (!ht)
	{
		printk(KERN_WARNING "%s: no timer(%x)\n", __FUNCTION__, id);
		return(-ENODEV);
	}
	dev_free_timer(ht); 
	return(0);
}

