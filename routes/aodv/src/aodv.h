/*
* $Id: aodv.h,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#ifndef __AODV_H__
#define __AODV_H__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/socket.h>
#include <linux/sched.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <linux/netfilter.h>

#include <linux/netdevice.h>

#include "aodv_protocol.h"

#define AODV_DEBUG			0

#define trace			printk("%s->%s(%d lines)\r\n", __FILE__, __FUNCTION__, __LINE__)

#define TRUE				1
#define FALSE 			0

#define AODV_LIST_HEAD				LIST_HEAD
#define AODV_LIST_NODE				struct list_head
#define AODV_LIST_FOR_EACH			list_for_each_entry
#define AODV_LIST_FOR_EACH_SAFE	list_for_each_entry_safe
#define AODV_LIST_ENTRY			list_entry
#define AODV_LIST_ADD_HEAD		list_add				/* stack : FILO */
#define AODV_LIST_ADD_TAIL			list_add_tail			/* queue : FIFO */
#define AODV_LIST_DEL				list_del
//#define AODV_LIST_DEL				
#define AODV_LIST_CHECK_EMPTY		list_empty



typedef enum
{
	AODV_TASK_RREQ					= 1,
	AODV_TASK_RREP					= 2,
	AODV_TASK_RERR					= 3,
	AODV_TASK_RREP_ACK				= 4,

	AODV_TASK_RESEND_RREQ			= 101,
	AODV_TASK_HELLO					= 102,
	AODV_TASK_NEIGHBOR				= 103,
	AODV_TASK_CLEANUP				= 104,
	AODV_TASK_ROUTE_CLEANUP		= 105
}aodv_task_type_t;


// Structures

// Route table
struct _flood_id 
{
	u_int32_t 				src_ip;
	u_int32_t 				dst_ip;

	u_int32_t 				id;
	unsigned long  			lifetime;

	AODV_LIST_NODE			list;
};
typedef struct _flood_id aodv_flood_t;

typedef enum
{
	AODV_ROUTE_KERNEL	=	1,		/* route entry from kernel route, eg. net_device */
	AODV_ROUTE_AODV		=	2		/* route entry created by AODV Protocol */
}aodv_route_type_t;

struct _aodv_route 
{
	u_int32_t 				ip;
	u_int32_t 				netmask;
	u_int8_t  					metric;
	u_int32_t 				next_hop;

	u_int32_t 				seq;
	u_int32_t 				old_seq;
	
	u_int32_t 				rreq_id;
	unsigned long  			lifetime;

	struct 					net_device *dev;
	u_int8_t 					route_valid:1;
	u_int8_t 					route_seq_valid:1;
	u_int8_t 					self_route:1;

	aodv_route_type_t			type;
	
	AODV_LIST_NODE			list;
};
typedef struct _aodv_route aodv_route_t;

struct _aodv_dev 
{
	int 						index;
	char 					name[IFNAMSIZ];
	
	u_int32_t 				ip;
	u_int32_t 				netmask;

	struct net_device 			*dev;
	aodv_route_t 				*route_entry;

	struct socket 				*sock;
	
	AODV_LIST_NODE			list;
};
typedef struct _aodv_dev aodv_dev_t;


struct _aodv_neigh 
{
	u_int32_t 				ip;
	u_int32_t 				seq;
	unsigned long  			lifetime;
	unsigned char 			hw_addr[ETH_ALEN];
	
	struct net_device 			*dev;
	aodv_route_t 				*route_entry;
	
	int 						link;
	u_int8_t 					valid_link;

	AODV_LIST_NODE			list;
};
typedef struct _aodv_neigh aodv_neigh_t;


struct _task 
{
	aodv_task_type_t			type;
	u_int32_t 				id;
#if 0	
	unsigned long  			time;
#endif
	unsigned long 				time;
	u_int32_t 				dst_ip;
	u_int32_t 				src_ip;

	struct net_device 			*dev;
	u_int8_t 					ttl;
	u_int16_t 				retries;

	unsigned char 			src_hw_addr[ETH_ALEN];
	unsigned int 				data_len;
	void 					*data;

