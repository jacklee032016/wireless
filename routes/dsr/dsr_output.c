#include "dsr_output.h"
#include "dsr_debug.h"
#include "dsr_route.h"
#include "dsr_queue.h"

extern __u32 IPADDR;
extern __u32 NETMASK;
extern __u32 BCAST;
extern __u32 NETWORK;
extern int   gw;
extern int   hhl;
extern __u16 ack_id;
extern __u16 rtreq_id;
extern struct sk_buff_head rxmt_q;
extern struct rt_entry rt_cache[];
extern struct rt_req_entry rt_req_table[];
extern int rt_req_table_size;
extern struct sk_buff_head jitter_q;
/* stats */
extern unsigned int stat_send_rt_err;
extern unsigned int stat_send_rt_req;
extern unsigned int stat_send_rt_reply;
extern unsigned int stat_dsr_send;
extern unsigned int stat_dsr_frag;
extern unsigned int stat_output_err;

/*
 * this function allows us to send packets without traversing
 * the rest of netfilter. used for dsr specific packets.
 */
int dsr_output(struct sk_buff *skb)
{
	struct net_device *dev = skb->dst->dev;
	struct dst_entry *dst = skb->dst;
	struct hh_cache *hh = dst->hh;

	skb->dev = dev;
	skb->protocol = __constant_htons(ETH_P_IP);

	if (hh)
	{
		read_lock_bh(&hh->hh_lock);
		memcpy(skb->data - 16, hh->hh_data, 16);
		read_unlock_bh(&hh->hh_lock);
		skb_push(skb, hh->hh_len);
		return hh->hh_output(skb);
	}
	else if (dst->neighbour)
		return dst->neighbour->output(skb);

	printk(KERN_DEBUG "dsr_output\n");
	kfree_skb(skb);

	return -EINVAL;
}

struct sk_buff * create_dsr_packet(int length, __u32 daddr)
{
	struct sk_buff * newskb;
	struct iphdr * iph;

	/* 20 for the ip header */
	newskb = alloc_skb(20+length+hhl+15,GFP_ATOMIC);
	if (newskb == NULL)
	{
		return NULL;
	}
	else
	{
		skb_reserve(newskb, (hhl+15)&~15);
		skb_put(newskb, 20+length); /* iph + dsr stuff */
		newskb->nh.raw = newskb->data;

		iph = newskb->nh.iph;
		iph->version  = 4;
		iph->ihl      = 5;
		iph->tos      = 0;
		iph->frag_off = 0;
		iph->frag_off |= htons(IP_DF);
		iph->ttl      = MAX_ROUTE_LEN + 1;
		iph->daddr    = daddr;
		iph->saddr    = IPADDR;
		iph->protocol = DSR_PROTOCOL;
		iph->tot_len  = htons(newskb->len);
		iph->id       = 0;

		ip_send_check(iph);
		return newskb;
	}
}

void jitter_send_timeout(unsigned long data)
{
	struct sk_buff * skb = (struct sk_buff *) data;
	skb_unlink(skb);
	dsr_output(skb);
}

void jitter_send(struct sk_buff * skb) {

  struct jitter_send * jitter = (struct jitter_send *) skb->cb;

  skb_queue_tail(&jitter_q, skb);
  init_timer(&(jitter->timer));
  jitter->timer.function = jitter_send_timeout;
  jitter->timer.data = (unsigned long) skb;
  jitter->timer.expires = jiffies +
    ((net_random() % BCAST_JITTER) * HZ) / 1000;
  add_timer(&(jitter->timer));
}

void add_ack_req_opt(unsigned char * buff) {
  
  struct dsr_ack_req_opt * ackreq = (struct dsr_ack_req_opt *)(buff);

  ackreq->type = ACK_REQ;
  ackreq->len = 6;
  ack_id++;
  ackreq->ident = ack_id;
  ackreq->saddr = IPADDR;
}

