/*
* $Id: mesh_dev_rx.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "mesh.h"

#if 0
static __inline__ void __skb_bond(struct sk_buff *skb)
{
	MESH_DEVICE *dev = (MESH_DEVICE *)skb->dev;

	if (dev->master)
	{
		skb->real_dev = skb->dev;
		skb->dev = dev->master;
	}
}
#endif

static __inline__ int handle_bridge(struct sk_buff *skb, MESH_PACKET *pt_prev)
{
	int ret = MESH_RX_DROP;

	if (pt_prev)
	{
#if 0	
		if (!pt_prev->data)
			ret = deliver_to_old_ones(pt_prev, skb, 0);
		else
#endif			
		{
			atomic_inc(&skb->users);
			ret = pt_prev->func(skb, (MESH_DEVICE *)skb->dev, pt_prev);
		}
	}

	return ret;
}

int __meshif_receive_skb(struct sk_buff *skb)
{
	MESH_PACKET *ptype, *pt_prev;
	int ret = MESH_RX_DROP;
	unsigned short type = skb->protocol;
	MESH_MGR *mgr = MESH_GET_MGR();

	if (skb->stamp.tv_sec == 0)
		do_gettimeofday(&skb->stamp);
#if 0
	skb_bond(skb);
#endif

	meshdev_rx_stat[smp_processor_id()].total++;

#ifdef CONFIG_NET_FASTROUTE
	if (skb->pkt_type == PACKET_FASTROUTE)
	{
		meshdev_rx_stat[smp_processor_id()].fastroute_deferred_out++;
		return swjtu_mesh_queue_xmit(skb);
	}
#endif

	skb->h.raw = skb->nh.raw = skb->data;

	pt_prev = NULL;
	for (ptype = mgr->ptype_all; ptype; ptype = ptype->next)
	{
		if (!ptype->dev || ptype->dev == (MESH_DEVICE *)skb->dev)
		{
			if (pt_prev)
			{
#if 0			
				if (!pt_prev->data)
				{
					ret = deliver_to_old_ones(pt_prev, skb, 0);
				}
				else
#endif					
				{
					atomic_inc(&skb->users);
					ret = pt_prev->func(skb, (MESH_DEVICE *)skb->dev, pt_prev);
				}
			}
			pt_prev = ptype;
		}
	}

#ifdef CONFIG_NET_DIVERT
	if (skb->dev->divert && skb->dev->divert->divert)
		ret = handle_diverter(skb);
#endif /* CONFIG_NET_DIVERT */
			
#if 0// defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
	if (skb->dev->br_port != NULL && br_handle_frame_hook != NULL)
	{
		int ret;

		ret = handle_bridge(skb, pt_prev);
		if (br_handle_frame_hook(skb) == 0)
			return ret;
		pt_prev = NULL;
	}
#endif

	for (ptype= mgr->ptype_base[ntohs(type)&15];ptype;ptype=ptype->next)
	{
		if (ptype->type == type && (!ptype->dev || ptype->dev == (MESH_DEVICE *)skb->dev))
		{
			if (pt_prev)
			{
#if 0		
				if (!pt_prev->data)
				{
					ret = deliver_to_old_ones(pt_prev, skb, 0);
				}
				else
#endif				
				{
					atomic_inc(&skb->users);
					ret = pt_prev->func(skb, (MESH_DEVICE *)skb->dev, pt_prev);
				}
			}
			pt_prev = ptype;
		}
	}

	if (pt_prev)
	{
#if 0	
		if (!pt_prev->data)
		{
			ret = deliver_to_old_ones(pt_prev, skb, 1);
		}
		else
#endif			
		{
			ret = pt_prev->func(skb, (MESH_DEVICE *)skb->dev, pt_prev);
		}
	}
	else
	{
		kfree_skb(skb);
		/* Jamal, now you will not able to escape explaining
		 * me how you were going to use this. :-)
		 */
		ret = MESH_RX_DROP;
	}

	return ret;
}

/* used only by blog_device */
int swjtu_mesh_process_backlog(MESH_DEVICE *backlog_dev, int *budget)
{
	int work = 0;
	int quota = min(backlog_dev->quota, *budget);
	int this_cpu = smp_processor_id();
	struct softmesh_data *queue = &softmesh_datas[this_cpu];
	unsigned long start_time = jiffies;

	for (;;)
	{
		struct sk_buff	*skb;
		MESH_DEVICE	*dev;

		local_irq_disable();
		skb = __skb_dequeue(&queue->input_pkt_queue);
		if (skb == NULL)
			goto job_done;
		local_irq_enable();

		dev = (MESH_DEVICE *)skb->dev;

		MESH_DEBUG_INFO(MESH_DEBUG_DATAIN, "skb for %s device in backlog process\n", (dev)?dev->name: "NULL");
		__meshif_receive_skb(skb);

//		dev_put(dev);
		__mesh_dev_put(dev);

		work++;

		if (work >= quota || jiffies - start_time > 1)
			break;

#ifdef CONFIG_NET_HW_FLOWCONTROL
		if (queue->throttle && queue->input_pkt_queue.qlen < no_cong_thresh )
		{
			queue->throttle = 0;
			if (atomic_dec_and_test(&netdev_dropping))
			{
				netdev_wakeup();
				break;
			}
		}
#endif
	}

	backlog_dev->quota -= work;
	*budget -= work;
	return -1;

job_done:
	backlog_dev->quota -= work;
	*budget -= work;

	/* no skb need to process, so remove it from poll list */
	MESH_LIST_DEL( &backlog_dev->poll_list);
	smp_mb__before_clear_bit();
	meshif_poll_enable(backlog_dev);

	if (queue->throttle)
	{
		queue->throttle = 0;
#ifdef CONFIG_NET_HW_FLOWCONTROL
		if (atomic_dec_and_test(&netdev_dropping))
			netdev_wakeup();
#endif
	}

	local_irq_enable();
	return 0;
}

