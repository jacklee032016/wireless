#include "dsr-kmodule.h"

MODULE_AUTHOR("Alex Song <s369677@student.uq.edu.au>");
MODULE_DESCRIPTION("Dynamic Source Routing Kernel Module");
static char * ifname = "eth0";
MODULE_PARM(ifname, "s");
MODULE_PARM_DESC(ifname, "Network interface name. default is eth0.");
static int gw = 0;
MODULE_PARM(gw, "i");
MODULE_PARM_DESC(gw, "Gateway mode 0 is off 1 is on. default is off.");

/* variables */
static __u32 IPADDR;
static __u32 NETMASK;
static __u32 BCAST;
static __u32 NETWORK;
static int hhl;
static struct rt_entry rt_cache[ROUTE_CACHE_SIZE];
static int rt_cache_size = 0;
static struct id_fifo rt_req_ids[REQ_TABLE_SIZE];
static int rt_req_ids_size = 0;
static struct rt_req_entry rt_req_table[REQ_TABLE_SIZE];
static int rt_req_table_size = 0;
static __u16 ack_id;
static __u16 rtreq_id;
static struct sk_buff_head rxmt_q;
static struct sk_buff_head send_q;
static struct sk_buff_head jitter_q;

/* statistic variables */
unsigned int stat_send_q_timeout = 0;
unsigned int stat_send_q_drop = 0;
unsigned int stat_ack_q_timeout = 0;
unsigned int stat_ack_q_drop = 0;
unsigned int stat_ack_q_resend = 0;
unsigned int stat_send_rt_err = 0;
unsigned int stat_send_rt_req = 0;
unsigned int stat_send_rt_reply = 0;
unsigned int stat_forward_pkts = 0;
unsigned int stat_add_f_rt = 0;
unsigned int stat_add_r_rt = 0;
unsigned int stat_remove_rt = 0;
unsigned int stat_dsr_send = 0;
unsigned int stat_dsr_frag = 0;
unsigned int stat_output_err = 0;

// struct needed to hook at pre route
static struct nf_hook_ops pre_route = {
  {NULL, NULL},
  pre_route_handler,
  PF_INET,
  NF_IP_PRE_ROUTING,
  NF_IP_PRI_FIRST,
};
// struct needed to hook at local out
static struct nf_hook_ops local_out = {
  {NULL, NULL},
  local_out_handler,
  PF_INET,
  NF_IP_LOCAL_OUT,
  NF_IP_PRI_MANGLE,
};
// struct needed to hook at post route for gateway
static struct nf_hook_ops gw_post_route = {
  {NULL, NULL},
  gw_post_route_handler,
  PF_INET,
  NF_IP_POST_ROUTING,
  NF_IP_PRI_NAT_SRC + 1,
};
// struct needed to hook at post route for gateway
static struct nf_hook_ops gw_post_route_out = {
  {NULL, NULL},
  gw_post_route_out_handler,
  PF_INET,
  NF_IP_POST_ROUTING,
  NF_IP_PRI_FIRST,
};
#ifdef DSR_DEBUG
// struct needed to hook at post route
static struct nf_hook_ops post_route = 
{
	{NULL, NULL},
	post_route_handler,
	PF_INET,
	NF_IP_POST_ROUTING,
	NF_IP_PRI_FIRST,
};
// struct needed to hook at local in
static struct nf_hook_ops local_in = {
  {NULL, NULL},
  local_in_handler,
  PF_INET,
  NF_IP_LOCAL_IN,
  NF_IP_PRI_FIRST,
};
#endif