int add_src_rt_opt(struct sk_buff * skb, unsigned char * buff,
                   const struct rt_entry * rt_ptr) {

  const struct iphdr * iph = skb->nh.iph;
  struct dsr_src_rt_opt * srcopt = (struct dsr_src_rt_opt *) (buff);
  struct rxmt_info * rxmtinfo = (struct rxmt_info * ) skb->cb;
  int i;
  __u32 fwdaddr;

  DSR_ASSERT(rt_ptr != NULL);

  srcopt->type = SRC_ROUTE;
  srcopt->segs_left = rt_ptr->segs_left;
  srcopt->len = (rt_ptr->segs_left * 4) + 2;
  // not implemented yet
  // srcopt->salvage = 0;
  // srcopt->reserved = 0;
  // srcopt->lasthopx = 0;
  // srcopt->firsthopx = 0;

  for (i = 0;i < rt_ptr->segs_left;i++)
    srcopt->addr[i] = rt_ptr->addr[i];

  if (rt_ptr->segs_left == 0)
    fwdaddr = iph->daddr;
  else
    fwdaddr = srcopt->addr[0];

  rxmtinfo->srcrt = srcopt;
  //printk("rxmtinfo:%u\n", rxmtinfo);
  //printk("add src rt rxmtinfo->srcrt:%u\n", rxmtinfo->srcrt);
  
  if (gw == 1 && DSR_SUBNET(fwdaddr) && NOT_DSR_SUBNET(iph->saddr)) {
    /* release old route */
    dst_release(skb->dst);
    if (reroute_output(skb, IPADDR, fwdaddr) != 0) {
      printk("add_src_rt: route failed\n");
      return -1;
    }
  } else { 
    if (reroute_output(skb, iph->saddr, fwdaddr) != 0)
      return -1;
  }
  return 0;
}

void dsr_options_fragment(struct sk_buff * skb) 
{
  unsigned char * optptr = skb->nh.raw;
  struct ip_options * opt = &(IPCB(skb)->opt);
  int  l = opt->optlen;
  int  optlen;

  while (l > 0) {
    switch (*optptr) {
    case IPOPT_END:
      return;
    case IPOPT_NOOP:
      l--;
      optptr++;
      continue;
    }
    optlen = optptr[1];
    if (optlen<2 || optlen>l)
      return;
    if (!IPOPT_COPIED(*optptr))
      memset(optptr, IPOPT_NOOP, optlen);
    l -= optlen;
    optptr += optlen;
  }
  opt->ts = 0;
  opt->rr = 0;
  opt->rr_needaddr = 0;
  opt->ts_needaddr = 0;
  opt->ts_needtime = 0;
  return;
}

