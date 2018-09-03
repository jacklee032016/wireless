/*
* $Id: mesh_route.h,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
*/
 
#ifndef __MESH_ROUTE_H__
#define __MESH_ROUTE_H__


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include "aodv_protocol.h"

#define ROUTE_DEBUG					1

#define trace			printk("%s->%s(%d lines)\r\n", __FILE__, __FUNCTION__, __LINE__)

#define MY_ETH_ALEN		16


#if ROUTE_DEBUG
#define	ROUTE_DEBUG_INIT				0x00000001		/*  */
#define 	ROUTE_DEBUG_DEV				0x00000002		/* Route Device */
#define	ROUTE_DEBUG_ENGINE			0x00000004
#define 	ROUTE_DEBUG_ROUTE			0x00000008

#define	ROUTE_DEBUG_RREQ				0x00000010		/*  */
#define	ROUTE_DEBUG_RREP				0x00000020		/*  */
#define	ROUTE_DEBUG_RERR				0x00000040		/*  */
#define	ROUTE_DEBUG_RREP_ACK			0x00000080		/*  */

#define 	ROUTE_DEBUG_TASK				0x00000100
#define	ROUTE_DEBUG_NEIGH			0x00000200
#define	ROUTE_DEBUG_TIMER			0x00000400
#define	ROUTE_DEBUG_FLOOD			0x00000800

#define	ROUTE_DPRINTF(_engine, _m, _fmt, ...) do {	\
	if (_engine->debug & (_m))			\
		printk("%s : " _fmt, _engine->name,  __VA_ARGS__);		\
} while (0)
#define	ROUTE_DPRINTF_ADDRESS(_engine, _m, _fmt1, _fmt2, _address1, _address2 ) do {	\
	if (_engine->debug & (_m)){			\
		printk("%s : " _fmt1, _engine->name,  _address1 );		\
		printk( _fmt2,  _address2);}		\
} while (0)
#else
#define	ROUTE_DPRINTF(_engine, _m, _fmt, ...)
#define	ROUTE_DPRINTF_ADDRESS(_engine, _m, _fmt1, _fmt2, _address1, _address2 ) 
#endif


#define TRUE								1
#define FALSE 							0

#define ROUTE_LIST_HEAD				LIST_HEAD
#define ROUTE_LIST_NODE				struct list_head
#define ROUTE_LIST_FOR_EACH			list_for_each_entry
#define ROUTE_LIST_FOR_EACH_SAFE		list_for_each_entry_safe
#define ROUTE_LIST_ENTRY				list_entry
#define ROUTE_LIST_ADD_HEAD			list_add				/* stack : FILO */
#define ROUTE_LIST_ADD_TAIL			list_add_tail			/* queue : FIFO */
#define ROUTE_LIST_DEL					list_del
//#define ROUTE_LIST_DEL				
#define ROUTE_LIST_CHECK_EMPTY		list_empty


#define ROUTE_PROC_NAME		"HWMP"

typedef int (*route_read_proc_t)(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data);

typedef struct
{
	unsigned char			name[32];
	route_read_proc_t		read;
	struct proc_dir_entry	*proc;
}route_proc_entry;


struct _route_engine;
struct _route_dev;

struct _route_neigh;
struct _mesh_route ;
struct _route_task;

typedef enum
{
	ROUTE_OP_FORWARD					= 1,
	ROUTE_OP_QUEUE					= 2,
	ROUTE_OP_DENY						= 3
}route_op_type_t;


typedef enum
{
	ROUTE_TASK_RREQ					= 1,
	ROUTE_TASK_RREP					= 2,
	ROUTE_TASK_RERR					= 3,
	ROUTE_TASK_RREP_ACK				= 4,

	ROUTE_TASK_RESEND_RREQ			= 101,
	ROUTE_TASK_HELLO					= 102,
	ROUTE_TASK_NEIGHBOR				= 103,
	ROUTE_TASK_CLEANUP				= 104,
	ROUTE_TASK_ROUTE_CLEANUP		= 105
}route_task_type_t;


struct _flood_id 
{
	u_int32_t 				src_ip;
	u_int32_t 				dst_ip;

	u_int32_t 				id;
	unsigned long  			lifetime;

	ROUTE_LIST_NODE		list;
};
typedef struct _flood_id route_flood_t;

typedef enum
{
	ROUTE_TYPE_KERNEL		=	1,		/* route entry from kernel route, eg. net_device */
	ROUTE_TYPE_AODV		=	2		/* route entry created by AODV Protocol */
}route_type_t;

