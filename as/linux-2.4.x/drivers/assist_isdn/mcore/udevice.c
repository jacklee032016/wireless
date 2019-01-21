/*
 * $Id: udevice.c,v 1.1.1.1 2006/11/29 08:55:14 lizhijie Exp $
 */

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/config.h>
#include <linux/timer.h>
/* added by lizhijie*/
#include <linux/init.h>

#include "core.h"

#define MAX_HEADER_LEN				4

#define FLG_MGR_SETSTACK			1
#define FLG_MGR_OWNSTACK			2

#define FLG_MGR_TIMER_INIT			1
#define FLG_MGR_TIMER_RUNING		2

#define AS_ISDN_DEV_NAME			"asisdn"

typedef struct _devicelayer
{
	struct list_head		list;
	AS_ISDNdevice_t		*dev;
	AS_ISDNinstance_t		inst;
	AS_ISDNinstance_t		*slave;
	AS_ISDNif_t			s_up;
	AS_ISDNif_t			s_down;
	int					iaddr;
	int					lm_st;
	u_long				Flags;
} devicelayer_t;

typedef struct _devicestack
{
	struct list_head	list;
	AS_ISDNdevice_t	*dev;
	AS_ISDNstack_t	*st;
	int				extentions;
} devicestack_t;

typedef struct _AS_ISDNtimer
{
	struct list_head		list;
	struct _AS_ISDNdevice	*dev;
	struct timer_list		tl;
	int					id;
	u_long				Flags;
} AS_ISDNtimer_t;

typedef struct entity_item
{
	struct list_head	head;
	int				entity;
} entity_item_t;

static LIST_HEAD(AS_ISDN_devicelist);
static rwlock_t	AS_ISDN_device_lock = RW_LOCK_UNLOCKED;

static AS_ISDNobject_t	udev_obj;

static int device_debug ;

static int from_up_down(AS_ISDNif_t *, struct sk_buff *);

// static int from_peer(AS_ISDNif_t *, u_int, int, int, void *);
// static int to_peer(AS_ISDNif_t *, u_int, int, int, void *);

#include <linux/slab.h>

/* sleep in user space until woken up. Equivilant of tsleep() in BSD */
int  as_schedule_delay(wait_queue_head_t *q)
{
	/* declare a wait_queue_t variable as 'wait', and add 'current' task into it */
	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue(q, &wait);
	current->state = TASK_INTERRUPTIBLE;

	if (!signal_pending(current)) 
		schedule();
	
	/* here, this task is waken up, and going to continue */
	current->state = TASK_RUNNING;
	remove_wait_queue(q, &wait);

	if (signal_pending(current)) 
		return -ERESTARTSYS;

	return(0);
}

static AS_ISDNdevice_t *get_device4minor(int minor)
{
	AS_ISDNdevice_t	*dev;

	read_lock(&AS_ISDN_device_lock);
	list_for_each_entry(dev, &AS_ISDN_devicelist, list)
	{
		if (dev->minor == minor)
		{
			read_unlock(&AS_ISDN_device_lock);
			return(dev);
		}
	}
	read_unlock(&AS_ISDN_device_lock);
	return(NULL);
}

/* this function is used as bch->dev->rport->pif which is called in BCH->bh handler 
* This function is added for accelebrate the data read and write loop execute both
* in BH context and write thread context, lizhijie, 2005.12.21
*/
int as_isdn_rdata_raw_data(AS_ISDNif_t *hif,  int prim, struct sk_buff *skb)
{
	AS_ISDNdevice_t	*dev;
	u_long		flags;
	int		retval = 0;

	if (!hif || !hif->fdata ||  !skb)
		return(-EINVAL);
	dev = hif->fdata;

	if ( prim == (PH_DATA | INDICATION))
	{/* case 1: Rx Data from low layer
	  put skb into dev->rport.queue, wakeup process wait on dev->read */
		if (test_bit(FLG_AS_ISDNPORT_OPEN, &dev->rport.Flag))
		{
			spin_lock_irqsave(&dev->rport.lock, flags);
			if (skb_queue_len(&dev->rport.queue) >= dev->rport.maxqlen)
			{
				printk(KERN_WARNING "%s : queue in rport is fulled\n", __FUNCTION__ );
				dev_kfree_skb(skb);/*added 09.16 */
				retval = -ENOSPC;
			}
			else
				skb_queue_tail(&dev->rport.queue, skb);
			
//	TRACE();
			spin_unlock_irqrestore(&dev->rport.lock, flags);
			wake_up_interruptible(&dev->rport.procq);
		}
		else
		{
			printk(KERN_WARNING "%s: PH_DATA_IND device(%d) not open for read\n", __FUNCTION__, dev->minor);
			dev_kfree_skb(skb);/*added 09.16 */
			retval = -ENOENT;
		}
	}
	else if ( prim == (PH_DATA | CONFIRM))
	{/* case 2 : Rx Data Confirm and transfer other data in wport 
		free skb, dequeue wport->queue into skb, call wport->if->func to handle skb 
		from wport->queue, then wakeup process wait on wport */
		test_and_clear_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag);

		spin_lock_irqsave(&dev->wport.lock, flags);
		if (test_and_set_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag))
		{
			spin_unlock_irqrestore(&dev->wport.lock, flags);
			return(0);
		}
		
		while ((skb = skb_dequeue(&dev->wport.queue)))
		{
#if AS_ISDN_DEBUG_CORE_DATA
			if (device_debug & DEBUG_DEV_OP)
				printk(KERN_ERR  "%s: wflg(%lx)\n", __FUNCTION__, dev->wport.Flag);
#endif

			if (test_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag))
			{
				skb_queue_head(&dev->wport.queue, skb); 
//				wake_up_interruptible(&dev->rport.procq);	/*added, lizhijie,2005.12.21 */
				break;
			}

			if (test_bit(FLG_AS_ISDNPORT_ENABLED, &dev->wport.Flag))
			{
				spin_unlock_irqrestore(&dev->wport.lock, flags);
				retval = if_newhead(&dev->wport.pif, PH_DATA | REQUEST, (int)skb, skb);
				spin_lock_irqsave(&dev->wport.lock, flags);
				if (retval)
				{
					printk(KERN_WARNING "%s: dev(%d) down err(%d)\n", __FUNCTION__, dev->minor, retval);
					dev_kfree_skb(skb);
				}
				else
				{
					test_and_set_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag);
					wake_up(&dev->wport.procq);
					break;
				}
			}
			else
			{
				printk(KERN_WARNING "%s: dev(%d) wport not enabled\n", __FUNCTION__, dev->minor);
				dev_kfree_skb(skb);
			}
			wake_up(&dev->wport.procq);
//	TRACE();
		}
		
		test_and_clear_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag);
		spin_unlock_irqrestore(&dev->wport.lock, flags);
	}
	else
	{
		printk(KERN_WARNING "%s: prim(%x) not supported\n",	__FUNCTION__, prim);
		retval = -EINVAL;
	}

//	TRACE();
	return(retval);
}


/* this function is used as bch->dev->rport->pif which is called in BCH->bh handler */
static int _isdn_rdata_raw(AS_ISDNif_t *hif, struct sk_buff *skb)
{
	AS_ISDNdevice_t	*dev;
	AS_ISDN_head_t	*hh;
	u_long		flags;
	int		retval = 0;

	if (!hif || !hif->fdata || !skb)
		return(-EINVAL);
	dev = hif->fdata;
	hh = AS_ISDN_HEAD_P(skb);

#if 0
	if (hh->prim == (PH_DATA | INDICATION))
	{/* case 1: Rx Data from low layer
	  put skb into dev->rport.queue, wakeup process wait on dev->read */
		if (test_bit(FLG_AS_ISDNPORT_OPEN, &dev->rport.Flag))
		{
			spin_lock_irqsave(&dev->rport.lock, flags);
			if (skb_queue_len(&dev->rport.queue) >= dev->rport.maxqlen)
			{
				printk(KERN_WARNING "%s : queue in rport is fulled\n", __FUNCTION__ );
				dev_kfree_skb(skb);/*added 09.16 */
				retval = -ENOSPC;
			}
			else
				skb_queue_tail(&dev->rport.queue, skb);
			
//	TRACE();
			spin_unlock_irqrestore(&dev->rport.lock, flags);
			wake_up_interruptible(&dev->rport.procq);
		}
		else
		{
			printk(KERN_WARNING "%s: PH_DATA_IND device(%d) not open for read\n", __FUNCTION__, dev->minor);
			dev_kfree_skb(skb);/*added 09.16 */
			retval = -ENOENT;
		}
	}
	else if (hh->prim == (PH_DATA | CONFIRM))
	{/* case 2 : Rx Data Confirm and transfer other data in wport 
		free skb, dequeue wport->queue into skb, call wport->if->func to handle skb 
		from wport->queue, then wakeup process wait on wport */
		test_and_clear_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag);

		dev_kfree_skb(skb);

		spin_lock_irqsave(&dev->wport.lock, flags);
		if (test_and_set_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag))
		{
			spin_unlock_irqrestore(&dev->wport.lock, flags);
			return(0);
		}
		
		while ((skb = skb_dequeue(&dev->wport.queue)))
		{
#if AS_ISDN_DEBUG_CORE_DATA
			if (device_debug & DEBUG_DEV_OP)
				printk(KERN_ERR  "%s: wflg(%lx)\n", __FUNCTION__, dev->wport.Flag);
#endif

			if (test_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag))
			{
				skb_queue_head(&dev->wport.queue, skb); 
				break;
			}

			if (test_bit(FLG_AS_ISDNPORT_ENABLED, &dev->wport.Flag))
			{
				spin_unlock_irqrestore(&dev->wport.lock, flags);
				retval = if_newhead(&dev->wport.pif, PH_DATA | REQUEST, (int)skb, skb);
				spin_lock_irqsave(&dev->wport.lock, flags);
				if (retval)
				{
					printk(KERN_WARNING "%s: dev(%d) down err(%d)\n", __FUNCTION__, dev->minor, retval);
					dev_kfree_skb(skb);
				}
				else
				{
					test_and_set_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag);
					wake_up(&dev->wport.procq);
					break;
				}
			}
			else
			{
				printk(KERN_WARNING "%s: dev(%d) wport not enabled\n", __FUNCTION__, dev->minor);
				dev_kfree_skb(skb);
			}
			wake_up(&dev->wport.procq);
