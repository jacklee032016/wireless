#include "dsr_input.h"
#include "dsr_header.h"
#include "dsr_debug.h"
#include "dsr_route.h"
#include "dsr_output.h"

extern __u32 IPADDR;
extern __u32 NETMASK;
extern __u32 NETWORK;
extern int   gw;
extern __u16 ack_id;
extern struct sk_buff_head rxmt_q;
extern struct rt_entry rt_cache[];
extern struct id_fifo rt_req_ids[];
extern int rt_req_ids_size;

int in_rt_req(unsigned char * optptr) {

  struct dsr_rt_req_opt * rtreq = (struct dsr_rt_req_opt *) optptr;
  int i;

  for (i = 0;i < ((rtreq->len - 6) / 4);i++)
    if (rtreq->addr[i] == IPADDR) {
      //printk("in rt req\n");
      return 1;
    }
  //printk("not in rt req\n");
  return 0;
}

int lookup_rt_req_id(__u32 saddr, __u16 id) {

  int i, j, count;

  //printk("lookup id %i size %i\n", id, rt_req_ids_size);

  for (i = 0, count = 0; count < rt_req_ids_size;i++) {
    if (rt_req_ids[i].saddr != 0) {
      count++;
      if (rt_req_ids[i].saddr == saddr) {
	for (j = 0;j < rt_req_ids[i].len;j++) {
	  if (rt_req_ids[i].id[(rt_req_ids[i].head + j) % REQ_TABLE_IDS] == id)
	    return 1;
	}
	//printk("not found\n");
	return 0;
      }
    }
  }
  //printk("not found\n");
  return 0;
}

void add_rt_req_id(__u32 saddr, __u16 id) {

  int i, count;

  for (i = 0, count = 0;count < rt_req_ids_size;i++) {
    if (rt_req_ids[i].saddr != 0) {
      count++;
      if (rt_req_ids[i].saddr == saddr) {
	//printk("add rt req id found saddr\n");
	rt_req_ids[i].id[rt_req_ids[i].tail] = id;
	rt_req_ids[i].tail = (rt_req_ids[i].tail + 1) % REQ_TABLE_IDS;
	if (rt_req_ids[i].len < REQ_TABLE_IDS) {
	  rt_req_ids[i].len++;
	} else {
	  rt_req_ids[i].head = (rt_req_ids[i].head + 1) % REQ_TABLE_IDS;
	}
	return;
      }
    }
  }

  /* have to crete a new entry or replace an entry */
  if (rt_req_ids_size < REQ_TABLE_SIZE) {
    for (i = 0;i < REQ_TABLE_SIZE;i++) {
      if (rt_req_ids[i].saddr == 0) {
	rt_req_ids[i].saddr = saddr;
	rt_req_ids[i].time = CURRENT_TIME;
	rt_req_ids[i].id[0] = id;
	rt_req_ids[i].head = 0;
	rt_req_ids[i].tail = 1;
	rt_req_ids[i].len = 1;
	rt_req_ids_size++;
	//printk("add rt req id found empty entry\n");
	return;
      }
    }
    DSR_ASSERT(0);
  } else {
    int oldest = 0;

    for (i = 1;i < REQ_TABLE_SIZE;i++) {
      if (rt_req_ids[i].time < rt_req_ids[oldest].time) {
	oldest = i;
      }
    }
    rt_req_ids[oldest].saddr = saddr;
    rt_req_ids[oldest].time = CURRENT_TIME;
    rt_req_ids[oldest].id[0] = id;
    rt_req_ids[oldest].head = 0;
    rt_req_ids[oldest].tail = 1;
    rt_req_ids[oldest].len = 1;
    //printk("add rt req id found oldest entry\n");
    return;
  }
}

void rmv_dsr_hdr(struct sk_buff * skb) {

  struct iphdr * iph = skb->nh.iph;
  struct dsr_hdr * dsrhdr = (struct dsr_hdr *) (skb->nh.raw + (iph->ihl)*4);
  int size = ntohs(dsrhdr->length) + 4;

  iph->protocol = dsrhdr->nexthdr;

  skb->nh.raw = skb_pull(skb, size);
  /* iph will point to the "old" header at this moment */
  memmove(skb->nh.raw, skb->nh.raw - size, (iph->ihl)*4);

  /* set iph to the "new" header */
  iph = skb->nh.iph;
  iph->tot_len = htons(skb->len);

  /* recalculate check sum */
  ip_send_check(iph);
}

void proc_rt_req_opt(struct sk_buff * skb, unsigned char * optptr) {

  struct iphdr * iph = skb->nh.iph;
  struct dsr_rt_req_opt * rtreq = (struct dsr_rt_req_opt *) optptr;

  if (iph->saddr == IPADDR)
    return;

  add_reverse_route(iph->saddr, rtreq->addr, (rtreq->len - 6) / 4);

  if (rtreq->taddr == IPADDR) {
#ifdef DSR_DEBUG
    printk("send rt reply\n");
#endif
    send_rt_reply(skb, rtreq);
  } else {
    if (in_rt_req(optptr) == 0 &&
	lookup_rt_req_id(iph->saddr, rtreq->ident) == 0) {

#ifdef DSR_DEBUG
      printk("proc rt req rebroadcasting\n");
#endif
      add_rt_req_id(iph->saddr, rtreq->ident);
      rebcast_rt_req(skb, optptr);
    }
  }
}

