/*
 * $Id: mesh_dev.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
#ifndef __MESH_DEVICE_H__
#define __MESH_DEVICE_H__

#include <linux/interrupt.h>

#define 	MESH_MAX_ADDR_LEN		8		/* Largest hardware address length */
#define 	MESH_LL_MAX_HEADER		48

#define	MESH_TX_QUEUE_MAX			100

#define	MESH_XMIT_SUCCESS	0
#define	MESH_XMIT_DROP		1	/* skb dropped			*/
#define	MESH_XMIT_CN			2	/* congestion notification	*/
#define	MESH_XMIT_POLICED		3	/* skb is shot by police	*/
#define	MESH_XMIT_BYPASS		4	/* packet does not leave via dequeue;
					   (TC use only - dev_queue_xmit returns this as NET_XMIT_SUCCESS) */

/* Backlog congestion levels */
#define	MESH_RX_SUCCESS		0   /* keep 'em coming, baby */
#define	MESH_RX_DROP			1  /* packet dropped */
#define	MESH_RX_CN_LOW		2   /* storm alert, just in case */
#define	MESH_RX_CN_MOD		3   /* Storm on its way! */
#define	MESH_RX_CN_HIGH		4   /* The storm is here */
#define	MESH_RX_BAD			5  /* packet dropped due to kernel error */

#define net_xmit_errno(e)	((e) != NET_XMIT_CN ? -ENOBUFS : 0)


/* tag multicasts with these structures. */
struct mesh_dev_mc_list
{	
	struct mesh_dev_mc_list		*next;
	__u8						dmi_addr[MESH_MAX_ADDR_LEN];
	unsigned char					dmi_addrlen;
	int							dmi_users;
	int							dmi_gusers;
};

struct mesh_hh_cache
{
	struct mesh_hh_cache 				*hh_next;	/* Next entry			     */
	atomic_t							hh_refcnt;	/* number of users                   */
	unsigned short  					hh_type;	/* protocol identifier, f.e ETH_P_IP
                                         *  NOTE:  For VLANs, this will be the
                                         *  encapuslated type. --BLG
                                         */
	int								hh_len;		/* length of header */
	int								(*hh_output)(struct sk_buff *skb);
	rwlock_t							hh_lock;

	/* cached hardware header; allow for machine alignment needs.        */
#define HH_DATA_MOD	16
#define HH_DATA_OFF(__len) \
	(HH_DATA_MOD - ((__len) & (HH_DATA_MOD - 1)))
#define HH_DATA_ALIGN(__len) \
	(((__len)+(HH_DATA_MOD-1))&~(HH_DATA_MOD - 1))
	unsigned long						hh_data[HH_DATA_ALIGN(MESH_LL_MAX_HEADER) / sizeof(long)];
};

enum meshdev_state_t
{
	__MESH_LINK_STATE_XOFF = 0,
	__MESH_LINK_STATE_START,
	__MESH_LINK_STATE_PRESENT,
	__MESH_LINK_STATE_SCHED,
	__MESH_LINK_STATE_NOCARRIER,
	__MESH_LINK_STATE_RX_SCHED
};

#if 1
#define	MESH_TYPE_PORTAL		0x00000001	/* Portal */
#define	MESH_TYPE_MAP			0x00000002	/* MAP */
#define	MESH_TYPE_TRANSFER	0x00000004
#else
typedef enum
{
	MESH_TYPE_PORTAL = 0,		/* Portal node */
	MESH_TYPE_TRANSFER		/* normal transfer node */
}mesh_dev_type_t;
#endif

struct _mesh_device_stats
{
	unsigned long	rx_packets;		/* total packets received	*/
	unsigned long	tx_packets;		/* total packets transmitted	*/
	unsigned long	rx_bytes;		/* total bytes received 	*/
	unsigned long	tx_bytes;		/* total bytes transmitted	*/
	unsigned long	rx_errors;		/* bad packets received		*/
	unsigned long	tx_errors;		/* packet transmit problems	*/
	unsigned long	rx_dropped;		/* no space in linux buffers	*/
	unsigned long	tx_dropped;		/* no space available in linux	*/
	unsigned long	multicast;		/* multicast packets received	*/
	unsigned long	collisions;

