/*
* $Id: route_neigh.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/

#include "mesh_route.h"
#include "_route.h"
/*
* All neigh operation is only involved with data of pdu_task_t 
* except neigh_delete which is executed by kernel thread 
*/

/* neigh is always searched by the last hop */
route_neigh_t *_neigh_lookup(route_node_t *node, pdu_task_t *task)
{
	route_neigh_t 		*neigh; 

	ROUTE_READ_LOCK(node->neighLock);
	ROUTE_LIST_FOR_EACH(neigh, node->neighs, list)
	{
//		if ( neigh->address <= task->header.src )
		{
			if ( ADDRESS_EQUAL( &neigh->address, &task->header.src ) && neigh->dev == task->header.dev )
			{
				ROUTE_READ_UNLOCK(node->neighLock);
				return neigh;
			}
		}
	}
	ROUTE_READ_UNLOCK(node->neighLock);
	return NULL;
}

route_neigh_t *_neigh_lookup_mac(route_node_t *node, char *hw_addr)
{
	route_neigh_t 		*neigh; 

	ROUTE_READ_LOCK(node->neighLock);
	ROUTE_LIST_FOR_EACH(neigh, node->neighs, list)
	{
		if (!memcmp(&( neigh->myHwAddr), hw_addr, ETH_ALEN))
		{
			ROUTE_READ_UNLOCK(node->neighLock);
			return neigh;
		}	
	}
	
	ROUTE_READ_UNLOCK(node->neighLock);
	return NULL;
}

/* need to merge some logic in these 2 functions */
void _neigh_update_link(route_node_t *node, char *hw_addr, u_int8_t link)
{
	route_neigh_t 		*neigh; 
	unsigned long		flags;

	ROUTE_WRITE_LOCK(node->neighLock, flags);
	ROUTE_LIST_FOR_EACH(neigh, node->neighs, list)
	{
		if (!memcmp(&( neigh->myHwAddr), hw_addr, ETH_ALEN))
		{
			if (link)
				neigh->link = 0x100 - link;
			else
				neigh->link = 0;
			break;
		}
	}
	ROUTE_WRITE_UNLOCK(node->neighLock, flags);
}

/* called by Mgmt Task periodically */
int _neigh_delete(route_node_t *node, aodv_address_t *address/* address of neight */ )
{
	route_neigh_t 		*neigh, *tmp; 
	mesh_route_t		*route;
	unsigned long		flags;
	unsigned long		curTime = getcurrtime(node->epoch);
	int count = 0;

	ROUTE_WRITE_LOCK(node->neighLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(neigh, tmp, node->neighs, list)
	{
		if(!address || ( ADDRESS_EQUAL(& neigh->address, address) && time_before(neigh->lifetime, curTime)  ))
		{
			ROUTE_LIST_DEL(&(neigh->list) );

			node->mgmtOps->timer_delete(node, &neigh->address, ROUTE_MGMT_TASK_NEIGHBOR, 0);
			node->mgmtOps->timers_update(node);
	//		ROUTE_WRITE_UNLOCK( node->neighLock, flags);

			route = node->mgmtOps->route_lookup(node, &neigh->address, ROUTE_TYPE_AODV);
			node->protoOps->send_error(node, neigh );

			count++;

			kfree( neigh);
			neigh = NULL;
//			return 0;
		}
	}

	ROUTE_WRITE_UNLOCK( node->neighLock, flags);

	return count;//-ENODATA;
}

/* create a neigh and its route */
route_neigh_t *_neigh_insert(route_node_t *node, pdu_task_t *task)
{
	route_neigh_t *neigh = NULL;
	mesh_route_t	*route = NULL;
	unsigned long	flags;

	ROUTE_DPRINTF(node, ROUTE_DEBUG_NEIGH, "create neight with IP : %s\n", inet_ntoa(task->header.src.address));

	neigh = node->mgmtOps->neigh_lookup(node, task );
	if ( neigh )
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_NEIGH, "NEIGH : try to create a duplicate neighbor %s\n", inet_ntoa(task->header.src.address));
		return neigh;
	}

	MALLOC( neigh, (route_neigh_t *),sizeof(route_neigh_t), M_NOWAIT|M_ZERO, "NEIGH");
	if (!neigh)
		return NULL;

	ADDRESS_ASSIGN(&neigh->address, &task->header.src);
	neigh->lifetime = 0;
	neigh->myroute = NULL;
	neigh->link = 0;
	neigh->dev = task->header.dev;
	neigh->myroute = route;
	neigh->priv = task->header.priv;
	
