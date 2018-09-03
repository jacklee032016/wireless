#include "dsr_route.h"
#include "dsr_debug.h"
#include "dsr_output.h"

extern __u32 IPADDR;
extern __u32 NETMASK;
extern __u32 NETWORK;
extern struct sk_buff_head send_q;
extern struct rt_entry rt_cache[];
extern struct rt_req_entry rt_req_table[];
extern int rt_cache_size;
extern int rt_req_table_size;
extern unsigned int stat_add_f_rt;
extern unsigned int stat_add_r_rt;
extern unsigned int stat_remove_rt;

/* FIXME: change in oif may mean change in hh_len.  Check and realloc --RR */
/* modified from ipv4/netfilter/iptable_mangle.c */
int reroute_output(struct sk_buff *skb, __u32 src, __u32 dst) {
  struct iphdr *iph = skb->nh.iph;
  struct rtable *rt;
  struct rt_key key = { dst:dst,
			src:src,
			oif:skb->sk ? skb->sk->bound_dev_if : 0,
			tos:RT_TOS(iph->tos)|RTO_CONN,
  };
  
  if (ip_route_output_key(&rt, &key) != 0) {
    printk("dsr reroute_output: No more route.\n");
    return -EINVAL;
  }
  
  /* Drop old route. */
  dst_release(skb->dst);

  skb->dst = &rt->u.dst;
  return 0;
}

void remove_route(struct rt_entry * rt_cache, __u32 srchop, __u32 dsthop) {

  int i, j;
  stat_remove_rt++;
#ifdef DSR_DEBUG
  printk("rmv rt, srchop:%u.%u.%u.%u dsthop:%u.%u.%u.%u\n", NIPQUAD(srchop),
	 NIPQUAD(dsthop));
#endif
  /*
   * brute force search for now, hash table later
   * remove every entry with srchop and dsthop
   */
  for (i = 0;i < ROUTE_CACHE_SIZE;i++) {
    if (rt_cache[i].dst != 0) {
      if (CURRENT_TIME - rt_cache[i].time < ROUTE_CACHE_TIMEOUT) {
	int finished;
	/* start - end - middle */
	if (rt_cache[i].segs_left == 0) {
	  if(IPADDR == srchop && rt_cache[i].dst == dsthop) {
	    rt_cache[i].dst = 0;
	    rt_cache_size--;
	  }
	} else if (IPADDR == srchop && rt_cache[i].addr[0] == dsthop) {
	  rt_cache[i].dst = 0;
	  rt_cache_size--;
	} else if (rt_cache[i].addr[rt_cache[i].segs_left - 1] == srchop
		   && rt_cache[i].dst == dsthop) {
	  rt_cache[i].dst = 0;
	  rt_cache_size--;
	} else {
	  finished = 0;
	  for (j = 0;j < rt_cache[i].segs_left && finished == 0;j++) {
	    if (rt_cache[i].addr[j] == srchop &&
		rt_cache[i].addr[j+1] == dsthop) {
	      rt_cache[i].dst = 0;
	      rt_cache_size--;
	      finished = 1;
	    }
	  }
	}
      } else {
	/* timed out entry, clean it */
	rt_cache[i].dst = 0;
	rt_cache_size--;
      }
    }
  }
}

void add_new_forward_route(__u32 daddr, __u32 * addr, int n_addr) {

  struct rt_entry * rt;
  int i;

  if (rt_cache_size < ROUTE_CACHE_SIZE) {
    int finished = 0;
    rt = rt_cache;
    /* create new entry */
    for (i = 0;i < ROUTE_CACHE_SIZE && finished == 0;i++) {
      if (rt_cache[i].dst == 0) {
	finished = 1;
	rt = rt_cache + i;
      }
    }
    DSR_ASSERT(finished == 1);
    rt_cache_size++;
  } else {
    int oldest = 0;
    /* replace oldest entry */
    for (i = 1;i < ROUTE_CACHE_SIZE;i++) {
      if (rt_cache[i].dst != 0) {
	if (rt_cache[i].time < rt_cache[oldest].time) {
	  oldest = i;
	}
      }
    }
    rt = rt_cache + oldest;
  }

  rt->dst = daddr;
  rt->time = CURRENT_TIME;
  rt->segs_left = n_addr;

  for (i = 0;i < n_addr;i++)
    rt->addr[i] = addr[i];
}

void add_new_reverse_route(__u32 daddr, __u32 * addr, int n_addr) {

  struct rt_entry * rt;
  int i;

  if (rt_cache_size < ROUTE_CACHE_SIZE) {
    int finished = 0;
    rt = rt_cache;
    /* create new entry */
    for (i = 0;i < ROUTE_CACHE_SIZE && finished == 0;i++) {
      if (rt_cache[i].dst == 0) {
	finished = 1;
	rt = rt_cache + i;
      }
    }
    DSR_ASSERT(finished == 1);
    rt_cache_size++;
  } else {
    int oldest = 0;
    /* replace oldest entry */
    for (i = 1;i < ROUTE_CACHE_SIZE;i++) {
      if (rt_cache[i].dst != 0) {
	if (rt_cache[i].time < rt_cache[oldest].time) {
	  oldest = i;
	}
      }
    }
    rt = rt_cache + oldest;
  }

  rt->dst = daddr;
  rt->time = CURRENT_TIME;
  rt->segs_left = n_addr;

  for (i = 0;i < n_addr;i++)
    rt->addr[i] = addr[n_addr - i - 1];
}

