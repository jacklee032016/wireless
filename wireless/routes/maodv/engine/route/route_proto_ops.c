/*
* $Id: route_proto_ops.c,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
*/
 
#include "mesh_route.h"

#define ROUTE_REPLY_TO_HB( reply) \
	reply->dst_seq = ntohl(reply->dst_seq); \
	reply->lifetime = ntohl(reply->lifetime)

#define ROUTE_REPLY_TO_NB( reply) \
	reply->dst_seq = htonl(reply->dst_seq); \
	reply->lifetime = htonl(reply->lifetime)

#ifdef LINK_LIMIT
extern int g_link_limit;
#endif


int _engine_rx_hello(route_engine_t *engine, route_task_t *packet, aodv_pdu_reply * reply)
{
//	mesh_route_t *recv_route;
	route_neigh_t *neigh;

	neigh = engine->mgmtOps->neigh_lookup_ip(engine, reply->dst_ip);
	if ( neigh == NULL)
	{
		neigh = engine->mgmtOps->neigh_insert(engine, reply->dst_ip);
		if (!neigh)
		{
			ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREP, "Error, creating neighbor: %s\n", inet_ntoa(reply->dst_ip));
			return -1;
		}
		memcpy(&(neigh->myHwAddr), &(packet->srcHwAddr), sizeof(unsigned char) * ETH_ALEN);
		neigh->dev = packet->dev;
#ifdef ROUTE_SIGNAL
		aodv_iw_set_spy(engine);
#endif
	} 

	engine->mgmtOps->timer_delete(engine, neigh->ip, ROUTE_TASK_NEIGHBOR);
	engine->mgmtOps->timer_insert(engine, ROUTE_TASK_NEIGHBOR, neigh->ip, HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 100);

	neigh->lifetime = reply->lifetime + getcurrtime(engine->epoch) + 20;
	engine->mgmtOps->neigh_update_route(engine, neigh, reply); 

	return 0;
}


static int __check_reply(route_engine_t *engine, aodv_pdu_reply * reply, route_task_t  *packet)
{
	if (reply->src_ip == reply->dst_ip)
	{
		//its a hello messages! HELLO WORLD!
		engine->protoOps->rx_hello(engine, packet, reply);
		return 0;
	}

	if (!engine->mgmtOps->neigh_validate(engine, packet->src_ip))
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREP, "ERROR, Not processing RREP from %s, not a valid neighbor\n", inet_ntoa(packet->src_ip));
		return 0;
	}

	engine->mgmtOps->timer_delete(engine, reply->dst_ip, ROUTE_TASK_RESEND_RREQ);
	engine->mgmtOps->timers_update(engine);
	ROUTE_DPRINTF_ADDRESS(engine, ROUTE_DEBUG_RREP, "recv a route to: %s ", "next hop: %s \n", inet_ntoa(reply->dst_ip), inet_ntoa(packet->src_ip));

	return 1;

}


int _engine_rx_reply(route_engine_t *engine, route_task_t *packet)
{
	mesh_route_t 		*route;
	aodv_pdu_reply 	*reply;
	//    mesh_route_t *recv_route;

	reply = packet->data;
	ROUTE_REPLY_TO_HB(reply);

	if (!__check_reply(engine, reply, packet) )
	{
		return 0;
	}

	reply->metric++;

	engine->mgmtOps->route_update(engine, reply->dst_ip, packet->src_ip, reply->metric, reply->dst_seq, packet->dev->dev);
	route = engine->mgmtOps->route_lookup(engine, reply->src_ip, ROUTE_TYPE_AODV);

	if (!route)
	{
		ROUTE_DPRINTF_ADDRESS(engine, ROUTE_DEBUG_RREP, "No reverse-route for RREP from: %s to: ", "%s\n", inet_ntoa(reply->dst_ip), inet_ntoa(reply->src_ip));
		return 0;
	}

	if (!route->self_route)
	{
		ROUTE_DPRINTF_ADDRESS(engine, ROUTE_DEBUG_RREP, "Forwarding a route to: %s from node: ", "%s\n", inet_ntoa(reply->dst_ip), inet_ntoa(reply->src_ip) );
		ROUTE_REPLY_TO_NB(reply);
		engine->unicast(engine, route->nextHop, NET_DIAMETER, reply, sizeof(aodv_pdu_reply) );
	}

	/* If I'm not the destination of the RREP I forward it */
	return 0;
}