	/* detailed rx_errors: */
	unsigned long	rx_length_errors;
	unsigned long	rx_over_errors;		/* receiver ring buff overflow	*/
	unsigned long	rx_crc_errors;		/* recved pkt with crc error	*/
	unsigned long	rx_frame_errors;	/* recv'd frame alignment error */
	unsigned long	rx_fifo_errors;		/* recv'r fifo overrun		*/
	unsigned long	rx_missed_errors;	/* receiver missed packet	*/

	/* detailed tx_errors */
	unsigned long	tx_aborted_errors;
	unsigned long	tx_carrier_errors;
	unsigned long	tx_fifo_errors;
	unsigned long	tx_heartbeat_errors;
	unsigned long	tx_window_errors;
	
	/* for cslip etc */
	unsigned long	rx_compressed;
	unsigned long	tx_compressed;
};


struct _mesh_mgr;


struct _mesh_device
{
	mesh_inode				dev;
	
	char						name[MESHNAMSIZ];	/* 16 bytes */
	int						index;			/* is the minor number of device file */
//	mesh_dev_type_t			type;
	int						type;

	/* These may be needed for future network-power-down code. */
	unsigned long				trans_start;	/* Time (in jiffies) of last Tx	*/
	unsigned long				last_rx;	/* Time of last Rx	*/

	unsigned short			flags;	/* interface flags (a la BSD)	*/

	unsigned long				rmem_end;	/* shmem "recv" end	*/
	unsigned long				rmem_start;	/* shmem "recv" start	*/
	unsigned long				mem_end;	/* shared mem end	*/
	unsigned long				mem_start;	/* shared mem start	*/
	unsigned long				base_addr;	/* device I/O address	*/
	unsigned int				irq;		/* device IRQ number	*/

	unsigned long				state;
	
	struct _mesh_device		*next;
	
	/* The device initialization function. Called only once. */
	int						(*init)(struct _mesh_device *dev);

	/* ------- Fields preinitialized in Space.c finish here ------- */

	struct _mesh_device		*next_sched;

	/* Interface index. Unique device identifier	*/
	int						ifindex;
	int						iflink;

	struct _mesh_device_stats* (*get_stats)(struct _mesh_device *dev);

	struct meshw_statistics*		(*get_wireless_stats)(struct _mesh_device *dev);
	struct meshw_handler_def *	wireless_handlers;
	
	/* Interface address info. */
	unsigned char				broadcast[MESH_MAX_ADDR_LEN];	/* hw bcast add	*/
	unsigned char				dev_addr[MESH_MAX_ADDR_LEN];	/* hw address	*/
	unsigned char				addr_len;	/* hardware address length	*/

	struct mesh_dev_mc_list	*mc_list;	/* Multicast mac addresses	*/
	int						mc_count;	/* Number of installed mcasts	*/
	int						promiscuity;
	int						allmulti;

	int						watchdog_timeo;
	struct timer_list			watchdog_timer;

	struct list_head			poll_list;	/* Link to poll list	*/
	int						quota;
	int						weight;

#if 	WITH_QOS
	struct Qdisc				*qdisc;
	struct Qdisc				*qdisc_sleeping;
	struct Qdisc				*qdisc_list;
	struct Qdisc				*qdisc_ingress;
#else
	struct sk_buff_head		txPktQueue;
#endif

	unsigned long				tx_queue_len;	/* Max frames per queue allowed */

	/* hard_start_xmit synchronizer */
	spinlock_t				xmit_lock;
	/* cpu id of processor entered to hard_start_xmit or -1,
	   if nobody entered there.
	 */
	int						xmit_lock_owner;
	/* device queue lock */
	spinlock_t				queue_lock;
	/* Number of references to this device */
	atomic_t					refcnt;
	/* The flag marking that device is unregistered, but held by an user */
	int						deadbeaf;

	/* Called after device is detached from network. */
	void						(*uninit)(struct _mesh_device *dev);
	/* Called after last user reference disappears. */
	void						(*destructor)(struct _mesh_device *dev);