void dsr_fragment_send(struct sk_buff *skb, int dsrlen, int ackreqoff,
                       int srcrtoff, int (*output)(struct sk_buff*))
{
  struct iphdr *iph;
  unsigned char *raw;
  unsigned char *ptr;
  struct net_device *dev;
  struct sk_buff *skb2;
  unsigned int mtu, hlen, left, len; 
  int offset;
  int not_last_frag;
  struct rtable *rt = (struct rtable*)skb->dst;
  int err = 0;

  /* alex: */
  struct rxmt_info * rxmtinfo;

  dev = rt->u.dst.dev;

  /* Point into the IP datagram header */
  raw = skb->nh.raw;
  iph = (struct iphdr*)raw;

  /* Setup starting values. */
  /* alex: +dsrlen */
  hlen = iph->ihl * 4 + dsrlen;
  left = ntohs(iph->tot_len) - hlen;  /* Space per frame */
  mtu = rt->u.dst.pmtu - hlen;        /* Size of data space */
  ptr = raw + hlen;                   /* Where to start from */

  /* Fragment the datagram. */
  offset = (ntohs(iph->frag_off) & IP_OFFSET) << 3;
  not_last_frag = iph->frag_off & htons(IP_MF);

  /* Keep copying data until we run out. */
  while(left > 0) {
    len = left;
    /* IF: it doesn't fit, use 'mtu' - the data space left */
    if (len > mtu)
      len = mtu;
    /* IF: we are not sending upto and including the packet end
       then align the next start on an eight byte boundary */
    if (len < left) {
      len &= ~7;
    }
    /* Allocate buffer. */
    if ((skb2 = alloc_skb(len+hlen+dev->hard_header_len+15,
			  GFP_ATOMIC)) == NULL) {
      NETDEBUG(printk(KERN_INFO "dsr: frag: no memory for new fragment!\n"));
      err = -ENOMEM;
      goto fail;
    }

    /* Set up data on packet */
    skb2->pkt_type = skb->pkt_type;
    skb2->priority = skb->priority;
    skb_reserve(skb2, (dev->hard_header_len+15)&~15);
    skb_put(skb2, len + hlen);
    skb2->nh.raw = skb2->data;
    skb2->h.raw = skb2->data + hlen;

    /* Charge the memory for the fragment to any owner it might possess */
    if (skb->sk)
      skb_set_owner_w(skb2, skb->sk);
    skb2->dst = dst_clone(skb->dst);
    skb2->dev = skb->dev;

    /* Copy the packet header into the new buffer. */
    memcpy(skb2->nh.raw, raw, hlen);

    /* Copy a block of the IP datagram. */
    memcpy(skb2->h.raw, ptr, len);
    left -= len;

    /* Fill in the new header fields. */
    iph = skb2->nh.iph;
    iph->frag_off = htons((offset >> 3));

    /* ANK: dirty, but effective trick. Upgrade options only if
     * the segment to be fragmented was THE FIRST (otherwise,
     * options are already fixed) and make it ONCE
     * on the initial skb, so that all the following fragments
     * will inherit fixed options.
     */
    if (offset == 0)
      dsr_options_fragment(skb);

    /* 
     * Added AC : If we are fragmenting a fragment that's not the
     * last fragment then keep MF on each bit
     */
    if (left > 0 || not_last_frag)
      iph->frag_off |= htons(IP_MF);
    ptr += len;
    offset += len;

#ifdef CONFIG_NETFILTER
    /* Connection association is same as pre-frag packet */
    skb2->nfct = skb->nfct;
    nf_conntrack_get(skb2->nfct);
#ifdef CONFIG_NETFILTER_DEBUG
    skb2->nf_debug = skb->nf_debug;
#endif
#endif

    /* Put this fragment into the sending queue. */
    IP_INC_STATS(IpFragCreates);

    iph->tot_len = htons(len + hlen);

    ip_send_check(iph);

    /* alex: */
    rxmtinfo = (struct rxmt_info *) skb2->cb;
    rxmtinfo->srcrt = (struct dsr_src_rt_opt *)
      (skb2->nh.raw + srcrtoff);
    add_ack_req_opt(skb2->nh.raw + ackreqoff);
    /* alex: */
    ack_q_add(&rxmt_q, skb2, DSR_RXMT_BUFFER_SIZE);

    err = output(skb2);
    if (err)
      goto fail;
  }
  kfree_skb(skb);
  //IP_INC_STATS(IpFragOKs);
  //return err;
  return;
 fail:
  /* alex: */
  stat_output_err++;
  kfree_skb(skb); 
#ifdef DSR_DEBUG
  printk("dsr_fragment_send failed, err:%i\n", err);
#endif
  return;
  //IP_INC_STATS(IpFragFails);
  //return err;
}