//	TRACE();
		}
		
		test_and_clear_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag);
		spin_unlock_irqrestore(&dev->wport.lock, flags);
	}
	
	else 
#endif
	if ((hh->prim == (PH_ACTIVATE | CONFIRM)) ||(hh->prim == (PH_ACTIVATE | INDICATION))) 
	{/* no skb is provided in this primitive which is created in ashfc_signal.c */
		test_and_set_bit(FLG_AS_ISDNPORT_ENABLED, &dev->wport.Flag);
		test_and_clear_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag);
	}
	else if ((hh->prim == (PH_DEACTIVATE | CONFIRM)) ||(hh->prim == (PH_DEACTIVATE | INDICATION)))
	{/* no skb is provided in this primitive which is created in ashfc_signal.c */
		test_and_clear_bit(FLG_AS_ISDNPORT_ENABLED,	&dev->wport.Flag);
	}
	else
	{
		printk(KERN_WARNING "%s: prim(%x) dinfo(%x) not supported\n",	__FUNCTION__, hh->prim, hh->dinfo);
		retval = -EINVAL;
	}

//	TRACE();

	return(retval);
}

/* put this skb into dev->rport.queue and wakeup user process */
static int  _isdn_rdata(AS_ISDNdevice_t *dev, struct sk_buff *skb)
{
	AS_ISDN_head_t	*hp;
	u_long		flags;

	hp = AS_ISDN_HEAD_P(skb);
	if (hp->len <= 0)
		skb_trim(skb, 0);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_RDATA)
		printk(KERN_ERR  "%s: %x:%x %x %d %d\n", __FUNCTION__, hp->addr, hp->prim, hp->dinfo, hp->len, skb->len);
#endif

	spin_lock_irqsave(&dev->rport.lock, flags);
	if (skb_queue_len(&dev->rport.queue) >= dev->rport.maxqlen)
	{
		spin_unlock_irqrestore(&dev->rport.lock, flags);
		printk(KERN_WARNING "%s: rport queue overflow %d/%d\n",	__FUNCTION__, skb_queue_len(&dev->rport.queue), dev->rport.maxqlen);
		return(-ENOSPC);
	}
	skb_queue_tail(&dev->rport.queue, skb);
	spin_unlock_irqrestore(&dev->rport.lock, flags);

	wake_up_interruptible(&dev->rport.procq);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_RDATA)
		printk(KERN_ERR  "%s: %x:%x %x %d %d after wakeup\n", __FUNCTION__, hp->addr, hp->prim, hp->dinfo, hp->len, skb->len);
#endif

	return(0);
}

static int error_answer(AS_ISDNdevice_t *dev, struct sk_buff *skb, int err)
{
	AS_ISDN_head_t	*hp;

	hp = AS_ISDN_HEAD_P(skb);
	hp->prim |= 1; /* CONFIRM or RESPONSE */
	hp->len = err;
	return(_isdn_rdata(dev, skb));
}

static devicelayer_t *get_devlayer(AS_ISDNdevice_t *dev, int addr)
{
	devicelayer_t *dl;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: addr:%x\n", __FUNCTION__, addr);
#endif

	list_for_each_entry(dl, &dev->layerlist, list)
	{
//		if (device_debug & DEBUG_MGR_FUNC)
//			printk(KERN_ERR  "%s: dl(%p) iaddr:%x\n",
//				__FUNCTION__, dl, dl->iaddr);
		if ((u_int)dl->iaddr == (IF_IADDRMASK & addr))
			return(dl);
	}
	return(NULL);
}

static devicestack_t *get_devstack(AS_ISDNdevice_t *dev, int addr)
{
	devicestack_t *ds;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: addr:%x\n", __FUNCTION__, addr);
#endif
	
	list_for_each_entry(ds, &dev->stacklist, list)
	{
		if (ds->st && (ds->st->id == (u_int)addr))
			return(ds);
	}
	return(NULL);
}

static AS_ISDNtimer_t *get_devtimer(AS_ISDNdevice_t *dev, int id)
{
	AS_ISDNtimer_t	*ht;

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

/* set all instance flags of stack */
static int stack_inst_flg(AS_ISDNdevice_t *dev, AS_ISDNstack_t *st, int bit, int clear)
{
	int ret;
	devicelayer_t *dl;

	list_for_each_entry(dl, &dev->layerlist, list)
	{
		if (dl->inst.st == st)
		{
			if (clear)
				ret = test_and_clear_bit(bit, &dl->Flags);
			else
				ret = test_and_set_bit(bit, &dl->Flags);
			return(ret);
		}
	}
	return(-1);
}

/* execute in user process context */
static int new_devstack(AS_ISDNdevice_t *dev, stack_info_t *si)
{
	int		err;
	AS_ISDNstack_t	*st;
	AS_ISDNinstance_t	inst;
	devicestack_t	*nds;

	memset(&inst, 0, sizeof(AS_ISDNinstance_t));
	st = get_stack4id(si->id);
	if (si->extentions & EXT_STACK_CLONE)
	{
		if (st)
		{/* if CLONE is required, this stack must be exist */
			inst.st = st;
		}
		else
		{
			int_errtxt("ext(%x) st(%x)", si->extentions, si->id);
			return(-EINVAL);
		}
	}

	/* call new_stack, eg. master is null */
	err = udev_obj.ctrl(NULL, MGR_NEWSTACK | REQUEST, &inst);
	if (err)
	{
		int_error();
		return(err);
	}

	if (!(nds = kmalloc(sizeof(devicestack_t), GFP_ATOMIC)))
	{
		printk(KERN_ERR "kmalloc devicestack failed\n");
		udev_obj.ctrl(inst.st, MGR_DELSTACK | REQUEST, NULL);
		return(-ENOMEM);
	}
	memset(nds, 0, sizeof(devicestack_t));
	nds->dev = dev;
	if (si->extentions & EXT_STACK_CLONE)
	{
//		memcpy(&inst.st->pid, &st->pid, sizeof(AS_ISDN_pid_t));
		// FIXME that is a ugly idea, but I don't have a better one 
		inst.st->childlist.prev = &st->childlist;
	}
	else
	{
		memcpy(&inst.st->pid, &si->pid, sizeof(AS_ISDN_pid_t));
	}
	nds->extentions = si->extentions;
	inst.st->extentions |= si->extentions;
	inst.st->mgr = get_instance4id(si->mgr);	/* stack->mgr maybe not instance(->stack)*/
	nds->st = inst.st;
	
	list_add_tail(&nds->list, &dev->stacklist);
	return(inst.st->id);
}

static AS_ISDNstack_t * sel_channel(u_int addr, u_int channel)
{
	AS_ISDNstack_t	*st;
	channel_info_t	ci;

	st = get_stack4id(addr);
	if (!st)
		return(NULL);
	ci.channel = channel;
	ci.st.p = NULL;
	if (udev_obj.ctrl(st, MGR_SELCHANNEL | REQUEST, &ci))
		return(NULL);
	return(ci.st.p);
}

static int create_layer(AS_ISDNdevice_t *dev, struct sk_buff *skb)
{
	layer_info_t	*linfo;
	AS_ISDNlayer_t	*layer;
	AS_ISDNstack_t	*st;
	int		i, ret;
	devicelayer_t	*nl;
	AS_ISDNobject_t	*obj;
	AS_ISDNinstance_t *inst = NULL;
	AS_ISDN_head_t		*hp;

	hp = AS_ISDN_HEAD_P(skb);
	linfo = (layer_info_t *)skb->data;
	if (!(st = get_stack4id(linfo->st)))
	{
		int_error();
		return(-ENODEV);
	}

	/* just used to check the instance in devicelayer_t has a slave instane in exist kernel */
	if (linfo->object_id != -1)
	{
		obj = get_object(linfo->object_id);
		if (!obj)
		{
			printk(KERN_WARNING "%s: no object %x found\n",	__FUNCTION__, linfo->object_id);
			return(-ENODEV);
		}
		ret = obj->own_ctrl(st, MGR_NEWLAYER | REQUEST, &linfo->pid);
		if (ret)
		{
			printk(KERN_WARNING "%s: error nl req %d\n",	__FUNCTION__, ret);
			return(ret);
		}
		layer = getlayer4lay(st, linfo->pid.layermask);
		if (!layer)
		{
			printk(KERN_WARNING "%s: no layer for lm(%x)\n",	__FUNCTION__, linfo->pid.layermask);
			return(-EINVAL);
		}
		inst = layer->inst;
		if (!inst)
		{
			printk(KERN_WARNING "%s: no inst in layer(%p)\n",	__FUNCTION__, layer);
			return(-EINVAL);
		}
	}
	else if ((layer = getlayer4lay(st, linfo->pid.layermask)))
	{
		if (!(linfo->extentions & EXT_INST_MIDDLE))
		{
			printk(KERN_WARNING	"AS_ISDN create_layer st(%x) LM(%x) inst not empty(%p)\n",st->id, linfo->pid.layermask, layer);
			return(-EBUSY);
		}
	}

	if (!(nl = kmalloc(sizeof(devicelayer_t), GFP_ATOMIC)))
	{
		printk(KERN_ERR "kmalloc devicelayer failed\n");
		return(-ENOMEM);
	}
	memset(nl, 0, sizeof(devicelayer_t));

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "AS_ISDN create_layer LM(%x) nl(%p) nl inst(%p)\n",linfo->pid.layermask, nl, &nl->inst);
#endif

	nl->dev = dev;
	memcpy(&nl->inst.pid, &linfo->pid, sizeof(AS_ISDN_pid_t));
	strcpy(nl->inst.name, linfo->name);
	nl->inst.extentions = linfo->extentions;

	for (i=0; i<= MAX_LAYER_NR; i++)
	{
		if (linfo->pid.layermask & ISDN_LAYER(i))
		{
			if (st && (st->pid.protocol[i] == ISDN_PID_NONE))
			{
				st->pid.protocol[i] = linfo->pid.protocol[i];
				nl->lm_st |= ISDN_LAYER(i);
			}
		}
	}
	
	if (st && (linfo->extentions & EXT_INST_MGR))
	{
		st->mgr = &nl->inst;
		test_and_set_bit(FLG_MGR_OWNSTACK, &nl->Flags);
	}
	nl->inst.down.owner = &nl->inst;
	nl->inst.up.owner = &nl->inst;
	nl->inst.obj = &udev_obj;
	nl->inst.data = nl;
	list_add_tail(&nl->list, &dev->layerlist);
	
	nl->inst.obj->ctrl(st, MGR_REGLAYER | INDICATION, &nl->inst);
	nl->iaddr = nl->inst.id;
	skb_trim(skb, 0);
	memcpy(skb_put(skb, sizeof(nl->iaddr)), &nl->iaddr, sizeof(nl->iaddr));
	
	if (inst)
	{
		nl->slave = inst;
		memcpy(skb_put(skb, sizeof(inst->id)), &inst->id, sizeof(inst->id));
	}
	else
	{
		memset(skb_put(skb, sizeof(nl->iaddr)), 0, sizeof(nl->iaddr));
	}
/*	
	return(8);
	NEW_FRAME_REQ frame return to user space is 28(12+16(Header Length) )	
*/
	return (12);
}

