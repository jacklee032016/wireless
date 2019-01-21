/*
* $Id: mesh_init.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
*/

#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/netfilter.h>

#include "mesh_route.h"

extern	dev_ops_t	mac_dev_ops;


static ROUTE_LIST_HEAD(macRouteDevs);

static route_dev_t *_mesh_mac_layer_route_dev( struct net_device *dev)
{
	route_dev_t 		*routeDev= NULL;
	struct in_ifaddr 	*ifa;
	struct in_device 	*in_dev;
	char 			netmask[16];

	if ((in_dev= in_dev_get(dev)) == NULL)
	{
		return 0;
	}

	read_lock(&in_dev->lock);

	ifa = in_dev->ifa_list;
	if(ifa== NULL)
		printk("NULL\r\n");
	for_primary_ifa(in_dev)
//	for_ifa(in_dev)
	{
#if ROUTE_DEBUG	
	printk("%s is added\r\n", dev->name);
#endif
		{
			if (ifa==NULL)
			{/* no ip address configed for this net_device */
				return NULL;
			}

			if ((routeDev = (route_dev_t *) kmalloc(sizeof(route_dev_t), GFP_ATOMIC)) == NULL)
			{
				/* Couldn't create a new entry in the routing table */
				printk(KERN_WARNING "AODV: Not enough memory to create Route Table Entry\n");
				return NULL;
			}
			memset(routeDev, 0 ,sizeof(route_dev_t) );
			
			routeDev->ip = ifa->ifa_address;
			routeDev->netmask = ifa->ifa_mask;
			routeDev->myroute = NULL;
			routeDev->dev = dev;

			skb_queue_head_init(&(routeDev->outputq) );
			spin_lock_init(&routeDev->outputq.lock);
					
			strncpy(routeDev->name, dev->name, strlen(dev->name)>IFNAMSIZ?IFNAMSIZ:strlen(dev->name) );

		//	aodv_insert_kroute( route);
			strcpy(netmask, inet_ntoa(routeDev->netmask & routeDev->ip));
#if ROUTE_DEBUG			
			printk(KERN_INFO "INTERFACE LIST: Adding interface: %s  IP: %s Subnet: %s\n", routeDev->name, inet_ntoa(ifa->ifa_address), netmask);
#endif
		}
	}

	endfor_ifa(in_dev);
	read_unlock(&in_dev->lock);
	
	return routeDev;
}

int mesh_route_dev_create(struct net_device *dev)
{
	route_dev_t		*routeDev;

	routeDev = _mesh_mac_layer_route_dev(dev);
	if(!routeDev)
	{
		printk("Net Device %s can not be used as route device!Maybe it is not a validate net device name or IP address not configed for it.\n", dev->name);
		return (-ENXIO);
	}
	routeDev->ops = &mac_dev_ops;

	if(route_register_device(routeDev) )
	{
		printk("Route Device %s can not be registered in Mesh Route Engine!\n", routeDev->name);
		return (-ENXIO);
	}
	ROUTE_LIST_ADD_HEAD(&(routeDev->mylist), &macRouteDevs);
	
	printk("MAC Layer Route Device %s added into Mesh Route Engine\n", dev->name );
	return 0;
}

void mesh_route_dev_destory(struct net_device *dev)
{
	route_dev_t	*routeDev, *d2;

	ROUTE_LIST_FOR_EACH_SAFE(routeDev, d2, &macRouteDevs, mylist)
	{
		if( dev== routeDev->dev )
		{
			route_unregister_device(routeDev);
			ROUTE_LIST_DEL(&(routeDev->mylist) );
			kfree(routeDev);
			routeDev = NULL;
		}	
	}

	printk("MESH : Unregister %s\n", dev->name );
}

/* 0 : forward; others, drop or queue it */
unsigned int mesh_packet_output(struct net_device *netdev, struct sk_buff *skb)
{
	route_dev_t	*routeDev;
	route_op_type_t result;
	routeDev = route_dev_lookup( netdev );
	if(!routeDev)
	{
		printk( "Bug, No Route Device for Net Device(out) %s\n", netdev->name);
		return 3;
	}
	
	result = routeDev->ops->output_packet(routeDev, skb );
	if(result ==ROUTE_OP_DENY )
	{/* where drop it in order to free memory ?? */
		return 1;
	}
	else if(result == ROUTE_OP_QUEUE)
	{/* go ahead for next skb(mgmt or data) */
		
		skb = NULL;
		return 2;
	}
	/* continue send out */
	/*
	else if(result == ROUTE_OP_FORWARD)
	{
	}
	*/

	return 0;
}


