/*
* $Id: aodv_netfilter.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/netfilter_ipv4.h>

#include "aodv.h"

#if AODV_DEBUG
char *aodv_packet_type[] =
{
		"RREQ",
		"RREP",
		"RERR",
		"RREP_ACK",
};
#endif

typedef struct
{
	struct nf_hook_ops 	input;
	struct nf_hook_ops 	output;
}aodv_nf_t;

static aodv_nf_t 		filter;

extern aodv_t 		myAODV;

/******************************************************
*  Following for AODV protocol Packet In 
******************************************************/
int __valid_aodv_packet(int numbytes, int type, char *data)
{
	aodv_pdu_error 	*rerr;
	aodv_pdu_request 	*request;
	aodv_pdu_reply 	*reply;

	switch (type)
	{
		case AODV_PACKET_RREQ:
			request = (aodv_pdu_request *) data;
			//If it is a normal route rreq
			if (numbytes == sizeof(aodv_pdu_request))
				return 1;
			break;

		case AODV_PACKET_RREP:
			reply = (aodv_pdu_reply *) data;
			if (numbytes == sizeof(aodv_pdu_reply))
				return 1;
			break;

		case AODV_PACKET_RERR:
			rerr = (aodv_pdu_error *) data;
			if (numbytes == (sizeof(aodv_pdu_error) + (sizeof(aodv_dest_t) *rerr->dst_count)))
			{
				return 1;
			}
			break;

		case AODV_PACKET_RREP_ACK:                    //Normal RREP-ACK
			if (numbytes == sizeof(aodv_pdu_reply_ack))
				return 1;
			break;
		default:
			break;
	}

	return 0;
}


int __aodv_packet_in(aodv_t *aodv, struct sk_buff *packet)
{
	struct net_device *dev;
	struct iphdr *ip;
//	aodv_route_t *tmp_route;
//	aodv_neigh_t *tmp_neigh;
//	u_int32_t tmp_ip;
	
	// Create aodv message types
	u_int8_t aodv_type;

	//The packets which come in still have their headers from the IP and UDP
	int start_point = sizeof(struct udphdr) + sizeof(struct iphdr);

	//get pointers to the important parts of the message
	ip = packet->nh.iph;
	dev = packet->dev;
	/*   if (strcmp(dev->name, "lo") == 0)
	{
		return NF_DROP;
	}*/
	//For all AODV packets the type is the first byte.
	aodv_type = (int) packet->data[start_point];
	if (!__valid_aodv_packet(packet->len - start_point, aodv_type, packet->data + start_point))
	{
		printk(KERN_NOTICE "AODV: Packet of type: %d and of size %u from: %s failed packet check!\n", aodv_type, packet->len - start_point, inet_ntoa(ip->saddr));
		return NF_DROP;
	}

	/*	tmp_neigh = find_aodv_neigh_by_hw(&(packet->mac.ethernet->h_source));
	if (tmp_neigh != NULL)
	{
		delete_timer(tmp_neigh->ip, AODV_TASK_NEIGHBOR);
		insert_timer(AODV_TASK_NEIGHBOR, HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 100, tmp_neigh->ip);
		update_timer_queue();
	}
	*/

	//place packet in the event queue!
#if AODV_DEBUG	
	printk("AODV : rx AODV %s Packet\n", aodv_packet_type[aodv_type-1]);
#endif

	aodv_task_insert(aodv, aodv_type, packet);
	return NF_ACCEPT;
}


/* RX a AODV UDP packet and insert into task queue */
unsigned int __aodv_nf_input(unsigned int hooknum, struct sk_buff **skb, const struct net_device *in, const struct net_device *out, int (*okfn) (struct sk_buff *))
{
	struct iphdr *ip = (*skb)->nh.iph;
	struct iphdr *dev_ip = in->ip_ptr;
	void *p = (uint32_t *) ip + ip->ihl;
	struct udphdr *udp = p; //(struct udphdr *) ip + ip->ihl;
	struct ethhdr *mac = (*skb)->mac.ethernet;  //Thanks to Randy Pitz for adding this extra check...
	aodv_t *aodv = &myAODV;
	
//	trace;
	if ((*skb)->h.uh != NULL)
	{
		if ((udp->dest == htons(AODVPORT)) && (mac->h_proto == htons(ETH_P_IP)))
		{
			if (dev_ip->saddr != ip->saddr)
			{/* is not the packet send by myself */
				return __aodv_packet_in(aodv, *(skb));
			}
			else
			{
				printk("dropping packet from: %s\n",inet_ntoa(ip->saddr));
				return NF_DROP;
			}
		}
	}

	return NF_ACCEPT;
}