static int remove_if(devicelayer_t *dl, int stat)
{
	AS_ISDNif_t *hif,*phif,*shif;
	int err;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: dl(%p) stat(%x)\n", __FUNCTION__, 	dl, stat);
#endif

	phif = NULL;
	if (stat & IF_UP)
	{
		hif = &dl->inst.up;
		shif = &dl->s_up;
		if (shif->owner)
			phif = &shif->owner->down;
	}
	else if (stat & IF_DOWN)
	{
		hif = &dl->inst.down;
		shif = &dl->s_down;
		if (shif->owner)
			phif = &shif->owner->up;
	}
	else
	{
		printk(KERN_WARNING "%s: stat not UP/DOWN\n", __FUNCTION__);
		return(-EINVAL);
	}

	err = udev_obj.ctrl(hif->peer, MGR_DISCONNECT | REQUEST, hif);
	if (phif)
	{
		memcpy(phif, shif, sizeof(AS_ISDNif_t));
		memset(shif, 0, sizeof(AS_ISDNif_t));
	}
	
	if (hif->predecessor)
		hif->predecessor->clone = hif->clone;

	if (hif->clone)
		hif->clone->predecessor = hif->predecessor;
	return(err);
}

/* release devicestack_t */
static int del_stack(devicestack_t *ds)
{
	AS_ISDNdevice_t	*dev;

	if (!ds)
	{
		int_error();
		return(-EINVAL);
	}
	dev = ds->dev;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
	{
		printk(KERN_ERR  "%s: ds(%p) dev(%p)\n", 	__FUNCTION__, ds, dev);
	}
#endif

	if (!dev)
		return(-EINVAL);
	
	if (ds->st)
	{
		if (ds->extentions & EXT_STACK_CLONE)
			INIT_LIST_HEAD(&ds->st->childlist);
		udev_obj.ctrl(ds->st, MGR_DELSTACK | REQUEST, NULL);
	}
	
	list_del(&ds->list);
	kfree(ds);
	
	return(0);
}

static int del_layer(devicelayer_t *dl)
{
	AS_ISDNinstance_t *inst = &dl->inst;
	AS_ISDNdevice_t	*dev = dl->dev;
	int		i;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
	{
		printk(KERN_ERR  "%s: dl(%p) inst(%p) LM(%x) dev(%p)\n", 	__FUNCTION__, dl, inst, inst->pid.layermask, dev);
		printk(KERN_ERR  "%s: iaddr %x inst %s slave %p\n", __FUNCTION__, dl->iaddr, inst->name, dl->slave);
	}
#endif
	
	remove_if(dl, IF_UP);
	remove_if(dl, IF_DOWN);
	if (dl->slave)
	{
		if (dl->slave->obj)
			dl->slave->obj->own_ctrl(dl->slave,
				MGR_UNREGLAYER | REQUEST, NULL);
		else
			dl->slave = NULL; 
	}
	if (dl->lm_st && inst->st)
	{
		for (i=0; i<= MAX_LAYER_NR; i++) 
		{
			if (dl->lm_st & ISDN_LAYER(i))
			{
				inst->st->pid.protocol[i] = ISDN_PID_NONE;
			}
		}
		dl->lm_st = 0;
	}
	
	if (test_and_clear_bit(FLG_MGR_SETSTACK, &dl->Flags) && inst->st)
	{

#if AS_ISDN_DEBUG_CORE_DATA
		if (device_debug & DEBUG_MGR_FUNC)
			printk(KERN_ERR  "del_layer: CLEARSTACK id(%x)\n",	inst->st->id);
#endif
		
		udev_obj.ctrl(inst->st, MGR_CLEARSTACK | REQUEST, NULL);
	}

	if (inst->up.peer)
	{
		inst->up.peer->obj->ctrl(inst->up.peer,
			MGR_DISCONNECT | REQUEST, &inst->up);
	}

	if (inst->down.peer)
	{
		inst->down.peer->obj->ctrl(inst->down.peer,
			MGR_DISCONNECT | REQUEST, &inst->down);
	}

	dl->iaddr = 0;
	list_del(&dl->list);
	udev_obj.ctrl(inst, MGR_UNREGLAYER | REQUEST, NULL);

	if (test_and_clear_bit(FLG_MGR_OWNSTACK, &dl->Flags))
	{
		if (dl->inst.st)
		{
			del_stack(get_devstack(dev, dl->inst.st->id));
		}
	}
	
	kfree(dl);
	dl = NULL;
	return(0);
}

static AS_ISDNinstance_t *clone_instance(devicelayer_t *dl, AS_ISDNstack_t  *st, AS_ISDNinstance_t *peer)
{
	int		err;

	if (dl->slave)
	{
		printk(KERN_WARNING "%s: layer has slave, cannot clone\n", __FUNCTION__);
		return(NULL);
	}
	if (!(peer->extentions & EXT_INST_CLONE))
	{
		printk(KERN_WARNING "%s: peer cannot clone\n", __FUNCTION__);
		return(NULL);
	}
	dl->slave = (AS_ISDNinstance_t *)st;
	if ((err = peer->obj->own_ctrl(peer, MGR_CLONELAYER | REQUEST,	&dl->slave)))
	{
		dl->slave = NULL;
		printk(KERN_WARNING "%s: peer clone error %d\n",	__FUNCTION__, err);
		return(NULL);
	}
	return(dl->slave);
}