int  _engine_rx_reply_ack(route_engine_t *engine, route_task_t *packet)
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
	mesh_route_t *tmp_route;
	rerr_route 	*bad_routes = NULL;
	rerr_route 	*tmp_rerr_route = NULL;
	int num_routes = 0;

	//    int rerrhdr_created = 0;
	tmp_route = aodv_route_first();

	//go through list
	while (tmp_route != NULL)
	{
		if ((tmp_route->nextHop == brk_dst_ip)  && !tmp_route->self_route) //&& valid_aodv_route(tmp_route)
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

int _engine_rx_error(route_engine_t *engine, route_task_t *packet)
{
	mesh_route_t *route;
	aodv_pdu_error *errpdu;
	aodv_dest_t *errDest;
	rerr_route *rerrRoute = NULL;
	rerr_route *lastEerrRoute = NULL;
	int numOutgoingRoutes = 0;
	int i;

	errpdu = (aodv_pdu_error *)packet->data;
	errDest = (aodv_dest_t *) ((void*)packet->data + sizeof(aodv_pdu_error));
	ROUTE_DPRINTF(engine, ROUTE_DEBUG_RERR, "recieved a route error from %s, count= %u\n", inet_ntoa(packet->src_ip), errpdu->dst_count);

	for (i = 0; i < errpdu->dst_count; i++)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RERR, "       -> %s\n", inet_ntoa(errDest->ip));
		route = engine->mgmtOps->route_lookup(engine, errDest->ip, ROUTE_TYPE_AODV);
		if ( route && !route->self_route)
		{
			// if the route is valid and not a self route
			if ( route->nextHop == packet->src_ip)
			{
				//if we are using the person who sent the rerr as a next hop
				ROUTE_DPRINTF(engine, ROUTE_DEBUG_RERR, "Removing route %s\n", inet_ntoa(route->ip));
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
					_route_expire( route);
			}
		}

		errDest = (aodv_dest_t *) ((void*) errDest + sizeof(aodv_dest_t));
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RERR,"%s\n" ,"");
	}

	if (numOutgoingRoutes)
	{
//		aodv_pdu_error_send(outgoing_rerr_routes, num_outgoing_routes);
	}
	return 0;
}

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


int __check_request(route_engine_t *engine, aodv_pdu_request * request, u_int32_t last_hop_ip)
{
//	route_neigh_t *tmp_neigh;
	mesh_route_t *src_route;

	if ( engine->mgmtOps->flood_lookup(engine, request->src_ip, request->rreq_id))
	{
		return 0;
	}

	src_route = engine->mgmtOps->route_lookup(engine, request->src_ip, ROUTE_TYPE_AODV);
	if (!engine->mgmtOps->neigh_validate(engine, last_hop_ip))
	{
		return 0;
	}

	if ((src_route != NULL) && (src_route->nextHop != last_hop_ip) && (src_route->nextHop == src_route->ip))
	{
		ROUTE_DPRINTF_ADDRESS(engine, ROUTE_DEBUG_RREQ, "Last Hop of: %s ", "Doesn't match route: %s ", inet_ntoa(last_hop_ip), inet_ntoa(request->src_ip));
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREQ, "dest: %s RREQ_ID: %u, expires: %ld\n",inet_ntoa(request->dst_ip), request->rreq_id, src_route->lifetime - getcurrtime(engine->epoch));
	}

	return 1;
}

static int __request_should_be_reply(route_engine_t *engine, aodv_pdu_request * request)
{
	mesh_route_t *dst_route;

	dst_route = engine->mgmtOps->route_lookup(engine, request->dst_ip, ROUTE_TYPE_AODV);
	if ( !engine->check_subnet(engine, request->dst_ip))
	{
		if (engine->supportGateway)
		{

			ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREQ, "Gatewaying for address: %s, ", inet_ntoa( request->dst_ip ));
			if (dst_route == NULL)
			{
				ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREQ, "creating route for: %s \n",inet_ntoa( request->dst_ip ));
				dst_route = engine->mgmtOps->route_insert(engine, request->dst_ip, ROUTE_TYPE_AODV);
				dst_route->seq = engine->myRoute->seq;
				dst_route->nextHop = request->dst_ip;
				dst_route->metric = 1;
				dst_route->dev =   engine->myRoute->dev;
			}
			else
			{
				ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREQ, "using route: %s \n",inet_ntoa( dst_route->ip ));
			}

			dst_route->lifetime =  getcurrtime(engine->epoch) +  ACTIVE_ROUTE_TIMEOUT;
			dst_route->route_valid = TRUE;
			dst_route->route_seq_valid = TRUE;
			return 1;
		}

		//it is a local subnet and we need to create a route for it before we can reply...
		if ((dst_route!=NULL) && (dst_route->netmask!= engine->broadcastIp) )//g_broadcast_ip))
		{
			ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREQ, "creating route for local address: %s \n",inet_ntoa( request->dst_ip ));

			dst_route = engine->mgmtOps->route_insert(engine, request->dst_ip, ROUTE_TYPE_AODV);
			dst_route->seq = engine->myRoute->seq;
			dst_route->nextHop = request->dst_ip;
			dst_route->metric = 1;
			dst_route->dev =   engine->myRoute->dev;
			dst_route->lifetime =  getcurrtime(engine->epoch) +  ACTIVE_ROUTE_TIMEOUT;
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
			
			ROUTE_DPRINTF_ADDRESS(engine, ROUTE_DEBUG_RREQ, "AODV: Destination, Generating RREP -  src: %s ", "dst: %s \n", inet_ntoa(request->src_ip), inet_ntoa(request->dst_ip));

			return 1;
		}
	}

	/* Test to see if we should send a RREP AKA we have or are the desired route */
	return 0;
}

