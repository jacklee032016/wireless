#ifndef DSR_QUEUE_H
#define DSR_QUEUE_H

#include "dsr_header.h"
#include "dsr_debug.h"
#include "dsr_output.h"

void ack_q_add(struct sk_buff_head * list, struct sk_buff * skb, int q_len);
void send_q_add(struct sk_buff_head * list, struct sk_buff * skb,
		int q_len, __u32 fwdaddr, int (*output)(struct sk_buff *));

#endif /* DSR_QUEUE_H */