/* initialise the module */
int init_module()
{
	int i;
	struct proc_dir_entry * proc_file;
	struct net_device * netdev = dev_get_by_name(ifname);
	struct in_device * indev = in_dev_get(netdev);

	/* short circuit if statement */
	if (netdev != NULL && indev != NULL && indev->ifa_list != NULL)
	{
#ifdef DSR_DEBUG
		printk("local:%u.%u.%u.%u\n", NIPQUAD(indev->ifa_list->ifa_local));
		printk("dst:%u.%u.%u.%u\n", NIPQUAD(indev->ifa_list->ifa_address));
		printk("mask:%u.%u.%u.%u\n", NIPQUAD(indev->ifa_list->ifa_mask));
		printk("bcast:%u.%u.%u.%u\n", NIPQUAD(indev->ifa_list->ifa_broadcast));
#endif
		IPADDR = indev->ifa_list->ifa_local;
		NETMASK = indev->ifa_list->ifa_mask;
		BCAST = indev->ifa_list->ifa_broadcast;
	}
	else
	{
		return -1;
	}

	hhl = netdev->hard_header_len;
	in_dev_put(indev);
	dev_put(netdev);

	if ((sizeof(struct rxmt_info) >= 48) ||
		(sizeof(struct jitter_send) >= 48) || (sizeof(struct send_info) >= 48))
	{
		printk("struct size error %i %i %i\n",   sizeof(struct rxmt_info),
			sizeof(struct jitter_send),  sizeof(struct send_info));
		return -1;
	}

	/* calculate network address */
	NETWORK = IPADDR & NETMASK;

	/* init our route cache */
	for (i = 0;i < ROUTE_CACHE_SIZE;i++)
		rt_cache[i].dst = 0;

	/* init our rt req id table */
	for (i = 0;i < REQ_TABLE_SIZE;i++)
		rt_req_ids[i].saddr = 0;

	/* init our rt req table */
	for (i = 0;i < REQ_TABLE_SIZE;i++)
		rt_req_table[i].taddr = 0;

	/* nf_register_hook returns int but seems to be always 0 */
	nf_register_hook(&pre_route);
	nf_register_hook(&local_out);

	if (gw == 1)
	{
		nf_register_hook(&gw_post_route_out);
		nf_register_hook(&gw_post_route);
	}

#ifdef DSR_DEBUG
	nf_register_hook(&post_route);
	nf_register_hook(&local_in);
#endif

  //printk("creating proc read entry\n");
	proc_file = create_proc_read_entry("dsr", 0/*mode_t*/, &proc_root, dsr_read_proc, NULL/*data*/);

	ack_id = net_random();
	rtreq_id = net_random();
	skb_queue_head_init(&rxmt_q);
	skb_queue_head_init(&send_q);
	skb_queue_head_init(&jitter_q);

	//printk("success\n");
	printk(KERN_INFO "dsr:Dynamic Source Routing activated on %s.\n", ifname);
	if (gw == 1)
		printk(KERN_INFO "dsr:Running as a gateway.\n");
	else
		printk(KERN_INFO "dsr:Running as a node\n");

	printk(KERN_INFO "dsr:(c) Alex Song <s369677@student.uq.edu.au>\n");
	return 0;// 0 means module succesfully loaded
}

/* cleanup module unregister everything */
void cleanup_module() {
  int i;
  struct sk_buff * skbptr;

  /* nf_unregister_hook returns void */
  nf_unregister_hook(&pre_route);
  nf_unregister_hook(&local_out);
  if (gw == 1) {
    nf_unregister_hook(&gw_post_route_out);
    nf_unregister_hook(&gw_post_route);
  }

#ifdef DSR_DEBUG
  nf_unregister_hook(&post_route);
  nf_unregister_hook(&local_in);
#endif

  /* clean retransmit queue */
  skbptr = skb_peek(&rxmt_q);
  for (i = 0; i < skb_queue_len(&rxmt_q);i++) {
    struct rxmt_info * rxmtinfo;
    DSR_ASSERT(skbptr != NULL);
    rxmtinfo = (struct rxmt_info *) skbptr->cb;
    del_timer(&(rxmtinfo->timer));
    skbptr = skbptr->next;
  }
  skb_queue_purge(&rxmt_q);

  /* clean route request timers */
  for (i = 0;i < REQ_TABLE_SIZE;i++) {
    if (rt_req_table[i].taddr != 0) {
      del_timer(&(rt_req_table[i].timer));
    }
  }

  /* clean send queue */
  skbptr = skb_peek(&send_q);
  for (i = 0; i < skb_queue_len(&send_q);i++) {
    struct send_info * sendinfo;
    DSR_ASSERT(skbptr != NULL);
    sendinfo = (struct send_info *) skbptr->cb;
    del_timer(&(sendinfo->timer));
    skbptr = skbptr->next;
  }
  skb_queue_purge(&send_q);

  /* clean jitter send queue */
  skbptr = skb_peek(&jitter_q);
  for (i = 0; i < skb_queue_len(&jitter_q);i++) {
    struct jitter_send * jittersend;
    DSR_ASSERT(skbptr != NULL);
    jittersend = (struct jitter_send *) skbptr->cb;
    del_timer(&(jittersend->timer));
    skbptr = skbptr->next;
  }
  skb_queue_purge(&jitter_q);

  //printk("remove proc entry\n");
  remove_proc_entry("dsr", &proc_root);
  printk(KERN_INFO "dsr:Dynamic Source Routing deactivated on %s.\n", ifname);
}

