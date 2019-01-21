/*
* $Id: aodv_pdu_request.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include "aodv.h"
#include "packet_queue.h"

static void __convert_rreq_to_host(aodv_pdu_request * request)
{
	request->rreq_id = ntohl(request->rreq_id);
	request->dst_seq = ntohl(request->dst_seq);
	request->src_seq = ntohl(request->src_seq);
}

static void __convert_rreq_to_network(aodv_pdu_request * request)
{
	request->rreq_id = htonl(request->rreq_id);
	request->dst_seq = htonl(request->dst_seq);
	request->src_seq = htonl(request->src_seq);
}


int __check_request(aodv_t *aodv, aodv_pdu_request * request, u_int32_t last_hop_ip)
{
//	aodv_neigh_t *tmp_neigh;
	aodv_route_t *src_route;

	if (aodv_flood_find(aodv, request->src_ip, request->rreq_id))
	{
		return 0;
	}

	src_route = aodv_route_find(aodv, request->src_ip);
	if (!aodv_neigh_valid(aodv, last_hop_ip))
	{
		return 0;
	}

	if ((src_route != NULL) && (src_route->next_hop != last_hop_ip) && (src_route->next_hop == src_route->ip))
	{
		printk("Last Hop of: %s ",inet_ntoa(last_hop_ip) );
		printk("Doesn't match route: %s ",inet_ntoa(request->src_ip) );
		printk("dest: %s RREQ_ID: %u, expires: %ld\n",inet_ntoa(request->dst_ip), request->rreq_id, src_route->lifetime - getcurrtime(aodv->epoch));
	}

	return 1;
}


int aodv_pdu_request_reply(aodv_t *aodv, aodv_pdu_request * request)
{
	char src_ip[16];
	char dst_ip[16];
//	char pkt_ip[16];
	aodv_route_t *dst_route;

	dst_route = aodv_route_find(aodv, request->dst_ip);
	if ( !aodv_dev_local_check(aodv, request->dst_ip))
	{
		if (aodv->supportGateway)
		{
#if AODV_DEBUG		
			printk("Gatewaying for address: %s, ", inet_ntoa( request->dst_ip ));
#endif
			if (dst_route == NULL)
			{
#if AODV_DEBUG		
				printk("creating route for: %s \n",inet_ntoa( request->dst_ip ));
#endif
				dst_route = aodv_route_create(aodv, request->dst_ip, AODV_ROUTE_AODV);
				dst_route->seq = aodv->myRoute->seq;
				dst_route->next_hop = request->dst_ip;
				dst_route->metric = 1;
				dst_route->dev =   aodv->myRoute->dev;
			}
			else
			{
				printk("using route: %s \n",inet_ntoa( dst_route->ip ));
			}

			dst_route->lifetime =  getcurrtime(aodv->epoch) +  ACTIVE_ROUTE_TIMEOUT;
			dst_route->route_valid = TRUE;
			dst_route->route_seq_valid = TRUE;
			return 1;
		}

		//it is a local subnet and we need to create a route for it before we can reply...
		if ((dst_route!=NULL) && (dst_route->netmask!= aodv->broadcastIp) )//g_broadcast_ip))
		{
#if AODV_DEBUG		
			printk("creating route for local address: %s \n",inet_ntoa( request->dst_ip ));
#endif
			dst_route = aodv_route_create(aodv, request->dst_ip, AODV_ROUTE_AODV);
			dst_route->seq = aodv->myRoute->seq;
			dst_route->next_hop = request->dst_ip;
			dst_route->metric = 1;
			dst_route->dev =   aodv->myRoute->dev;
			dst_route->lifetime =  getcurrtime(aodv->epoch) +  ACTIVE_ROUTE_TIMEOUT;
			dst_route->route_valid = TRUE;
			dst_route->route_seq_valid = TRUE;

			return 1;
		}
	}
	else
	{
		// if it is not outside of the AODV subnet and I am the dst...
		if (dst_route && dst_route->self_route)
		{
			if (seq_less_or_equal(dst_route->seq, request->dst_seq))
			{
				dst_route->seq = request->dst_seq + 1;
			}
			strcpy(src_ip, inet_ntoa(request->src_ip));
			strcpy(dst_ip, inet_ntoa(request->dst_ip));
#if AODV_DEBUG		
			printk(KERN_INFO "AODV: Destination, Generating RREP -  src: %s dst: %s \n", src_ip, dst_ip);
#endif
			return 1;
		}
	}

	/* Test to see if we should send a RREP AKA we have or are the desired route */
	return 0;
}