void proc_rt_reply_opt(struct sk_buff * skb, unsigned char * optptr) {

  const struct iphdr * iph = skb->nh.iph;
  struct dsr_rt_reply_opt * rtreply = (struct dsr_rt_reply_opt *) optptr;

  if (iph->daddr == IPADDR) {
    add_forward_route(iph->saddr, rtreply->addr, (rtreply->len - 3) / 4);
  }
}

void proc_rt_err_opt(struct sk_buff *skb, unsigned char * optptr) {
  struct dsr_rt_err_opt * rterr = (struct dsr_rt_err_opt *) optptr;
  remove_route(rt_cache, rterr->errsaddr, rterr->typeinfo);
}

void proc_ack_req_opt(struct sk_buff * skb, unsigned char * optptr/*,
		      const struct net_device * dev*/) {

  struct iphdr * iph;
  struct dsr_ack_req_opt * ackreq = (struct dsr_ack_req_opt *) optptr;

  send_ack_reply(skb, optptr);

  /*
   * if we are going to forward this packet then update the ack request
   */
  iph = skb->nh.iph;
  if (iph->daddr != IPADDR && (gw == 0 || DSR_SUBNET(iph->daddr))) {
    ack_id++;
    ackreq->ident = ack_id;
    ackreq->saddr = IPADDR;
  }
}

void proc_ack_reply_opt(struct sk_buff *skb, unsigned char * optptr) {

  const struct dsr_ack_req_opt * ackopt = (struct dsr_ack_req_opt *) optptr;
  int q_len = skb_queue_len(&rxmt_q);
  struct sk_buff * skbptr;// = skb_dequeue(&rxmt_q);
  struct rxmt_info * rxmtinfo;// = (struct rxmt_info *) skbptr->cb;
  int finished = 0;
  int i;

  /* optimisation: add neighbour node to route */
  /* forward and backward should be the same since route length is zero */
  add_forward_route(skb->nh.iph->saddr, NULL, 0);

  if (q_len == 0) {
    return;
  }

  /* find and remove the packet from the ack queue */
  for (i = 0;i < q_len && finished == 0;i++) {
    skbptr = skb_dequeue(&rxmt_q);;
    rxmtinfo = (struct rxmt_info *) skbptr->cb;
    DSR_ASSERT(skbptr != NULL);
    if (ackopt->ident == rxmtinfo->ident) {
      finished = 1;
      del_timer(&(rxmtinfo->timer));
      skb_unlink(skbptr);
      kfree_skb(skbptr);
    } else {
      skb_queue_tail(&rxmt_q, skbptr);
    }
  }
}

int proc_src_rt_opt(struct sk_buff * skb, unsigned char * optptr) {

  const struct iphdr * iph = skb->nh.iph;
  struct dsr_src_rt_opt * srcopt = (struct dsr_src_rt_opt *) optptr;
  struct rxmt_info * rxmtinfo = (struct rxmt_info * ) skb->cb;
  struct rtable * rth;
  __u32 fwdaddr;
  int err;

  int n_addr; /* number of addresses on source route */
  int i;      /* index to the next address to be visited */

  n_addr = (srcopt->len - 2) / 4;

  /* don't process src rt if it is to us or we are the gateway */
  if (iph->daddr == IPADDR || (gw == 1 && NOT_DSR_SUBNET(iph->daddr))) {
    add_reverse_route(iph->saddr, srcopt->addr, n_addr);
    return 0;
  }

  /* we are not gateway and it is headed for an external node */
  //if (NOT_DSR_SUBNET(iph->daddr))
  //return -1;

  (srcopt->segs_left)--;
  i = n_addr - srcopt->segs_left;
  //DSR_ASSERT(i > 0);
  
  add_forward_route(iph->daddr, srcopt->addr + i, srcopt->segs_left);
  add_reverse_route(iph->saddr, srcopt->addr, i - 1);

  /* if ttl is about to expire and we are supposed to forward */
  if (iph->ttl == 1) {
    return 0;
  }

  rxmtinfo->srcrt = srcopt;
  //printk("rxmtinfo:%u\n", rxmtinfo);
  //printk("proc src rt rxmtinfo->srcrt:%u\n", rxmtinfo->srcrt);
  if (srcopt->segs_left == 0) {
    //printk("route to %u.%u.%u.%u\n", NIPQUAD(iph->daddr));
    /* this will also forward the last hop to the gateway correctly as well */
    fwdaddr = iph->daddr;
  } else {
    //printk("route to %u.%u.%u.%u\n", NIPQUAD(srcopt->addr[i]));
    fwdaddr = srcopt->addr[i];
  }
  
  /*
   * we know the route right here so we do the routing first using dsr info
   * and the os won't route it again since it already routed by us. the os
   * would not route correctly since it will assume the destination is
   * reachable since we are on the same subnet.
   */
  if ((err = ip_route_input(skb, fwdaddr, iph->saddr, iph->tos, skb->dev))) {
    printk("ip_route_input failed dropping packet err:%i\n", err);
    dump_skb_ptr(skb);
    dump_skb(PF_INET, skb);
    return -1;
  }

  /* 
   * we are forwarding packets within the same subnet and ip_route_input
   * will send icmp redirects at a backed off rate. we don't really want
   * this. the next two lines is to stop icmp redirects at ip_forward()
   */
  rth = (struct rtable *) skb->dst;
  DSR_ASSERT(rth != NULL);
  rth->rt_flags &= (~(RTCF_DOREDIRECT));
  return 0;
}