	/* Pointers to interface service routines.	*/
	int						(*open)(struct _mesh_device *dev);
	int						(*stop)(struct _mesh_device *dev);
	int						(*hard_start_xmit) (struct sk_buff *skb, struct _mesh_device *dev);
#define HAVE_NETDEV_POLL
	int						(*poll) (struct _mesh_device *dev, int *quota);
	int						(*hard_header) (struct sk_buff *skb, struct _mesh_device *dev,
								unsigned short type, void *daddr,	void *saddr,	unsigned len);
	int						(*rebuild_header)(struct sk_buff *skb);
#define HAVE_MULTICAST			 
	void						(*set_multicast_list)(struct _mesh_device *dev);
#define HAVE_SET_MAC_ADDR  		 
	int						(*set_mac_address)(struct _mesh_device *dev, void *addr);
#define HAVE_PRIVATE_IOCTL
	int						(*do_ioctl)(struct _mesh_device *dev,unsigned long data, int cmd);
#define HAVE_SET_CONFIG
	int						(*set_config)(struct _mesh_device *dev,  struct ifmap *map);
#if 0
#define HAVE_HEADER_CACHE
	int						(*hard_header_cache)(struct neighbour *neigh, struct hh_cache *hh);
	void						(*header_cache_update)(struct hh_cache *hh, struct _mesh_device *dev, unsigned char *haddr);
#endif

#define HAVE_CHANGE_MTU
	int						(*change_mtu)(struct _mesh_device *dev, int new_mtu);

#define HAVE_TX_TIMEOUT
	void						(*tx_timeout) (struct _mesh_device *dev);

	int						(*hard_header_parse)(struct sk_buff *skb,  unsigned char *haddr);
#if 0	
	int						(*neigh_setup)(struct _mesh_device *dev, struct neigh_parms *);
	int						(*accept_fastpath)(struct _mesh_device *, struct dst_entry*);
#endif

	void						*macStack;
	void						*phyStack;

	mesh_proc_entry			procEntry;
	
	unsigned					mtu;	/* interface MTU value		*/
//	unsigned short			type;	/* interface hardware type	*/
	unsigned short			hard_header_len;	/* hardware hdr length	*/

	void						*priv;	/* pointer to private data	*/

	struct _mesh_mgr			*mgr;

	struct _mesh_fwd_table	*fwdTable;

	struct module 			*owner;
	MESH_LIST_NODE			node;
};

extern struct _mesh_device		meshloopback_dev;		/* The loopback */
extern struct _mesh_device		*meshDevBase;		/* All devices */
extern rwlock_t				meshDevBaseLock;		/* Device list lock */


struct meshif_rx_stats
{
	unsigned		total;
	unsigned		dropped;
	unsigned		time_squeeze;
	unsigned		throttled;
	unsigned		fastroute_hit;
	unsigned		fastroute_success;
	unsigned		fastroute_defer;
	unsigned		fastroute_deferred_out;
	unsigned		fastroute_latency_reduction;
	unsigned		cpu_collision;
} ____cacheline_aligned;

extern struct meshif_rx_stats  meshdev_rx_stat[];

/*
 * Incoming packets are placed on per-cpu queues so that
 * no locking is needed.
 */
struct softmesh_data
{
	int						throttle;			/* bool, is cpu overload */
	int						cng_level;
	int						avg_blog;
	struct sk_buff_head		input_pkt_queue;
	struct list_head			poll_list;
	struct _mesh_device		*output_queue;
	struct sk_buff			*completion_queue;

	struct _mesh_device		blog_dev;
} ____cacheline_aligned;

struct	_mesh_packet_type 
{
	unsigned short		type;	/* This is really htons(ether_type).	*/
	struct _mesh_device	*dev;	/* NULL is wildcarded here		*/
	int			(*func) (struct sk_buff *, struct _mesh_device *, struct _mesh_packet_type *);
	void			*data;	/* Private to the packet type		*/
	struct _mesh_packet_type	*next;
};


#define __mesh_dev_put(dev) 		atomic_dec(&(dev)->refcnt)
#define mesh_dev_hold(dev) 			atomic_inc(&(dev)->refcnt)

#define	MESH_RX_SCHEDULE(dev)		\
	MESH_TASK_SCHEDULE(&(dev)->mgr->rxTask, NULL)

#define	MESH_TX_SCHEDULE(dev)		\
	MESH_TASK_SCHEDULE(&(dev)->mgr->txTask, NULL)


extern struct softmesh_data softmesh_datas[NR_CPUS];

#define HAVE_NETIF_QUEUE

