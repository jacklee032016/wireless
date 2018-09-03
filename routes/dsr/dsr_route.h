#ifndef DSR_ROUTE_H
#define DSR_ROUTE_H

#include <linux/netfilter_ipv4.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <net/checksum.h>
#include <net/icmp.h>

#include "dsr_header.h"

int reroute_output(struct sk_buff *skb, __u32 src, __u32 dst);
void add_reverse_route(__u32 daddr, __u32 * addr, int n_addr);
void add_forward_route(__u32 daddr, __u32 * addr, int n_addr);

/*
 * look up a route and clean out any expired routes
 * if a route is found it is set as new if no route is found NULL
 * is returned
 */
struct rt_entry * lookup_route(struct rt_entry * rt_cache, __u32 dst);
void remove_route(struct rt_entry * rt_cache, __u32 srchop, __u32 dsthop);

#endif /* DSR_ROUTE_H */