int _engine_rx_request(route_engine_t *engine, route_task_t *packet)
{
	aodv_pdu_request 	*request;
//	mesh_route_t *src_route;      /* Routing table entry */
	unsigned long  current_time;     /* Current time */
//	int size_out;

	request = packet->data;
	__convert_rreq_to_host(request);
	current_time = getcurrtime(engine->epoch);       /* Get the current time */

	/* Look in the route request list to see if the node has already received this request. */
	if ( packet->ttl <= 1)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREQ, "AODV TTL for RREQ from: %s expired\n", inet_ntoa(request->src_ip));
		return -ETIMEDOUT;
	}

	if (!__check_request(engine, request, packet->src_ip))
	{
		return 1;
	}

	request->metric++;
	/* Have not received this RREQ within BCAST_ID_SAVE time */
	/* Add this RREQ to the list for further checks */

	/* UPDATE REVERSE */
	engine->mgmtOps->route_update(engine, request->src_ip,  packet->src_ip, request->metric, request->src_seq,  packet->dev->dev);
	engine->mgmtOps->flood_insert(engine, request->src_ip, request->dst_ip, request->rreq_id, current_time + PATH_DISCOVERY_TIME);

	if(__request_should_be_reply(engine, request))
	{
		engine->protoOps->send_reply(engine, request->src_ip, request->dst_ip, request);
		return 0;
	}

	/* forward the RREQ */
	__convert_rreq_to_network(request);
	engine->broadcast(engine, packet->ttl - 1, request, sizeof(aodv_pdu_request));
	return 0;
}

int find_metric(u_int32_t tmp_ip)
{
	return 1;
}

int _engine_send_error(route_engine_t *engine, u_int32_t brk_dst_ip)
{
	mesh_route_t 		*route;
	
	aodv_pdu_error 	*errpdu;
	int 				errSize;
	rerr_route 		*dead = NULL;
	aodv_dest_t 		*errDest = NULL;
	void 			*data;

	rerr_route 		*badRoutes = NULL;
	rerr_route 		*errRoute = NULL;

	int numRoutes = 0;

	//    int rerrhdr_created = 0;
	ROUTE_READ_LOCK(engine->routeLock);
	ROUTE_LIST_FOR_EACH(route, engine->routes, list)
	{
		if (( route->nextHop == brk_dst_ip)  && !route->self_route) //&& valid_aodv_route(tmp_route)
		{
			if (( errRoute = (rerr_route *) kmalloc(sizeof(rerr_route), GFP_ATOMIC)) == NULL)
			{
				ROUTE_DPRINTF(engine, ROUTE_DEBUG_RERR, "RERR Can't allocate new entry %s\n", __FUNCTION__ );
				ROUTE_READ_UNLOCK(engine->routeLock);
				return 0;
			}
			
			errRoute->next = badRoutes;
			badRoutes = errRoute;
			
			errRoute->ip = route->ip;
			errRoute->seq = htonl( route->seq);
			numRoutes++;
			if ( route->route_valid)
				_route_expire( route);
			ROUTE_DPRINTF(engine, ROUTE_DEBUG_RERR,  "RERR: Broken link as next hop for - %s \n", inet_ntoa( route->ip));
		}
		//move on to the next entry
	}
	
	if( numRoutes == 0)
	{
		ROUTE_READ_UNLOCK(engine->routeLock);
		return 0;
	}	
	
	errSize = sizeof(aodv_pdu_error) + (sizeof(aodv_dest_t)*numRoutes);
	if ((data = kmalloc(errSize, GFP_ATOMIC)) == NULL)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RERR, " Error creating packet to send %s", " RERR\n");
		ROUTE_READ_UNLOCK(engine->routeLock);
		return -ENOMEM;
	}

	errpdu = (aodv_pdu_error *) data;
	errpdu->type = 3;
	errpdu->dst_count = numRoutes;
	errpdu->reserved = 0;
	errpdu->n = 0;

	//add in the routes that have broken...
	errDest = (aodv_dest_t *) ((void *)data + sizeof(aodv_pdu_error));
	while ( errRoute)
	{
		errDest->ip = errRoute->ip;
		errDest->seq = errRoute->seq;
		dead = errRoute;     
		errRoute = errRoute->next;
		kfree(dead );
		errDest = (void *)errDest + sizeof(aodv_dest_t);
	}

	engine->broadcast(engine,NET_DIAMETER, data, errSize);
	kfree(data);
	ROUTE_READ_UNLOCK(engine->routeLock);
	return 0;
}


