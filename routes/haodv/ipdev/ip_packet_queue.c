/*
* $Id: ip_packet_queue.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/
 
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/spinlock.h>
#include <linux/rtnetlink.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <net/route.h>
#include <net/sock.h>
#include <net/icmp.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#include "mesh_route.h"

#define IPQ_QMAX_DEFAULT 		1024
#define IPQ_PROC_FS_NAME 		"ip_queue"
#define NET_IPQ_QMAX 			2088
#define NET_IPQ_QMAX_NAME 	"ip_queue_maxlen"

typedef struct ipq_rt_info 
{
	__u8 				tos;
	__u32 				daddr;
	__u32 				saddr;
} ipq_rt_info_t;

typedef struct ipq_queue_element 
{
	struct list_head 		list;				/* Links element into queue */
	int 					verdict;			/* Current verdict */
	struct nf_info 		*info;			/* Extra info from netfilter */
	struct sk_buff 		*skb;			/* Packet inside */
	ipq_rt_info_t 			rt_info;			/* May need post-mangle routing */
} ipq_queue_element_t;

typedef int (*ipq_send_cb_t)(ipq_queue_element_t *e);

typedef struct ipq_queue 
{
	int 					len;				/* Current queue len */
	int 					*maxlen;		/* Maximum queue len, via sysctl */
	unsigned char 		flushing;		/* If queue is being flushed */
	unsigned char 		terminate;		/* If the queue is being terminated */
	struct list_head 		list;				/* Head of packet queue */
	spinlock_t 			lock;			/* Queue spinlock */
} ipq_queue_t;

static ipq_queue_t *q;
static int netfilter_receive(struct sk_buff *skb, struct nf_info *info, void *data);

/* Dequeue a packet if matched by cmp, or the next available if cmp is NULL */
static ipq_queue_element_t * ipq_dequeue( int (*cmp)(ipq_queue_element_t *, u_int32_t), u_int32_t data)
{
	struct list_head *i;

	spin_lock_bh(&q->lock);
	for (i = q->list.prev; i != &q->list; i = i->prev)
	{
		ipq_queue_element_t *e = (ipq_queue_element_t *)i;

		if (!cmp || cmp(e, data))
		{
			list_del(&e->list);
			q->len--;
			spin_unlock_bh(&q->lock);
			return e;
		}
	}
	spin_unlock_bh(&q->lock);
	return NULL;
}

/* Flush all packets */
static void ipq_flush()
{
	ipq_queue_element_t *e;

	spin_lock_bh(&q->lock);
	q->flushing = 1;
	spin_unlock_bh(&q->lock);

	while ((e = ipq_dequeue( NULL, 0)))
	{
		e->verdict = NF_DROP;
		nf_reinject(e->skb, e->info, e->verdict);
		kfree(e);
	}

	spin_lock_bh(&q->lock);
	q->flushing = 0;
	spin_unlock_bh(&q->lock);
}

static int ipq_create_queue(int *sysctl_qmax)
{
	int status;

	q = kmalloc(sizeof(ipq_queue_t), GFP_KERNEL);
	if (q == NULL)
	{
		return 0;
	}

	q->len = 0;
	q->maxlen = sysctl_qmax;
	q->flushing = 0;
	q->terminate = 0;
	
	INIT_LIST_HEAD(&q->list);
	spin_lock_init(&q->lock);

	status = nf_register_queue_handler(PF_INET, netfilter_receive, q);
	if (status < 0)
	{
		printk("Could not create Packet Queue! %d\n",status);
		kfree(q);
		return 0;
	}

	return 1;
}

static int ipq_enqueue(ipq_queue_t *g, struct sk_buff *skb, struct nf_info *info)
{
	ipq_queue_element_t *e;

	if (q!=g)
	{
		printk("trying to enqueue but do not have right queue!!!\n");
		return 0;
	}

	e = kmalloc(sizeof(*e), GFP_ATOMIC);
	if (e == NULL)
	{
		printk(KERN_ERR "ip_queue: OOM in enqueue\n");
		return -ENOMEM;
	}

	e->verdict = NF_DROP;
	e->info = info;
	e->skb = skb;

	if (e->info->hook == NF_IP_POST_ROUTING)
	{
		struct iphdr *iph = skb->nh.iph;
		e->rt_info.tos = iph->tos;
		e->rt_info.daddr = iph->daddr;
		e->rt_info.saddr = iph->saddr;
	}

	spin_lock_bh(&q->lock);
	if (q->len >= *q->maxlen)
	{
		spin_unlock_bh(&q->lock);
		if (net_ratelimit())
			printk(KERN_WARNING "ip_queue: full at %d entries, dropping packet(s).\n", q->len);
		goto free_drop;
	}

	if (q->flushing || q->terminate)
	{
		spin_unlock_bh(&q->lock);
		goto free_drop;
	}

	list_add(&e->list, &q->list);
	q->len++;
	spin_unlock_bh(&q->lock);

	return q->len;
free_drop:
	kfree(e);
	return -EBUSY;
}

static void ipq_destroy_queue()
{
	nf_unregister_queue_handler(PF_INET);

	spin_lock_bh(&q->lock);
	q->terminate = 1;
	spin_unlock_bh(&q->lock);
	ipq_flush();

	kfree(q);
}