int aodv_pdu_request_resend(aodv_t *aodv, aodv_pdu_request * request, int ttl)
{
	__convert_rreq_to_network(request);
	/* Call send_datagram to send and forward the RREQ */
	aodv_local_broadcast(aodv, ttl - 1, request, sizeof(aodv_pdu_request));
	return 0;
}

int aodv_pdu_request_rx(aodv_t *aodv, aodv_task_t *packet)
{
	aodv_pdu_request 	*request;
//	aodv_route_t *src_route;      /* Routing table entry */
	unsigned long  current_time;     /* Current time */
//	int size_out;

	request = packet->data;
	__convert_rreq_to_host(request);
	current_time = getcurrtime(aodv->epoch);       /* Get the current time */

	/* Look in the route request list to see if the node has already received this request. */
	if ( packet->ttl <= 1)
	{
		printk(KERN_INFO "AODV TTL for RREQ from: %s expired\n", inet_ntoa(request->src_ip));
		return -ETIMEDOUT;
	}

	if (!__check_request(aodv, request, packet->src_ip))
	{
		return 1;
	}

	request->metric++;
	/* Have not received this RREQ within BCAST_ID_SAVE time */
	/* Add this RREQ to the list for further checks */

	/* UPDATE REVERSE */
	aodv_route_update(aodv, request->src_ip,  packet->src_ip, request->metric, request->src_seq,  packet->dev);
	aodv_flood_insert(aodv, request->src_ip, request->dst_ip, request->rreq_id, current_time + PATH_DISCOVERY_TIME);

	switch (aodv_pdu_request_reply(aodv, request))
	{
		case 1:
			aodv_pdu_reply_create(aodv, request->src_ip, request->dst_ip, request);
			return 0;
			break;
	}

//	aodv_pdu_reply_forward(request, tmp_packet->ttl);
	return 0;
}

int aodv_pdu_request_resend_ttl(aodv_t *aodv, aodv_task_t  *packet)
{
	aodv_route_t 			*route;
	aodv_pdu_request 		*request;
	u_int8_t 				out_ttl;

	if (packet->retries <= 0)
	{
		ipq_drop_ip(packet->dst_ip);
		return 0;
	}

	if ((request = (aodv_pdu_request *) kmalloc(sizeof(aodv_pdu_request), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV : Can't allocate new aodv_pdu_request\n");
		return 0;
	}

	/* Get routing table entry for destination */
	route = aodv_route_find(aodv, packet->dst_ip);
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
	route = aodv_route_find(aodv, packet->src_ip);
	if ( route == NULL)
	{
		printk(KERN_WARNING "AODV: Can't get route to source: %s\n", inet_ntoa(packet->src_ip));
		kfree(request);
		return -EHOSTUNREACH;
	}

	/* Get our own sequence number */
	route->rreq_id = route->rreq_id + 1;
	route->seq = route->seq + 1;
	request->src_seq = htonl(route->seq);
	request->rreq_id = htonl(route->rreq_id);

	/* Fill in the package */
	request->dst_ip = packet->dst_ip;
	request->src_ip = packet->src_ip;
	request->type = RREQ_MESSAGE;
	request->metric = htonl(0);
	request->j = 0;
	request->r = 0;
	request->d = 0;
	request->reserved = 0;
	request->second_reserved = 0;
	request->g = 1;

	/* Get the broadcast address and ttl right */
	if (aodv_flood_insert(aodv, packet->src_ip, packet->dst_ip, route->rreq_id, getcurrtime(aodv->epoch) + PATH_DISCOVERY_TIME) < 0)
	{
		kfree(request);
		printk(KERN_WARNING "AODV: Can't add to broadcast list\n");
		return -ENOMEM;
	}
	//    insert_timer_queue_entry(getcurrtime(aodv->epoch) + NET_TRAVERSAL_TIME,timer_rreq, sizeof(struct rreq),out_rreq->dst_ip,0,out_ttl, EVENT_RREQ);
	//insert_timer_queue_entry(getcurrtime(aodv->epoch) + NET_TRAVERSAL_TIME,timer_rreq, sizeof(struct rreq),out_rreq->dst_ip,RREQ_RETRIES,out_ttl, EVENT_RREQ);
	aodv_timer_insert_request(aodv, request, packet->retries - 1);
	aodv_timer_update(aodv);

	//local_broadcast(out_ttl,out_rreq,sizeof(struct rreq));
	aodv_local_broadcast(aodv, out_ttl, request, sizeof(aodv_pdu_request));
	kfree(request);

	return 0;
}