/******************************************************
*  Following for Packet out 
******************************************************/
/*  create and send a RREQ, add it into FLOOD and TIMER queue */
static int __aodv_pdu_request_create(aodv_t *aodv, unsigned long destIp)
{
	aodv_route_t 			*route;
	aodv_pdu_request 		*request;
	u_int8_t 				out_ttl;

	if (aodv_timer_find(aodv, destIp, AODV_TASK_RESEND_RREQ) != NULL)
	{/* RREQ has been send for this dest */
		return 0;
	}

#if 0
	/* Allocate memory for the rreq message */
	if ((request = (aodv_pdu_request *) kmalloc(sizeof(aodv_pdu_request), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV: Can't allocate new aodv_pdu_request\n");
		return 0;
	}
#endif

#if AODV_DEBUG
	printk(KERN_INFO "Generating a RREQ for: %s\n", inet_ntoa(destIp));
#endif
	if ((request = (aodv_pdu_request *) kmalloc(sizeof(aodv_pdu_request), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV: Can't allocate new aodv_pdu_request\n");
		return -ENOMEM;
	}

	/* Get routing table entry for destination */
	route = aodv_route_find(aodv, destIp);
	if ( route == NULL)
	{
		/* Entry does not exist -> set to initial values */
		request->dst_seq = htonl(0);
		request->u = TRUE;
		//out_ttl = TTL_START;
	}
	else
	{
		/* Entry does exist -> get value from rt */
		request->dst_seq = htonl( route->seq);
		request->u = FALSE;
		//out_ttl = tmp_route->metric + TTL_INCREMENT;
	}

	out_ttl = NET_DIAMETER;

	/* Get routing table entry for source, when this is ourself this one should always exist */
	route = aodv_route_find(aodv, aodv->myIp);
	if( route == NULL)
	{
		printk(KERN_WARNING "AODV: Can't get route to source: %s\n", inet_ntoa(aodv->myIp));
		kfree(request);
		return -EHOSTUNREACH;
	}

	/* Get our own sequence number */
	route->rreq_id = route->rreq_id + 1;
	route->seq = route->seq + 1;
	request->src_seq = htonl( route->seq);
	request->rreq_id = htonl( route->rreq_id);

	/* Fill in the package */
	request->dst_ip = destIp;
	request->src_ip = aodv->myIp;
	request->type = RREQ_MESSAGE;
	request->metric = htonl(0);
	request->j = 0;
	request->r = 0;
	request->d = 1;
	request->reserved = 0;
	request->second_reserved = 0;
	request->g = 1;

	if (aodv_flood_insert(aodv, aodv->myIp, destIp, route->rreq_id, getcurrtime(aodv->epoch) + PATH_DISCOVERY_TIME) < 0)
	{
		kfree(request);
		printk(KERN_WARNING "AODV : Can't add to broadcast list\n");
		return -ENOMEM;
	}

	aodv_timer_insert_request(aodv, request, RREQ_RETRIES);
	aodv_timer_update(aodv);
	aodv_local_broadcast(aodv, out_ttl, request, sizeof(aodv_pdu_request));
	kfree(request);

	return 0;
}

static int __aodv_packet_tx(aodv_t *aodv, struct iphdr *packetIpHeader)
{
	aodv_route_t *route;

	if (packetIpHeader->daddr == aodv->broadcastIp )
	{
		return NF_ACCEPT;
	}

	/*Need this additional check, otherwise users on the
	* gateway will not be able to access the externel 
	* network. Remote user located outside the aodv_subnet
	* also require this check if they are access services on the gateway node. 
	*/
	if(aodv->supportGateway && !aodv_dev_local_check(aodv, packetIpHeader->daddr))
	{
		return NF_ACCEPT;
	}
	
	//Try to get a route to the destination
	route = aodv_route_find(aodv, packetIpHeader->daddr);
	if ((route == NULL) || !(route->route_valid))
	{
		printk("AODV : Not Found route to %s, so begin AODV RREQ\n", inet_ntoa(packetIpHeader->daddr));
		__aodv_pdu_request_create(aodv,  packetIpHeader->daddr);
		return NF_QUEUE;
	}
#if AODV_DEBUG	
	printk("AODV : Found route to %s\n", inet_ntoa(packetIpHeader->daddr));
#endif

	if (( route != NULL) && (route->route_valid))
	{
		if (!route->self_route)
			route->lifetime =  getcurrtime(aodv->epoch) + ACTIVE_ROUTE_TIMEOUT;
	}

	return NF_ACCEPT;
}

unsigned int __aodv_nf_output(unsigned int hooknum, struct sk_buff **skb, const struct net_device *in, const struct net_device *out, int (*okfn) (struct sk_buff *))
{
	struct iphdr *iph= (*skb)->nh.iph;
	aodv_t *aodv = &myAODV;
//	struct net_device *dev= (*skb)->dev;
//	aodv_neigh_t *tmp_neigh;
//	void *p = (uint32_t *) ip + ip->ihl;
//	struct udphdr *udp = p; //(struct udphdr *) ip + ip->ihl;
//	struct ethhdr *mac = (*skb)->mac.ethernet;  //Thanks to Randy Pitz for adding this extra check...
	return __aodv_packet_tx(aodv, iph);

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

