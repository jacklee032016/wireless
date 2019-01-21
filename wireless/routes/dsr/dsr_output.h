#ifndef DSR_OUTPUT_H
#define DSR_OUTPUT_H

#include <linux/skbuff.h>
#include "dsr_header.h"

void dsr_send(struct sk_buff **skb, struct rt_entry * dsr_rt,
	      int (*output)(struct sk_buff*));
void send_rt_reply(const struct sk_buff * skb, struct dsr_rt_req_opt * rtreq);
void send_rt_err(struct sk_buff * skb);
void send_rt_req(__u32 taddr);
void rebcast_rt_req (const struct sk_buff * skb, unsigned char * optptr);
void send_ack_reply(const struct sk_buff * skb, unsigned char * optptr);
int dsr_output(struct sk_buff *skb);

#endif /* DSR_OUTPUT_H */