static int connect_if_req(AS_ISDNdevice_t *dev, struct sk_buff *skb)
{
	devicelayer_t		*dl;
	interface_info_t	*ifi = (interface_info_t *)skb->data;
	AS_ISDNinstance_t		*owner;
	AS_ISDNinstance_t		*peer;
	AS_ISDNinstance_t		*pp;
	AS_ISDNif_t		*hifp;
	int			stat;
	AS_ISDN_head_t		*hp;

	hp = AS_ISDN_HEAD_P(skb);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: addr:%x own(%x) peer(%x)\n", __FUNCTION__, hp->addr, ifi->owner, ifi->peer);
#endif
	
	if (!(dl=get_devlayer(dev, ifi->owner)))
	{
		int_errtxt("no devive_layer for %08x", ifi->owner);
		return(-ENXIO);
	}

	if (!(owner = get_instance4id(ifi->owner)))
	{
		printk(KERN_WARNING "%s: owner(%x) not found\n",__FUNCTION__, ifi->owner);
		return(-ENODEV);
	}
	
	if (!(peer = get_instance4id(ifi->peer)))
	{
		printk(KERN_WARNING "%s: peer(%x) not found\n",	__FUNCTION__, ifi->peer);
		return(-ENODEV);
	}
	
	if (owner->pid.layermask < peer->pid.layermask)
	{
		hifp = &peer->down;
		stat = IF_DOWN;
	}
	else if (owner->pid.layermask > peer->pid.layermask)
	{
		hifp = &peer->up;
		stat = IF_UP;
	}
	else
	{
		int_errtxt("OLM == PLM: %x", owner->pid.layermask);
		return(-EINVAL);
	}
	
	if (ifi->extentions == EXT_IF_CHAIN)
	{
		if (!(pp = hifp->peer))
		{
			printk(KERN_WARNING "%s: peer if has no peer\n",	__FUNCTION__);
			return(-EINVAL);
		}
		
		if (stat == IF_UP)
		{
			memcpy(&owner->up, hifp, sizeof(AS_ISDNif_t));
			memcpy(&dl->s_up, hifp, sizeof(AS_ISDNif_t));
			owner->up.owner = owner;
			hifp->peer = owner;
			hifp->func = from_up_down;
			hifp->fdata = dl;
			hifp = &pp->down;
			memcpy(&owner->down, hifp, sizeof(AS_ISDNif_t));
			memcpy(&dl->s_down, hifp, sizeof(AS_ISDNif_t));
			owner->down.owner = owner;
			hifp->peer = owner;
			hifp->func = from_up_down;
			hifp->fdata = dl;
		}
		else
		{
			memcpy(&owner->down, hifp, sizeof(AS_ISDNif_t));
			memcpy(&dl->s_down, hifp, sizeof(AS_ISDNif_t));
			owner->up.owner = owner;
			hifp->peer = owner;
			hifp->func = from_up_down;
			hifp->fdata = dl;
			hifp = &pp->up;
			memcpy(&owner->up, hifp, sizeof(AS_ISDNif_t));
			memcpy(&dl->s_up, hifp, sizeof(AS_ISDNif_t));
			owner->down.owner = owner;
			hifp->peer = owner;
			hifp->func = from_up_down;
			hifp->fdata = dl;
		}
		return(0);
	}
	
	if (ifi->extentions & EXT_IF_CREATE)
	{/* create new instance if allready in use */
		if (hifp->stat != IF_NOACTIV)
		{
			if ((peer = clone_instance(dl, owner->st, peer)))
			{
				if (stat == IF_UP)
					hifp = &peer->up;
				else
					hifp = &peer->down;
			}
			else
			{
				printk(KERN_WARNING "%s: cannot create new peer instance\n", __FUNCTION__);
				return(-EBUSY);
			}
		}
	}
	
	if (ifi->extentions & EXT_IF_EXCLUSIV)
	{
		if (hifp->stat != IF_NOACTIV)
		{
			printk(KERN_WARNING "%s: peer if is in use\n",	__FUNCTION__);
			return(-EBUSY);
		}
	}			
	return(AS_ISDN_ConnectIF(owner, peer));
}

static int set_if_req(AS_ISDNdevice_t *dev, struct sk_buff *skb)
{
	AS_ISDNif_t		*hif,*phif,*shif;
	int				stat;
	interface_info_t	*ifi = (interface_info_t *)skb->data;
	devicelayer_t		*dl;
	AS_ISDNinstance_t	*inst, *peer;
	AS_ISDN_head_t	*hp;

	hp = AS_ISDN_HEAD_P(skb);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: addr:%x own(%x) peer(%x)\n", __FUNCTION__, hp->addr, ifi->owner, ifi->peer);
#endif

	if (!(dl=get_devlayer(dev, hp->addr)))
		return(-ENXIO);
	if (!(inst = get_instance4id(ifi->owner))) 
	{
		printk(KERN_WARNING "%s: owner(%x) not found\n", __FUNCTION__, ifi->owner);
		return(-ENODEV);
	}
	if (!(peer = get_instance4id(ifi->peer)))
	{
		printk(KERN_WARNING "%s: peer(%x) not found\n",	__FUNCTION__, ifi->peer);
		return(-ENODEV);
	}

	if (ifi->stat == IF_UP)
	{
		hif = &dl->inst.up;
		phif = &peer->down;
		shif = &dl->s_up;
		stat = IF_DOWN;
	}
	else if (ifi->stat == IF_DOWN)
	{
		hif = &dl->inst.down;
		shif = &dl->s_down;
		phif = &peer->up;
		stat = IF_UP;
	}
	else
	{
		printk(KERN_WARNING "%s: if not UP/DOWN\n", __FUNCTION__);
		return(-EINVAL);
	}

	
	if (shif->stat != IF_NOACTIV)
	{
		printk(KERN_WARNING "%s: save if busy\n", __FUNCTION__);
		return(-EBUSY);
	}
	if (hif->stat != IF_NOACTIV) {
		printk(KERN_WARNING "%s: own if busy\n", __FUNCTION__);
		return(-EBUSY);
	}
	hif->stat = stat;
	hif->owner = inst;
	memcpy(shif, phif, sizeof(AS_ISDNif_t));
	memset(phif, 0, sizeof(AS_ISDNif_t));
	return(peer->obj->own_ctrl(peer, hp->prim, hif));
}

static int add_if_req(AS_ISDNdevice_t *dev, struct sk_buff *skb)
{
	AS_ISDNif_t		*hif;
	interface_info_t	*ifi = (interface_info_t *)skb->data;
	AS_ISDNinstance_t		*inst, *peer;
	AS_ISDN_head_t		*hp;

	hp = AS_ISDN_HEAD_P(skb);
	
#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: addr:%x own(%x) peer(%x)\n", __FUNCTION__, hp->addr, ifi->owner, ifi->peer);
#endif

	if (!(inst = get_instance4id(ifi->owner)))
	{
		printk(KERN_WARNING "%s: owner(%x) not found\n", __FUNCTION__, ifi->owner);
		return(-ENODEV);
	}
	if (!(peer = get_instance4id(ifi->peer)))
	{
		printk(KERN_WARNING "%s: peer(%x) not found\n",	__FUNCTION__, ifi->peer);
		return(-ENODEV);
	}

	if (ifi->stat == IF_DOWN)
	{
		hif = &inst->up;
	}
	else if (ifi->stat == IF_UP)
	{
		hif = &inst->down;
	}
	else
	{
		printk(KERN_WARNING "%s: if not UP/DOWN\n", __FUNCTION__);
		return(-EINVAL);
	}
	return(peer->obj->ctrl(peer, hp->prim, hif));
}

static int del_if_req(AS_ISDNdevice_t *dev, u_int addr)
{
	devicelayer_t *dl;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: addr:%x\n", __FUNCTION__, addr);
#endif

	if (!(dl=get_devlayer(dev, addr)))
		return(-ENXIO);
	return(remove_if(dl, addr));
}

static int new_entity_req(AS_ISDNdevice_t *dev, int *entity)
{
	int		ret;
	entity_item_t	*ei = kmalloc(sizeof(entity_item_t), GFP_ATOMIC);

	if (!ei)
		return(-ENOMEM);
	ret = isdn_alloc_entity(entity);
	ei->entity = *entity;
	if (ret)
		kfree(entity);
	else
		list_add((struct list_head *)ei, &dev->entitylist);
	return(ret);
}

static int del_entity_req(AS_ISDNdevice_t *dev, int entity)
{
	struct list_head	*item, *nxt;

	list_for_each_safe(item, nxt, &dev->entitylist)
	{
		if (((entity_item_t *)item)->entity == entity)
		{
			list_del(item);
			isdn_delete_entity(entity);
			kfree(item);
			return(0);
		}
	}
	return(-ENODEV);
}

static void dev_expire_timer(AS_ISDNtimer_t *ht)
{
	struct sk_buff	*skb;
	AS_ISDN_head_t	*hp;

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
		if (_isdn_rdata(ht->dev, skb))
			dev_kfree_skb(skb);
	}
	else
		printk(KERN_WARNING "%s: timer(%x) not active\n", __FUNCTION__, ht->id);
}

