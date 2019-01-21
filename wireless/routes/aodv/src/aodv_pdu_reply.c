/*
* $Id: aodv_pdu_reply.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include "aodv.h"

#define AODV_REPLY_TO_HB( reply) \
	reply->dst_seq = ntohl(reply->dst_seq); \
	reply->lifetime = ntohl(reply->lifetime)

#define AODV_REPLY_TO_NB( reply) \
	reply->dst_seq = htonl(reply->dst_seq); \
	reply->lifetime = htonl(reply->lifetime)

#ifdef LINK_LIMIT
extern int g_link_limit;
#endif

int aodv_pdu_hello_send(aodv_t  *aodv)
{
	aodv_pdu_reply *reply;
//	int i;
//	aodv_dest_t * tmp_dst;
//	struct interface_list_entry *tmp_interface;
	
	if ((reply = (aodv_pdu_reply *) kmalloc(sizeof(aodv_pdu_reply) , GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV: Can't allocate new aodv_pdu_request\n");
//		neigh_read_unlock();
		return 0;
	}

	reply->type = RREP_MESSAGE;
	reply->reserved1 = 0;
	reply->src_ip = aodv->myRoute->ip;
	reply->dst_ip = aodv->myRoute->ip;
	reply->dst_seq = htonl(aodv->myRoute->seq);
	reply->lifetime = htonl(MY_ROUTE_TIMEOUT);

	aodv_local_broadcast(aodv, 1, reply, sizeof(aodv_pdu_reply) );
	kfree (reply);
	
	aodv_timer_insert(aodv, AODV_TASK_HELLO, HELLO_INTERVAL, aodv->myIp);

	return 0;
}


int __recv_hello(aodv_t *aodv, aodv_pdu_reply * reply, aodv_task_t *packet)
{
//	aodv_route_t *recv_route;
	aodv_neigh_t *neigh;

	neigh = aodv_neigh_find(aodv, reply->dst_ip);
	if ( neigh == NULL)
	{
		neigh = aodv_neigh_insert(aodv, reply->dst_ip);
		if (!neigh)
		{
			printk(KERN_WARNING "AODV: Error creating neighbor: %s\n", inet_ntoa(reply->dst_ip));
			return -1;
		}
		memcpy(&(neigh->hw_addr), &(packet->src_hw_addr), sizeof(unsigned char) * ETH_ALEN);
		neigh->dev = packet->dev;
#ifdef AODV_SIGNAL
		aodv_iw_set_spy(aodv);
#endif
	} 

	aodv_timer_delete(aodv, neigh->ip, AODV_TASK_NEIGHBOR);
	aodv_timer_insert(aodv, AODV_TASK_NEIGHBOR, HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 100, neigh->ip);

	neigh->lifetime = reply->lifetime + getcurrtime(aodv->epoch) + 20;
	aodv_neigh_update(aodv, neigh, reply); 

	return 0;
}


static int __check_reply(aodv_t *aodv, aodv_pdu_reply * reply, aodv_task_t  *packet)
{
	char dst_ip[16];
	char src_ip[16];
	//    u_int32_t tmp;
	//    unsigned long  timer;

	if (reply->src_ip == reply->dst_ip)
	{
		//its a hello messages! HELLO WORLD!
		__recv_hello(aodv, reply, packet);
		return 0;
	}

	if (!aodv_neigh_valid(aodv, packet->src_ip))
	{
		printk(KERN_INFO "AODV: Not processing RREP from %s, not a valid neighbor\n", inet_ntoa(packet->src_ip));
		return 0;
	}

	aodv_timer_delete(aodv, reply->dst_ip, AODV_TASK_RESEND_RREQ);
	aodv_timer_update(aodv);
	strcpy(src_ip, inet_ntoa(packet->src_ip));
	strcpy(dst_ip, inet_ntoa(reply->dst_ip));
	printk(KERN_INFO "AODV: recv a route to: %s next hop: %s \n", dst_ip, src_ip);

	return 1;

}


int aodv_pdu_reply_rx(aodv_t *aodv, aodv_task_t * packet)
{
	aodv_route_t 		*route;
	aodv_pdu_reply 	*reply;
	//    aodv_route_t *recv_route;
	char dst_ip[16];
	char src_ip[16];

	reply = packet->data;
	AODV_REPLY_TO_HB(reply);

	if (!__check_reply(aodv, reply, packet) )
	{
		return 0;
	}

	reply->metric++;

	aodv_route_update(aodv, reply->dst_ip, packet->src_ip, reply->metric, reply->dst_seq, packet->dev);
	route = aodv_route_find(aodv, reply->src_ip);

	if (!route)
	{
		strcpy(src_ip, inet_ntoa(reply->src_ip));
		strcpy(dst_ip, inet_ntoa(reply->dst_ip));
		printk(KERN_WARNING "AODV: No reverse-route for RREP from: %s to: %s", dst_ip, src_ip);
		return 0;
	}

	if (!route->self_route)
	{
		strcpy(src_ip, inet_ntoa(reply->src_ip));
		strcpy(dst_ip, inet_ntoa(reply->dst_ip));
		printk(KERN_INFO "AODV: Forwarding a route to: %s from node: %s \n", dst_ip, src_ip);
		AODV_REPLY_TO_NB(reply);
		send_message(aodv, route->next_hop, NET_DIAMETER, reply, sizeof(aodv_pdu_reply), NULL);
	}

	/* If I'm not the destination of the RREP I forward it */
	return 0;
}