void remove_rt_req_id(__u32 daddr) {
  int i, count;

  /* clear entry in rt req table */
  for (i = 0, count = 0;count < rt_req_table_size;i++) {
    DSR_ASSERT(i < REQ_TABLE_SIZE);
    if (rt_req_table[i].taddr != 0) {
      count++;
      if (rt_req_table[i].taddr == daddr) {
	del_timer(&(rt_req_table[i].timer));
	rt_req_table[i].taddr = 0;
	rt_req_table_size--;
	return;
      }
    }
  }
  return;
}

void check_send_q() {

  int i;
  struct rt_entry * rt;
  int q_len = skb_queue_len(&send_q);
  struct sk_buff * skbptr;
  struct send_info * sendinfo;

  /* see if we can send anything in the send queue */
  for (i = 0;i < q_len;i++) {
    skbptr = skb_dequeue(&send_q);
    sendinfo = (struct send_info *) skbptr->cb;
    DSR_ASSERT(skbptr != NULL);
    rt = lookup_route(rt_cache, sendinfo->fwdaddr);
    if (rt != NULL) {
      del_timer(&(sendinfo->timer));
      /* dsr_send should free the skb */
      dsr_send(&skbptr, rt, sendinfo->output);
    } else {
      skb_queue_tail(&send_q, skbptr);
    }
  }
}

int have_route(__u32 daddr, __u32 * addr, int n_addr) {

  int i, j;

  /* look through all the routes to see if we already have the SAME route */
  for (i = 0;i < ROUTE_CACHE_SIZE;i++) {
    if (rt_cache[i].dst != 0) {
      if (CURRENT_TIME - rt_cache[i].time < ROUTE_CACHE_TIMEOUT) {
	if (rt_cache[i].dst == daddr && rt_cache[i].segs_left == n_addr) {
	  int diff = 0;
	  for (j = 0;j < n_addr && diff == 0;j++) {
	    /* route is different */
	    if (rt_cache[i].addr[j] != addr[j])
	      diff = 1;
	  }
	  /* if the route is the same then we return */
	  if (diff == 0) {
	    rt_cache[i].time = CURRENT_TIME;
	    return 1;
	  }
	}
      } else {
	/* timed out entry, clean it */
	rt_cache[i].dst = 0;
	rt_cache_size--;
      }
    }
  }
  return 0;
}

/* addr can be null */
void add_reverse_route(__u32 daddr, __u32 * addr, int n_addr) {

  int i;
  stat_add_r_rt++;

  for (i = 0;i < n_addr;i++) {
    if (have_route(addr[n_addr - i - 1], addr + n_addr - i, i) == 0) {
      add_new_reverse_route(addr[n_addr - i - 1], addr + n_addr - i, i);
      remove_rt_req_id(addr[n_addr - i - 1]);
    }
  }

  if (DSR_SUBNET(daddr) && have_route(daddr, addr, n_addr) == 0) {
    add_new_reverse_route(daddr, addr, n_addr);
    remove_rt_req_id(daddr);
  }
  check_send_q();
}

/* addr can be null */
void add_forward_route(__u32 daddr, __u32 * addr, int n_addr) {

  int i;
  stat_add_f_rt++;

  for (i = 0;i < n_addr;i++) {
    if (have_route(addr[i], addr + i - 1, i) == 0) {
      add_new_forward_route(addr[i], addr + i - 1, i);
      remove_rt_req_id(addr[i]);
    }
  }

  if (DSR_SUBNET(daddr) && have_route(daddr, addr, n_addr) == 0) {
    add_new_forward_route(daddr, addr, n_addr);
    remove_rt_req_id(daddr);
  }
  check_send_q();
}

struct rt_entry * lookup_route(struct rt_entry * rt_cache, __u32 dst) {

  int i;
  struct rt_entry * rt_ptr = NULL;

  /*
   * brute force search for now, hash table later
   * the most current entry with the smallest hop count will be used
   */
  for (i = 0;i < ROUTE_CACHE_SIZE;i++) {
    if (rt_cache[i].dst != 0) {
      if (CURRENT_TIME - rt_cache[i].time < ROUTE_CACHE_TIMEOUT) {
	if (rt_cache[i].dst == dst) {
	  /* found an entry */
	  if (rt_ptr == NULL)
	    rt_ptr = rt_cache + i;
	  else if (rt_cache[i].segs_left < rt_ptr->segs_left)
	    rt_ptr = rt_cache + i;
	  else if (rt_cache[i].segs_left == rt_ptr->segs_left)
	    if (time_after(rt_cache[i].time, rt_ptr->time))
	      rt_ptr = rt_cache + i;
	}
      } else {
	/* timed out entry, clean it */
	rt_cache[i].dst = 0;
	rt_cache_size--;
      }
    }
  }

  /* refresh the route we found */
  if (rt_ptr != NULL)
    rt_ptr->time = CURRENT_TIME;

  return rt_ptr;
}