static int dev_init_timer(AS_ISDNdevice_t *dev, u_int id)
{
	AS_ISDNtimer_t	*ht;

	ht = get_devtimer(dev, id);
	if (!ht)
	{
		ht = kmalloc(sizeof(AS_ISDNtimer_t), GFP_ATOMIC);
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

static int dev_add_timer(AS_ISDNdevice_t *dev, AS_ISDN_head_t *hp)
{
	AS_ISDNtimer_t	*ht;

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
static int dev_del_timer(AS_ISDNdevice_t *dev, u_int id)
{
	AS_ISDNtimer_t	*ht;

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

static void dev_free_timer(AS_ISDNtimer_t *ht)
{
#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_TIMER)
		printk(KERN_ERR  "%s: timer(%x)\n", __FUNCTION__, ht->id);
#endif

	del_timer(&ht->tl);
	list_del(&ht->list);
	kfree(ht);
}

static int dev_remove_timer(AS_ISDNdevice_t *dev, u_int id)
{
	AS_ISDNtimer_t	*ht;

	ht = get_devtimer(dev, id);
	if (!ht)
	{
		printk(KERN_WARNING "%s: no timer(%x)\n", __FUNCTION__, id);
		return(-ENODEV);
	}
	dev_free_timer(ht); 
	return(0);
}

/* get status info of instance */
static int get_status(struct sk_buff *skb)
{
	AS_ISDN_head_t	*hp;
	status_info_t	*si = (status_info_t *)skb->data;
	AS_ISDNinstance_t	*inst;
	int		err;

	hp = AS_ISDN_HEAD_P(skb);
	if (!(inst = get_instance4id(hp->addr & IF_ADDRMASK)))
	{
		printk(KERN_WARNING "%s: no instance\n", __FUNCTION__);
		err = -ENODEV;
	}
	else
	{
		err = inst->obj->own_ctrl(inst, MGR_STATUS | REQUEST, si);
	}

	if (err)
		hp->len = err;
	else
	{
		hp->len = si->len + 2*sizeof(int);
		skb_put(skb, hp->len);
	}
	return(err);	
}

static void get_layer_info(struct sk_buff *skb)
{
	AS_ISDN_head_t	*hp;
	AS_ISDNinstance_t 	*inst;
	layer_info_t	*li = (layer_info_t *)skb->data;

	hp = AS_ISDN_HEAD_P(skb);
	if (!(inst = get_instance4id(hp->addr & IF_ADDRMASK)))
	{
		printk(KERN_WARNING "%s: no instance\n", __FUNCTION__);
		hp->len = -ENODEV;
		return;
	}
	memset(li, 0, sizeof(layer_info_t));
	if (inst->obj)
		li->object_id = inst->obj->id;
	strcpy(li->name, inst->name);
	li->extentions = inst->extentions;
	li->id = inst->id;
	if (inst->st)
		li->st = inst->st->id;
	memcpy(&li->pid, &inst->pid, sizeof(AS_ISDN_pid_t));
	hp->len = sizeof(layer_info_t);
	skb_put(skb, hp->len);
}

/* fill interface info into interface_info_t in skb. 
* before this function, request primitive is filled in skb 
*/
static void get_if_info(struct sk_buff *skb)
{
	AS_ISDN_head_t	*hp;
	AS_ISDNinstance_t	*inst;
	AS_ISDNif_t		*hif;
	interface_info_t	*ii = (interface_info_t *)skb->data;
	
	hp = AS_ISDN_HEAD_P(skb);
	
	if (!(inst = get_instance4id(hp->addr & IF_ADDRMASK)))
	{
		printk(KERN_WARNING "%s: no instance\n", __FUNCTION__);
		hp->len = -ENODEV;
		return;
	}
	if (hp->dinfo == IF_DOWN)
		hif = &inst->down;
	else if (hp->dinfo == IF_UP)
		hif = &inst->up;
	else
	{
		printk(KERN_WARNING "%s: wrong interface %x\n",	__FUNCTION__, hp->dinfo);
		hp->len = -EINVAL;
		return;
	}
	
	hp->dinfo = 0;
	memset(ii, 0, sizeof(interface_info_t));
	if (hif->owner)
		ii->owner = hif->owner->id;
	if (hif->peer)
		ii->peer = hif->peer->id;

	ii->extentions = hif->extentions;
	ii->stat = hif->stat;
	hp->len = sizeof(interface_info_t);
	skb_put(skb, hp->len);
}

/* write data frame with interface between instances 
* this case only is in no primitive is found in skb 
*/
static int wdata_frame(AS_ISDNdevice_t *dev, struct sk_buff *skb)
{
	AS_ISDN_head_t	*hp;
	AS_ISDNif_t	*hif = NULL;
	devicelayer_t	*dl;
	int		err = -ENXIO;

//	TRACE();
	hp = AS_ISDN_HEAD_P(skb);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_WDATA)
		printk(KERN_ERR  "%s: addr:%x\n", __FUNCTION__, hp->addr);
#endif

	if (!(dl=get_devlayer(dev, hp->addr)))
		return(err);
	
	if (hp->addr & IF_UP)
	{
		hif = &dl->inst.up;
//	TRACE();
		if (IF_TYPE(hif) != IF_DOWN)
		{
			printk(KERN_WARNING "%s: inst(%s)..up no down\n", __FUNCTION__, dl->inst.name);
			hif = NULL;
		}
	}
	else if (hp->addr & IF_DOWN)
	{
		hif = &dl->inst.down;
//	TRACE();
		if (IF_TYPE(hif) != IF_UP)
		{
			printk(KERN_WARNING "%s: inst(%s).down no up\n", __FUNCTION__, dl->inst.name);
			if (IF_TYPE(hif) != IF_DOWN)
				printk(KERN_WARNING "%s: inst(%s).down is a down interface\n", __FUNCTION__, dl->inst.name);
				
			hif = NULL;
		}
	}
	
	if (hif)
	{
//	TRACE();

#if AS_ISDN_DEBUG_CORE_DATA
		if (device_debug & DEBUG_WDATA)
			printk(KERN_ERR  "%s: pr(%x) di(%x) l(%d)\n", __FUNCTION__, hp->prim, hp->dinfo, hp->len);
#endif

		if (hp->len < 0)
		{
			printk(KERN_WARNING "%s: data negativ(%d)\n", __FUNCTION__, hp->len);
			return(-EINVAL);
		}
#if AS_ISDN_DEBUG_CORE_DATA
		printk(KERN_ERR  "%s: write in hif->func %s of %s\n", __FUNCTION__, (IF_TYPE(hif) == IF_UP)?"UP":"DOWN", hif->peer->name);
#endif
		err = hif->func(hif, skb);

#if AS_ISDN_DEBUG_CORE_DATA
		if (device_debug & DEBUG_WDATA && err)
			printk(KERN_ERR  "%s: hif->func ret(%x)\n", __FUNCTION__, err);
#endif

	} 
	else
	{
//	TRACE();
#if AS_ISDN_DEBUG_CORE_DATA
		if (device_debug & DEBUG_WDATA)
			printk(KERN_ERR  "AS_ISDN: no matching interface\n");
#endif

	}
	return(err);
}

/* main distributing process point for device, eg. enter into stack process 
 * All primitives are handled here. Used by D channel
*/
static int  _isdn_wdata_if(AS_ISDNdevice_t *dev, struct sk_buff *skb)
{
	struct sk_buff	*nskb = NULL;
	AS_ISDN_head_t	*hp;
	AS_ISDNstack_t		*st;
	devicelayer_t		*dl;
	AS_ISDNlayer_t    	*layer;
	int				lay;
	int				err = 0;

	hp = AS_ISDN_HEAD_P(skb);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_WDATA)
		printk(KERN_ERR  "%s: %x:%x %x %d %d\n", __FUNCTION__, hp->addr, hp->prim, hp->dinfo, hp->len, skb->len);
#endif

//	TRACE();
	if ((hp->len > 0) && (skb->len < hp->len))
	{
		printk(KERN_WARNING "%s: frame(%d/%d) too short\n",	__FUNCTION__, skb->len, hp->len);
		return(error_answer(dev, skb, -EINVAL));
	}
	
	switch(hp->prim)
	{
		case (MGR_VERSION | REQUEST):
		/* case 1 */	
			hp->prim = MGR_VERSION | CONFIRM;
			hp->len = 0;
			hp->dinfo = AS_ISDN_VERSION;
			break;
			
		case (MGR_GETSTACK | REQUEST):
		/* case 2.1 */	
			hp->prim = MGR_GETSTACK | CONFIRM;
			hp->dinfo = 0;
			if (hp->addr <= 0)
			{/* no stack is assigned */
				hp->dinfo = get_stack_cnt();
				hp->len = 0;
			}
			else
			{/* get info about stack assigned by hp->addr */
				nskb = alloc_stack_skb(1000, 0);
				if (!nskb)
					return(error_answer(dev, skb, -ENOMEM));
				memcpy(AS_ISDN_HEAD_P(nskb), hp, sizeof(AS_ISDN_head_t));
				get_stack_info(nskb);

//				TRACE();
			}
			break;
		case (MGR_SETSTACK | REQUEST):
		/* case 2.2 */	
			if (skb->len < sizeof(AS_ISDN_pid_t))
				return(error_answer(dev, skb, -EINVAL));
			
			hp->dinfo = 0;
			if ((st = get_stack4id(hp->addr)))
			{
				stack_inst_flg(dev, st, FLG_MGR_SETSTACK, 0);
				/* call central_manager to process SET_STACK|REQUEST */
				hp->len = udev_obj.ctrl(st, hp->prim, skb->data);
			}
			else
				hp->len = -ENODEV;

			hp->prim = MGR_SETSTACK | CONFIRM;
			break;
		case (MGR_NEWSTACK | REQUEST):
		/* case 2.3 */	
			hp->dinfo = 0;
			hp->prim = MGR_NEWSTACK | CONFIRM;
			hp->len = 0;
			err = new_devstack(dev, (stack_info_t *)skb->data);
			if (err<0)
				hp->len = err;
			else
				hp->dinfo = err;
			break;	
		case (MGR_CLEARSTACK | REQUEST):
		/* case 2.4 */	
			hp->dinfo = 0;
			if ((st = get_stack4id(hp->addr)))
			{
				stack_inst_flg(dev, st, FLG_MGR_SETSTACK, 1);
				hp->len = udev_obj.ctrl(st, hp->prim, NULL);
			}
			else
				hp->len = -ENODEV;
			hp->prim = MGR_CLEARSTACK | CONFIRM;

			break;
			
		case (MGR_SELCHANNEL | REQUEST):
		/* case 3 */	
			hp->prim = MGR_SELCHANNEL | CONFIRM;
			st = sel_channel(hp->addr, hp->dinfo);
			if (st)
			{
				hp->len = 0;
				hp->dinfo = st->id;
			}
			else
			{
				hp->dinfo = 0;
				hp->len = -ENODEV;
			}
			break;
			
		case (MGR_GETLAYERID | REQUEST):
		/* case 4.1 */	
			hp->prim = MGR_GETLAYERID | CONFIRM;
			lay = hp->dinfo;
			hp->dinfo = 0;
			if (LAYER_OUTRANGE(lay))
			{
				hp->len = -EINVAL;
			}
			else
			{
				hp->len = 0;
				lay = ISDN_LAYER(lay);
				if ((st = get_stack4id(hp->addr)))
				{
					if ((layer = getlayer4lay(st, lay)))
					{
						if (layer->inst)
							hp->dinfo = layer->inst->id;
					}
				}
			}
			break;
		case (MGR_GETLAYER | REQUEST):
		/* case 4.2 */	
			hp->prim = MGR_GETLAYER | CONFIRM;
			hp->dinfo = 0;
			skb_trim(skb, 0);
			if (skb_tailroom(skb) < sizeof(layer_info_t))
			{
				nskb = alloc_stack_skb(sizeof(layer_info_t), 0);
				if (!nskb)
					return(error_answer(dev, skb, -ENOMEM));
				memcpy(AS_ISDN_HEAD_P(nskb), hp, sizeof(AS_ISDN_head_t));
				get_layer_info(nskb);
			}
			else
			{
				get_layer_info(skb);
			}
			break;
		case (MGR_NEWLAYER | REQUEST):
		/* case 4.3 */	
			if (skb->len < sizeof(layer_info_t))
				return(error_answer(dev, skb, -EINVAL));
			hp->dinfo = 0;
			hp->prim = MGR_NEWLAYER | CONFIRM;
			hp->len = create_layer(dev, skb);
			break;
		case (MGR_DELLAYER | REQUEST):
		/* case 4.4 */	
			hp->prim = MGR_DELLAYER | CONFIRM;
			hp->dinfo = 0;
			if ((dl = get_devlayer(dev, hp->addr)))
				hp->len = del_layer(dl);
			else
				hp->len = -ENXIO;
			break;

		case (MGR_GETIF | REQUEST):
		/* case 5.1 */	
			hp->prim = MGR_GETIF | CONFIRM;
#if 0
/* commented for IF_DOWN or IF_UP */
			hp->dinfo = 0;
#endif
			skb_trim(skb, 0);
			if (skb_tailroom(skb) < sizeof(interface_info_t))
			{
				nskb = alloc_stack_skb(sizeof(interface_info_t), 0);
				if (!nskb)
					return(error_answer(dev, skb, -ENOMEM));
				memcpy(AS_ISDN_HEAD_P(nskb), hp, sizeof(AS_ISDN_head_t));
				get_if_info(nskb);
			}
			else
			{
				get_if_info(skb);
			}
			break;
		case (MGR_CONNECT | REQUEST):
		/* case 5.2 */	
			if (skb->len < sizeof(interface_info_t))
				return(error_answer(dev, skb, -EINVAL));
			hp->len = connect_if_req(dev, skb);
			hp->dinfo = 0;
			hp->prim = MGR_CONNECT | CONFIRM;
			break;
		case (MGR_SETIF | REQUEST):
		/* case 5.3 */	
			hp->len = set_if_req(dev, skb);
			hp->prim = MGR_SETIF | CONFIRM;
			hp->dinfo = 0;
			break;
		case (MGR_ADDIF | REQUEST):
		/* case 5.4 */	
			hp->len = add_if_req(dev, skb);
			hp->prim = MGR_ADDIF | CONFIRM;
			hp->dinfo = 0;
			break;
		case (MGR_DISCONNECT | REQUEST):
		/* case 5.5 */	
			hp->len = del_if_req(dev, hp->addr);
			hp->prim = MGR_DISCONNECT | CONFIRM;
			hp->dinfo = 0;
			break;

		case (MGR_NEWENTITY | REQUEST):
		/* case 6.1 */	
			hp->prim = MGR_NEWENTITY | CONFIRM;
			hp->len = new_entity_req(dev, &hp->dinfo);
			break;
		case (MGR_DELENTITY | REQUEST):
		/* case 6.2 */	
			hp->prim = MGR_DELENTITY | CONFIRM;
			hp->len = del_entity_req(dev, hp->dinfo);
			break;

		case (MGR_INITTIMER | REQUEST):
		/* case 7.1 */	
			hp->len = dev_init_timer(dev, hp->addr);
			hp->prim = MGR_INITTIMER | CONFIRM;
			break;
		case (MGR_ADDTIMER | REQUEST):
		/* case 7.2 */	
			hp->len = dev_add_timer(dev, hp);
			hp->prim = MGR_ADDTIMER | CONFIRM;
			hp->dinfo = 0;
			break;
		case (MGR_DELTIMER | REQUEST):
		/* case 7.3 */	
			hp->len = dev_del_timer(dev, hp->addr);
			hp->prim = MGR_DELTIMER | CONFIRM;
			break;
		case (MGR_REMOVETIMER | REQUEST):
		/* case 7.4 */	
			hp->len = dev_remove_timer(dev, hp->addr);
			hp->prim = MGR_REMOVETIMER | CONFIRM;
			hp->dinfo = 0;
			break;

		case (MGR_TIMER | RESPONSE):
		/* case 8 */	
			dev_kfree_skb(skb);
			return(0);
			break;
			
		case (MGR_STATUS | REQUEST):
		/* case 9 */	
			hp->prim = MGR_STATUS | CONFIRM;
			nskb = alloc_stack_skb(1000, 0);
			if (!nskb)
				return(error_answer(dev, skb, -ENOMEM));
			memcpy(AS_ISDN_HEAD_P(nskb), hp, sizeof(AS_ISDN_head_t));
			get_status(nskb);
			hp->dinfo = 0;
			break;
			
		case (MGR_SETDEVOPT | REQUEST):
		/* case 10.1 */	
			hp->prim = MGR_SETDEVOPT | CONFIRM;
			hp->len = 0;
			if (hp->dinfo == FLG_AS_ISDNPORT_ONEFRAME)
			{
				test_and_set_bit(FLG_AS_ISDNPORT_ONEFRAME, &dev->rport.Flag);
			}
			else if (!hp->dinfo)
			{
				test_and_clear_bit(FLG_AS_ISDNPORT_ONEFRAME, &dev->rport.Flag);
			}
			else
			{
				hp->len = -EINVAL;
			}
			hp->dinfo = 0;
			break;
		case (MGR_GETDEVOPT | REQUEST):
		/* case 10.2 */	
			hp->prim = MGR_GETDEVOPT | CONFIRM;
			hp->len = 0;
			if (test_bit(FLG_AS_ISDNPORT_ONEFRAME, &dev->rport.Flag))
				hp->dinfo = FLG_AS_ISDNPORT_ONEFRAME;
			else
				hp->dinfo = 0;
			break;
			
		default:
		/* case 11 */	
			if (hp->addr & IF_TYPEMASK)
			{/* skb is data frame : no frame is need to be read by user process */
				err = wdata_frame(dev, skb);
				if (err)
				{
					if (device_debug & DEBUG_WDATA)
						printk(KERN_ERR  "wdata_frame returns error %d\n", err);
					err = error_answer(dev, skb, err);
				}
			}
			else
			{
				printk(KERN_WARNING "AS_ISDN: prim %x addr %x not implemented\n",	hp->prim, hp->addr);
				err = error_answer(dev, skb, -EINVAL);
			}
			return(err);
			break;
	}

	/* finally add skb into dev->rport, and wakeup user process waiting on this device */
	if (nskb)
	{
		err = _isdn_rdata(dev, nskb);
		if (err)
			kfree_skb(nskb);
		else
			kfree_skb(skb);
	}
	else
		err = _isdn_rdata(dev, skb);
	
	return(err);
}

/* construct minor device as the need of open file */
static AS_ISDNdevice_t *init_device(u_int minor)
{
	AS_ISDNdevice_t	*dev;
	u_long		flags;

	dev = kmalloc(sizeof(AS_ISDNdevice_t), GFP_KERNEL);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: dev(%d) %p\n",	__FUNCTION__, minor, dev); 
#endif

	if (dev)
	{
		memset(dev, 0, sizeof(AS_ISDNdevice_t));
		dev->minor = minor;
		init_waitqueue_head(&dev->rport.procq);
		init_waitqueue_head(&dev->wport.procq);
		skb_queue_head_init(&dev->rport.queue);
		skb_queue_head_init(&dev->wport.queue);
		init_MUTEX(&dev->io_sema);
		INIT_LIST_HEAD(&dev->layerlist);
		INIT_LIST_HEAD(&dev->stacklist);
		INIT_LIST_HEAD(&dev->timerlist);
		INIT_LIST_HEAD(&dev->entitylist);
		write_lock_irqsave(&AS_ISDN_device_lock, flags);
		list_add_tail(&dev->list, &AS_ISDN_devicelist);
		write_unlock_irqrestore(&AS_ISDN_device_lock, flags);
	}
	return(dev);
}

AS_ISDNdevice_t *get_free_rawdevice(void)
{
	AS_ISDNdevice_t	*dev;
	u_int		minor;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s:\n", __FUNCTION__);
#endif
	
	for (minor=AS_ISDN_MINOR_RAW_MIN; minor<=AS_ISDN_MINOR_RAW_MAX; minor++)
	{
		dev = get_device4minor(minor);

#if AS_ISDN_DEBUG_CORE_DATA
		if (device_debug & DEBUG_MGR_FUNC)
			printk(KERN_ERR  "%s: dev(%d) %p\n",	__FUNCTION__, minor, dev); 
#endif

		if (!dev) 
		{
			dev = init_device(minor);
			if (!dev)
				return(NULL);
			
//			TRACE();

			printk(KERN_ERR "read port function for raw device %d is inited\n", minor );
			
			dev->rport.pif.func = _isdn_rdata_raw;
			dev->rport.pif.fdata = dev;
			return(dev);
		}
	}
	return(NULL);
}

int free_device(AS_ISDNdevice_t *dev)
{
	struct list_head *item, *ni;
	u_long	flags;

	if (!dev)
		return(-ENODEV);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: dev(%d)\n", __FUNCTION__, dev->minor);
#endif

	/* release related stuff */
	list_for_each_safe(item, ni, &dev->layerlist)
		del_layer(list_entry(item, devicelayer_t, list));
	list_for_each_safe(item, ni, &dev->stacklist)
		del_stack(list_entry(item, devicestack_t, list));
	list_for_each_safe(item, ni, &dev->timerlist)
		dev_free_timer(list_entry(item, AS_ISDNtimer_t, list));
	if (!skb_queue_empty(&dev->rport.queue))
		discard_queue(&dev->rport.queue);
	if (!skb_queue_empty(&dev->wport.queue))
		discard_queue(&dev->wport.queue);
	write_lock_irqsave(&AS_ISDN_device_lock, flags);
	list_del(&dev->list);
	write_unlock_irqrestore(&AS_ISDN_device_lock, flags);
	if (!list_empty(&dev->entitylist))
	{
		printk(KERN_WARNING "AS_ISDN %s: entitylist not empty\n", __FUNCTION__);
		list_for_each_safe(item, ni, &dev->entitylist)
		{
			struct entity_item *ei = list_entry(item, struct entity_item, head);
			list_del(item);
			isdn_delete_entity(ei->entity);
			kfree(ei);
		}
	}
	kfree(dev);
	return(0);
}

static int _isdn_file_open(struct inode *ino, struct file *filep)
{
	u_int		minor = 	MINOR(ino->i_rdev);
	AS_ISDNdevice_t 	*dev = NULL;
	int		isnew = 0;

//	TRACE();
#if AS_ISDN_DEBUG_CORE_INIT
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR "file_open in: minor(%d) %p %p mode(%x)\n",	minor, filep, filep->private_data, filep->f_mode);
#endif

	if (minor)
	{
		dev = get_device4minor(minor);
		if (dev)
		{
			if ((dev->open_mode & filep->f_mode) & (FMODE_READ | FMODE_WRITE))
				return(-EBUSY);
		}
		else
			return(-ENODEV);
	} 
	else if ((dev = init_device(minor))) /* minor is 0*/
		isnew = 1;
	else
		return(-ENOMEM);
	
	dev->open_mode |= filep->f_mode & (FMODE_READ | FMODE_WRITE);

	if (dev->open_mode & FMODE_READ)
	{
		dev->rport.lock = SPIN_LOCK_UNLOCKED;
		dev->rport.maxqlen = DEFAULT_PORT_QUEUELEN;
		test_and_set_bit(FLG_AS_ISDNPORT_OPEN, &dev->rport.Flag);
	}
	if (dev->open_mode & FMODE_WRITE)
	{
		dev->wport.lock = SPIN_LOCK_UNLOCKED;
		dev->wport.maxqlen = DEFAULT_PORT_QUEUELEN;
		test_and_set_bit(FLG_AS_ISDNPORT_OPEN, &dev->wport.Flag);
	}

	filep->private_data = dev;
	
#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR  "file_open out: %p %p\n", filep, filep->private_data);
#endif

//	TRACE();
	return(0);
}

static int _isdn_file_close(struct inode *ino, struct file *filep)
{
	AS_ISDNdevice_t	*dev, *nd;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR  "AS_ISDN: AS_ISDN_close %p %p\n", filep, filep->private_data);
#endif

	read_lock(&AS_ISDN_device_lock);
	list_for_each_entry_safe(dev, nd, &AS_ISDN_devicelist, list)
	{
		if (dev == filep->private_data)
		{

#if AS_ISDN_DEBUG_CORE_DATA
			if (device_debug & DEBUG_DEV_OP)
				printk(KERN_ERR  "AS_ISDN: dev(%d) %p mode %x/%x\n", dev->minor, dev, dev->open_mode, filep->f_mode);
#endif

			dev->open_mode &= ~filep->f_mode;
			read_unlock(&AS_ISDN_device_lock);
			if (filep->f_mode & FMODE_READ)
			{
				test_and_clear_bit(FLG_AS_ISDNPORT_OPEN,  &dev->rport.Flag);
			}
			if (filep->f_mode & FMODE_WRITE)
			{
				test_and_clear_bit(FLG_AS_ISDNPORT_OPEN, &dev->wport.Flag);
			}
			filep->private_data = NULL;
			if (!dev->minor)
				free_device(dev);
			return 0;
		}
	}
	
	read_unlock(&AS_ISDN_device_lock);
	printk(KERN_WARNING "AS_ISDN: No private data while closing device\n");

	return 0;
}

