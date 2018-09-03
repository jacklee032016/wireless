/*
* $Id: route_engine.c,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
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
static route_dev_t *_engine_find_dev_by_net(route_engine_t *engine, struct net_device * dev)
{
	route_dev_t 		*routeDev;
	struct in_device 	*indev;
	u_int32_t 		ip;

	if (dev == NULL)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_DEV,  "No Net Device is provided, %s!\n", __FUNCTION__);
		return NULL;
	}
	if (dev->ip_ptr == NULL)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_DEV, "No IP Addres has configed for Net Device %s\n", dev->name);
		return NULL;
	}
	
	indev = (struct in_device *) dev->ip_ptr;
	if ( indev->ifa_list == NULL)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_DEV, "ERROR, Net Device %s has no ifa_list\n", dev->name);
		return NULL;
	}
	ip = indev->ifa_list->ifa_address;

	ROUTE_LIST_FOR_EACH(routeDev, engine->devs, list)
	{
		if ( routeDev->ip == (u_int32_t) ip)
			return routeDev;
	}
/*
	// for POST_ROUTE filter, all net_device is check
	ROUTE_DPRINTF(engine, ROUTE_DEBUG_DEV,  "Failed search for matching interface for: %s which has an ip of: %s\n", dev->name, inet_ntoa(ip));
*/	
	return NULL;
}

/* check destIp is in one local subnet of my engine device */
static route_dev_t* _engine_find_dev_by_ip(route_engine_t *engine, u_int32_t destIp)
{
	route_dev_t 		*routeDev = NULL;

	ROUTE_LIST_FOR_EACH(routeDev, engine->devs, list)
	{
		if ((routeDev->ip & routeDev->netmask)  == (routeDev->netmask & destIp ))
			return routeDev;
	}

	return NULL;
}


/* check destIp is in one local subnet of my engine device */
static int _engine_check_subnet(route_engine_t *engine, u_int32_t destIp)
{
	route_dev_t 		*routeDev;

	ROUTE_LIST_FOR_EACH(routeDev, engine->devs, list)
	{
		if ((routeDev->ip & routeDev->netmask)  == (routeDev->netmask & destIp ))
			return 1;
	}

	return 0;
}

static int _engine_unicast(route_engine_t *engine, u_int32_t destip, u_int8_t ttl, void *data, const size_t datalen )
{
	route_dev_t			*routeDev;
	int 					length = 0;

	if (ttl == 0)
		return 0;

	ROUTE_LIST_FOR_EACH(routeDev, engine->devs, list)
	{
		if( (routeDev->ip&routeDev->netmask)==(destip&routeDev->netmask) )
		{/* most match, shpuld be enhanced, lzj */
			length = routeDev->ops->unicast(routeDev, destip, ttl, data, datalen);
		}	
	}

	return length;
}

int _engine_broadcast(route_engine_t *engine, u_int8_t ttl, void *data,const size_t datalen)
{
	route_dev_t 	*routeDev;
	int 			length = 0;

	if (ttl < 1)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_ENGINE, "TTL error : ttl=%d\n", ttl);
		return 0;
	}

	if(ROUTE_LIST_CHECK_EMPTY(engine->devs) )
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_ENGINE, "Error, no Route Device is attached when try broadcast, %s\n" , __FUNCTION__);
		return 0;
	}

	ROUTE_LIST_FOR_EACH(routeDev, engine->devs, list)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_DEV, "broadcast on %s\n", routeDev->name);
		length += routeDev->ops->broadcast(routeDev, ttl, data, datalen);
	}

	return length;
}

int engine_init(route_engine_t *engine, char *localSubnet)
{
	char* 			str;
	u_int32_t 		localSubnetIp=0;
	u_int32_t 		localSubnetMask=0;
	unsigned char 	proc_name[32];
	route_proc_entry 	*proc = engine->procs;
	mesh_route_t 		*route;
#if 0	
	u_int32_t			lo;
#endif

	inet_aton("255.255.255.255",&(engine->broadcastIp) );
	
	engine->epoch = getAodvEpoch();

	engine->check_subnet = _engine_check_subnet;
	engine->dev_lookup = _engine_find_dev_by_net;
	engine->dev_lookup_by_ip = _engine_find_dev_by_ip;
	engine->unicast = _engine_unicast;
	engine->broadcast = _engine_broadcast,
	
	init_timer(&(engine->timer) );

#if 0
	inet_aton("127.0.0.1", &lo);
	route = engine_route_create(engine, lo, ROUTE_ROUTE_KERNEL);
	route->nextHop = lo;
	route->metric = 0;
	route->self_route = TRUE;
	route->route_valid = TRUE;
	route->seq = 0;
#endif

	if ( localSubnet==NULL)
		return -ENXIO;
	else
	{
		str = strchr(localSubnet,'/');
		if ( str)
		{
			(*str)='\0';
			str++;
			ROUTE_DPRINTF(engine,  ROUTE_DEBUG_INIT, "Adding Local Routed Subnet %s/%s to the routing table...\n", localSubnet, str);

			inet_aton(localSubnet, &localSubnetIp);
			inet_aton(str, &localSubnetMask);

			route = engine->mgmtOps->route_insert(engine, localSubnetIp, ROUTE_TYPE_AODV);
			route->route_valid = TRUE;
			route->route_seq_valid = TRUE;
			route->netmask = localSubnetMask;
			route->self_route = TRUE;
			route->metric = 1;
			route->seq = 1;
			route->rreq_id = 1;
			
			engine->myRoute = route;
			engine->myIp = route->ip;
		}
	}
#if 0
	if (!_route_dev_init(engine))
	{
		printk("Route Device failed\n");
		return (-EPERM);
	}

	if(!aodv_engine_init(engine) )
	{
		printk("AODV engine failed\n");
		return (-EPERM);
	}
#endif

	while( proc->read )
	{
		sprintf(proc_name, "%s/%s", ROUTE_PROC_NAME, proc->name);
		proc->proc = create_proc_read_entry( proc_name, 0, NULL, proc->read, (void *)engine);
		proc->proc->owner=THIS_MODULE;
		proc++;
	}

	return 0;
}