	AODV_LIST_NODE			list;
};
typedef struct _task aodv_task_t;


typedef struct
{
	char					devName[32];
	int					epoch;				/* keep start time when AODV boot */
	struct timer_list 		timer;
	
	int					supportGateway;
	int					broadcastIp;		/* IP Limited Broadcast, eg. one hop */

	int					myIp;				/* play as RREQ ID */
	int					myNetMask;

	aodv_route_t			*myRoute;			/* maintain sequence of this node */


	AODV_LIST_NODE		*devs;
	
	AODV_LIST_NODE		*routes;
	rwlock_t 				*routeLock;
	
	AODV_LIST_NODE		*neighs;
	rwlock_t 				*neighLock;

	AODV_LIST_NODE		*tasks;
	spinlock_t  			*taskLock;
	
	AODV_LIST_NODE		*timers;
	rwlock_t 				*timerLock;
	
	AODV_LIST_NODE		*floods;
	rwlock_t 				*floodLock;
	
}aodv_t;


typedef int (*aodv_read_proc_t)(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data);

typedef struct
{
	unsigned char			name[32];
	aodv_read_proc_t		read;
	struct proc_dir_entry	*proc;
}aodv_proc_entry;



#if 1
#define  AODV_READ_LOCK( lock) \
	read_lock(lock )

#define AODV_READ_UNLOCK(lock) \
	read_unlock(lock)

#define  AODV_WRITE_LOCK( lock, flags) \
	write_lock_irqsave(lock, flags)

#define AODV_WRITE_UNLOCK(lock, flags) \
	write_unlock_irqrestore(lock, flags)

#define AODV_SPIN_LOCK(lock) \
	spin_lock_bh(lock)

#define AODV_SPIN_UNLOCK(lock) \
	spin_unlock_bh(lock);

#else	
#define  AODV_BH_READ_LOCK( lock) 
#define  AODV_BH_READ_UNLOCK(lock) 
#define  AODV_BH_WRITE_LOCK( lock) 
#define  AODV_BH_WRITE_UNLOCK(lock) 

#define  AODV_IRQ_READ_LOCK( lock, flags) 
#define  AODV_IRQ_READ_UNLOCK(lock, flags) 
#define  AODV_IRQ_WRITE_LOCK( lock, flags)
#define  AODV_IRQ_WRITE_UNLOCK(lock, flags) 

#define  AODV_SPIN_LOCK(lock) 
#define  AODV_SPIN_UNLOCK(lock) 

#endif

#define AODV_PROC_NAME		"aodv"


#ifdef AODV_SIGNAL
#include <linux/wireless.h>
void aodv_iw_sock_init(void);
void aodv_iw_sock_close(void);
int aodv_iw_set_spy(aodv_t *aodv);
void aodv_iw_get_stats(aodv_t *aodv);
#endif
int init_sock(struct socket *sock, u_int32_t ip, char *dev_name);
int aodv_local_broadcast(aodv_t *aodv,u_int8_t ttl, void *data,const size_t datalen);
int send_message(aodv_t *aodv, u_int32_t dst_ip, u_int8_t ttl, void *data, const size_t datalen, struct net_device *dev);

void ipq_send_ip(u_int32_t ip);
void ipq_drop_ip(u_int32_t ip);
int ipq_insert_packet(struct sk_buff *skb, struct nf_info *info);
int init_packet_queue(void);
void cleanup_packet_queue(void);


unsigned long  getAodvEpoch();
unsigned long getcurrtime(unsigned long epoch);
char *inet_ntoa(u_int32_t ina);
int inet_aton(const char *cp, u_int32_t * addr);
int local_subnet_test(u_int32_t tmp_ip);
u_int32_t calculate_netmask(int t);
int calculate_prefix(u_int32_t t);
int seq_less_or_equal(u_int32_t seq_one,u_int32_t seq_two);
int seq_greater(u_int32_t seq_one,u_int32_t seq_two);

int aodv_netfilter_init();
void aodv_netfilter_destroy();


