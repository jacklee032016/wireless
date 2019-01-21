/*
* $Id: ip_netfilter.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/
 
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/netfilter_ipv4.h>

#include "mesh_route.h"
#include "_ip_aodv.h"

typedef struct
{
	struct nf_hook_ops 	input;
	struct nf_hook_ops 	output;
}aodv_nf_t;

static aodv_nf_t 		filter;

extern route_node_t 		myAODV;


/* RX a AODV UDP packet and insert into task queue */
unsigned int __aodv_nf_input(unsigned int hooknum, struct sk_buff **skb, const struct net_device *in, const struct net_device *out, int (*okfn) (struct sk_buff *))
{
 	route_dev_t	*routeDev;
	route_op_type_t result ;

#if 0
	result = node->input_packet(node, skb);
	switch(result)
	{
		case ROUTE_OP_FORWARD:
			return NF_ACCEPT;
		case ROUTE_OP_QUEUE:
			return NF_QUEUE;
		case ROUTE_OP_DENY:
		default:
			return NF_DROP;
	}
#else
	struct iphdr *ip = (*skb)->nh.iph;
	struct iphdr *dev_ip = in->ip_ptr;
	void *p = (uint32_t *) ip + ip->ihl;
	struct udphdr *udp = p; //(struct udphdr *) ip + ip->ihl;
	struct ethhdr *mac = (*skb)->mac.ethernet;
	
	if ((*skb)->h.uh != NULL)
	{
		if ((udp->dest == htons(AODVPORT)) && (mac->h_proto == htons(ETH_P_IP)))
		{			
			if (dev_ip->saddr != ip->saddr)
			{/* is not the packet send by myself */

				routeDev = route_dev_lookup( (*skb)->dev );
				if(!routeDev)
				{/* AODV packet must for a route device */
					printk("Bug, No Route Device for Net Device(int) %s\n",in->name);
					return NF_ACCEPT;
				}
	
				result = routeDev->ops->input_packet(routeDev, *skb, NULL);
				switch(result)
				{
					case ROUTE_OP_FORWARD:
						return NF_ACCEPT;
					case ROUTE_OP_QUEUE:
						return NF_QUEUE;
					case ROUTE_OP_DENY:
					default:
						return NF_DROP;
				}
			}
			else
			{
				printk( "dropping packet from: %s\n",inet_ntoa(ip->saddr));
				return NF_DROP;
			}
		}
	}
#endif
	return NF_ACCEPT;
}

unsigned int __aodv_nf_output(unsigned int hooknum, struct sk_buff **skb, const struct net_device *in, const struct net_device *out, int (*okfn) (struct sk_buff *))
{
//	struct iphdr *iph= (*skb)->nh.iph;
	route_dev_t	*routeDev;
	route_op_type_t result;
//	struct net_device *dev= (*skb)->dev;
//	route_neigh_t *tmp_neigh;
//	void *p = (uint32_t *) ip + ip->ihl;
//	struct udphdr *udp = p; //(struct udphdr *) ip + ip->ihl;
//	struct ethhdr *mac = (*skb)->mac.ethernet;  //Thanks to Randy Pitz for adding this extra check...
#if 0
	return __aodv_packet_tx(node, iph);
#else
	routeDev = route_dev_lookup( (*skb)->dev );
	if(!routeDev)
	{/* netfilter is for protocol stack, not for net_device ,lizhijie */
//		printk( "Bug, No Route Device for Net Device(out) %s\n", out->name);
		return NF_ACCEPT;
	}
	
	result = routeDev->ops->output_packet(routeDev, (*skb) );
	switch(result)
	{
		case ROUTE_OP_FORWARD:
			return NF_ACCEPT;
		case ROUTE_OP_QUEUE:
			return NF_QUEUE;
		case ROUTE_OP_DENY:
		default:
			return NF_DROP;
	}
#endif

	return NF_ACCEPT;
}

int aodv_netfilter_init()
{
	int result;
	aodv_nf_t *nf = &(filter);
	
	nf->input.list.next = NULL;
	nf->input.list.prev = NULL;
	nf->input.hook = __aodv_nf_input;
	nf->input.pf = PF_INET; // IPv4
	nf->input.hooknum = NF_IP_PRE_ROUTING;

	nf->output.list.next = NULL;
	nf->output.list.prev = NULL;
	nf->output.hook = __aodv_nf_output;
	nf->output.pf = PF_INET; // IPv4
	nf->output.hooknum = NF_IP_POST_ROUTING;

	result = nf_register_hook(&(nf->output));
	result = nf_register_hook(&(nf->input) );
	return 0;
}

void aodv_netfilter_destroy()
{
	aodv_nf_t *nf = &(filter);

	nf_unregister_hook(&(nf->input) );
	nf_unregister_hook(&(nf->output) );
}