#ifdef DSR_DEBUG
unsigned int local_in_handler(unsigned int hooknum,
                              struct sk_buff **skb,
                              const struct net_device *in,
                              const struct net_device *out,
                              int (*okfn)(struct sk_buff *)) {

  struct iphdr *iph = (*skb)->nh.iph;

  if (DSR_SUBNET(iph->saddr)
      /*&& (iph->protocol == DSR_PROTOCOL || iph->protocol == 1)*/) {
    printk("\nlocal in enter\n");

    dump_skb_ptr(*skb);    
    dump_skb(PF_INET, *skb);
    
    printk("local in leave\n");
  }
  return NF_ACCEPT;
}

unsigned int post_route_handler(unsigned int hooknum,
                                struct sk_buff **skb,
                                const struct net_device *in,
                                const struct net_device *out,
                                int (*okfn)(struct sk_buff *)) {
  struct iphdr *iph = (*skb)->nh.iph;
  
  if (DSR_SUBNET(iph->saddr)
      /*&& (iph->protocol == DSR_PROTOCOL || iph->protocol == 1)*/) {
    printk("\npost route enter\n");
    
    dump_skb_ptr(*skb);    
    dump_skb(PF_INET, *skb);
    
    printk("post route leave\n");
  }
  return NF_ACCEPT;
}
#endif

unsigned int local_out_handler(unsigned int hooknum,
                               struct sk_buff **skb,
                               const struct net_device *in,
                               const struct net_device *out,
                               int (*okfn)(struct sk_buff *)) {

  struct iphdr *iph = (*skb)->nh.iph;

  if (iph->daddr == IPADDR || NOT_DSR_SUBNET(iph->saddr) 
      || BADCLASS(iph->daddr) || ZERONET(iph->daddr)
      || LOOPBACK(iph->daddr) || MULTICAST(iph->daddr) ) {

    return NF_ACCEPT;

  } else {
    /* if we are here, src must be on dsr network */
    struct rtable * rt = (struct rtable *) (*skb)->dst;
    struct rt_entry * rt_ptr;
    __u32 fwdaddr;

#ifdef DSR_DEBUG
    printk("\nlocal out enter\n");
    dump_skb_ptr(*skb);
    dump_skb(PF_INET, *skb);
#endif

    (*skb)->nfcache |= NFC_UNKNOWN;

    /* check if we are sending to dsr network or external */
    if (DSR_SUBNET(iph->daddr)) {
      /* we are sending to the dsr network */
      fwdaddr = iph->daddr;
    } else {
      /* we are sending to external network */
      fwdaddr = rt->rt_gateway;
    }
    
    rt_ptr = lookup_route(rt_cache, fwdaddr);
    if (rt_ptr == NULL) {
#ifdef DSR_DEBUG
      printk("send route discovery\n");
#endif
      send_rt_req(fwdaddr);
      send_q_add(&send_q, *skb, SEND_BUFFER_SIZE, fwdaddr, okfn);
      //printk("leave\n");
      //kfree_skb(*skb);
    } else {
      dsr_send(skb, rt_ptr, okfn);
    }
#ifdef DSR_DEBUG
    printk("local out leave\n");
#endif
    return NF_STOLEN;
  }
}

