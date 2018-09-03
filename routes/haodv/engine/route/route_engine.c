/*
* $Id: route_engine.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/

#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/string.h>
#include <linux/inetdevice.h>
#include "mesh_route.h"
#include "_route.h"


/* why not use the net_device data member in ROUTE_DEVICE */
static route_dev_t *_node_find_dev_by_net(route_node_t *node, struct net_device * dev)
{
	route_dev_t 		*routeDev;
	struct in_device 	*indev;
	u_int32_t 		ip;

	if (dev == NULL)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_DEV,  "No Net Device is provided, %s!\n", __FUNCTION__);
		return NULL;
	}
	if (dev->ip_ptr == NULL)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_DEV, "No IP Addres has configed for Net Device %s\n", dev->name);
		return NULL;
	}
	
	indev = (struct in_device *) dev->ip_ptr;
	if ( indev->ifa_list == NULL)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_DEV, "ERROR, Net Device %s has no ifa_list\n", dev->name);
		return NULL;
	}
	ip = indev->ifa_list->ifa_address;

	ROUTE_LIST_FOR_EACH(routeDev, node->devs, list)
	{
		if ( routeDev->address.address == (u_int32_t) ip)
			return routeDev;
	}
/*
	// for POST_ROUTE filter, all net_device is check
	ROUTE_DPRINTF(node, ROUTE_DEBUG_DEV,  "Failed search for matching interface for: %s which has an ip of: %s\n", dev->name, inet_ntoa(ip));
*/	
	return NULL;
}

/* check destIp is in one local subnet of my node device */
static route_dev_t* _node_find_dev_by_ip(route_node_t *node, u_int32_t destIp)
{
	route_dev_t 		*routeDev = NULL;

	ROUTE_LIST_FOR_EACH(routeDev, node->devs, list)
	{
		if ((routeDev->address.address & routeDev->netmask)  == (routeDev->netmask & destIp ))
			return routeDev;
	}

	return NULL;
}


/* check destIp is in one local subnet of my node device */
static int _node_check_subnet(route_node_t *node, aodv_address_t *dest)
{
	route_dev_t 		*routeDev;

	ROUTE_LIST_FOR_EACH(routeDev, node->devs, list)
	{
		if ((routeDev->address.address & routeDev->netmask)  == (routeDev->netmask & dest->address))
			return 1;
	}

	return 0;
}

#if 0
static int _node_unicast(route_node_t *node, u_int32_t destip, u_int8_t ttl, void *data, const size_t datalen )
{
	route_dev_t			*routeDev;
	int 					length = 0;

	if (ttl == 0)
		return 0;

	ROUTE_LIST_FOR_EACH(routeDev, node->devs, list)
	{
		if( (routeDev->ip&routeDev->netmask)==(destip&routeDev->netmask) )
		{/* most match, shpuld be enhanced, lzj */
			length = routeDev->ops->unicast(routeDev, destip, ttl, data, datalen);
		}	
	}

	return length;
}
#endif

int _node_broadcast(route_node_t *node, u_int8_t ttl, void *data,const size_t datalen)
{
	route_dev_t 	*routeDev;
	int 			length = 0;

	if (ttl < 1)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_ENGINE, "TTL error : ttl=%d\n", ttl);
		return 0;
	}

	if(ROUTE_LIST_CHECK_EMPTY(node->devs) )
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_ENGINE, "Error, no Route Device is attached when try broadcast, %s\n" , __FUNCTION__);
		return 0;
	}

	ROUTE_LIST_FOR_EACH(routeDev, node->devs, list)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_DEV, "broadcast on %s\n", routeDev->name);
		length += routeDev->ops->broadcast(routeDev, ttl, data, datalen);
	}

	return length;
}

int node_init(route_node_t *node, char *localSubnet)
{
	char* 			str;
	aodv_address_t	localSubnetIp={0, 0};
	u_int32_t 		localSubnetMask=0;
	unsigned char 	proc_name[32];
	route_proc_entry 	*proc = node->procs;
	mesh_route_t 		*route;
#if 0	
	u_int32_t			lo;
#endif

	inet_aton("255.255.255.255",&(node->broadcastAddress.address) );
	
	node->epoch = getAodvEpoch();

	node->check_subnet = _node_check_subnet;
	node->dev_lookup = _node_find_dev_by_net;
	node->dev_lookup_by_ip = _node_find_dev_by_ip;
//	node->unicast = _node_unicast;
	node->broadcast = _node_broadcast,
	
	init_timer(&(node->timer) );

#if 0
	inet_aton("127.0.0.1", &lo);
	route = node_route_create(node, lo, ROUTE_ROUTE_KERNEL);
	route->nextHop = lo;
	route->metric = 0;
	route->selfRoute = TRUE;
	route->routeValid = TRUE;
	route->seq = 0;
#endif

	if ( localSubnet==NULL)
		return (-EINVAL);
	else
	{
		str = strchr(localSubnet,'/');
		if ( str)
		{
			(*str)='\0';
			str++;
			ROUTE_DPRINTF(node,  ROUTE_DEBUG_INIT, "Adding Local Routed Subnet %s/%s to the routing table...\n", localSubnet, str);

			inet_aton(localSubnet, &(localSubnetIp.address));
			inet_aton(str, &localSubnetMask);

			route = _route_insert(node, NULL/* nextHop*/, &localSubnetIp, ROUTE_TYPE_AODV);
			route->routeValid = TRUE;
			route->routeSeqValid = TRUE;
			route->netmask = localSubnetMask;
			route->selfRoute = TRUE;
			route->metric = 1;
			route->seq = 1;
			route->reqId = 1;
			
			node->myRoute = route;
			ADDRESS_ASSIGN((&node->address), (&localSubnetIp) );
		}
	}
#if 0
	if (!_route_dev_init(node))
	{
		printk("Route Device failed\n");
		return (-EPERM);
	}

	if(!aodv_node_init(node) )
	{
		printk("AODV node failed\n");
		return (-EPERM);
	}
#endif

	while( proc->read )
	{
		sprintf(proc_name, "%s/%s", ROUTE_PROC_NAME, proc->name);
		proc->proc = create_proc_read_entry( proc_name, 0, NULL, proc->read, (void *)node);
		proc->proc->owner=THIS_MODULE;
		proc++;
	}

	return 0;
}


