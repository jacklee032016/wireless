#include "dsr_debug.h"

void dump_skb(int pf, struct sk_buff *skb)
{

  const struct iphdr *ip = skb->nh.iph;
  __u32 *opt = (__u32 *) (ip + 1);
  int opti;
  __u16 src_port = 0, dst_port = 0;
  int icmptype = -1;

  if (ip->protocol == IPPROTO_ICMP)
    icmptype = skb->nh.raw[20];
    
  printk("dsrskb: %s len=%u shared=%u cloned=%u PROTO=%d:%i\n",
	 skb->sk ? "(owned)" : "(unowned)",
	 skb->len, skb_shared(skb), skb_cloned(skb), ip->protocol, icmptype);
  
  if (ip->protocol == IPPROTO_TCP
      || ip->protocol == IPPROTO_UDP) {
    struct tcphdr *tcp=(struct tcphdr *)((__u32 *)ip+ip->ihl);
    src_port = ntohs(tcp->source);
    dst_port = ntohs(tcp->dest);
  }
    
  printk("s:%u.%u.%u.%u:%hu d:%u.%u.%u.%u:%hu"
	 " L=%hu T=%hu S=0x%2.2hX I=%hu F=0x%4.4hX\n",
	 NIPQUAD(ip->saddr),
	 src_port, NIPQUAD(ip->daddr),
	 dst_port,
	 ntohs(ip->tot_len), ip->ttl, ip->tos, ntohs(ip->id),
	 ntohs(ip->frag_off));
  
  /* dump some stuff after the ip header */
  for (opti = 0; opti < 6; opti++)
    printk("O=%8.8X ", *opt++);
  printk("\n");
}

void dump_skb_ptr(struct sk_buff *skb) {

  const struct rtable * rt = (struct rtable *) skb->dst;

  if (rt != NULL) {
    printk("route dump: rt_src %u.%u.%u.%u rt_dst %u.%u.%u.%u"
	   " rt_gw %u.%u.%u.%u\n",
	   NIPQUAD(rt->rt_src), NIPQUAD(rt->rt_dst), NIPQUAD(rt->rt_gateway) );
  } else {
    printk("route dump: no route info\n");
  }
}