static __inline__ ssize_t _do_isdn_read(struct file *file, char *buf, size_t count, loff_t * off)
{
	AS_ISDNdevice_t	*dev = file->private_data;
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
				if (len)
					break;
				spin_unlock_irqrestore(&dev->rport.lock, flags);
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

static __inline__ ssize_t _do_isdn_read_one_by_one(struct file *file, char *buf, size_t count, loff_t * off)
{
	AS_ISDNdevice_t	*dev = file->private_data;
	size_t			len;
	u_long			flags;
	struct sk_buff	*skb;
	int ret;

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

	if ((skb = skb_dequeue(&dev->rport.queue)))
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
		}
		else
		{/* raw device */
			if (skb->len > (count - len))
			{
			    nospace:
				skb_queue_head(&dev->rport.queue, skb);
//				if (len)
//					break;
				spin_unlock_irqrestore(&dev->rport.lock, flags);
				return(-ENOSPC);
			}
			if (skb->len >0)
			{
	/*		
	// jolly patch start
#warning note this: copy_*_user was called in locked context, so i unlock it. this workarround only works for single processor machines.
	*/
				spin_unlock_irqrestore(&dev->rport.lock, flags);
				if (copy_to_user(buf, skb->data , skb->len ))
				{
//				    efault:
					skb_queue_head(&dev->rport.queue, skb);
					return(-EFAULT);
				}
				spin_lock_irqsave(&dev->rport.lock, flags);
	// jolly patch stop
				len += skb->len ;
				buf += skb->len ;
			}
			else
				printk(KERN_ERR "B Channel skb length =%d\n", skb->len);
			dev_kfree_skb(skb);
		}
		
//		if (test_bit(FLG_AS_ISDNPORT_ONEFRAME, &dev->rport.Flag))
//			break;
	}
	*off += len;
	spin_unlock_irqrestore(&dev->rport.lock, flags);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_DEV_OP)
		printk(KERN_ERR  "%s : file(%d) %d\n",__FUNCTION__,  dev->minor, len);