struct _mesh_route 
{
	u_int32_t 				ip;
	u_int32_t 				netmask;
	u_int8_t  					metric;
	
	u_int32_t 				nextHop;
	unsigned char 			nextHopHwAddr[ETH_ALEN];
	unsigned char				hopCount;

	struct _route_neigh		*myNeigh;

	u_int32_t 				seq;
	u_int32_t 				old_seq;
	
	u_int32_t 				rreq_id;
	unsigned long  			lifetime;

	struct _route_dev			*dev;
	u_int8_t 					route_valid:1;
	u_int8_t 					route_seq_valid:1;
	u_int8_t 					self_route:1;

	route_type_t				type;

	/* pointer to struct ieee80211_node *ni for this route when mesh route is used 
	* copy from task_t 
	*/
	void						*priv;
	
	ROUTE_LIST_NODE		list;
};
typedef struct _mesh_route 	mesh_route_t;


struct _route_neigh 
{
	u_int32_t 				ip;
	u_int32_t 				seq;
	unsigned long  			lifetime;
	unsigned char 			myHwAddr[ETH_ALEN];
	
	struct _route_dev			*dev;
	mesh_route_t 				*route_entry;
	
	int 						link;
	u_int8_t 					valid_link;

	ROUTE_LIST_NODE		list;
};
typedef struct _route_neigh 	route_neigh_t;


struct _route_task 
{
	route_task_type_t			type;
	u_int32_t 				id;
#if 0	
	unsigned long  			time;
#endif
	unsigned long 				time;
	u_int32_t 				dst_ip;
	u_int32_t 				src_ip;

	struct _route_dev			*dev;
	u_int8_t 					ttl;
	u_int16_t 				retries;

	unsigned char 			srcHwAddr[ETH_ALEN];
	unsigned int 				data_len;
	void 					*data;

	void						*priv;		/* pointer to struct ieee80211_node *ni */
	ROUTE_LIST_NODE		list;
};
typedef struct _route_task		route_task_t;

struct _dev_ops
{
	/* tx data packet and enqueue data packet into skb queue */
	route_op_type_t	(*output_packet)(struct _route_dev *dev, struct sk_buff *skb);
	/* enqueue a data packet for later tx , 0: success; <0 : failed*/
	int	(*enqueue)(struct _route_dev *dev, struct sk_buff *skb);
	/* dequeue skb and resend packet dest to detsIp after route build */
	int 	(*resend)(struct _route_dev *dev, unsigned int destIp);
	/* drop all packet dest to destIp after retry request failed */
	int	(*drop)(struct _route_dev *dev, unsigned int destIp);
	/* dequeue a data packet for tx */
//	struct skb_buff* (*dequeue)(struct _route_dev *dev);
	
	/* rx AODV pdu and insert a route_task_t into task queue : private data for different route machnism */
	route_op_type_t	(*input_packet)(struct _route_dev *dev, struct sk_buff *skb, void *priv);
//	int (*task_insert)(struct _route_dev *dev, route_task_type_t  type, struct skb_buff *packet);

	/* tx operations for route packet */
	int (*broadcast)(struct _route_dev *dev,unsigned char ttl, void *data,unsigned int length);
	int (*unicast)(struct _route_dev *dev, unsigned int destip,unsigned char ttl, void *data,unsigned int length);


	int	(*init)(struct _route_dev *routedev);
	void (*destroy)(struct _route_dev *routedev);

};
typedef struct _dev_ops	dev_ops_t;


struct _route_dev 
{
	int 						index;
	char 					name[IFNAMSIZ];
	struct _route_engine		*engine;
	
	u_int32_t 				ip;
	u_int32_t 				netmask;

	struct net_device 			*dev;
	mesh_route_t 				*myroute;

	struct socket 				*sock;

	struct sk_buff_head		outputq;			/* queue for output packets */

	void						*priv;

	dev_ops_t				*ops;

	ROUTE_LIST_NODE		list;				/* list mgmt by engine */

	ROUTE_LIST_NODE		mylist;			/* list mgmt by myself */
};
typedef struct _route_dev 		route_dev_t;

