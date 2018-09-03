/*
* $Id: route_dev_tx.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/
 
#include "mesh_route.h"
#include "_route.h"

#ifdef LINK_LIMIT
extern int g_link_limit;
#endif

int find_metric(u_int32_t tmp_ip)
{
	return 1;
}


/*  create and send a RREQ, add it into FLOOD and TIMER queue */
int _dev_send_request(route_dev_t *dev, route_task_t  *retry)
{
	mesh_route_t 			*route;
	route_node_t			*node = dev->node;
	aodv_pdu_request 		*request;
	u_int8_t 				out_ttl;

	if (dev->node->mgmtOps->timer_lookup(dev->node, retry->id, ROUTE_MGMT_TASK_RESEND_RREQ) != NULL)
	{/* RREQ has been send for this dest and not expired */
		ROUTE_DPRINTF(dev->node, ROUTE_DEBUG_RREQ, "%s : RREQ has been send to %s, so waiting next interval...\n", dev->node->name, inet_ntoa(retry->dest.address));
		return 0;
	}
	
	if (retry->retries <= 0)
	{
		dev->ops->drop(dev, retry->dest.address );
		kfree(retry);
		return 0;
	}

	ROUTE_DPRINTF(dev->node, ROUTE_DEBUG_RREQ, "%s : Generating a RREQ for: %s\n", dev->node->name, inet_ntoa(retry->dest.address));
	MALLOC(request, aodv_pdu_request *, sizeof(aodv_pdu_request), M_NOWAIT|M_ZERO, "RREQ PDU");
	if (!request)
	{
		return -ENOMEM;
	}

	/* Get routing table entry for destination */
	route = dev->node->mgmtOps->route_lookup(dev->node, &retry->dest, ROUTE_TYPE_AODV);
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
	route = node->mgmtOps->route_lookup(node, &dev->address, ROUTE_TYPE_AODV);
	if(!route)
	{/* no route for our route device, so is bug */
		ROUTE_DPRINTF(node, ROUTE_DEBUG_RREQ,  "BUG, Can't get route to source: %s\n", inet_ntoa(dev->address.address));
		kfree(request);
		return -EHOSTUNREACH;
	}

	/* Get our own sequence number */
	request->src_seq = htonl( ++route->seq);
	request->rreq_id = htonl( ++route->reqId);

	/* Fill in the package */
	request->dst_ip = retry->dest.address;
	request->src_ip = dev->address.address;
	request->type = RREQ_MESSAGE;
	request->metric = htonl(0);
	request->j = 0;
	request->r = 0;
	request->d = 0;
	request->reserved = 0;
	request->second_reserved = 0;
	request->g = 1;

	/* Get the broadcast address and ttl right */
	if (node->mgmtOps->flood_insert(node, &dev->address, &retry->dest, route->reqId, getcurrtime(dev->node->epoch) + PATH_DISCOVERY_TIME) < 0)
	{
		kfree(request);
		ROUTE_DPRINTF(node, ROUTE_DEBUG_FLOOD, " Can't add to broadcast list with ID %d(%s)\n", route->reqId, inet_ntoa(route->reqId));
		return -ENOMEM;
	}

	retry->time = getcurrtime(node->epoch) + ((2 ^ (RREQ_RETRIES - retry->retries)) * NET_TRAVERSAL_TIME);

	retry->retries--;
	if(retry<=0)
		return 0;
	
	_timer_enqueue( node, retry);
//	node->mgmtOps->timer_insert(node, &retry->dest, ROUTE_MGMT_TASK_RESEND_RREQ, PATH_DISCOVERY_TIME);
	node->mgmtOps->timers_update(node);

	dev->ops->broadcast(dev, out_ttl, request, sizeof(aodv_pdu_request));
	kfree(request);

	return 0;
}


void _dev_send_reply_ack(route_dev_t *dev, pdu_task_t *task)
{
	aodv_pdu_reply_ack 	*ack;
//	aodv_pdu_reply 		*reply = (aodv_pdu_reply *)task->data;
	route_neigh_t			*neigh;
	route_node_t			*node = dev->node;

	neigh = node->mgmtOps->neigh_lookup(node, task);
	if(!neigh )
		return;

	MALLOC(ack, (aodv_pdu_reply_ack*), sizeof (aodv_pdu_reply_ack), M_NOWAIT|M_ZERO, "RREP ACK");
	if (!ack )
	{
		return;
	}

	ack->type=4;
	dev->ops->unicast(dev, neigh, 1, ack, sizeof(aodv_pdu_reply_ack) );;
	kfree( ack);
}


int _dev_send_reply(route_dev_t *dev, pdu_task_t *task)
{
	mesh_route_t 		*src;
	mesh_route_t 		*dest;
	aodv_pdu_reply 	*reply;
	route_node_t		*node = dev->node;
	aodv_pdu_request 	*request = (aodv_pdu_request *)task->data;
	aodv_address_t	*origAdd, *destAdd;

	origAdd = (aodv_address_t *)&request->src_ip;
	destAdd = (aodv_address_t *)&request->dst_ip;

#if 0
	/* Get the source and destination IP address from the RREQ */
	if (!(src = node->mgmtOps->route_lookup(node, &task->header.src, ROUTE_TYPE_AODV)))
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_RREP, "RREP  No route to Source! src: %s\n", inet_ntoa(task->header.src.address));
		return -EHOSTUNREACH;
	}

	if (!(dest = node->mgmtOps->route_lookup(node, &task->header.dest, ROUTE_TYPE_AODV)))
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_RREP,"RREP  No route to Dest! dst: %s\n", inet_ntoa(task->header.dest.address));
		return -EHOSTUNREACH;
	}
#else
	if (!(src = node->mgmtOps->route_lookup(node, &task->header.src, ROUTE_TYPE_AODV)))
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_RREP, "RREP  No route to Source! src: %s\n", inet_ntoa(task->header.src.address));
		return -EHOSTUNREACH;
	}

	if (!(dest = node->mgmtOps->route_lookup(node, &task->header.dest, ROUTE_TYPE_AODV)))
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_RREP,"RREP  No route to Dest! dst: %s\n", inet_ntoa(task->header.dest.address));
		return -EHOSTUNREACH;
	}
#endif
	MALLOC(reply, (aodv_pdu_reply *), sizeof(aodv_pdu_reply), M_NOWAIT|M_ZERO, "RREP PDU" );
	if (!reply)
	{
		return 0;
	}

	reply->type = RREP_MESSAGE;
	reply->reserved1 = 0;
	reply->reserved2 = 0;
	reply->src_ip = src->address.address;
	reply->dst_ip = dest->address.address;
	reply->dst_seq = htonl(dest->seq);
	reply->metric = dest->metric;
	if (dest->selfRoute)
	{
		reply->lifetime = htonl(MY_ROUTE_TIMEOUT);
	}
	else
	{
		reply->lifetime = htonl(dest->lifetime - getcurrtime(dev->node->epoch));
	}

	dev->ops->unicast(dev, src->nextHop, NET_DIAMETER, reply, sizeof(aodv_pdu_reply));
	kfree(reply);

	return 0;
}