unsigned int pre_route_handler(unsigned int hooknum,
                               struct sk_buff **skb,
                               const struct net_device *in,
                               const struct net_device *out,
                               int (*okfn)(struct sk_buff *)) {

  struct iphdr *iph = (*skb)->nh.iph;
  
  if (iph->protocol == DSR_PROTOCOL) {

    const struct dsr_hdr * dsrhdr = (struct dsr_hdr *) ((*skb)->nh.raw +
                                                        (iph->ihl)*4);
    const unsigned char * optstart = (unsigned char *) (dsrhdr + 1);
    unsigned char * optptr = (unsigned char *) (dsrhdr + 1);
    struct dsr_opt_hdr * dsropt;
    int ackreq = 0;

#ifdef DSR_DEBUG
    printk("\npre route enter\n");
    dump_skb_ptr(*skb);
    dump_skb(PF_INET, *skb);
#endif
    
    (*skb)->nfcache |= NFC_UNKNOWN;
    (*skb)->nfcache |= NFC_ALTERED;
    
    while (optptr - optstart != ntohs(dsrhdr->length)) {

      dsropt = (struct dsr_opt_hdr *) optptr;
      //printk("i:%u len:%u\n", optptr - optstart, ntohs(dsrhdr->length));
      switch (dsropt->type) {
      case PAD1:
        //printk("got pad\n");
        break;
      case PADN:
        //printk("got padn\n");
        break;
      case ROUTE_REQ:
        //printk("got rt req\n");
        proc_rt_req_opt(*skb, optptr);
        break;
      case ROUTE_REPLY:
        //printk("got rt reply\n");
        proc_rt_reply_opt(*skb, optptr);
        break;
      case ROUTE_ERROR:
        //printk("got rt err\n");
        proc_rt_err_opt(*skb, optptr);
        break;
      case ACK_REQ:
        //printk("got ack req\n");
        proc_ack_req_opt(*skb, optptr/*, in*/);
        ackreq = 1;
        break;
      case ACK:
        //printk("got ack\n");
        proc_ack_reply_opt(*skb, optptr);
        break;
      case SRC_ROUTE:
        //printk("got src rt opt\n");
        //proc_src_rt_opt(*skb, optptr);
        if (proc_src_rt_opt(*skb, optptr) != 0) {
          return NF_DROP;
        }
        break;
      default:
        printk("unknown dsr option %i\n", dsropt->type);
        dump_skb_ptr(*skb);
        dump_skb(PF_INET, *skb);
        return NF_DROP;
        break;
      }
      
      if (dsropt->type == PAD1)
        optptr++;
      else
        optptr += dsropt->len + 2;
    }
    
    if (iph->daddr == IPADDR || iph->daddr == BCAST) {
      //printk("for us\n");
      if ((iph->ihl)*4 + ntohs(dsrhdr->length) + 4 >= ntohs(iph->tot_len)) {
        //printk("no payload, drop\n");
        return NF_DROP;
      }
      rmv_dsr_hdr(*skb);
      okfn(*skb);
      return NF_STOLEN;
    } else if (gw == 1 && NOT_DSR_SUBNET(iph->daddr)) {
      stat_forward_pkts++;
      rmv_dsr_hdr(*skb);
    } else if (iph->ttl == 1) {
      //printk("ttl expired\n");
      /* if ttl is about to expire and we are supposed to forward */
      rmv_dsr_hdr(*skb);
    } else {
      //printk("forward packet\n");
      if (ackreq == 1)
        ack_q_add(&rxmt_q, *skb, DSR_RXMT_BUFFER_SIZE);
      stat_forward_pkts++;
      //okfn(*skb);
      //return NF_STOLEN;
    }
#ifdef DSR_DEBUG
    printk("pre route leave\n");
#endif
    return NF_ACCEPT;
  }
  return NF_ACCEPT;
}

unsigned int gw_post_route_handler(unsigned int hooknum,
                                   struct sk_buff **skb,
                                   const struct net_device *in,
                                   const struct net_device *out,
                                   int (*okfn)(struct sk_buff *)) {
  
  struct iphdr *iph = (*skb)->nh.iph;
  DSR_ASSERT(gw == 1);

  if (iph->protocol != DSR_PROTOCOL && NOT_DSR_SUBNET(iph->saddr) &&
      DSR_SUBNET(iph->daddr)) {
    struct rt_entry * rt_ptr;
    
    //printk("gw post route:%u.%u.%u.%u d:%u.%u.%u.%u\n", NIPQUAD(iph->saddr),
    //       NIPQUAD(iph->daddr));
#ifdef DSR_DEBUG
    dump_skb_ptr(*skb);
    dump_skb(PF_INET, *skb);
#endif

    /* packet from outside dsr net and we are a gateway */
    rt_ptr = lookup_route(rt_cache, iph->daddr);
    if (rt_ptr == NULL) {
#ifdef DSR_DEBUG
      printk("gw send route discovery\n");
#endif
      send_rt_req(iph->daddr);
      send_q_add(&send_q, *skb, SEND_BUFFER_SIZE, iph->daddr, okfn);
    } else {
      //printk("gw have route\n");
      dsr_send(skb, rt_ptr, okfn);
    }
    return NF_STOLEN;
  }
  return NF_ACCEPT;
}

unsigned int gw_post_route_out_handler(unsigned int hooknum,
                                       struct sk_buff **skb,
                                       const struct net_device *in,
                                       const struct net_device *out,
                                       int (*okfn)(struct sk_buff *)) {
  
  struct iphdr *iph = (*skb)->nh.iph;
  
  DSR_ASSERT(gw == 1);
  if (iph->protocol == DSR_PROTOCOL) {
    okfn(*skb);
    return NF_STOLEN;
  }
  return NF_ACCEPT;
}

