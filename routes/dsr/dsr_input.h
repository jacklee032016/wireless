#ifndef DSR_INPUT_H
#define DSR_INPUT_H

#include <linux/skbuff.h>

void rmv_dsr_hdr(struct sk_buff * skb);
int proc_src_rt_opt(struct sk_buff *skb, unsigned char * optptr);
void proc_rt_req_opt(struct sk_buff *skb, unsigned char * optptr);
void proc_rt_reply_opt(struct sk_buff *skb, unsigned char * optptr);
void proc_rt_err_opt(struct sk_buff *skb, unsigned char * optptr);
void proc_ack_req_opt(struct sk_buff *skb, unsigned char * optptr);
void proc_ack_reply_opt(struct sk_buff *skb, unsigned char * optptr);

#endif /* DSR_INPUT_H */
