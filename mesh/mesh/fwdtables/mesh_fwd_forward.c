/*
 * $Id: mesh_fwd_forward.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "_fwd.h"

static inline int __should_deliver(MESH_DEVICE *dev, struct sk_buff *skb)
{
	if (dev == (MESH_DEVICE *)skb->dev )//||dev->state != BR_STATE_FORWARDING)
		return 0;

	return 1;
}

int __dev_queue_push_xmit(struct sk_buff *skb)
{
#ifdef CONFIG_NETFILTER
	if (skb->nf_bridge)
		memcpy(skb->data - 16, skb->nf_bridge->hh, 16);
#endif
	/* frame header must be rebuild ? */
//	skb_push(skb, ETH_HLEN);
	swjtu_mesh_queue_xmit(skb);

	return 0;
}

static void __fwd_deliver(MESH_DEVICE *to, struct sk_buff *skb)
{
	skb->dev = (struct net_device *)to;
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug = 0;
#endif
	MESH_DEBUG_INFO(MESH_DEBUG_DATAOUT, "frame are deliver to mesh device %s\n", to->name );

//	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev,
	__dev_queue_push_xmit(skb);
}

static void __fwd_forward(MESH_DEVICE *to, struct sk_buff *skb)
{
	MESH_DEVICE *indev;

	indev = (MESH_DEVICE *)skb->dev;
	skb->dev = (struct net_device *)to;
	skb->ip_summed = CHECKSUM_NONE;

	MESH_DEBUG_INFO(MESH_DEBUG_DATAOUT, "frame are forward to mesh device %s\n", to->name );
//	NF_HOOK(PF_BRIDGE, NF_BR_FORWARD, skb, indev, skb->dev,
	__dev_queue_push_xmit(skb);
}

/* called under bridge lock; send with the device which has been set in skb, eg. not change device of skb */
void fwd_deliver(MESH_DEVICE *to, struct sk_buff *skb)
{
	if (__should_deliver(to, skb))
	{
		__fwd_deliver(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called under bridge lock, device of skb must be replaced with 'to' device */
void fwd_forward(MESH_DEVICE *to, struct sk_buff *skb)
{
	if (__should_deliver(to, skb) )
	{
		__fwd_forward(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called under bridge lock */
static void __fwd_flood(MESH_FWD_TABLE *table, struct sk_buff *skb, int clone,
	void (*__packet_hook)(MESH_DEVICE *dev, struct sk_buff *skb))
{
	MESH_DEVICE *p, *prev;
	MESH_MGR	*mgr = table->mgr;

	if (clone)
	{
		struct sk_buff *skb2;

		if ((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL)
		{
			table->mib.txDropped++;
			return;
		}

		skb = skb2;
	}

	prev = NULL;

	MESH_READ_LOCK(&mgr->lock);
#if 0	
	MESH_LIST_FOR_EACH(p, &(mgr->ports), node )
	{
		if(__should_deliver(p, skb))
		{
			if (prev != NULL)
			{
				struct sk_buff *skb2;

				if ((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL)
				{
					table->mib.txDropped++;
					kfree_skb(skb);
					MESH_READ_UNLOCK(&mgr->lock);
					return;
				}

				__packet_hook(prev, skb2);
			}
			prev = p;
		}
	}
	
	if (prev != NULL)
	{/* the last device use this skb(save one time of clone) */
		__packet_hook(prev, skb);
		MESH_READ_UNLOCK(&mgr->lock);
		return;
	}
#else
	MESH_LIST_FOR_EACH(p, &(mgr->ports), node )
	{
		if(__should_deliver(p, skb))
		{
			struct sk_buff *skb2;

			if ((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL)
			{
				table->mib.txDropped++;
				kfree_skb(skb);
				MESH_READ_UNLOCK(&mgr->lock);
				return;
			}

			__packet_hook(p, skb2);
		}
	}
	
#endif
	MESH_READ_UNLOCK(&mgr->lock);

//	kfree_skb(skb);
}

/* called under lock */
void mesh_fwd_flood_deliver(MESH_FWD_TABLE *table, struct sk_buff *skb, int clone)
{
	__fwd_flood(table, skb, clone, __fwd_deliver);
}

/* called under lock */
void mesh_fwd_flood_forward(MESH_FWD_TABLE *table, struct sk_buff *skb, int clone)
{
	__fwd_flood(table, skb, clone, __fwd_forward);
}