struct _engine_mgmt_ops
{
	int	(*route_delete)(struct _route_engine *engine,unsigned int destIp);
	mesh_route_t* (*route_insert)(struct _route_engine *engine,unsigned int destip, route_type_t type);
	mesh_route_t* (*route_lookup)(struct _route_engine *engine,unsigned int destip, route_type_t type);
	int  (*route_update)(struct _route_engine *engine, u_int32_t ip, u_int32_t next_hop_ip, u_int8_t metric, u_int32_t seq, struct net_device *dev);

	int	(*neigh_delete)(struct _route_engine *engine,unsigned int destip);
	route_neigh_t* (*neigh_insert)(struct _route_engine *engine,unsigned int destip);
	route_neigh_t* (*neigh_lookup_ip)(struct _route_engine *engine,unsigned  int destip);
	route_neigh_t* (*neigh_lookup_mac)(struct _route_engine *engine, char *mac);
	/* return 1 :validate; 0 : invalidate */
	int (*neigh_validate)(struct _route_engine *engine, unsigned int destip);
	void (*neigh_update_link)(struct _route_engine *engine, char *mac, unsigned char link);
	void (*neigh_update_route)(struct _route_engine *engine, route_neigh_t *neigh, aodv_pdu_reply *reply);

	void (*timers_update)(struct _route_engine *engine);
	int (*timer_insert)(struct _route_engine *engine,route_task_type_t type, unsigned int destip, unsigned int delay);
	int (*timer_insert_request)(struct _route_engine *engine, aodv_pdu_request *request, unsigned char retries);
	void (*timer_delete)(struct _route_engine *engine, route_task_type_t type, unsigned int id);
	route_task_t* (*timer_lookup)(struct _route_engine *engine,unsigned int id, route_task_type_t type);

	int (*flood_insert)(struct _route_engine *engine, u_int32_t srcip, u_int32_t dstip, u_int32_t id, unsigned long  lifetime);
	route_flood_t* (*flood_lookup)(struct _route_engine *engine,  u_int32_t srcip, u_int32_t id );

	int (*task_insert)(struct _route_engine *engine, route_task_t *task);
	int (*task_insert_timer)(struct _route_engine *engine, route_task_t * timer);

	int	(*init)(struct _route_engine *engine);
	void (*destroy)(struct _route_engine *engine);
};
typedef struct _engine_mgmt_ops 	mgmt_ops_t;

struct _engine_proto_ops
{
	int	(*rx_request)(struct _route_engine *engine, route_task_t *request);
	int	(*rx_reply)(struct _route_engine *engine, route_task_t *reply);
	int	(*rx_reply_ack)(struct _route_engine *engine, route_task_t *reply_ack);
	int	(*rx_error)(struct _route_engine *engine, route_task_t *error);
	int	(*rx_hello)(struct _route_engine *engine, route_task_t *hello, aodv_pdu_reply * reply);

	int	(*send_request)(struct _route_engine *engine, unsigned long destIp);
	int 	(*resend_request)(struct _route_engine *engine, route_task_t  *packet);
	int	(*send_reply)(struct _route_engine *engine, u_int32_t src_ip, u_int32_t dst_ip, aodv_pdu_request *request);
	void	(*send_reply_ack)(struct _route_engine *engine,  aodv_pdu_reply *reply);
	int 	(*send_error)(struct _route_engine *engine, u_int32_t brkDstIp);
		
	int 	(*send_hello)(struct _route_engine *engine);

	/* return 0 : not validate AODV pdu */
	int  	(*valid_protocol_pdu)(int numbytes, int type, char *data);
};
typedef struct _engine_proto_ops	proto_ops_t;

struct _route_engine
{
	char					name[32];
	
	char					devName[32];
	int					epoch;				/* keep start time when AODV boot */
	struct timer_list 		timer;
	
	int					supportGateway;
	int					broadcastIp;		/* IP Limited Broadcast, eg. one hop */

	int					myIp;				/* play as RREQ ID */
	int					myNetMask;

	mesh_route_t			*myRoute;			/* maintain sequence of this node */
	
	int					debug;

	void					*priv;

	route_proc_entry 		*procs;
	mgmt_ops_t			*mgmtOps;
	proto_ops_t			*protoOps;
	
	/* whether dest ip is belong to one of route device */
	int (*check_subnet)(struct _route_engine *engine, unsigned int destip);
	route_dev_t* (*dev_lookup)(struct _route_engine *engine, struct net_device *net);
	route_dev_t* (*dev_lookup_by_ip)(struct _route_engine *engine, unsigned int detip);

	int (*broadcast)(struct _route_engine *engine, unsigned char ttl, void *data, unsigned int length);
	int (*unicast)(struct _route_engine *engine, unsigned int destIp, unsigned char ttl, void *data, unsigned int length);