void dsr_send(struct sk_buff **skb, struct rt_entry * dsr_rt,
              int (*output)(struct sk_buff*)) {

  struct iphdr * iph = (*skb)->nh.iph;
  struct sk_buff * newskb;
  struct dsr_hdr * dsrhdr;
  unsigned char * optptr;
  /* size is the total amout of stuff added to the packet */
  int size = (dsr_rt->segs_left*4)+4 + 4 + 8; /* src rt + dsrhdr + ackreq */

  /*
   * nasty nasty nasty !!! need to keep original headroom free
   * otherwise the kernel will panic and crash
   * more nasty stuff, must do (size+15)&~15
   */
  newskb = skb_copy_expand(*skb, ((size+15)&~15) + skb_headroom(*skb),
                           skb_tailroom(*skb), GFP_ATOMIC);
  
  /* can't allocate extra buffer space */
  if (newskb == NULL) {
    kfree_skb(*skb);
    return;
  }

  /* set original owner */
  if ((*skb)->sk != NULL)
    skb_set_owner_w(newskb, (*skb)->sk);
  
  /* free old skb */
  kfree_skb(*skb);
  *skb = newskb;
  
  (*skb)->nh.raw = skb_push(*skb, size);
  
  /* copy ip header, iph points to the old header */
  memmove( (*skb)->nh.raw, (*skb)->nh.raw + size, (iph->ihl)*4);

  /* iph points to the new header now */
  iph = (*skb)->nh.iph;
  dsrhdr = (struct dsr_hdr *) ( (*skb)->nh.raw + (iph->ihl)*4);
  dsrhdr->nexthdr = iph->protocol;
  dsrhdr->reserved = 0;
  dsrhdr->length = htons(size - 4);
  iph->protocol = DSR_PROTOCOL;

  /* set the don't fragment flag */
  iph->frag_off |= htons(IP_DF);

  /* change length and recalculate checksum */
  iph->tot_len = htons( (*skb)->len ); 
  ip_send_check(iph);

  /* ip and dsr header is done, now have to add dsr options */
  optptr = (unsigned char *) (dsrhdr + 1);
  add_src_rt_opt(*skb, optptr, dsr_rt);
  optptr += dsr_rt->segs_left * 4 + 4;

  /* optptr now points to the empty ack req option */
  if ( (*skb)->len > (*skb)->dst->pmtu) {
    int ackreqoff = optptr - (*skb)->nh.raw;
    int srcrtoff = ackreqoff - (dsr_rt->segs_left * 4 + 4);
#ifdef DSR_DEBUG
    printk("dsr fragment send\n");
#endif
    /* select new id if we need to */
    if ( (*skb)->nh.iph->id == 0)
//      ip_select_ident( (*skb)->nh.iph, (*skb)->dst);
      ip_select_ident( (*skb)->nh.iph, (*skb)->dst, NULL);

    stat_dsr_frag++;
    dsr_fragment_send(*skb, size, ackreqoff, srcrtoff, output);
  } else {
#ifdef DSR_DEBUG
    printk("dsr normal send\n");
#endif
    add_ack_req_opt(optptr);
    ack_q_add(&rxmt_q, *skb, DSR_RXMT_BUFFER_SIZE);
    stat_dsr_send++;
    if(output(*skb)) {
      stat_output_err++;
#ifdef DSR_DEBUG
      printk("dsr send failed\n");
#endif
    }
  }
  return;
}

void finish_send_rt_req(__u32 taddr) {

  struct iphdr * iph;
  struct sk_buff * newskb;
  struct dsr_hdr * dsrhdr;
  struct dsr_rt_req_opt * reqopt;

  /* create rt req packet */
  newskb = create_dsr_packet(4+8, BCAST);

  if (newskb != NULL) {
    iph = newskb->nh.iph;
    dsrhdr = (struct dsr_hdr *) (newskb->nh.raw + (iph->ihl)*4);
    reqopt = (struct dsr_rt_req_opt *) (dsrhdr + 1);

    dsrhdr->nexthdr = NO_NEXT_HEADER;
    dsrhdr->length = htons(8);

    reqopt->type = ROUTE_REQ;
    reqopt->len = 6; /* 2 + 4 */
    reqopt->ident = rtreq_id;
    rtreq_id++;
    reqopt->taddr = taddr;

    if (reroute_output(newskb, iph->saddr, iph->daddr) == 0) {
      stat_send_rt_req++;
      jitter_send(newskb);
    } else {
      kfree_skb(newskb);
    }
  }
}

void rt_req_timeout(unsigned long data) {

  //printk("rt req timeout at %u\n", CURRENT_TIME);
  DSR_ASSERT(data < REQ_TABLE_SIZE);
  if (rt_req_table[data].n_req < MAX_REQ_RXMT) {
    /* send another rt req */
    finish_send_rt_req(rt_req_table[data].taddr);
    rt_req_table[data].n_req++;

    if (rt_req_table[data].timeout * 2 < (MAX_REQ_PERIOD * HZ))
      rt_req_table[data].timeout *= 2;
    else
      rt_req_table[data].timeout = (MAX_REQ_PERIOD * HZ);
      
    rt_req_table[data].timer.expires = jiffies + rt_req_table[data].timeout;
    add_timer(&(rt_req_table[data].timer));
  } else {
    /* remove rt req entry */
    rt_req_table[data].taddr = 0;
    rt_req_table_size--;
  }
}

