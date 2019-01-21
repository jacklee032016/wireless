#include "dsr_queue.h"

extern __u16 ack_id;
/* stats */
extern unsigned int stat_send_q_timeout;
extern unsigned int stat_send_q_drop;
extern unsigned int stat_ack_q_drop;
extern unsigned int stat_ack_q_timeout;
extern unsigned int stat_ack_q_resend;

void send_timeout(unsigned long data) {

  struct sk_buff * skb = (struct sk_buff *) data;
  DSR_ASSERT(skb != NULL);

  //printk("send timed out !!!\n");
  stat_send_q_timeout++;
  skb_unlink(skb);
  kfree_skb(skb);
}

void send_q_add(struct sk_buff_head * list, struct sk_buff * skb,
		int q_len, __u32 fwdaddr, int (*output)(struct sk_buff *)) {

  struct send_info * sendinfo;

  DSR_ASSERT(skb_queue_len(list) <= q_len);
  if (skb_queue_len(list) == q_len) {
    struct sk_buff * headskb = skb_dequeue(list);
    sendinfo = (struct send_info *) headskb->cb;
    del_timer(&(sendinfo->timer));
    kfree_skb(headskb);
    stat_send_q_drop++;
#ifdef DSR_DEBUG
    printk("send q too long\n");
#endif
  }

  sendinfo = (struct send_info *) skb->cb;
  init_timer(&(sendinfo->timer));
  sendinfo->fwdaddr = fwdaddr;
  sendinfo->output = output;
  sendinfo->timer.function = send_timeout;
  sendinfo->timer.expires = jiffies + (SEND_BUFFER_TIMEOUT * HZ);
  sendinfo->timer.data = (unsigned long) skb;
  add_timer(&(sendinfo->timer));
  skb_queue_tail(list, skb);
}

void ack_timeout(unsigned long data) {

  struct sk_buff * skb = (struct sk_buff *) data;
  struct rxmt_info * rxmtinfo = (struct rxmt_info *) skb->cb;
  DSR_ASSERT(skb != NULL);

#ifdef DSR_DEBUG
  printk("ack timed out !!!\n");
  //printk("rxmtinfo:%u\n", rxmtinfo);
  //printk("rxmtinfo->srcrt:%u\n", rxmtinfo->srcrt);
#endif
  if (rxmtinfo->n_rxmt < DSR_MAXRXTSHIFT) {
    struct sk_buff * newskb;
    rxmtinfo->n_rxmt++;
#ifdef DSR_DEBUG
    printk("resend %i\n", rxmtinfo->n_rxmt);
#endif
    rxmtinfo->timer.expires = jiffies + ACK_TIMEOUT_JF;
    add_timer(&(rxmtinfo->timer));

    if ((newskb = skb_clone(skb, GFP_ATOMIC)) == NULL)
      return;

    stat_ack_q_resend++;
    dsr_output(newskb);
  } else {
#ifdef DSR_DEBUG
    printk("send rt err for ack %u\n", rxmtinfo->ident);
#endif
    stat_ack_q_timeout++;
    skb_unlink(skb);
    send_rt_err(skb);
    kfree_skb(skb);
  }
}

void ack_q_add(struct sk_buff_head * list, struct sk_buff * skb,
	       int q_len/*, int (*output)(struct sk_buff *)*/) {

  struct sk_buff * newskb;
  struct rxmt_info * rxmtinfo;

  DSR_ASSERT(skb_queue_len(list) <= q_len);
  if (skb_queue_len(list) == q_len) {
    stat_ack_q_drop++;
    return;
  }

  newskb = skb_clone(skb, GFP_ATOMIC);
  if (newskb == NULL)
    return;

  rxmtinfo = (struct rxmt_info *) newskb->cb;

  rxmtinfo->ident = ack_id;
  rxmtinfo->n_rxmt = 0;

  init_timer(&(rxmtinfo->timer));
  rxmtinfo->timer.function = ack_timeout;
  rxmtinfo->timer.expires = jiffies + ACK_TIMEOUT_JF;
  rxmtinfo->timer.data = (unsigned long) newskb;
  add_timer(&(rxmtinfo->timer));

  skb_queue_tail(list, newskb);
}
