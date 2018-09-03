/*
* $Id: _ip_aodv.h,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
*/

#ifndef __IP_AODV_H__
#define __IP_AODV_H__

void ipq_send_ip(u_int32_t ip);
void ipq_drop_ip(u_int32_t ip);
int ipq_insert_packet(struct sk_buff *skb, struct nf_info *info);
int init_packet_queue(void);
void cleanup_packet_queue(void);

int aodv_netfilter_init();
void aodv_netfilter_destroy();

extern	dev_ops_t	ip_dev_ops;

#endif