static int __pdu_reply_send(aodv_t *aodv, aodv_route_t * src, aodv_route_t * dest, aodv_pdu_request * request)
{
	aodv_pdu_reply *reply;

	if (( reply = (aodv_pdu_reply *) kmalloc(sizeof(aodv_pdu_reply), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV: Can't allocate new aodv_pdu_request\n");
		return 0;
	}

	reply->type = RREP_MESSAGE;
	reply->reserved1 = 0;
	reply->reserved2 = 0;
	reply->src_ip = src->ip;
	reply->dst_ip = dest->ip;
	reply->dst_seq = htonl(dest->seq);
	reply->metric = dest->metric;
	if (dest->self_route)
	{
		reply->lifetime = htonl(MY_ROUTE_TIMEOUT);
	}
	else
	{
		reply->lifetime = htonl(dest->lifetime - getcurrtime(aodv->epoch));
	}

	send_message(aodv, src->next_hop, NET_DIAMETER, reply, sizeof(aodv_pdu_reply) , NULL);
	kfree(reply);
	return 0;
}


int aodv_pdu_reply_create(aodv_t *aodv, u_int32_t src_ip, u_int32_t dst_ip, aodv_pdu_request *request)
{
	aodv_route_t *src;
	aodv_route_t *dest;

	/* Get the source and destination IP address from the RREQ */
	if (!(src = aodv_route_find(aodv, src_ip)))
	{
		printk(KERN_WARNING "AODV: RREP  No route to Source! src: %s\n", inet_ntoa(src_ip));
		return -EHOSTUNREACH;
	}

	if (!(dest = aodv_route_find(aodv, dst_ip)))
	{
		printk(KERN_WARNING "AODV: RREP  No route to Dest! dst: %s\n", inet_ntoa(dst_ip));
		return -EHOSTUNREACH;
	}

	return __pdu_reply_send(aodv, src, dest, request);
//	return 0;
}

void aodv_pdu_reply_ack_create(aodv_t *aodv,  aodv_pdu_reply *reply, aodv_task_t *packet)
{
	aodv_pdu_reply_ack *ack;

	if (( ack = (aodv_pdu_reply_ack*) kmalloc(sizeof (aodv_pdu_reply_ack),GFP_ATOMIC)) == NULL)
	{
		printk("RREP_ACK: error with tmp_ack\n");
		return;
	}

	ack->type=4;
	send_message(aodv, reply->src_ip, 1, ack, sizeof(aodv_pdu_reply_ack), packet->dev);
	kfree( ack);
}

int  aodv_pdu_reply_ack_rx(aodv_t *aodv, aodv_task_t *working_packet)
{
	return 1;
}

#if 0
int aodv_pdu_error_send(rerr_route * bad_routes, int num_routes)
{
	aodv_pdu_error 	*errpdu;
	int 				rerr_size;
	rerr_route 		*tmp_rerr_route = bad_routes;
	rerr_route 		*dead_rerr_route = NULL;
	aodv_dest_t 		*tmp_rerr_dst = NULL;
	void 			*data;

//    int datalen, i;

	rerr_size = sizeof(aodv_pdu_error) + (sizeof(aodv_dest_t) * num_routes);
	if ((data = kmalloc(rerr_size, GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV: Error creating packet to send RERR\n");
		return -ENOMEM;
	}

	errpdu = (aodv_pdu_error *) data;
	errpdu->type = 3;
	errpdu->dst_count = num_routes;
	errpdu->reserved = 0;
	errpdu->n = 0;

	//add in the routes that have broken...
	tmp_rerr_dst = (aodv_dest_t *) ((void *)data + sizeof(aodv_pdu_error));
	while (tmp_rerr_route)
	{
		tmp_rerr_dst->ip = tmp_rerr_route->ip;
		tmp_rerr_dst->seq = tmp_rerr_route->seq;
		dead_rerr_route = tmp_rerr_route;     
		tmp_rerr_route = tmp_rerr_route->next;
		kfree(dead_rerr_route);
		tmp_rerr_dst = (void *)tmp_rerr_dst + sizeof(aodv_dest_t);
	}

	local_broadcast(NET_DIAMETER, data, rerr_size);
	kfree(data);
	return 0;
}


int aodv_pdu_error_create(u_int32_t brk_dst_ip)
{
	aodv_route_t *tmp_route;
	rerr_route 	*bad_routes = NULL;
	rerr_route 	*tmp_rerr_route = NULL;
	int num_routes = 0;

	//    int rerrhdr_created = 0;
	tmp_route = aodv_route_first();

	//go through list
	while (tmp_route != NULL)
	{
		if ((tmp_route->next_hop == brk_dst_ip)  && !tmp_route->self_route) //&& valid_aodv_route(tmp_route)
		{
			if ((tmp_rerr_route = (rerr_route *) kmalloc(sizeof(rerr_route), GFP_ATOMIC)) == NULL)
			{
				printk(KERN_WARNING "AODV: RERR Can't allocate new entry\n");
				return 0;
			}
			tmp_rerr_route->next = bad_routes;
			bad_routes = tmp_rerr_route;
			tmp_rerr_route->ip = tmp_route->ip;
			tmp_rerr_route->seq = htonl(tmp_route->seq);
			num_routes++;
			if (tmp_route->route_valid)
				aodv_route_expire(tmp_route);
			printk(KERN_INFO "RERR: Broken link as next hop for - %s \n", inet_ntoa(tmp_route->ip));
		}
		//move on to the next entry
		tmp_route = tmp_route->next;
	}
	
	if (num_routes)
	{
		__send_error_pdu(bad_routes, num_routes);
	}
	
	return 0;
}
#endif

int aodv_pdu_error_rx(aodv_t *aodv, aodv_task_t *packet)
{
	aodv_route_t *route;
	aodv_pdu_error *errpdu;
	aodv_dest_t *errDest;
	rerr_route *rerrRoute = NULL;
	rerr_route *lastEerrRoute = NULL;
	int numOutgoingRoutes = 0;
	int i;

	errpdu = (aodv_pdu_error *)packet->data;
	errDest = (aodv_dest_t *) ((void*)packet->data + sizeof(aodv_pdu_error));
	printk(KERN_INFO "AODV: recieved a route error from %s, count= %u\n", inet_ntoa(packet->src_ip), errpdu->dst_count);

	for (i = 0; i < errpdu->dst_count; i++)
	{
		//go through all the unr dst in the rerr
		printk(KERN_INFO "       -> %s", inet_ntoa(errDest->ip));
		route = aodv_route_find(aodv, errDest->ip);
		if ( route && !route->self_route)
		{
			// if the route is valid and not a self route
			if ( route->next_hop == packet->src_ip)
			{
				//if we are using the person who sent the rerr as a next hop
				printk(KERN_INFO " Removing route\n");
				route->seq = ntohl( errDest->seq); //convert to host format for local storage
				//create a temporary rerr route to use for the outgoing RERR
				if (( rerrRoute = (rerr_route *) kmalloc(sizeof(rerr_route), GFP_ATOMIC)) == NULL)
				{
					printk(KERN_WARNING "AODV: RERR Can't allocate new entry\n");
					return 0;
				}

				lastEerrRoute->next = rerrRoute;
				rerrRoute = lastEerrRoute;
				lastEerrRoute->ip = errDest->ip;
				lastEerrRoute->seq = errDest->seq; //Already in network format...
				numOutgoingRoutes++;
				if ( route->route_valid)
					aodv_route_expire(aodv, route);
			}
		}

		errDest = (aodv_dest_t *) ((void*) errDest + sizeof(aodv_dest_t));
		printk(KERN_INFO "\n");
	}

	if (numOutgoingRoutes)
	{
//		aodv_pdu_error_send(outgoing_rerr_routes, num_outgoing_routes);
	}
	return 0;
}