/* this function is copied from the linux source code fs/proc/proc_misc.c */
int proc_calc_metrics(char *page, char **start, off_t off,
                      int count, int *eof, int len) {
  if (len <= off+count) *eof = 1;
  *start = page + off;
  len -= off;
  if (len>count) len = count;
  if (len<0) len = 0;
  return len;
}

int dsr_read_proc(char *page, char **start, off_t off, int count, int *eof,
                  void *data) {

  int len = 0;
#ifdef DSR_DEBUG
  int i, j;
#endif

  MOD_INC_USE_COUNT;
  len += sprintf(page + len, "IP:%u.%u.%u.%u "
                 "BCAST:%u.%u.%u.%u NETMASK:%u.%u.%u.%u gw:%u\n",
		 NIPQUAD(IPADDR), NIPQUAD(BCAST), NIPQUAD(NETMASK), gw);

  /* stats */
  len += sprintf(page + len, "send_q_drop:%u send_q_timeout:%u ack_q_drop:%u "
                 "ack_q_timeout:%u\n", stat_send_q_drop, stat_send_q_timeout,
                 stat_ack_q_drop, stat_ack_q_timeout);

  len += sprintf(page + len, "send_rt_err:%u send_rt_reply:%u "
                 "add_rt f:%u r:%u send_rt_req:%u\n",
                 stat_send_rt_err, stat_send_rt_reply, stat_add_f_rt,
                 stat_add_r_rt, stat_send_rt_req);

  len += sprintf(page + len, "forward pkts:%u ack_q_resend:%u remove_rt:%u "
                 "output_err:%u\n", stat_forward_pkts, stat_ack_q_resend,
                 stat_remove_rt, stat_output_err);

  len += sprintf(page + len, "dsr_send:%u dsr_frag:%u\n",
                 stat_dsr_send, stat_dsr_frag);

  len += sprintf(page + len, "send_q len:%i\n", skb_queue_len(&send_q));
  len += sprintf(page + len, "rxmt_q len:%i\n", skb_queue_len(&rxmt_q));
  /* dump rt req ids */
  len += sprintf(page + len, "rt_req_ids len:%i\n", rt_req_ids_size);
#ifdef DSR_DEBUG
  for (i = 0;i < REQ_TABLE_SIZE;i++) {
    if (rt_req_ids[i].saddr != 0) {
      len += sprintf(page + len, "saddr:%u.%u.%u.%u len:%i\n",
                     NIPQUAD(rt_req_ids[i].saddr), rt_req_ids[i].len);
      for (j = 0;j < rt_req_ids[i].len;j++) {
        len += sprintf(page + len, "%i ",
                       rt_req_ids[i].id[(rt_req_ids[i].head + j)
                                       % REQ_TABLE_IDS]);
      }
      len += sprintf(page + len, "\n");
    }
  }
#endif
  /* dump rt req table */
  len += sprintf(page + len, "rt_req_table len:%i\n", rt_req_table_size);
#ifdef DSR_DEBUG
  for (i = 0;i < REQ_TABLE_SIZE;i++) {
    if (rt_req_table[i].taddr != 0) {
      len += sprintf(page + len, "taddr:%u.%u.%u.%u n_req:%i ",
                     NIPQUAD(rt_req_table[i].taddr), rt_req_table[i].n_req);
      len += sprintf(page + len, "timeout:%i",
                     (int)(rt_req_table[i].timeout *1000)/HZ);
      len += sprintf(page + len, "\n");
    }
  }
#endif
  /* dump route cache */
  len += sprintf(page + len, "rt_cache len:%i\n", rt_cache_size);
#ifdef DSR_DEBUG
  for (i = 0;i < ROUTE_CACHE_SIZE;i++) {
    if (rt_cache[i].dst != 0) {

      len += sprintf(page + len, "%i %u.%u.%u.%u age:%u segs:%u\n",
                     i, NIPQUAD(rt_cache[i].dst),
                     (unsigned int) (CURRENT_TIME - rt_cache[i].time),
                     rt_cache[i].segs_left);
      for (j = 0;j < rt_cache[i].segs_left;j++) {
        len += sprintf(page + len, " %u.%u.%u.%u",
                       NIPQUAD(rt_cache[i].addr[j]));
      }
      len += sprintf(page + len, "\n");
    }
  }
#endif
  MOD_DEC_USE_COUNT;
  DSR_ASSERT(len <= count);
  return proc_calc_metrics(page, start, off, count, eof, len);
}
