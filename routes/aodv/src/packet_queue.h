/*
* $Id: packet_queue.h,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#ifndef __IP_QUEUE_H__
#define __IP_QUEUE_H__

#include <linux/module.h>
#include <linux/skbuff.h>
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
#include <linux/netfilter_ipv4/ip_tables.h>


#ifdef __KERNEL__
#ifdef DEBUG_IPQ
#define QDEBUG(x...) printk(KERN_DEBUG ## x)
#else
#define QDEBUG(x...)
#endif  /* DEBUG_IPQ */
#else
#include <net/if.h>
#endif	/* ! __KERNEL__ */

/* Messages sent from kernel */
typedef struct ipq_packet_msg 
{
	unsigned long packet_id;	/* ID of queued packet */
	unsigned long mark;		/* Netfilter mark value */
	long timestamp_sec;		/* Packet arrival time (seconds) */
	long timestamp_usec;		/* Packet arrvial time (+useconds) */
	unsigned int hook;		/* Netfilter hook we rode in on */
	char indev_name[IFNAMSIZ];	/* Name of incoming interface */
	char outdev_name[IFNAMSIZ];	/* Name of outgoing interface */
	unsigned short hw_protocol;	/* Hardware protocol (network order) */
	unsigned short hw_type;		/* Hardware type */
	unsigned char hw_addrlen;	/* Hardware address length */
	unsigned char hw_addr[8];	/* Hardware address */
	size_t data_len;		/* Length of packet data */
	unsigned char payload[0];	/* Optional packet data */
} ipq_packet_msg_t;

/* Messages sent from userspace */
typedef struct ipq_mode_msg 
{
	unsigned char value;		/* Requested mode */
	size_t range;			/* Optional range of packet requested */
} ipq_mode_msg_t;

typedef struct ipq_verdict_msg 
{
	unsigned int value;		/* Verdict to hand to netfilter */
	unsigned long id;		/* Packet ID for this verdict */
	size_t data_len;		/* Length of replacement data */
	unsigned char payload[0];	/* Optional replacement packet */
} ipq_verdict_msg_t;


#define IPQ_COPY_MAX IPQ_COPY_PACKET

void ipq_send_ip(u_int32_t ip);
void ipq_drop_ip(u_int32_t ip);
int ipq_insert_packet(struct sk_buff *skb,struct nf_info *info);
int init_packet_queue(void);
void cleanup_packet_queue(void);
#endif /*_IP_QUEUE_H*/