void send_rt_req(__u32 taddr) {

  int i;
  int oldest = 0;
  int empty = 0;
  int replace;

  for (i = 0;i < REQ_TABLE_SIZE;i++) {
    if (rt_req_table[i].taddr != 0) {
      if (rt_req_table[i].taddr == taddr) {
        //printk("send rt req found taddr\n");
        return;
      } else {
        if (rt_req_table[i].time < rt_req_table[oldest].time)
          oldest = i;
      }
    } else {
      empty = i;
    }
  }

  if (rt_req_table_size < REQ_TABLE_SIZE) {
    /* create new entry */
    replace = empty;
    rt_req_table_size++;
  } else {
    /* replace old entry */
    replace = oldest;
    del_timer(&(rt_req_table[oldest].timer));
  }

  rt_req_table[replace].taddr = taddr;
  rt_req_table[replace].time = jiffies;
  rt_req_table[replace].timeout = (REQ_PERIOD * HZ) / 1000;
  rt_req_table[replace].n_req = 1;

  init_timer(&(rt_req_table[replace].timer));
  rt_req_table[replace].timer.function = rt_req_timeout;
  rt_req_table[replace].timer.data = replace;
  rt_req_table[replace].timer.expires = jiffies +
    rt_req_table[replace].timeout;
  add_timer(&(rt_req_table[replace].timer));
  //printk("finish send\n");
  finish_send_rt_req(taddr);
}

void send_rt_reply(const struct sk_buff * skb, struct dsr_rt_req_opt * rtreq) {

  /* skb is a rt req packet */
  struct iphdr * iph = skb->nh.iph;
  struct dsr_src_rt_opt * srcrt;
  struct rxmt_info * rxmtinfo;
  struct dsr_rt_reply_opt * rtreply;
  struct dsr_padn_opt * padn;
  struct dsr_hdr * dsrhdr;
  unsigned char * optptr;
  struct sk_buff * newskb;
  int n_addr = (rtreq->len - 6) / 4;
  int src_rt_size = 4 + (n_addr * 4);
  int rt_reply_size = 5 + (n_addr * 4);
  int j;
  __u32 fwdaddr;

  //printk("send rt err size %i\n", src_rt_size);
  /* dsr hdr + rt reply + ack req + src rt + pad3*/
  newskb = create_dsr_packet(4+rt_reply_size+8+src_rt_size+3, iph->saddr);

  if (newskb != NULL) {
    iph = newskb->nh.iph;
    dsrhdr = (struct dsr_hdr *) (newskb->nh.raw + (iph->ihl)*4);
    //ackopt = (struct dsr_ack_opt *) (dsrhdr + 1);

    dsrhdr->nexthdr = NO_NEXT_HEADER;
    dsrhdr->length = htons(rt_reply_size+8+src_rt_size+3);

    /* pad3 */
    optptr = (unsigned char *) (dsrhdr + 1); 
    padn = (struct dsr_padn_opt *) optptr;
    padn->type = PADN;
    padn->len = 3 - 2;

    optptr += 3;
    rtreply = (struct dsr_rt_reply_opt *) optptr;
    rtreply->type = ROUTE_REPLY;
    rtreply->len = rt_reply_size - 2;
    rtreply->ident = rtreq->ident;
    optptr += rt_reply_size;

    //printk("send err next hop: %u.%u.%u.%u\n", NIPQUAD(rterr->typeinfo));

    /* add an ack req to route error message */
    add_ack_req_opt(optptr);
    optptr += 8; /* ack req */

    srcrt = (struct dsr_src_rt_opt *) optptr;
    srcrt->type = SRC_ROUTE;
    srcrt->len = src_rt_size - 2;
    srcrt->segs_left = n_addr;
    for (j = 0;j < n_addr;j++) {
      rtreply->addr[j] = rtreq->addr[j];
      srcrt->addr[j] = rtreq->addr[n_addr - j - 1];
    }

    if (srcrt->segs_left == 0)
      fwdaddr = iph->daddr;
    else
      fwdaddr = srcrt->addr[0];
    
    rxmtinfo = (struct rxmt_info *) newskb->cb;
    rxmtinfo->srcrt = srcrt;
    //printk("rxmtinfo:%u\n", rxmtinfo);
    //printk("send rt reply rxmtinfo->srcrt:%u\n", rxmtinfo->srcrt);

    if (reroute_output(newskb, iph->saddr, fwdaddr) == 0) {
      stat_send_rt_reply++;
      ack_q_add(&rxmt_q, newskb, DSR_RXMT_BUFFER_SIZE);
      dsr_output(newskb);
    } else {
      kfree_skb(newskb);
    }
  }
}

