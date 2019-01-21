/*
* $Id: as_frame_if.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
*/
#include "core.h"


static as_isdn_instance_t *clone_instance(devicelayer_t *dl, as_isdn_stack_t  *st, as_isdn_instance_t *peer)
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
	dl->slave = (as_isdn_instance_t *)st;
	if ((err = peer->obj->own_ctrl(peer, MGR_CLONELAYER | REQUEST,	&dl->slave)))
	{
		dl->slave = NULL;
		printk(KERN_WARNING "%s: peer clone error %d\n",	__FUNCTION__, err);
		return(NULL);
	}
	return(dl->slave);
}

int from_up_down(as_isdn_if_t *hif, struct sk_buff *skb)
{
	
	devicelayer_t	*dl;
	as_isdn_head_t	*hh; 
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

	retval = as_isdn_rdata_frame(dl->dev, skb);
	return(retval);
}

int connect_if_req(as_isdn_device_t *dev, struct sk_buff *skb)
{
	devicelayer_t		*dl;
	interface_info_t	*ifi = (interface_info_t *)skb->data;
	as_isdn_instance_t		*owner;
	as_isdn_instance_t		*peer;
	as_isdn_instance_t		*pp;
	as_isdn_if_t		*hifp;
	int			stat;
	as_isdn_head_t		*hp;

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
			memcpy(&owner->up, hifp, sizeof(as_isdn_if_t));
			memcpy(&dl->s_up, hifp, sizeof(as_isdn_if_t));
			owner->up.owner = owner;
			hifp->peer = owner;
			hifp->func = from_up_down;
			hifp->fdata = dl;
			hifp = &pp->down;
			memcpy(&owner->down, hifp, sizeof(as_isdn_if_t));
			memcpy(&dl->s_down, hifp, sizeof(as_isdn_if_t));
			owner->down.owner = owner;
			hifp->peer = owner;
			hifp->func = from_up_down;
			hifp->fdata = dl;
		}
		else
		{
			memcpy(&owner->down, hifp, sizeof(as_isdn_if_t));
			memcpy(&dl->s_down, hifp, sizeof(as_isdn_if_t));
			owner->up.owner = owner;
			hifp->peer = owner;
			hifp->func = from_up_down;
			hifp->fdata = dl;
			hifp = &pp->up;
			memcpy(&owner->up, hifp, sizeof(as_isdn_if_t));
			memcpy(&dl->s_up, hifp, sizeof(as_isdn_if_t));
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

int set_if_req(as_isdn_device_t *dev, struct sk_buff *skb)
{
	as_isdn_if_t		*hif,*phif,*shif;
	int				stat;
	interface_info_t	*ifi = (interface_info_t *)skb->data;
	devicelayer_t		*dl;
	as_isdn_instance_t	*inst, *peer;
	as_isdn_head_t	*hp;

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
	memcpy(shif, phif, sizeof(as_isdn_if_t));
	memset(phif, 0, sizeof(as_isdn_if_t));
	return(peer->obj->own_ctrl(peer, hp->prim, hif));
}

int add_if_req(as_isdn_device_t *dev, struct sk_buff *skb)
{
	as_isdn_if_t		*hif;
	interface_info_t	*ifi = (interface_info_t *)skb->data;
	as_isdn_instance_t		*inst, *peer;
	as_isdn_head_t		*hp;

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

int del_if_req(as_isdn_device_t *dev, u_int addr)
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

int remove_if(devicelayer_t *dl, int stat)
{
	as_isdn_if_t *hif,*phif,*shif;
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
		memcpy(phif, shif, sizeof(as_isdn_if_t));
		memset(shif, 0, sizeof(as_isdn_if_t));
	}
	
	if (hif->predecessor)
		hif->predecessor->clone = hif->clone;

	if (hif->clone)
		hif->clone->predecessor = hif->predecessor;
	return(err);
}