#ifdef LINK_LIMIT
	neigh->validLink = FALSE;
#else
	neigh->validLink = TRUE;
#endif

	route = node->mgmtOps->route_insert(node, neigh, &task->header.src, ROUTE_TYPE_AODV);
	if(!route)
		return NULL;
	route->hopCount = 1;
	route->netmask = task->header.dev->node->broadcastAddress.address;
#if 0
	MALLOC(route, mesh_route_t *, sizeof(mesh_route_t), M_NOWAIT|M_ZERO, "ROUTE of NEIGH");
	ADDRESS_ASSIGN( &route->address, &task->header.src );
	route->dev = task->header.dev;
	route->nextHop = neigh;
	route->hopCount = 1;
	route->netmask = task->header.dev->node->broadcastAddress.address;
	ROUTE_WRITE_LOCK( node->routeLock, flags);
	ROUTE_LIST_ADD_HEAD(&(route->list), node->routes );
	ROUTE_WRITE_UNLOCK(node->routeLock, flags);
#endif

	ROUTE_WRITE_LOCK( node->neighLock, flags);
	ROUTE_LIST_ADD_HEAD(&(neigh->list), node->neighs );
	ROUTE_WRITE_UNLOCK(node->neighLock, flags);

	return neigh;
}

void _neigh_update_route(route_node_t *node, route_neigh_t *nextHop, pdu_task_t *task)
{
	mesh_route_t 		*route;
	aodv_pdu_reply 	*reply = (aodv_pdu_reply *) task->data;

	route = node->mgmtOps->route_lookup(node, &nextHop->address, ROUTE_TYPE_AODV);
	if (route)
	{
/*				if (!tmp_route->routeValid)
				{
			    insert_kernel_route_entry(tmp_route->ip, tmp_route->nextHop, tmp_route->netmask,tmp_route->dev->name);
				}

				if (tmp_route->nextHop!=tmp_route->ip)
				{*/
//		aodv_delete_kroute(route);
//		aodv_insert_kroute(route);
				//}
	}
	else
	{	    
		ROUTE_DPRINTF(node, ROUTE_DEBUG_NEIGH, "Creating route for neighbor: %s Link: %d\n", inet_ntoa( nextHop->address.address), nextHop->link);
		route = node->mgmtOps->route_insert(node, nextHop, &nextHop->address, ROUTE_TYPE_AODV);
//		aodv_delete_kroute(route );
//		aodv_insert_kroute( route);
	}

	route->lifetime = getcurrtime(node->epoch) + reply->lifetime;
	route->dev = nextHop->dev;
	route->nextHop = nextHop;
	route->routeSeqValid = TRUE;
	route->routeValid = TRUE;
	route->seq = reply->dst_seq;
	route->metric = find_metric( route->address.address);

	nextHop->myroute = route;
	
}

int _neigh_update(route_node_t *node, route_neigh_t *neigh, aodv_pdu_reply *reply)
{
	if (neigh==NULL)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_NEIGH, "AODV: NULL neighbor %s", "passed!\n");
		return 0;
	}
#ifdef LINK_LIMIT
	if (neigh->link > (g_link_limit))
	{
		neigh->validLink = TRUE;
	}
	/*if (tmp_neigh->link < (g_link_limit - 5))
	{
		tmp_neigh->validLink = FALSE;
	}*/
#endif

	if (neigh->validLink)
	{
//		node->mgmtOps->neigh_update_route(node, neigh, reply);
	}

	return 1;
}