void send_rt_err(struct sk_buff * skb) {

  struct iphdr * iph = skb->nh.iph;
  struct rxmt_info * rxmtinfo = (struct rxmt_info *) skb->cb;
  struct dsr_rt_err_opt * rterr;
  struct dsr_src_rt_opt * srcrt;
  struct dsr_hdr * dsrhdr;
  unsigned char * optptr;
  struct sk_buff * newskb;
  const struct rtable * rt = (struct rtable *) skb->dst;
  int n_addr;     /* number of addresses on source route */
  int index;      /* index to the next address to be visited */
  int src_rt_size, j;
  __u32 fwdaddr;

  DSR_ASSERT(rt != NULL);
  /* shouldn't be sending a rt_err for a packet headed for external networks */
  DSR_ASSERT(gw == 0 || DSR_SUBNET(skb->nh.iph->daddr));
#ifdef DSR_DEBUG
  printk("send rt err\n");
#endif
  //printk("rxmtinfo:%u\n", rxmtinfo);
  //printk("rxmtinfo->srcrt:%u\n", rxmtinfo->srcrt);
  n_addr = (rxmtinfo->srcrt->len - 2) / 4;
  index = n_addr - rxmtinfo->srcrt->segs_left;

  if (rxmtinfo->srcrt->segs_left == 0) {
    if (NOT_DSR_SUBNET(skb->nh.iph->daddr)) {
      remove_route(rt_cache, IPADDR, rt->rt_gateway);
    } else {
      remove_route(rt_cache, IPADDR, skb->nh.iph->daddr);
    }
  } else {
    remove_route(rt_cache, IPADDR, rxmtinfo->srcrt->addr[index]);
  }

  if (index == 0)
    return;
  else
    src_rt_size = 4 + (index - 1) * 4;
  //printk("send rt err size %i\n", src_rt_size);
  /* dsr hdr + rt err + ack req + src rt */
  newskb = create_dsr_packet(4+16+8+src_rt_size, iph->saddr);

  if (newskb != NULL) {
    iph = newskb->nh.iph;
    dsrhdr = (struct dsr_hdr *) (newskb->nh.raw + (iph->ihl)*4);
    rterr = (struct dsr_rt_err_opt *) (dsrhdr + 1);
    //ackopt = (struct dsr_ack_opt *) (dsrhdr + 1);

    dsrhdr->nexthdr = NO_NEXT_HEADER;
    dsrhdr->length = htons(16+8+src_rt_size);

    rterr->type = ROUTE_ERROR;
    rterr->len = 16 - 2;
    rterr->errtype = 1;   /* node unreachable */
    rterr->errsaddr = IPADDR;
    rterr->errdaddr = iph->saddr;

    if (rxmtinfo->srcrt->segs_left == 0) {
      if (NOT_DSR_SUBNET(skb->nh.iph->daddr)) {
        rterr->typeinfo = rt->rt_gateway;
      } else {
        rterr->typeinfo = skb->nh.iph->daddr;
      }
    } else {
      rterr->typeinfo = rxmtinfo->srcrt->addr[index];
    }
    optptr = (unsigned char *) (rterr + 1);

    //printk("send err next hop: %u.%u.%u.%u\n", NIPQUAD(rterr->typeinfo));

    /* add an ack req to route error message */
    add_ack_req_opt(optptr);
    optptr += 8; /* ack req */

    /* need to reverse half of the orig src rt and use that as the src rt */
    srcrt = (struct dsr_src_rt_opt *) optptr;
    srcrt->type = SRC_ROUTE;
    srcrt->len = src_rt_size - 2;
    srcrt->segs_left = index - 1;
    for (j = 0;j < srcrt->segs_left;j++)
      srcrt->addr[j] = rxmtinfo->srcrt->addr[index-j-2];

    if (srcrt->segs_left == 0)
      fwdaddr = iph->daddr;
    else
      fwdaddr = srcrt->addr[0];

    rxmtinfo = (struct rxmt_info *) newskb->cb;
    rxmtinfo->srcrt = srcrt;
    //printk("rxmtinfo:%u\n", rxmtinfo);
    //printk("send rt err rxmtinfo->srcrt:%u\n", rxmtinfo->srcrt);

    if (reroute_output(newskb, iph->saddr, fwdaddr) == 0) {
      stat_send_rt_err++;
      ack_q_add(&rxmt_q, newskb, DSR_RXMT_BUFFER_SIZE);
      dsr_output(newskb);
    } else {
      kfree_skb(newskb);
    }
  }
}

