/*
*
*/

#include "mesh_route.h"
#include "_route.h"

static int __request_should_be_reply(route_dev_t *dev, pdu_task_t *task)
{
	mesh_route_t 		*dstRoute;
	aodv_pdu_request 	*request = (aodv_pdu_request *)task->data;
	route_node_t		*node = dev->node;
	aodv_address_t	*destAdd ;//= (aodv_address_t *)task->data+L3_AODV_RREQ_DETS;
	aodv_address_t	*origAdd ;//= (aodv_address_t *)task->data+L3_AODV_RREQ_SRC;
	origAdd = (aodv_address_t *)&request->src_ip;
	destAdd = (aodv_address_t *)&request->dst_ip;

	ROUTE_DUMP_PDU(dev->node, ROUTE_DEBUG_RAWPDU ,"REQ RAW PDU", task->data, task->data_len );

	dstRoute = node->mgmtOps->route_lookup(node, destAdd, ROUTE_TYPE_AODV);
	if ( !dev->node->check_subnet(dev->node, destAdd ) )
	{/* not in our subnet list */
		if (dev->node->supportGateway)
		{/* gateway */
			ROUTE_DPRINTF(dev->node, ROUTE_DEBUG_RREQ, "Gatewaying for address: %s, ", inet_ntoa( destAdd->address));
			if (dstRoute == NULL)
			{/* gateway and dest is not in our subnet, so represent it reply as a agent */
				ROUTE_DPRINTF(node, ROUTE_DEBUG_RREQ, "creating route for: %s \n",inet_ntoa(destAdd->address));
				dstRoute = node->mgmtOps->route_insert(node, NULL, destAdd, ROUTE_TYPE_AODV);
				dstRoute->seq = node->myRoute->seq;
//				dstRoute->nextHop = NULL;
				dstRoute->metric = 1;
				dstRoute->dev = dev;
			}
			else
			{
				ROUTE_DPRINTF(dev->node, ROUTE_DEBUG_RREQ, "using route: %s \n",inet_ntoa( dstRoute->address.address ));
			}

			dstRoute->lifetime =  getcurrtime(dev->node->epoch) +  ACTIVE_ROUTE_TIMEOUT;
			dstRoute->routeValid = TRUE;
			dstRoute->routeSeqValid = TRUE;
			return TRUE;
		}

		//it is a local subnet and we need to create a route for it before we can reply...
		if ((dstRoute!=NULL) && (dstRoute->netmask!= dev->node->broadcastAddress.address) )
		{
			ROUTE_DPRINTF(dev->node, ROUTE_DEBUG_RREQ, "creating route for local address: %s \n",inet_ntoa(destAdd->address));

			dstRoute = dev->node->mgmtOps->route_insert(node, NULL, destAdd, ROUTE_TYPE_AODV);
			dstRoute->seq = dev->node->myRoute->seq;
			dstRoute->nextHop = NULL;//request->dst_ip;
			dstRoute->metric = 1;
			dstRoute->dev = dev;
			dstRoute->lifetime =  getcurrtime(node->epoch) +  ACTIVE_ROUTE_TIMEOUT;
			dstRoute->routeValid = TRUE;
			dstRoute->routeSeqValid = TRUE;

			return TRUE;
		}
	}
	else
	{// I am the dest...
		if (dstRoute && dstRoute->selfRoute)
		{
			if (seq_less_or_equal(dstRoute->seq, destAdd->sequence))
			{
				dstRoute->seq = destAdd->sequence + 1;
			}
			
			ROUTE_DPRINTF_ADDRESS(dev->node, ROUTE_DEBUG_RREQ, "AODV: Destination, Generating RREP -  src: %s ", "dest: %s \n", inet_ntoa(origAdd->address), inet_ntoa( destAdd->address));

			return TRUE;
		}
	}

	/* Test to see if we should send a RREP AKA we have or are the desired route */
	return FALSE;
}