	ROUTE_LIST_NODE		*devs;
	rwlock_t 					*devLock;
	
	ROUTE_LIST_NODE		*routes;
	rwlock_t 					*routeLock;
	
	ROUTE_LIST_NODE		*neighs;
	rwlock_t 					*neighLock;

	ROUTE_LIST_NODE		*tasks;
	spinlock_t  				*taskLock;
	
	ROUTE_LIST_NODE		*timers;
	rwlock_t 					*timerLock;
	
	ROUTE_LIST_NODE		*floods;
	rwlock_t 					*floodLock;
	
};
typedef struct _route_engine		route_engine_t;




#if 1
#define  ROUTE_READ_LOCK( lock) \
	read_lock(lock )

#define ROUTE_READ_UNLOCK(lock) \
	read_unlock(lock)

#define  ROUTE_WRITE_LOCK( lock, flags) \
	write_lock_irqsave(lock, flags)

#define ROUTE_WRITE_UNLOCK(lock, flags) \
	write_unlock_irqrestore(lock, flags)

#define ROUTE_SPIN_LOCK(lock) \
	spin_lock_bh(lock)

#define ROUTE_SPIN_UNLOCK(lock) \
	spin_unlock_bh(lock);

#else	
#define  ROUTE_BH_READ_LOCK( lock) 
#define  ROUTE_BH_READ_UNLOCK(lock) 
#define  ROUTE_BH_WRITE_LOCK( lock) 
#define  ROUTE_BH_WRITE_UNLOCK(lock) 

#define  ROUTE_IRQ_READ_LOCK( lock, flags) 
#define  ROUTE_IRQ_READ_UNLOCK(lock, flags) 
#define  ROUTE_IRQ_WRITE_LOCK( lock, flags)
#define  ROUTE_IRQ_WRITE_UNLOCK(lock, flags) 

#define  ROUTE_SPIN_LOCK(lock) 
#define  ROUTE_SPIN_UNLOCK(lock) 

#endif


#define	IF_ENQUEUE(_q,_skb)	skb_queue_tail(_q,_skb)
#define	IF_DEQUEUE(_q,_skb)	(_skb = skb_dequeue(_q))
#define	IF_LOCK(_q)				spin_lock_bh(&(_q)->lock)
#define	IF_UNLOCK(_q)			spin_unlock_bh(&(_q)->lock)
#define	_IF_DRAIN(_q)			__skb_queue_purge(_q)	// without lock
#define	IF_DRAIN(_q)			skb_queue_purge(_q)	// with lock
#define	_IF_QLEN(_q)			skb_queue_len(_q)
#define	_IF_QFULL(_q)			(_IF_QLEN(_q) >= IEEE80211_PS_MAX_QUEUE)
#define	_IF_POLL(_q, _skb)		((_skb) = skb_peek(_q))	// TODO: peek or peek_tail
#define	IF_POLL(_q, skb)			_IF_POLL(_q, skb)
#define	_IF_DROP(_q)
#define	IF_LOCK_ASSERT(_q)

#ifdef ROUTE_SIGNAL
#include <linux/wireless.h>
void aodv_iw_sock_init(void);
void aodv_iw_sock_close(void);
int aodv_iw_set_spy(route_engine_t *engine);
void aodv_iw_get_stats(route_engine_t *engine);
#endif


u_int32_t calculate_netmask(int t);
int calculate_prefix(u_int32_t t);
int seq_less_or_equal(u_int32_t seq_one,u_int32_t seq_two);
int seq_greater(u_int32_t seq_one,u_int32_t seq_two);

int find_metric(u_int32_t tmp_ip);

int aodv_insert_kroute(mesh_route_t *route);
int aodv_delete_kroute(mesh_route_t *route);

void kick_aodv(void);
void aodv_kthread(void *data);
void kill_aodv(void);


int aodv_engine_init(route_engine_t *engine);

/* export symbols by route_core module */
int route_register_device(route_dev_t *routeDev);
void route_unregister_device(route_dev_t *routeDev);
route_task_t *route_task_create(route_engine_t *engine, route_task_type_t type);
route_dev_t	*route_dev_lookup(struct net_device *dev);

unsigned long getcurrtime(unsigned long epoch);
char *inet_ntoa(u_int32_t ina);
int inet_aton(const char *cp, u_int32_t * addr);

#endif

