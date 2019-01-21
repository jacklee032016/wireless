/*
* $Id: as_frame_ops.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
*/

#include "core.h"

devicelayer_t *get_devlayer(as_isdn_device_t *dev, int addr);

/* set all instance flags of stack */
static int stack_inst_flg(as_isdn_device_t *dev, as_isdn_stack_t *st, int bit, int clear)
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
static int new_devstack(as_isdn_device_t *dev, stack_info_t *si)
{
	int		err;
	as_isdn_stack_t	*st;
	as_isdn_instance_t	inst;
	devicestack_t	*nds;

	memset(&inst, 0, sizeof(as_isdn_instance_t));
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

static int new_entity_req(as_isdn_device_t *dev, int *entity)
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

static int del_entity_req(as_isdn_device_t *dev, int entity)
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


/* get status info of instance */
static int get_status(struct sk_buff *skb)
{
	as_isdn_head_t	*hp;
	status_info_t	*si = (status_info_t *)skb->data;
	as_isdn_instance_t	*inst;
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
	as_isdn_head_t	*hp;
	as_isdn_instance_t 	*inst;
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
	as_isdn_head_t	*hp;
	as_isdn_instance_t	*inst;
	as_isdn_if_t		*hif;
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

/* put this skb into dev->rport.queue and wakeup user process */
int  as_isdn_rdata_frame(as_isdn_device_t *dev, struct sk_buff *skb)
{
	as_isdn_head_t	*hp;
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

static int error_answer(as_isdn_device_t *dev, struct sk_buff *skb, int err)
{
	as_isdn_head_t	*hp;

	hp = AS_ISDN_HEAD_P(skb);
	hp->prim |= 1; /* CONFIRM or RESPONSE */
	hp->len = err;
	return(as_isdn_rdata_frame(dev, skb));
}


/* write data frame with interface between instances 
* this case only is in no primitive is found in skb 
*/
static int wdata_frame(as_isdn_device_t *dev, struct sk_buff *skb)
{
	as_isdn_head_t	*hp;
	as_isdn_if_t	*hif = NULL;
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

static as_isdn_stack_t * sel_channel_by_addr(u_int addr, u_int channel)
{
	as_isdn_stack_t	*st;
	channel_info_t		ci;

	st = get_stack4id(addr);
	if (!st)
		return(NULL);
	ci.channel = channel;
	ci.st.p = NULL;
	if (udev_obj.ctrl(st, MGR_SELCHANNEL | REQUEST, &ci))
		return(NULL);
	return(ci.st.p);
}

static int create_layer(as_isdn_device_t *dev, struct sk_buff *skb)
{
	layer_info_t	*linfo;
	as_isdn_layer_t	*layer;
	as_isdn_stack_t	*st;
	int		i, ret;
	devicelayer_t	*nl;
	as_isdn_object_t	*obj;
	as_isdn_instance_t *inst = NULL;
	as_isdn_head_t		*hp;

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

/* main distributing process point for device, eg. enter into stack process 
 * All primitives are handled here. Used by D channel
*/
int  as_isdn_wdata_if(as_isdn_device_t *dev, struct sk_buff *skb)
{
	struct sk_buff	*nskb = NULL;
	as_isdn_head_t	*hp;
	as_isdn_stack_t		*st;
	devicelayer_t		*dl;
	as_isdn_layer_t    	*layer;
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
				memcpy(AS_ISDN_HEAD_P(nskb), hp, sizeof(as_isdn_head_t));
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
			st = sel_channel_by_addr(hp->addr, hp->dinfo);
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
				memcpy(AS_ISDN_HEAD_P(nskb), hp, sizeof(as_isdn_head_t));
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
				memcpy(AS_ISDN_HEAD_P(nskb), hp, sizeof(as_isdn_head_t));
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
			memcpy(AS_ISDN_HEAD_P(nskb), hp, sizeof(as_isdn_head_t));
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
		err = as_isdn_rdata_frame(dev, nskb);
		if (err)
			kfree_skb(nskb);
		else
			kfree_skb(skb);
	}
	else
		err = as_isdn_rdata_frame(dev, skb);
	
	return(err);
}