/* With a chainsaw... */
static int route_me_harder(struct sk_buff *skb)
{
	struct iphdr *iph = skb->nh.iph;
	struct rtable *rt;

	struct rt_key key =
	{
                    dst		:	iph->daddr, 
			src		:	iph->saddr,
                    oif		:	skb->sk ? skb->sk->bound_dev_if : 0,
                    tos		:	RT_TOS(iph->tos)|RTO_CONN,
#ifdef CONFIG_IP_ROUTE_FWMARK
                    fwmark	:	skb->nfmark
#endif
	};

	if (ip_route_output_key(&rt, &key) != 0)
	{
		printk("route_me_harder: No more route.\n");
		return -EINVAL;
	}

	/* Drop old route. */
	dst_release(skb->dst);
	skb->dst = &rt->u.dst;

	return 0;
}

static inline int id_cmp(ipq_queue_element_t *e, u_int32_t id)
{
	return (id == (unsigned long )e);
}


static inline int dev_cmp(ipq_queue_element_t *e, u_int32_t ifindex)
{
	if (e->info->indev)
		if (e->info->indev->ifindex == ifindex)
			return 1;

	if (e->info->outdev)
		if (e->info->outdev->ifindex == ifindex)
			return 1;

	return 0;
}

static inline int ip_cmp(ipq_queue_element_t *e, u_int32_t ip)
{
	if (e->rt_info.daddr == ip)
		return 1;

	return 0;
}

/* Drop any queued packets associated with device ifindex */
static void ipq_dev_drop(int ifindex)
{
	ipq_queue_element_t *e;

	while ((e = ipq_dequeue(dev_cmp,  ifindex)))
	{
		e->verdict = NF_DROP;
		nf_reinject(e->skb, e->info, e->verdict);
		kfree(e);
	}
}

void ipq_send_ip(u_int32_t ip)
{
	ipq_queue_element_t *e;

	while ((e = ipq_dequeue( ip_cmp, ip)))
	{
		e->verdict = NF_ACCEPT;

		e->skb->nfcache |= NFC_ALTERED;
		route_me_harder(e->skb);
		nf_reinject(e->skb, e->info, e->verdict);

		kfree(e);
	}
}

void ipq_drop_ip(u_int32_t ip)
{
	ipq_queue_element_t *e;

	while ((e = ipq_dequeue( ip_cmp, ip))) 
	{
		e->verdict = NF_DROP;
		icmp_send(e->skb,ICMP_DEST_UNREACH,ICMP_HOST_UNREACH,0);
		nf_reinject(e->skb, e->info, e->verdict);
		kfree(e);
	}
}


/****************************************************************************
 * Netfilter interface
 ****************************************************************************/
/*
 * Packets arrive here from netfilter for queuing to userspace.
 * All of them must be fed back via nf_reinject() or Alexey will kill Rusty.
 */
int ipq_insert_packet(struct sk_buff *skb, struct nf_info *info)
{
	return ipq_enqueue(q, skb, info);
}

static int netfilter_receive(struct sk_buff *skb, struct nf_info *info, void *data)
{
	return ipq_enqueue((ipq_queue_t *)data, skb, info);
}

/****************************************************************************
 * System events
 ****************************************************************************/
static int receive_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;

	/* Drop any packets associated with the downed device */
	if (event == NETDEV_DOWN)
		ipq_dev_drop( dev->ifindex);
	return NOTIFY_DONE;
}

struct notifier_block ipq_dev_notifier = 
{
            receive_event,
            NULL,
            0
};

/****************************************************************************
 * Sysctl - queue tuning.
 ****************************************************************************/
static int sysctl_maxlen = IPQ_QMAX_DEFAULT;

static struct ctl_table_header *ipq_sysctl_header;

static ctl_table ipq_table[] = 
{
	{ NET_IPQ_QMAX, NET_IPQ_QMAX_NAME, &sysctl_maxlen,
		sizeof(sysctl_maxlen), 0644,  NULL, proc_dointvec },
	{ 0 }
};

static ctl_table ipq_dir_table[] = 
{
	{NET_IPV4, "ipv4", NULL, 0, 0555, ipq_table, 0, 0, 0, 0, 0},
	{ 0 }
};

static ctl_table ipq_root_table[] = 
{
	{CTL_NET, "net", NULL, 0, 0555, ipq_dir_table, 0, 0, 0, 0, 0},
	{ 0 }
};

/****************************************************************************
 * Module stuff.
 ****************************************************************************/
int init_packet_queue(void)
{
	int status = 0;
	//struct proc_dir_entry *proc;

	status= ipq_create_queue( &sysctl_maxlen);
	if (status == 0)
	{
		printk(KERN_ERR "ip_queue: initialisation failed: unable to create queue\n");
		return status;
	}

	/*
	proc = proc_net_create(IPQ_PROC_FS_NAME, 0, ipq_get_info);
    	if (proc)
    		proc->owner = THIS_MODULE;
    	else
    	{
    		ipq_destroy_queue(nlq);
    		sock_release(nfnl->socket);
    		return -ENOMEM;
    	}
    	*/

	ipq_sysctl_header = register_sysctl_table(ipq_root_table, 0);
	return status;
}

void cleanup_packet_queue(void)
{
	unregister_sysctl_table(ipq_sysctl_header);
	//proc_net_remove(IPQ_PROC_FS_NAME);
	ipq_destroy_queue(q);
}