int _dev_rx_request(route_dev_t *dev, pdu_task_t *task)
{
	aodv_pdu_request 	*request = (aodv_pdu_request *)task->data;
	route_node_t		*node = dev->node;
	mesh_route_t 		*srcRoute;
	route_neigh_t 		*lastHop;
	unsigned long		curTime;
	unsigned char 	*data = task->data;
	aodv_address_t	*origAdd, *destAdd;

	ROUTE_DUMP_PDU(dev->node, ROUTE_DEBUG_RAWPDU ,"REQ RAW PDU", task->data, task->data_len );

	origAdd = (aodv_address_t *)&request->src_ip;
	destAdd = (aodv_address_t *)&request->dst_ip;
//	ROUTE_RREQ_TO_HB(request);
	curTime = getcurrtime(dev->node->epoch);       /* Get the current time */

	/* Look in the route request list to see if the node has already received this request. */
	if ( task->header.ttl <= 1)
	{
		ROUTE_DPRINTF(dev->node, ROUTE_DEBUG_RREQ, "TTL for RREQ from: %s expired\n", inet_ntoa(request->src_ip));
		return -ETIMEDOUT;
	}

	if ( node->mgmtOps->flood_lookup(node, origAdd, request->rreq_id))
	{/* RREQ send by myself */
		return 0;
	}

	lastHop = node->mgmtOps->neigh_lookup(node, task);
	if(! lastHop)
	{
		lastHop = node->mgmtOps->neigh_insert(node, task);
	}

//	origAdd = (aodv_address_t *)data+L3_AODV_RREQ_SRC;
	srcRoute = node->mgmtOps->route_lookup(dev->node, origAdd, ROUTE_TYPE_AODV);
	if (!srcRoute)//node->mgmtOps->neigh_validate(node, last_hop_ip))
	{
		ROUTE_DPRINTF_ADDRESS(node, ROUTE_DEBUG_RREQ, "create Route to : %s ", "next hop: %s ",inet_ntoa(origAdd->address), inet_ntoa(lastHop->address.address));
		srcRoute = node->mgmtOps->route_insert(node, lastHop, origAdd, ROUTE_TYPE_AODV);
	}
	else if ( (srcRoute->nextHop != lastHop) )// && (src_route->nextHop == src_route->ip))
	{
		srcRoute->nextHop = lastHop;
		ROUTE_DPRINTF_ADDRESS(node, ROUTE_DEBUG_RREQ, "Last Hop of: %s ", "Doesn't match route: %s ", inet_ntoa(lastHop->address.address), inet_ntoa(request->src_ip));
		ROUTE_DPRINTF(node, ROUTE_DEBUG_RREQ, "dest: %s RREQ_ID: %u, expires: %ld\n",inet_ntoa(request->dst_ip), request->rreq_id, srcRoute->lifetime - getcurrtime(node->epoch));
	}

	request->metric++;
	/* Have not received this RREQ within BCAST_ID_SAVE time */
	/* Add this RREQ to the list for further checks */

	/* UPDATE REVERSE */
//	dev->node->mgmtOps->route_update(dev->node, request->src_ip,  packet->src_ip, request->metric, request->src_seq,  packet->dev->dev);
	dev->node->mgmtOps->flood_insert(dev->node,origAdd, destAdd, request->rreq_id, curTime + PATH_DISCOVERY_TIME);

	if(__request_should_be_reply(dev, task) )
	{
		dev->taskOps->send_reply(dev, task);
		return 0;
	}

	/* forward the RREQ */
//	ROUTE_RREQ_TO_NB(request);
	dev->node->broadcast(dev->node, task->header.ttl - 1, request, sizeof(aodv_pdu_request));
	return 0;
	
}