int aodv_flood_insert(aodv_t *aodv, u_int32_t src_ip, u_int32_t dst_ip, u_int32_t id, unsigned long  lifetime);
aodv_flood_t *aodv_flood_find(aodv_t *aodv, u_int32_t src_ip, u_int32_t id);

int aodv_dev_init(aodv_t *aodv );
aodv_dev_t *aodv_dev_find_by_net(aodv_t *aodv, struct net_device * dev);
int aodv_dev_local_check(aodv_t *aodv, u_int32_t ip);


aodv_neigh_t *aodv_neigh_find(aodv_t *aodv, u_int32_t target_ip);
int aodv_neigh_valid(aodv_t *aodv, u_int32_t target_ip);
void aodv_neigh_update_link(aodv_t *aodv, char *hw_addr, u_int8_t link);
int aodv_neigh_delete(aodv_t *aodv, u_int32_t ip);
aodv_neigh_t *aodv_neigh_insert(aodv_t *aodv, u_int32_t ip);
void aodv_neigh_update_route(aodv_t *aodv, aodv_neigh_t *neigh, aodv_pdu_reply *reply);
int aodv_neigh_update(aodv_t *aodv,  aodv_neigh_t *neigh, aodv_pdu_reply *reply);

aodv_task_t *aodv_task_create(aodv_t *aodv, aodv_task_type_t type);
int aodv_task_insert(aodv_t *aodv, aodv_task_type_t  type, struct sk_buff *packet);
int aodv_task_insert_from_timer(aodv_t *aodv, aodv_task_t * timer_task);


int aodv_pdu_hello_send(aodv_t *aodv );
int aodv_pdu_reply_rx(aodv_t *aodv, aodv_task_t * packet);
int aodv_pdu_reply_create(aodv_t *aodv, u_int32_t src_ip, u_int32_t dst_ip, aodv_pdu_request *request);
void aodv_pdu_reply_ack_create(aodv_t *aodv,  aodv_pdu_reply *reply, aodv_task_t *packet);
int  aodv_pdu_reply_ack_rx(aodv_t *aodv, aodv_task_t *working_packet);
int aodv_pdu_error_rx(aodv_t *aodv, aodv_task_t * tmp_packet);


int aodv_pdu_request_reply(aodv_t *aodv, aodv_pdu_request * request);
int aodv_pdu_request_resend(aodv_t *aodv, aodv_pdu_request * request, int ttl);
int aodv_pdu_request_rx(aodv_t *aodv, aodv_task_t * tmp_packet);
int aodv_pdu_request_resend_ttl(aodv_t *aodv, aodv_task_t  *packet);


int aodv_route_valid(aodv_t *aodv,aodv_route_t * route);
void aodv_route_expire(aodv_t *aodv, aodv_route_t * route);
int aodv_route_delete(aodv_t *aodv, u_int32_t target_ip);
aodv_route_t *aodv_route_create(aodv_t *aodv, uint32_t ip, aodv_route_type_t type);
int aodv_route_update(aodv_t *aodv, u_int32_t ip, u_int32_t next_hop_ip, u_int8_t metric, u_int32_t seq, struct net_device *dev);
aodv_route_t *aodv_route_find(aodv_t *aodv, unsigned long target_ip);
 int aodv_route_compare(aodv_route_t * route, u_int32_t target_ip);
int find_metric(u_int32_t tmp_ip);

int aodv_pdu_error_send(aodv_t *aodv, u_int32_t brk_dst_ip);
int aodv_insert_kroute(aodv_route_t *route);
int aodv_delete_kroute(aodv_route_t *route);


void aodv_timer_update(aodv_t *aodv );
int aodv_timer_insert(aodv_t *aodv, aodv_task_type_t type, u_int32_t delay, u_int32_t ip);
int aodv_timer_insert_request(aodv_t *aodv, aodv_pdu_request * rreq, u_int8_t retries);
aodv_task_t *aodv_timer_find(aodv_t *aodv, u_int32_t id, aodv_task_type_t type);
void aodv_timer_delete(aodv_t *aodv, u_int32_t id, aodv_task_type_t type);

void kick_aodv();
void aodv_kthread(void *data);
void kill_aodv();


#endif