void swjtu_mesh_rx_action(unsigned long data)//struct softirq_action *h)
{
	int this_cpu = smp_processor_id();
	struct softmesh_data  *queue = &softmesh_datas[this_cpu];
	unsigned long start_time = jiffies;
	int budget = meshdev_max_backlog;
	MESH_MGR	*mgr = (MESH_MGR *)mgr;//MESH_GET_MGR();

//	br_read_lock(BR_NETPROTO_LOCK);
	MESH_DEBUG_INFO(MESH_DEBUG_DATAIN, "RX tasklet is scheduled\n");
	MESH_READ_LOCK(mgr->rwLock);
	local_irq_disable();

	while (!MESH_LIST_CHECK_EMPTY(&queue->poll_list))
	{
		MESH_DEVICE *dev;

		if (budget <= 0 || jiffies - start_time > 1)
			goto softmesh_break;

		local_irq_enable();

		dev = MESH_LIST_ENTRY(queue->poll_list.next, MESH_DEVICE, poll_list);

		MESH_DEBUG_INFO(MESH_DEBUG_DATAIN, "data for device %s\n", dev->name );
		if (dev->quota <= 0 || dev->poll(dev, &budget))
		{
			local_irq_disable();
			MESH_LIST_DEL(&dev->poll_list);
			MESH_LIST_ADD_TAIL(&dev->poll_list, &queue->poll_list);
			if (dev->quota < 0)
				dev->quota += dev->weight;
			else
				dev->quota = dev->weight;
		}
		else
		{
//			dev_put(dev);
			__mesh_dev_put(dev);
			local_irq_disable();
		}
	}

	local_irq_enable();
//	br_read_unlock(BR_NETPROTO_LOCK);
	MESH_READ_UNLOCK(mgr->rwLock);
	return;

softmesh_break:
	meshdev_rx_stat[this_cpu].time_squeeze++;
//	__cpu_raise_softirq(this_cpu, NET_RX_SOFTIRQ);
	MESH_RX_SCHEDULE(&queue->blog_dev);

	local_irq_enable();
//	br_read_unlock(BR_NETPROTO_LOCK);
	MESH_READ_UNLOCK(mgr->rwLock);
}

/**
 *	MESH_RX_SUCCESS	(no congestion)           
 *	MESH_RX_CN_LOW     (low congestion) 
 *	MESH_RX_CN_MOD     (moderate congestion)
 *	MESH_RX_CN_HIGH    (high congestion) 
 *	MESH_RX_DROP    (packet was dropped)
 */
/* called by all mesh_device to send skb to forward tables */ 
int swjtu_meshif_rx(struct sk_buff *skb)
{
	int this_cpu = smp_processor_id();
	struct softmesh_data *queue;
	unsigned long flags;

	if (skb->stamp.tv_sec == 0)
		do_gettimeofday(&skb->stamp);

	queue = &softmesh_datas[this_cpu];

//	MESH_DEBUG_INFO( MESH_DEBUG_PACKET, "RX Data frame from Device\n");
	local_irq_save(flags);/* close IRQ on local CPU */

	meshdev_rx_stat[this_cpu].total++;

		
	if (queue->input_pkt_queue.qlen <= meshdev_max_backlog)
	{
		if (queue->input_pkt_queue.qlen)
		{
			if (queue->throttle)
				goto drop;

enqueue:
			mesh_dev_hold((MESH_DEVICE *)skb->dev);
			__skb_queue_tail(&queue->input_pkt_queue, skb);
			local_irq_restore(flags);
#if 0//ndef OFFLINE_SAMPLE
			get_sample_stats(this_cpu);
#endif
			/* enqueue this skb and return */
			return queue->cng_level;
		}

		if (queue->throttle)
		{
			queue->throttle = 0;
#ifdef CONFIG_NET_HW_FLOWCONTROL
			if (atomic_dec_and_test(&netdev_dropping))
				netdev_wakeup();
#endif
		}

		/* put blog_dev into softmesh_data->poll_list, and wakeup the softIRQ  */
		meshif_rx_schedule(&queue->blog_dev);
		/* go on to enqueue this skb and return */
		goto enqueue;
	}

	/* queue is full, so drop it */
	if (queue->throttle == 0)
	{/* and cpu must send as overload */
		queue->throttle = 1;
		meshdev_rx_stat[this_cpu].throttled++;
#ifdef CONFIG_NET_HW_FLOWCONTROL
		atomic_inc(&netdev_dropping);
#endif
	}

drop:
	meshdev_rx_stat[this_cpu].dropped++;
	local_irq_restore(flags);

	kfree_skb(skb);/* queue is full */
	return  MESH_RX_DROP;
}