int _dev_rx_error(route_dev_t *dev, pdu_task_t *task)
{
	route_node_t 		*node = dev->node;
	mesh_route_t 		*route;
	aodv_pdu_error 	*errpdu;
	
	aodv_address_t	*destAdd, *errDestAdd = NULL, *tmp;
	int numOutgoingRoutes = 0;
	void 			*data = NULL;
	int count, i;
	int len;

	errpdu = (aodv_pdu_error *)task->data;
	destAdd = (aodv_address_t *) ((void*)task->data + sizeof(aodv_pdu_error));
	count = errpdu->dst_count;
	ROUTE_DPRINTF(node, ROUTE_DEBUG_RERR, "recieved a route error from %s, count= %u\n", inet_ntoa(task->header.src.address), errpdu->dst_count);

	if(count<=0)
		return FALSE;

	MALLOC(errDestAdd, (aodv_address_t *), sizeof(aodv_address_t)*count, M_NOWAIT|M_ZERO, "ERROR DEST(RERR)" );
	tmp = errDestAdd;
				
	for (i = 0; i <count ; i++)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_RERR, "       -> %s\n", inet_ntoa(destAdd->address));
		route = node->mgmtOps->route_lookup(node, destAdd, ROUTE_TYPE_AODV);
		if ( route && !route->selfRoute)
		{
			// if the route is valid and not a self route
//			if ( route->nextHop == packet->src_ip)
			if(! ADDRESS_EQUAL(&route->nextHop->address, &task->header.src) )
			{
				ROUTE_DPRINTF(node, ROUTE_DEBUG_RERR, "Removing route %s\n", inet_ntoa(route->address.address));
				route->seq = ntohl( destAdd->sequence); //convert to host format for local storage

				tmp->address = destAdd->address;
				tmp->sequence = destAdd->sequence;
				numOutgoingRoutes++;
				if ( route->routeValid)
					_route_expire( route);
			}
		}
		destAdd++;
		tmp++;

		ROUTE_DPRINTF(node, ROUTE_DEBUG_RERR,"%s\n" ,"");
	}

	if (numOutgoingRoutes)
	{
		aodv_dest_t 	*errDest;
		len = sizeof(aodv_pdu_error) + (sizeof(aodv_dest_t) * numOutgoingRoutes);
		MALLOC(data, (void *), len, M_NOWAIT|M_ZERO, "ERROR DEST(RERR)");
		if (!data  )
		{
			return -ENOMEM;
		}

		errpdu = (aodv_pdu_error *) data;
		errpdu->type = 3;
		errpdu->dst_count = numOutgoingRoutes;

		//add in the routes that have broken...
		errDest = (aodv_dest_t *) ((void *)data + sizeof(aodv_pdu_error));
		tmp = errDestAdd;
		for(i=0; i<numOutgoingRoutes; i++)
		{
			errDest->ip = tmp->address;
			errDest->seq = tmp->sequence;
			errDest = (void *)errDest + sizeof(aodv_dest_t);
		}
		
		node->broadcast(node, NET_DIAMETER, data, len );
		if(!data)
			kfree(data);
	}
	
	if(!errDestAdd)
		kfree(errDestAdd);

	return 0;
}