void __meshdev_watchdog_up(struct _mesh_device *dev);

/* schdule TX of this device and place the previous as next */
static inline void __meshif_schedule(struct _mesh_device *dev)
{
	if (!test_and_set_bit(__MESH_LINK_STATE_SCHED, &dev->state))
	{
		unsigned long flags;
		int cpu = smp_processor_id();

		local_irq_save(flags);
		dev->next_sched = softmesh_datas[cpu].output_queue;
		softmesh_datas[cpu].output_queue = dev;
		
//		cpu_raise_softirq(cpu, NET_TX_SOFTIRQ);
		MESH_TX_SCHEDULE(dev);

		local_irq_restore(flags);
	}
}

/* schedule TX */
static inline void meshif_schedule(struct _mesh_device *dev)
{
	if (!test_bit(__MESH_LINK_STATE_XOFF, &dev->state))
		__meshif_schedule(dev);
}

static inline void meshif_start_queue(struct _mesh_device *dev)
{
	clear_bit(__MESH_LINK_STATE_XOFF, &dev->state);
}

static inline void meshif_wake_queue(struct _mesh_device *dev)
{
	if (test_and_clear_bit(__MESH_LINK_STATE_XOFF, &dev->state))
		__meshif_schedule(dev);
}

static inline void meshif_stop_queue(struct _mesh_device *dev)
{
	set_bit(__MESH_LINK_STATE_XOFF, &dev->state);
}

static inline int meshif_queue_stopped(struct _mesh_device *dev)
{
	return test_bit(__MESH_LINK_STATE_XOFF, &dev->state);
}

static inline int meshif_running(struct _mesh_device *dev)
{
	return test_bit(__MESH_LINK_STATE_START, &dev->state);
}

/* Use this variant when it is known for sure that it
 * is executing from interrupt context. eg. free skb in ISR context(only tag it)
 */
static inline void meshdev_kfree_skb_irq(struct sk_buff *skb)
{
	if (atomic_dec_and_test(&skb->users))
	{
		int cpu =smp_processor_id();
		unsigned long flags;

		local_irq_save(flags);
		/* put this skb on complete_queue which will be remove when next TX task execute */
		skb->next = softmesh_datas[cpu].completion_queue;
		softmesh_datas[cpu].completion_queue = skb;
		
//		cpu_raise_softirq(cpu, NET_TX_SOFTIRQ);
		MESH_TX_SCHEDULE(& softmesh_datas[cpu].blog_dev);

		local_irq_restore(flags);
	}
}

/* Use this variant in places where it could be invoked
 * either from interrupt or non-interrupt context.
 */
static inline void mshdev_kfree_skb_any(struct sk_buff *skb)
{
	if (in_irq())
		meshdev_kfree_skb_irq(skb);
	else
		dev_kfree_skb(skb);
}

/* Carrier loss detection, dial on demand. The functions netif_carrier_on
 * and _off may be called from IRQ context, but it is caller
 * who is responsible for serialization of these calls.
 */

static inline int meshif_carrier_ok(struct _mesh_device *dev)
{
	return !test_bit(__MESH_LINK_STATE_NOCARRIER, &dev->state);
}

static inline void meshif_carrier_on(struct _mesh_device *dev)
{
	clear_bit(__MESH_LINK_STATE_NOCARRIER, &dev->state);
	
	if (meshif_running(dev))
		__meshdev_watchdog_up(dev);
}

static inline void meshif_carrier_off(struct _mesh_device *dev)
{
	set_bit(__MESH_LINK_STATE_NOCARRIER, &dev->state);
}

/* Hot-plugging. */
static inline int meshif_device_present(struct _mesh_device *dev)
{
	return test_bit(__MESH_LINK_STATE_PRESENT, &dev->state);
}

static inline void meshif_device_detach(struct _mesh_device *dev)
{
	if (test_and_clear_bit(__MESH_LINK_STATE_PRESENT, &dev->state) &&
	    meshif_running(dev)) 
	{
		meshif_stop_queue(dev);
	}
}

static inline void meshif_device_attach(struct _mesh_device *dev)
{
	if (!test_and_set_bit(__MESH_LINK_STATE_PRESENT, &dev->state) &&
	    meshif_running(dev))
	{
		meshif_wake_queue(dev);
 		__meshdev_watchdog_up(dev);
	}
}