void rebcast_rt_req(const struct sk_buff * skb, unsigned char * optptr) {

  struct iphdr * iph = skb->nh.iph;
  struct dsr_rt_req_opt * rtreq;// = (struct dsr_rt_req_opt *) optptr;
  const int optoff = optptr - skb->nh.raw;
  struct sk_buff * newskb;
  struct dsr_hdr * dsrhdr = (struct dsr_hdr *) (skb->nh.raw + (iph->ihl)*4);

  newskb = skb_copy_expand(skb, skb_headroom(skb) + 4,
                           skb_tailroom(skb), GFP_ATOMIC);
  
  /* can't allocate extra buffer space */
  if (newskb == NULL)
    return;
  
  newskb->nh.raw = skb_push(newskb, 4);

  memmove(newskb->nh.raw, newskb->nh.raw + 4,
          (iph->ihl)*4 + 4 + ntohs(dsrhdr->length));
  
  iph = newskb->nh.iph;
  dsrhdr = (struct dsr_hdr *) (newskb->nh.raw + (iph->ihl)*4);
  rtreq = (struct dsr_rt_req_opt *) (newskb->nh.raw + optoff);
  
  rtreq->addr[(rtreq->len - 6)/4] = IPADDR;
  rtreq->len += 4;
  
  dsrhdr->length = htons(ntohs(dsrhdr->length) + 4);
  
  iph->tot_len = htons(newskb->len);
  ip_send_check(iph);
  
  if (reroute_output(newskb, IPADDR, BCAST) == 0) {
    jitter_send(newskb);
  } else {
    kfree_skb(newskb);
  }
}

void send_ack_reply(const struct sk_buff * skb, unsigned char * optptr) {

  struct iphdr * iph;
  struct dsr_ack_req_opt * ackreq = (struct dsr_ack_req_opt *) optptr;
  struct dsr_hdr * dsrhdr;
  struct dsr_ack_opt * ackopt;
  struct sk_buff * newskb;

  /* create ack reply */
  DSR_ASSERT(skb->dev != NULL);
  newskb = create_dsr_packet(4+12, ackreq->saddr);

  if (newskb != NULL) {
    iph = newskb->nh.iph;
    dsrhdr = (struct dsr_hdr *) (newskb->nh.raw + (iph->ihl)*4);
    ackopt = (struct dsr_ack_opt *) (dsrhdr + 1);

    dsrhdr->nexthdr = NO_NEXT_HEADER;
    dsrhdr->length = htons(12);

    ackopt->type = ACK;
    ackopt->len = 10;
    ackopt->ident = ackreq->ident;
    ackopt->saddr = iph->saddr;
    ackopt->daddr = iph->daddr;

    if (reroute_output(newskb, iph->saddr, iph->daddr) == 0) {
      dsr_output(newskb);
    } else {
      kfree_skb(newskb);
    }
  }
}
