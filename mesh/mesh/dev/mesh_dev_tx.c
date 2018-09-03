/*
* $Id: mesh_dev_tx.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "mesh.h"

/**
 *	Queue a buffer for transmission to a network device. The caller must
 *	have set the device and priority and built the buffer before calling this 
 *	function. The function can be called from an interrupt.
 *
 *	A negative errno code is returned on a failure. A success does not
 *	guarantee the frame will be transmitted as it may be dropped due
 *	to congestion or traffic shaping.
 */

/* entry point of TX frame, called by different module to send out frame */
int swjtu_mesh_queue_xmit(struct sk_buff *skb)
{
	MESH_DEVICE *dev = (MESH_DEVICE *)skb->dev;
#if WITH_QOS	
	struct Qdisc  *q;
#endif

#if 0
	if (skb_shinfo(skb)->frag_list && !(dev->features&NETIF_F_FRAGLIST) &&
	    skb_linearize(skb, GFP_ATOMIC) != 0)
	{
		kfree_skb(skb);
		return -ENOMEM;
	}

	/* Fragmented skb is linearized if device does not support SG,
	 * or if at least one of fragments is in highmem and device
	 * does not support DMA from it.
	 */
	if (skb_shinfo(skb)->nr_frags && (!(dev->features&NETIF_F_SG) 
		|| illegal_highdma(dev, skb)) && skb_linearize(skb, GFP_ATOMIC) != 0)
	{
		kfree_skb(skb);
		return -ENOMEM;
	}

	/* If packet is not checksummed and device does not support
	 * checksumming for this protocol, complete checksumming here.
	 */
	if (skb->ip_summed == CHECKSUM_HW &&
	    (!(dev->features&(NETIF_F_HW_CSUM|NETIF_F_NO_CSUM)) &&
	     (!(dev->features&NETIF_F_IP_CSUM) ||
	      skb->protocol != htons(ETH_P_IP))))
	{
		if ((skb = skb_checksum_help(skb)) == NULL)
			return -ENOMEM;
	}
#endif

	/* Grab device queue */
	spin_lock_bh(&dev->queue_lock);

//	ktrace;
#if WITH_QOS	
	q = dev->qdisc;
	if (q->enqueue)
	{/* with queue, then waiting TX tasklet send this frame */
		int ret = q->enqueue(skb, q);

		qdisc_run(dev);

		spin_unlock_bh(&dev->queue_lock);
		return ret == MESH_XMIT_BYPASS ? MESH_XMIT_SUCCESS : ret;
	}
#else
#if 0
	if(dev->type== MESH_TYPE_TRANSFER)
	{
		if( dev->tx_queue_len < MESH_TX_QUEUE_MAX )
		{
			__skb_queue_tail(&dev->txPktQueue, skb);
			dev->tx_queue_len++;
			ret = MESH_XMIT_SUCCESS;
		}
		else
		{
			kfree_skb(skb);
			ret = MESH_XMIT_DROP;
		}
		spin_unlock_bh(&dev->queue_lock);

		return ret;
	}
#endif

#endif

//	ktrace;
	/* send directly, 2006.09.23 */	 
	if (dev->flags& MESHF_UP)
	{/* send frame directly in the context of caller */
		int cpu = smp_processor_id();

//	ktrace;
//		if (dev->xmit_lock_owner != cpu)
		{
			spin_unlock(&dev->queue_lock);
			spin_lock(&dev->xmit_lock);
			dev->xmit_lock_owner = cpu;

//	ktrace;
			if (!meshif_queue_stopped(dev))
			{
#if 0			
				if (netdev_nit)
					dev_queue_xmit_nit(skb,dev);
#endif				
				/* send out */
//	ktrace;
				if (dev->hard_start_xmit(skb, dev) == 0)
				{
					dev->xmit_lock_owner = -1;
					spin_unlock_bh(&dev->xmit_lock);
					return MESH_XMIT_SUCCESS;
				}
			}
			
			dev->xmit_lock_owner = -1;
			spin_unlock_bh(&dev->xmit_lock);
#if 0
			if (net_ratelimit())
				printk(KERN_CRIT "Virtual device %s asks to queue packet!\n", dev->name);
#endif			
			kfree_skb(skb);
			return -ENETDOWN;
		}
#if 0
		else
		{
			/* Recursion is detected! It is possible, unfortunately */
			if (net_ratelimit())
				printk(KERN_CRIT "Dead loop on virtual device %s, fix it urgently!\n", dev->name);
		}
#endif		
	}
//	ktrace;
	
	spin_unlock_bh(&dev->queue_lock);

	kfree_skb(skb);
	return -ENETDOWN;
}

void swjtu_mesh_tx_action(unsigned long data)//struct softirq_action *h)
{
	int cpu = smp_processor_id();
//	MESH_MGR *mgr = (MESH_MGR *)data;

	if (softmesh_datas[cpu].completion_queue)
	{/* free all skb in complete_queue */
		struct sk_buff *clist;

		local_irq_disable();
		clist = softmesh_datas[cpu].completion_queue;
		softmesh_datas[cpu].completion_queue = NULL;
		local_irq_enable();

		while (clist != NULL)
		{
			struct sk_buff *skb = clist;
			clist = clist->next;

//			BUG_TRAP(atomic_read(&skb->users) == 0);
			__kfree_skb(skb);
		}
	}

	if (softmesh_datas[cpu].output_queue)
	{
		MESH_DEVICE *head;

		local_irq_disable();
		head = softmesh_datas[cpu].output_queue;
		softmesh_datas[cpu].output_queue = NULL;
		local_irq_enable();

		while (head != NULL)
		{
			MESH_DEVICE *dev = head;
			head = head->next_sched;

			smp_mb__before_clear_bit();
			clear_bit(__MESH_LINK_STATE_SCHED, &dev->state);

			if (spin_trylock(&dev->queue_lock))
			{
#if WITH_QOS			
				qdisc_run(dev);
#else
				MESH_DEBUG_INFO(MESH_DEBUG_PACKET, "MESH Queue TX is not implemeted fully now\n");
#endif
				spin_unlock(&dev->queue_lock);
			}
			else
			{/* can not lock queue, so schedule for next */
				meshif_schedule(dev);
			}
		}
	}
}


