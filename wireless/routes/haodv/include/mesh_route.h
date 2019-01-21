/*
* $Id: mesh_route.h,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/
 
#ifndef __MESH_ROUTE_H__
#define __MESH_ROUTE_H__


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include "linux_compat.h"
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

#define	ROUTE_DEBUG_RAWPDU			0x00001000

/*
		printk("%s : %s.%d:" _fmt, _node->name, __FUNCTION__, __LINE__,  __VA_ARGS__);		\
*/
#define 	ROUTE_DUMP_PDU(_node,_m,  _tag, _data, _length) \
	if (_node->debug & (_m))			\
		mesh_dump_pdu( _tag, _data, _length )

#define	ROUTE_DPRINTF(_node, _m, _fmt, ...) do {	\
	if (_node->debug & (_m))			\
		printk("%s : " _fmt, _node->name,  __VA_ARGS__);		\
} while (0)
#define	ROUTE_DPRINTF_ADDRESS(_node, _m, _fmt1, _fmt2, _address1, _address2 ) do {	\
	if (_node->debug & (_m)){			\
		printk("%s : " _fmt1, _node->name,  _address1 );		\
		printk( _fmt2,  _address2);}		\
} while (0)
#else
#define 	ROUTE_DUMP_PDU(_node,_m,  _tag, _data, _length) 
#define	ROUTE_DPRINTF(_node, _m, _fmt, ...)
#define	ROUTE_DPRINTF_ADDRESS(_node, _m, _fmt1, _fmt2, _address1, _address2 ) 
#endif


#define ROUTE_PROC_NAME		"mesh"

typedef int (*route_read_proc_t)(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data);

typedef struct
{
	unsigned char			name[32];
	route_read_proc_t		read;
	struct proc_dir_entry	*proc;
}route_proc_entry;


struct _route_node;
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
	ROUTE_TASK_PDU					= 1,
	ROUTE_TASK_MGMT					=2
}task_type_t;

typedef enum
{
	ROUTE_PDU_TASK_RREQ					= 1,
	ROUTE_PDU_TASK_RREP					= 2,
	ROUTE_PDU_TASK_RERR					= 3,
	ROUTE_PDU_TASK_RREP_ACK				= 4,

	ROUTE_MGMT_TASK_RESEND_RREQ			= 101,
	ROUTE_MGMT_TASK_HELLO					= 102,
	ROUTE_MGMT_TASK_NEIGHBOR				= 103,
	ROUTE_MGMT_TASK_CLEANUP				= 104,
	ROUTE_MGMT_TASK_ROUTE_CLEANUP		= 105
}task_subtype_t;


struct _flood_id 
{/* stored info about RREQ orginated or forward by myself */
	unsigned int 				reqId;			/* keep RREQ ID */
	aodv_address_t			orig;		/* keep orig Address of RREQ */
	
	aodv_address_t			dest;		/* keep dest Address of RREQ */
	
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
#if 0
	u_int32_t 				ip;
#endif
	aodv_address_t			address;
	u_int32_t 				netmask;
	u_int8_t  					metric;
	
	unsigned char 			nextHopHwAddr[ETH_ALEN];
	struct _route_neigh		*nextHop;
	
	unsigned char				hopCount;


	u_int32_t 				seq;
	u_int32_t 				oldSeq;
	
	u_int32_t 				reqId;
	unsigned long  			lifetime;

	struct _route_dev			*dev;
	
	u_int8_t 					routeValid;
	u_int8_t 					routeSeqValid;
	u_int8_t 					selfRoute;

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
	aodv_address_t			address;
/*	
	u_int32_t 				myIp;
	u_int32_t 				seq;
*/	
	unsigned char 			myHwAddr[ETH_ALEN];
	
	unsigned long  			lifetime;
	struct _route_dev			*dev;
	mesh_route_t 				*myroute;		/* route for this neigh */
	
	int 						link;
	u_int8_t 					validLink;

	void						*priv;

	ROUTE_LIST_NODE		list;
};
typedef struct _route_neigh 	route_neigh_t;


struct _route_task 
{
	task_type_t				type;
	task_subtype_t			subType;
	
	u_int32_t 				id;
/*	
	u_int32_t 				dst_ip;
	u_int32_t 				src_ip;
*/
	aodv_address_t			dest;
	aodv_address_t			src;
	
	u_int8_t 					ttl;
	u_int16_t 				retries;

	unsigned long 				time;

	struct _route_dev			*dev;
	void						*priv;		/* pointer to struct ieee80211_node *ni */
	ROUTE_LIST_NODE		list;
};
typedef struct _route_task		route_task_t;

struct _pdu_task
{
	route_task_t				header;
	
	unsigned char 			srcHwAddr[ETH_ALEN];
	unsigned int 				data_len;
	void 					*data;

};
typedef struct _pdu_task		pdu_task_t;


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
	int (*unicast)(struct _route_dev *dev, struct _route_neigh *neigh,unsigned char ttl, void *data,unsigned int length);


	int	(*init)(struct _route_dev *routedev);
	void (*destroy)(struct _route_dev *routedev);

};
typedef struct _dev_ops	dev_ops_t;


struct _dev_task_ops
{

	int (*rx_request)(struct _route_dev *dev, pdu_task_t *task);
	int (*rx_reply)(struct _route_dev *dev, pdu_task_t *task);
	int (*rx_reply_ack)(struct _route_dev *dev, pdu_task_t *task);
	int (*rx_hello)(struct _route_dev *dev, pdu_task_t *task);
	int (*rx_error)(struct _route_dev *dev, pdu_task_t *task);

	int	(*send_reply)(struct _route_dev *dev, pdu_task_t  *packet);
	/* send route_task_t, not pdu_task. changed when request is send */
	int	(*send_request)(struct _route_dev *dev, route_task_t  *packet);
};
typedef struct _dev_task_ops	task_ops_t;