int _dev_rx_reply(route_dev_t *dev, pdu_task_t *task)
{
	mesh_route_t 		*route;
	aodv_pdu_reply 	*reply = (aodv_pdu_reply *) task->data;
	route_node_t		*node = dev->node;
	route_neigh_t		*lastHop;
	aodv_address_t	*destAdd = (aodv_address_t *)reply->dst_ip;// (aodv_address_t *)task->data+L3_AODV_RREP_DEST;
	aodv_address_t	origAdd;

	origAdd.address = reply->src_ip;
	ROUTE_REPLY_TO_HB(reply);

	if( (reply->src_ip == reply->dst_ip) && (task->header.src.address==reply->src_ip) )
	{//its a hello messages! HELLO WORLD! only one hop for hello, 
		dev->taskOps->rx_hello(dev, task );
		return 0;
	}

#if 0
	/* for RREP, last hop may bit exist, because RREQ is broadcast */
	lastHop= _neigh_lookup(node, task);
	if ( !lastHop || ( lastHop->validLink && time_before(getcurrtime(node->epoch), lastHop->lifetime) ) ) 
	{/* reply is not send by my neigh: so ignored. PDU is forward and modified node by node */
		return 0;
	}
#endif

	node->mgmtOps->timer_delete(node, destAdd, ROUTE_MGMT_TASK_RESEND_RREQ, 0);
	node->mgmtOps->timers_update(node);
	ROUTE_DPRINTF_ADDRESS(node, ROUTE_DEBUG_RREP, "recv a route to: %s ", "next hop: %s \n", inet_ntoa(reply->dst_ip), inet_ntoa(task->header.src.address));

	reply->metric++;

	/* if lastHop is not exist, just create it */
	lastHop = node->mgmtOps->neigh_lookup( node, task);
	if(! lastHop)
	{
		lastHop = node->mgmtOps->neigh_insert(node, task);
	}
	node->mgmtOps->route_update(node, destAdd, lastHop, reply->metric);

	/* now forward the REPLY to orig node */
	route = node->mgmtOps->route_lookup(node, &origAdd, ROUTE_TYPE_AODV);

	if (!route)
	{
		ROUTE_DPRINTF_ADDRESS(node, ROUTE_DEBUG_RREP, "No reverse-route for RREP from: %s to: ", "%s\n", inet_ntoa(reply->dst_ip), inet_ntoa(reply->src_ip));
		return 0;
	}

	if (!route->selfRoute)
	{/* If I'm not the destination of the RREP I forward it */
		ROUTE_DPRINTF_ADDRESS(node, ROUTE_DEBUG_RREP, "Forwarding a route to: %s from node: ", "%s\n", inet_ntoa(reply->dst_ip), inet_ntoa(reply->src_ip) );
		ROUTE_REPLY_TO_NB(reply);
		if(route->dev && route->nextHop )
			route->dev->ops->unicast(route->dev, route->nextHop, NET_DIAMETER, reply, sizeof(aodv_pdu_reply) );
		else
			ROUTE_DPRINTF(node, ROUTE_DEBUG_ENGINE, "No device attached route %s", inet_ntoa(route->address.address) );
	}

	return 0;
}

int _dev_rx_hello(route_dev_t *dev, pdu_task_t *task)
{
	route_neigh_t 		*neigh;
	route_node_t		*node = dev->node;
	aodv_pdu_reply 	*reply = (aodv_pdu_reply *) task->data;
//	aodv_address_t	*destAdd = (aodv_address_t *)task->data+L3_AODV_RREP_DEST;

	neigh = node->mgmtOps->neigh_lookup(node, task);
	if ( neigh == NULL)
	{
		neigh = dev->node->mgmtOps->neigh_insert(node, task);
		if (!neigh)
		{
			ROUTE_DPRINTF(node, ROUTE_DEBUG_RREP, "Error, creating neighbor: %s\n", inet_ntoa(task->header.src.address) );
			return -1;
		}
		
		memcpy(&(neigh->myHwAddr), &(task->srcHwAddr), sizeof(unsigned char) * ETH_ALEN);
		neigh->dev = dev;
#ifdef ROUTE_SIGNAL
		aodv_iw_set_spy(node);
#endif
	} 

	node->mgmtOps->timer_delete(node, &neigh->address, ROUTE_MGMT_TASK_NEIGHBOR, 0);
	node->mgmtOps->timer_insert(node, &neigh->address, ROUTE_MGMT_TASK_NEIGHBOR, HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 100);

	neigh->lifetime = reply->lifetime + getcurrtime(node->epoch) + 20;

	/* update neigh when neigh is not newly created */
	node->mgmtOps->neigh_update_route(node, neigh, task); 

	return 0;
}


int  _dev_rx_reply_ack(route_dev_t *dev, pdu_task_t *task)
{
	return TRUE;
}