/* Schedule rx intr now? : whether rx has been schduled now*/
static inline int meshif_rx_schedule_prep(struct _mesh_device *dev)
{
	return meshif_running(dev) &&
		!test_and_set_bit(__MESH_LINK_STATE_RX_SCHED, &dev->state);
}

/* Add interface to tail of rx poll list. This assumes that _prep has
 * already been called and returned 1.
 * Only blog_dev and other device which implemented poll() can call this function to poll in RX tasklet
 */
static inline void __meshif_rx_schedule(struct _mesh_device *dev)
{
	unsigned long flags;
	int cpu = smp_processor_id();

	local_irq_save(flags);
	mesh_dev_hold(dev);
	MESH_LIST_ADD_TAIL( &dev->poll_list, &softmesh_datas[cpu].poll_list);

	if (dev->quota < 0)
		dev->quota += dev->weight;
	else
		dev->quota = dev->weight;

//	__cpu_raise_softirq(cpu, NET_RX_SOFTIRQ);
	MESH_RX_SCHEDULE(dev);
	local_irq_restore(flags);
}

/* Try to reschedule poll. Called by irq handler. */
/* softmesh_data->blog_dev and other device with poll() is set can call this function */
static inline void meshif_rx_schedule(struct _mesh_device *dev)
{
	if (meshif_rx_schedule_prep(dev))
		__meshif_rx_schedule(dev);
}

/* Try to reschedule poll. Called by dev->poll() after netif_rx_complete().
 * Do not inline this?
 */
static inline int meshif_rx_reschedule(struct _mesh_device *dev, int undo)
{
	if (meshif_rx_schedule_prep(dev))
	{
		unsigned long flags;
		int cpu = smp_processor_id();

		dev->quota += undo;

		local_irq_save(flags);
		MESH_LIST_ADD_TAIL(&dev->poll_list, &softmesh_datas[cpu].poll_list);
		
//		__cpu_raise_softirq(cpu, NET_RX_SOFTIRQ);
		MESH_RX_SCHEDULE(dev);

		local_irq_restore(flags);
		return 1;
	}
	return 0;
}

/* Remove interface from poll list: it must be in the poll list
 * on current cpu. This primitive is called by dev->poll(), when
 * it completes the work. The device cannot be out of poll list at this
 * moment, it is BUG().
 */
static inline void meshif_rx_complete(struct _mesh_device *dev)
{
	unsigned long flags;

	local_irq_save(flags);
	if (!test_bit(__MESH_LINK_STATE_RX_SCHED, &dev->state)) 
		BUG();
	
	MESH_LIST_DEL( &dev->poll_list);

	smp_mb__before_clear_bit();
	clear_bit(__MESH_LINK_STATE_RX_SCHED, &dev->state);
	local_irq_restore(flags);
}

static inline void meshif_poll_disable(struct _mesh_device *dev)
{
	while (test_and_set_bit(__MESH_LINK_STATE_RX_SCHED, &dev->state))
	{
		/* No hurry. */
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(1);
	}
}

static inline void meshif_poll_enable(struct _mesh_device *dev)
{
	clear_bit(__MESH_LINK_STATE_RX_SCHED, &dev->state);
}

/* same as netif_rx_complete, except that local_irq_save(flags)
 * has already been issued
 */
static inline void __meshif_rx_complete(struct _mesh_device *dev)
{
	if (!test_bit(__MESH_LINK_STATE_RX_SCHED, &dev->state))
		BUG();
	
	MESH_LIST_DEL( &dev->poll_list);

	smp_mb__before_clear_bit();
	clear_bit(__MESH_LINK_STATE_RX_SCHED, &dev->state);
}

static inline void meshif_tx_disable(struct _mesh_device *dev)
{
	spin_lock_bh(&dev->xmit_lock);
	meshif_stop_queue(dev);
	spin_unlock_bh(&dev->xmit_lock);
}

static inline void free_meshdev(struct _mesh_device *dev)
{
	kfree(dev);
}

typedef	struct _mesh_device_stats	MESH_DEVICE_STATS;
typedef	struct _mesh_device		MESH_DEVICE;
typedef	struct _mesh_packet_type	MESH_PACKET;


#endif