int _engine_send_hello(route_engine_t  *engine)
{
	aodv_pdu_reply *reply;

	if ((reply = (aodv_pdu_reply *) kmalloc(sizeof(aodv_pdu_reply) , GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING ": Can't allocate new aodv reply pdu\n");
		return 0;
	}

	reply->type = RREP_MESSAGE;
	reply->reserved1 = 0;
	reply->src_ip = engine->myRoute->ip;
	reply->dst_ip = engine->myRoute->ip;
	reply->dst_seq = htonl(engine->myRoute->seq);
	reply->lifetime = htonl(MY_ROUTE_TIMEOUT);

	engine->broadcast(engine, 1, reply, sizeof(aodv_pdu_reply) );
	kfree (reply);

	engine->mgmtOps->timer_insert(engine, ROUTE_TASK_HELLO, engine->myIp, HELLO_INTERVAL);
	return 0;
}

static int __pdu_reply_send(route_engine_t *engine, mesh_route_t * src, mesh_route_t * dest, aodv_pdu_request * request)
{
	aodv_pdu_reply *reply;

	if (( reply = (aodv_pdu_reply *) kmalloc(sizeof(aodv_pdu_reply), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING " : Can't allocate new aodv_pdu_request\n");
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
		reply->lifetime = htonl(dest->lifetime - getcurrtime(engine->epoch));
	}

	engine->unicast(engine, src->nextHop, NET_DIAMETER, reply, sizeof(aodv_pdu_reply));
	kfree(reply);
	return 0;
}


int _engine_send_reply(route_engine_t *engine, u_int32_t src_ip, u_int32_t dst_ip, aodv_pdu_request *request)
{
	mesh_route_t *src;
	mesh_route_t *dest;

	/* Get the source and destination IP address from the RREQ */
	if (!(src = engine->mgmtOps->route_lookup(engine, src_ip, ROUTE_TYPE_AODV)))
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREP, "RREP  No route to Source! src: %s\n", inet_ntoa(src_ip));
		return -EHOSTUNREACH;
	}

	if (!(dest = engine->mgmtOps->route_lookup(engine, dst_ip, ROUTE_TYPE_AODV)))
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREP,"RREP  No route to Dest! dst: %s\n", inet_ntoa(dst_ip));
		return -EHOSTUNREACH;
	}

	return __pdu_reply_send(engine, src, dest, request);
//	return 0;
}

void _engine_send_reply_ack(route_engine_t *engine,  aodv_pdu_reply *reply)
{
	aodv_pdu_reply_ack *ack;

	if (( ack = (aodv_pdu_reply_ack*) kmalloc(sizeof (aodv_pdu_reply_ack),GFP_ATOMIC)) == NULL)
	{
		printk("RREP_ACK: error with tmp_ack\n");
		return;
	}

	ack->type=4;
	engine->unicast(engine, reply->src_ip, 1, ack, sizeof(aodv_pdu_reply_ack) );//, packet->dev->dev );
	kfree( ack);
}