#endif

	return(len);
}

static ssize_t _isdn_file_read(struct file *file, char *buf, size_t count, loff_t * off)
{
	AS_ISDNdevice_t	*dev = file->private_data;
	ssize_t		ret;

	if (!dev)
		return(-ENODEV);
	down(&dev->io_sema);
	ret = _do_isdn_read(file, buf, count, off);
//	ret = _do_isdn_read_one_by_one(file, buf, count, off);
	up(&dev->io_sema);
	return(ret);
}

static loff_t  _isdn_file_llseek(struct file *file, loff_t offset, int orig)
{
	return -ESPIPE;
}

static __inline__ ssize_t  _do_isdn_write(struct file *file, const char *buf, size_t count, loff_t * off)
{
	AS_ISDNdevice_t	*dev = file->private_data;
	size_t			len;
	u_long			flags;
	struct sk_buff	*skb;
	AS_ISDN_head_t	head;

//	TRACE();
	if (*off != file->f_pos)
	{
//	TRACE();
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

	/* check precondition : wport.queue */	
	spin_lock_irqsave(&dev->wport.lock, flags);
	while (skb_queue_len(&dev->wport.queue) >= dev->wport.maxqlen)
	{
		spin_unlock_irqrestore(&dev->wport.lock, flags);
		if (file->f_flags & O_NONBLOCK)
			return(-EAGAIN);
#if 0		
		interruptible_sleep_on(&(dev->wport.procq));
		if (signal_pending(current))
			return(-ERESTARTSYS);
#else
		as_schedule_delay( &(dev->wport.procq));
#endif
		spin_lock_irqsave(&dev->wport.lock, flags);
	}
	
	if (dev->minor == AS_ISDN_CORE_DEVICE)
	{/* core device */
//		TRACE();
		len = count;
		while (len >= AS_ISDN_HEADER_LEN)
		{
/*		
// jolly patch start
#warning note this: copy_*_user was called in locked context, so i unlock it. this workarround only works for single processor machines.
*/
			spin_unlock_irqrestore(&dev->rport.lock, flags);
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
		spin_unlock_irqrestore(&dev->wport.lock, flags);
		while ((skb = skb_dequeue(&dev->wport.queue)))
		{
			if ( _isdn_wdata_if(dev, skb))
				dev_kfree_skb(skb);
			wake_up(&dev->wport.procq);
		}
		test_and_clear_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag);
	}
	else
	{ /* raw device */
//		TRACE();
		skb = alloc_stack_skb(count, PORT_SKB_RESERVE);
		if (!skb)
		{
			spin_unlock_irqrestore(&dev->wport.lock, flags);
//		TRACE();
			return(0);
		}
//		TRACE();
/*		
// jolly patch start
#warning note this: copy_*_user was called in locked context, so i unlock it. this workarround only works for single processor machines.
*/
		spin_unlock_irqrestore(&dev->wport.lock, flags);
		if (copy_from_user(skb_put(skb, count), buf, count))
		{
//		TRACE();
			dev_kfree_skb(skb);
			return(-EFAULT);
		}
		spin_lock_irqsave(&dev->wport.lock, flags);
// jolly patch stop

//		TRACE();
		len = 0;
		skb_queue_tail(&dev->wport.queue, skb);
		if (test_and_set_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag))
		{
			spin_unlock_irqrestore(&dev->wport.lock, flags);
//		TRACE();
			return(count);
		}
		
//		TRACE();
		while ((skb = skb_dequeue(&dev->wport.queue)))
		{

#if AS_ISDN_DEBUG_CORE_DATA
			if (device_debug & DEBUG_DEV_OP)
				printk(KERN_ERR  "%s: wflg(%lx)\n", __FUNCTION__, dev->wport.Flag);
#endif

#if 0
			if (test_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag))
			{/* when B channel is blocked, return it back to wport->queue */
				skb_queue_head(&dev->wport.queue, skb); 
				printk(KERN_ERR "%s: wflg(%lx) BLOCKED\n", __FUNCTION__, dev->wport.Flag);
				break;
			}
#endif
			
//		TRACE();
			if (test_bit(FLG_AS_ISDNPORT_ENABLED, &dev->wport.Flag))
			{
				int ret;
//		TRACE();
				spin_unlock_irqrestore(&dev->wport.lock, flags);
				ret = if_newhead(&dev->wport.pif, PH_DATA | REQUEST, (int)skb, skb);
				spin_lock_irqsave(&dev->wport.lock, flags);
				/*added by lizhijie , 2005.12.18 */
				if( (ret == -EBUSY) && (test_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag)) )
				{/* when B channel is blocked, return it back to wport->queue */
					skb_queue_head(&dev->wport.queue, skb); 
#if 1//AS_ISDN_DEBUG_CORE_DATA
					printk(KERN_ERR "%s: wflg(%lx) BLOCKED\n", __FUNCTION__, dev->wport.Flag);
#endif
					test_and_clear_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag);
//					wake_up(&dev->wport.procq);
					break;
				}
				/* end of added */
					
				if (ret)
				{
//		TRACE();
					printk(KERN_WARNING "%s: dev(%d) down err(%d)\n", __FUNCTION__, dev->minor, ret);
					dev_kfree_skb(skb);
				}
				else
					test_and_set_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag);
			}
			else
			{
//				printk(KERN_WARNING "%s: dev(%d) wport not enabled\n", __FUNCTION__, dev->minor);
				printk(KERN_ERR "%s: dev(%d) wport not enabled\n", __FUNCTION__, dev->minor);
				dev_kfree_skb(skb);
			}
			wake_up(&dev->wport.procq);	/* comment by lizhijie.2005.12.21*/
		}
		