struct _route_dev 
{
	int 						index;
	char 					name[IFNAMSIZ];
	struct _route_node		*node;
	
//	u_int32_t 				ip;
	aodv_address_t			address;
	u_int32_t 				netmask;

	struct net_device 			*dev;

//	mesh_route_t 				*myroute;	//not need

	ROUTE_LIST_NODE		*neighs;
	rwlock_t 					*neighLock;

	struct socket 				*sock;

	struct sk_buff_head		outputq;			/* queue for output packets */

	void						*priv;

	dev_ops_t				*ops;
	task_ops_t				*taskOps;

	ROUTE_LIST_NODE		list;				/* list mgmt by node */

	ROUTE_LIST_NODE		mylist;			/* list mgmt by myself */
};
typedef struct _route_dev 		route_dev_t;

struct _node_mgmt_ops
{
	int	(*route_delete)(struct _route_node *node,aodv_address_t *dest);
	mesh_route_t* (*route_insert)(struct _route_node *node, route_neigh_t *nextHop, aodv_address_t *dest, route_type_t type);
	mesh_route_t* (*route_lookup)(struct _route_node *node, aodv_address_t *dest, route_type_t type);
	int  (*route_update)(struct _route_node *node, aodv_address_t *dest, route_neigh_t *nextHop, u_int8_t metric);

	int	(*neigh_delete)(struct _route_node *node, aodv_address_t *address);
	route_neigh_t* (*neigh_insert)(struct _route_node *node, pdu_task_t *task);
	route_neigh_t* (*neigh_lookup)(struct _route_node *node, pdu_task_t *task);
	route_neigh_t* (*neigh_lookup_mac)(struct _route_node *node, char *mac);
	void (*neigh_update_link)(struct _route_node *node, char *mac, unsigned char link);
	void (*neigh_update_route)(struct _route_node *node, route_neigh_t *neigh, pdu_task_t *task);

	int (*timer_insert)(struct _route_node *node, aodv_address_t *address, task_subtype_t type, unsigned int delay);
	route_task_t* (*timer_lookup)(struct _route_node *node,unsigned int id, task_subtype_t type);
	void (*timer_delete)(struct _route_node *node, aodv_address_t *address, task_subtype_t type, unsigned int id);
	void (*timers_update)(struct _route_node *node);

	int (*flood_insert)(struct _route_node *node, aodv_address_t *orig, aodv_address_t *dest, u_int32_t reqId, unsigned long  lifetime);
	route_flood_t* (*flood_lookup)(struct _route_node *node, aodv_address_t *oirg, u_int32_t reqId);

	int (*task_insert)(struct _route_node *node, route_task_t *task);
	int (*task_insert_timer)(struct _route_node *node, route_task_t * timer);

	int	(*init)(struct _route_node *node);
	void (*destroy)(struct _route_node *node);
};
typedef struct _node_mgmt_ops 	mgmt_ops_t;

struct _node_proto_ops
{
	int 	(*send_error)(struct _route_node *node, route_neigh_t *neigh);
	int 	(*send_hello)(struct _route_node *node);

	/* return 0 : not validate AODV pdu */
	int  	(*valid_protocol_pdu)(int numbytes, int type, char *data);
};
typedef struct _node_proto_ops	proto_ops_t;

struct _route_node
{
	char					name[32];
	
	char					devName[32];
	int					epoch;				/* keep start time when AODV boot */
	struct timer_list 		timer;
	
	int					supportGateway;
//	int					broadcastIp;		/* IP Limited Broadcast, eg. one hop */
	aodv_address_t		broadcastAddress;

//	int					myIp;				/* play as RREQ ID */
	aodv_address_t		address;
	int					netMask;

	mesh_route_t			*myRoute;			/* maintain sequence of this node */
	
	int					debug;

#ifdef CONFIG_SYSCTL
	char						mesh_procname[12];		/* /proc/sys/net/mesh%d */
	struct ctl_table_header		*mesh_sysctl_header;
	struct ctl_table			*mesh_sysctls;
#endif
	void					*priv;

	route_proc_entry 		*procs;
	mgmt_ops_t			*mgmtOps;
	proto_ops_t			*protoOps;
	
	/* whether dest ip is belong to one of route device */
	int (*check_subnet)(struct _route_node *node, aodv_address_t *dest);
	route_dev_t* (*dev_lookup)(struct _route_node *node, struct net_device *net);
	route_dev_t* (*dev_lookup_by_ip)(struct _route_node *node, unsigned int detip);

	int (*broadcast)(struct _route_node *node, unsigned char ttl, void *data, unsigned int length);
//	int (*unicast)(struct _route_node *node, unsigned int destIp, unsigned char ttl, void *data, unsigned int length);


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
typedef struct _route_node		route_node_t;




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


#ifdef ROUTE_SIGNAL
#include <linux/wireless.h>
void aodv_iw_sock_init(void);
void aodv_iw_sock_close(void);
int aodv_iw_set_spy(route_node_t *node);
void aodv_iw_get_stats(route_node_t *node);
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


int aodv_node_init(route_node_t *node);

/* export symbols by route_core module */
int route_register_device(route_dev_t *routeDev);
void route_unregister_device(route_dev_t *routeDev);
route_task_t *route_task_create(route_node_t *node, task_subtype_t type);
route_dev_t	*route_dev_lookup(struct net_device *dev);

unsigned long getcurrtime(unsigned long epoch);
char *inet_ntoa(u_int32_t ina);
int inet_aton(const char *cp, u_int32_t * addr);

void mesh_dump_pdu(const char *tag, const void *data, u_int len);

#endif