/*  create and send a RREQ, add it into FLOOD and TIMER queue */
int _engine_send_request(route_engine_t *engine, unsigned long destIp)
{
	mesh_route_t 			*route;
	aodv_pdu_request 		*request;
	u_int8_t 				out_ttl;

	if (engine->mgmtOps->timer_lookup(engine, destIp, ROUTE_TASK_RESEND_RREQ) != NULL)
	{/* RREQ has been send for this dest */
		return 0;
	}

	ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREQ, "%s : Generating a RREQ for: %s\n", engine->name, inet_ntoa(destIp));
	if ((request = (aodv_pdu_request *) kmalloc(sizeof(aodv_pdu_request), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV: Can't allocate new aodv_pdu_request\n");
		return -ENOMEM;
	}

	/* Get routing table entry for destination */
	route = engine->mgmtOps->route_lookup(engine, destIp, ROUTE_TYPE_AODV);
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
	route = engine->mgmtOps->route_lookup(engine, engine->myIp, ROUTE_TYPE_AODV);
	if( route == NULL)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREQ,  " Can't get route to source: %s\n", inet_ntoa(engine->myIp));
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
	request->src_ip = engine->myIp;
	request->type = RREQ_MESSAGE;
	request->metric = htonl(0);
	request->j = 0;
	request->r = 0;
	request->d = 1;
	request->reserved = 0;
	request->second_reserved = 0;
	request->g = 1;

	if (engine->mgmtOps->flood_insert(engine, engine->myIp, destIp, route->rreq_id, getcurrtime(engine->epoch) + PATH_DISCOVERY_TIME) < 0)
	{
		kfree(request);
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_RREQ, " Can't add to broadcast list with ID %d(%s)\n",route->rreq_id, inet_ntoa(route->rreq_id));
		return -ENOMEM;
	}

	engine->mgmtOps->timer_insert_request(engine, request, RREQ_RETRIES);
	engine->mgmtOps->timers_update(engine);
	engine->broadcast(engine, out_ttl, request, sizeof(aodv_pdu_request));
	kfree(request);

	return 0;
}


int _engine_resend_request(route_engine_t *engine, route_task_t  *packet)
{
	mesh_route_t 			*route;
	aodv_pdu_request 		*request;
	u_int8_t 				out_ttl;

	if (packet->retries <= 0)
	{
//		ipq_drop_ip(packet->dst_ip);
		return 0;
	}

	if ((request = (aodv_pdu_request *) kmalloc(sizeof(aodv_pdu_request), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV : Can't allocate new aodv_pdu_request\n");
		return 0;
	}

	/* Get routing table entry for destination */
	route = engine->mgmtOps->route_lookup(engine, packet->dst_ip, ROUTE_TYPE_AODV);
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
	route = engine->mgmtOps->route_lookup(engine, packet->src_ip, ROUTE_TYPE_AODV);
	if ( route == NULL)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_ROUTE,  " Can't get route to source: %s\n", inet_ntoa(packet->src_ip));
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
	if (engine->mgmtOps->flood_insert(engine, packet->src_ip, packet->dst_ip, route->rreq_id, getcurrtime(engine->epoch) + PATH_DISCOVERY_TIME) < 0)
	{
		kfree(request);
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_FLOOD, " Can't add to broadcast list with ID %d(%s)\n", route->rreq_id, inet_ntoa(route->rreq_id));
		return -ENOMEM;
	}
	//    insert_timer_queue_entry(getcurrtime(engine->epoch) + NET_TRAVERSAL_TIME,timer_rreq, sizeof(struct rreq),out_rreq->dst_ip,0,out_ttl, EVENT_RREQ);
	//insert_timer_queue_entry(getcurrtime(engine->epoch) + NET_TRAVERSAL_TIME,timer_rreq, sizeof(struct rreq),out_rreq->dst_ip,RREQ_RETRIES,out_ttl, EVENT_RREQ);
	engine->mgmtOps->timer_insert_request(engine, request, packet->retries - 1);
	engine->mgmtOps->timers_update(engine);

	//local_broadcast(out_ttl,out_rreq,sizeof(struct rreq));
	engine->broadcast(engine, out_ttl, request, sizeof(aodv_pdu_request));
	kfree(request);

	return 0;
}

int _valid_protocol_pdu(int numbytes, int type, char *data)
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


proto_ops_t	aodv_ops =
{
	rx_request		:	_engine_rx_request,
	rx_reply			:	_engine_rx_reply,
	rx_reply_ack		:	_engine_rx_reply_ack,
	rx_error			:	_engine_rx_error,
	rx_hello			:	_engine_rx_hello,

	send_request		:	_engine_send_request,
	resend_request	:	_engine_resend_request,
	send_reply		:	_engine_send_reply,
	send_reply_ack	:	_engine_send_reply_ack,
	send_error		:	_engine_send_error,

	send_hello		:	_engine_send_hello,

	valid_protocol_pdu	:	_valid_protocol_pdu,
};

