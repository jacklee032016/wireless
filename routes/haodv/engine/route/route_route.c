/*
* $Id: route_route.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/

#include <linux/proc_fs.h>
#include "mesh_route.h"
#include <linux/inetdevice.h>

#include "_route.h"

int _route_valid( mesh_route_t * route)
{
	int res = 0;
	if(!route->dev)
	{
		printk("Error : no device involved for this route, self route %d\n", route->selfRoute);
		return res;
	}
	
	if (time_after(route->lifetime,  getcurrtime(route->dev->node->epoch)) && route->routeValid)
	{
		res = 1;
	}
	return res;
}

void _route_expire(mesh_route_t *route)
{
	//marks a route as expired
	if(!route->dev)
	{
		printk("Error : no device involved for this route, self route %d\n", route->selfRoute);
		return;
	}
	route->lifetime = (getcurrtime(route->dev->node->epoch) + DELETE_PERIOD);
	route->seq++;
	route->routeValid = FALSE;
	route->nextHop = NULL;
}

inline int _route_compare(mesh_route_t *route, aodv_address_t *address)
{
//	if ((route->ip & route->netmask) == (destIp & route->netmask))
//	if ( route->address->address  == address->address )
	if(ADDRESS_EQUAL(&route->address, address) )
		return 1;
	else
		return 0;
}

int _route_update(route_node_t *node, aodv_address_t *destAdd, route_neigh_t *nextHop, u_int8_t metric )
{
	mesh_route_t *route;
	unsigned long  curr_time;

	/*lock table */
	route = node->mgmtOps->route_lookup(node, destAdd, ROUTE_TYPE_AODV);    /* Get eprev_route->next->prev = new_route;ntry from RT if there is one */

/*
	if (!node->mgmtOps->neigh_validate(node, next_hop_ip))
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_ROUTE, "%s : Failed to update route: %s \n",node->name,  inet_ntoa(ip));
		return 0;
	}
*/
	if (route == NULL)
	{
		route = node->mgmtOps->route_insert(node, nextHop, destAdd, ROUTE_TYPE_AODV);
		if (route == NULL)
			return -ENOMEM;
	}

	if ( route && seq_greater( route->seq, destAdd->sequence) )
	{
		return 0;
	}

	if ( route && route->routeValid)
	{
		if ((destAdd->sequence == route->seq) && (metric >= route->metric))
		{
			return 0;
		}
	}


	if (route->selfRoute)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_ROUTE, "updating a SELF-ROUTE!!! %s hopcount %d\n", inet_ntoa(nextHop->address.address), metric);
		if (!route->routeValid)
			ROUTE_DPRINTF(node, ROUTE_DEBUG_ROUTE, "because route was %s", "invalid!\n");

		if (!route->routeSeqValid)
			ROUTE_DPRINTF(node, ROUTE_DEBUG_ROUTE, "because seq was %s", "invalid!\n");
		if (seq_greater(destAdd->sequence, route->seq))
			ROUTE_DPRINTF(node, ROUTE_DEBUG_ROUTE, "because seq of route was %s", "lower!\n");
		if ((destAdd->sequence == route->seq) && (metric < route->metric))
			ROUTE_DPRINTF(node, ROUTE_DEBUG_ROUTE, "becase seq same but hop %s", "lower!\n");
	}

	/* Update values in the RT entry */
	route->seq = destAdd->sequence;
	route->nextHop = nextHop;
	route->metric = metric;
	route->dev =   nextHop->dev;
	route->routeValid = TRUE;
	route->routeSeqValid = TRUE;

/*
	aodv_delete_kroute(route);
	aodv_insert_kroute(route);

	ipq_send_ip(ip);
*/
	if(nextHop && nextHop->dev)
		nextHop->dev->ops->resend(nextHop->dev, destAdd->address );

	curr_time = getcurrtime(node->epoch);  /* Get current time */
	/* Check if the lifetime in RT is valid, if not update it */

	route->lifetime =  curr_time +  ACTIVE_ROUTE_TIMEOUT;
	return 0;
}

mesh_route_t *_route_lookup(route_node_t *node, aodv_address_t *address, route_type_t type)
{
	mesh_route_t 		*route;

	unsigned long  curr_time = getcurrtime(node->epoch);

	ROUTE_READ_LOCK(node->routeLock);
	ROUTE_LIST_FOR_EACH(route, node->routes, list)
	{
//		if( route->ip <= target_ip )
		{
			if (time_before( route->lifetime, curr_time) && (!route->selfRoute) && (route->routeValid))
			{
				_route_expire( route );
			}

			if ( _route_compare(route, address))
			{/* destIp belone to this route, so return this route */
#if 0//ROUTE_DEBUG			
				char msg[1024];
				int len = 0;
				len += sprintf(msg+len, "it looks like the route %s ",inet_ntoa( route->ip));
				len += sprintf(msg+len, "is equal to: %s\n",inet_ntoa(destIp));
				printk("%s", msg);
#endif				
				ROUTE_READ_UNLOCK(node->routeLock);
				return route;
			}
		}
	}
	ROUTE_READ_UNLOCK(node->routeLock);

	return NULL;//possible;
}


mesh_route_t *_route_insert(route_node_t *node, route_neigh_t *nextHop, aodv_address_t *address, route_type_t type)
{
	mesh_route_t *route;
	unsigned long	flags;

	/* Allocate memory for new entry */
	MALLOC(route, (mesh_route_t *), sizeof(mesh_route_t), M_NOWAIT|M_ZERO, "ROUTE ENTRY");
	if (!route )
	{
		return NULL;
	}

	route->selfRoute = FALSE;
	route->reqId = 0;
	route->metric = 0;
	route->seq = address->sequence;
	ADDRESS_ASSIGN(&(route->address), address);
	route->nextHop = nextHop;
	if(nextHop)
		route->dev = nextHop->dev;
	
	route->routeValid = FALSE;
	route->netmask = node->broadcastAddress.address;
	route->routeSeqValid = FALSE;
	route->type = type;

//	if (ip)
	{
		ROUTE_WRITE_LOCK(node->routeLock, flags);
		ROUTE_LIST_ADD_HEAD( &(route->list), node->routes );
		ROUTE_WRITE_UNLOCK( node->routeLock, flags);
	}
	return route;
}

int _route_delete(route_node_t *node, aodv_address_t *dest)
{
	mesh_route_t *route, *tmp;
	unsigned long	flags;
	
	ROUTE_WRITE_LOCK(node->routeLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(route, tmp, node->routes, list)
	{
//		if( route->ip <= target_ip )
//		{
//			if (route->ip == target_ip)
			if(ADDRESS_EQUAL(&route->address, dest) )
			{
				ROUTE_LIST_DEL(&(route->list) );
				ROUTE_WRITE_UNLOCK(node->routeLock, flags);
				kfree(route);
				route = NULL;
				return 1;
			}
//		}	
	}
	ROUTE_WRITE_UNLOCK(node->routeLock, flags);

	return 0;
}


