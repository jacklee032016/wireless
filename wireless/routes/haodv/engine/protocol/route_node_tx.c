#include "mesh_route.h"
#include "_route.h"

/* called as MGMT task */
int _node_send_hello(route_node_t  *node )
{
	aodv_pdu_reply *reply;

	/* check active route device and route not self */
	ROUTE_READ_LOCK(node->routeLock);
	if(ROUTE_LIST_CHECK_EMPTY(node->routes) )
	{
		ROUTE_READ_UNLOCK(node->routeLock);
		return FALSE;
	}

	MALLOC(reply, (aodv_pdu_reply *), sizeof(aodv_pdu_reply), M_NOWAIT|M_ZERO, "HELLO(RREP)");
	if (!reply)
	{
		return 0;
	}
	
	reply->type = RREP_MESSAGE;
	reply->reserved1 = 0;
	reply->src_ip = node->address.address;
	reply->dst_ip = node->address.address;
	reply->dst_seq = htonl(node->myRoute->seq);
	reply->lifetime = htonl(MY_ROUTE_TIMEOUT);

	node->broadcast(node, 1, reply, sizeof(aodv_pdu_reply) );
	kfree (reply);

	node->mgmtOps->timer_insert(node, &node->address,ROUTE_MGMT_TASK_HELLO,  HELLO_INTERVAL);
	return 0;
}

/*
* called when neigh is delete just because of rx RERR 
* all route use this neigh as nextHop will be send as RERR 
*/
int _node_send_error(route_node_t *node, route_neigh_t *neigh)
{
	mesh_route_t 		*route;
	aodv_pdu_error 	*errpdu;
	int 				errSize;
	aodv_dest_t 		*tmp;
	void 			*data;
	int 				numRoutes = 0;

	ROUTE_READ_LOCK(node->routeLock);
	ROUTE_LIST_FOR_EACH(route, node->routes, list)
	{
		if ( (neigh==route->nextHop)  && !route->selfRoute) //&& valid_aodv_route(tmp_route)
		{
			numRoutes++;
		}	
	}
	
	if( numRoutes == 0)
	{
		ROUTE_READ_UNLOCK(node->routeLock);
		return 0;
	}	

	errSize = sizeof(aodv_pdu_error)+ sizeof(aodv_dest_t)*numRoutes;
	MALLOC(data, (void *), errSize, M_NOWAIT|M_ZERO, "ERR DEST");
	if (!data)
	{
		ROUTE_READ_UNLOCK(node->routeLock);
		return 0;
	}

	tmp =(aodv_dest_t *)( (void *)data + sizeof(aodv_pdu_error) );

	ROUTE_LIST_FOR_EACH(route, node->routes, list)
	{
		if ( (neigh == route->nextHop) && !route->selfRoute) //&& valid_aodv_route(tmp_route)
		{

			tmp->ip = route->address.address;
			tmp->seq = htonl(route->seq);
//				aodv_delete_kroute(route );
			if ( route->routeValid)
			{
				_route_expire( route);
			}	
			else
			{
//				node->mgmtOps->route_delete(node, &route->address );
			}
			ROUTE_DPRINTF(node, ROUTE_DEBUG_RERR,  "RERR: Broken link as next hop for - %s \n", inet_ntoa( route->address.address));
			tmp++;
		}

		//move on to the next entry
	}
	ROUTE_READ_UNLOCK(node->routeLock);
	
	errpdu = (aodv_pdu_error *) data;
	errpdu->type = 3;
	errpdu->dst_count = numRoutes;
	errpdu->reserved = 0;
	errpdu->n = 0;

	node->broadcast(node, NET_DIAMETER, data, errSize);

	kfree(data);
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
	send_error		:	_node_send_error,
	send_hello		:	_node_send_hello,
	valid_protocol_pdu	:	_valid_protocol_pdu,
};