//		TRACE();
		test_and_clear_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag);
		spin_unlock_irqrestore(&dev->wport.lock, flags);
	}
	
	return(count - len);
}

static ssize_t _isdn_file_write(struct file *file, const char *buf, size_t count, loff_t * off)
{
	AS_ISDNdevice_t	*dev = file->private_data;
	ssize_t		ret;

	if (!dev)
		return(-ENODEV);
	down(&dev->io_sema);
	ret = _do_isdn_write(file, buf, count, off);
	up(&dev->io_sema);
	return(ret);
}

static unsigned int _isdn_file_poll(struct file *file, poll_table * wait)
{
	unsigned int	mask = POLLERR;
	AS_ISDNdevice_t	*dev = file->private_data;
	AS_ISDNport_t	*rport = (file->f_mode & FMODE_READ) ? &dev->rport : NULL;
	AS_ISDNport_t	*wport = (file->f_mode & FMODE_WRITE) ? &dev->wport : NULL;

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

static struct file_operations _isdn_fops =
{
	llseek:		_isdn_file_llseek,
	read:		_isdn_file_read,
	write:		_isdn_file_write,
	poll:		_isdn_file_poll,
//	ioctl:		AS_ISDN_ioctl,
	open:		_isdn_file_open,
	release:		_isdn_file_close,
};

static int from_up_down(AS_ISDNif_t *hif, struct sk_buff *skb)
{
	
	devicelayer_t	*dl;
	AS_ISDN_head_t	*hh; 
	int		retval = -EINVAL;

	if (!hif || !hif->fdata || !skb)
		return(-EINVAL);
	dl = hif->fdata;
	hh = AS_ISDN_HEAD_P(skb);
	hh->len = skb->len;
	hh->addr = dl->iaddr | IF_TYPE(hif);

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_RDATA)
		printk(KERN_ERR  "from_up_down: %x(%x) dinfo:%x len:%d\n", hh->prim, hh->addr, hh->dinfo, hh->len);
#endif

	retval = _isdn_rdata(dl->dev, skb);
	return(retval);
}

static int set_if(devicelayer_t *dl, u_int prim, AS_ISDNif_t *hif)
{
	int err = 0;

	err = AS_ISDN_SetIF(&dl->inst, hif, prim, from_up_down, from_up_down, dl);
	return(err);
}

static int udev_manager(void *data, u_int prim, void *arg)
{
	AS_ISDNinstance_t *inst = data;
	AS_ISDNdevice_t	*dev;
	devicelayer_t	*dl;
	int		err = -EINVAL;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "DEV-MGR : data:%p(inst:%s) prim:%x arg:%p\n", data,inst->name, prim, arg);
#endif

	if (!data)
		return(-EINVAL);
	read_lock(&AS_ISDN_device_lock);
	list_for_each_entry(dev, &AS_ISDN_devicelist, list)
	{
		list_for_each_entry(dl, &dev->layerlist, list)
		{
			if (&dl->inst == inst)
			{
				err = 0;
				break;
			}
		}
		if (!err)
			break;
	}
	if (err)
	{
		printk(KERN_WARNING "DEV-MGR : prim %x without device layer\n", prim);
		goto out;
	}
	
	switch(prim)
	{
		case MGR_CONNECT | REQUEST:
			err = AS_ISDN_ConnectIF(inst, arg);
			break;

		case MGR_SETIF | REQUEST:
		case MGR_SETIF | INDICATION:
			err = set_if(dl, prim, arg);
			break;

		case MGR_DISCONNECT | REQUEST:
		case MGR_DISCONNECT | INDICATION:
			err = AS_ISDN_DisConnectIF(inst, arg);
			break;

		case MGR_RELEASE | INDICATION:
#if AS_ISDN_DEBUG_CORE_DATA
			if (device_debug & DEBUG_MGR_FUNC)
				printk(KERN_ERR  "DEV-MGR : release_dev id %x\n", dl->inst.st->id);
#endif
			del_layer(dl);
			err = 0;
			break;
			
		default:
			printk(KERN_WARNING "DEV-MGR :  prim %x not handled\n", prim);
			err = -EINVAL;
			break;
	}
out:
	read_unlock(&AS_ISDN_device_lock);
	return(err);
}

/* object for udevice is used in every layers of B and D channels */
int init_isdn_device (int debug)
{
	int err,i;

	udev_obj.name = "ISDN-Dev";
	for (i=0; i<=MAX_LAYER_NR; i++)
	{
		udev_obj.DPROTO.protocol[i] = ISDN_PID_ANY;
		udev_obj.BPROTO.protocol[i] = ISDN_PID_ANY;
	}
	
	INIT_LIST_HEAD(&udev_obj.ilist);
	udev_obj.own_ctrl = udev_manager;
	device_debug = debug;
	if (register_chrdev(AS_ISDN_DEV_MAJOR, AS_ISDN_DEV_NAME, &_isdn_fops))
	{
		printk(KERN_WARNING "AS ISDN: Could not register devices %s\n", AS_ISDN_DEV_NAME);
		return(-EIO);
	}
	
	if ((err = as_isdn_register(&udev_obj)))
	{
		printk(KERN_ERR "Can't register %s error(%d)\n", udev_obj.name, err);
	}
	return(err);
}

int free_isdn_device(void)
{
	int 		err = 0;
	AS_ISDNdevice_t	*dev, *nd;

	if (!list_empty(&AS_ISDN_devicelist))
	{
		printk(KERN_WARNING "AS ISDN: devices open on remove\n");
		list_for_each_entry_safe(dev, nd, &AS_ISDN_devicelist, list)
		{
			free_device(dev);
		}
		err = -EBUSY;
	}
	if ((err = as_isdn_unregister(&udev_obj)))
	{
		printk(KERN_ERR "Can't unregister %s (%d)\n", udev_obj.name, err);
	}
	if ((err = unregister_chrdev(AS_ISDN_DEV_MAJOR, AS_ISDN_DEV_NAME)))
	{
		printk(KERN_WARNING "AS ISDN: devices %s busy on remove\n",AS_ISDN_DEV_NAME);
	}
	return(err);
}
