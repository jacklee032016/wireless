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
#include "_ip_aodv.h"

char 	*paramDevName;

static ROUTE_LIST_HEAD(ipRouteDevs);

static route_dev_t *_create_ip_route_dev( struct net_device *dev)
{
	route_dev_t 		*routeDev= NULL;
	struct in_ifaddr 	*ifa;
	struct in_device 	*in_dev;
#if ROUTE_DEBUG			
	char 			netmask[16];
#endif

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
			MALLOC(routeDev, (route_dev_t *), sizeof(route_dev_t), M_NOWAIT|M_ZERO, "ROUTE DEVICE" );
		
			routeDev->address.address = ifa->ifa_address;
			routeDev->netmask = ifa->ifa_mask;
//			routeDev->myroute = NULL;
			routeDev->dev = dev;

		//	skb_queue_head_init(&(dev->outputq) );
					
			strncpy(routeDev->name, dev->name, strlen(dev->name)>IFNAMSIZ?IFNAMSIZ:strlen(dev->name) );

		//	aodv_insert_kroute( route);
#if ROUTE_DEBUG			
			strcpy(netmask, inet_ntoa(routeDev->netmask & routeDev->address.address));
			printk(KERN_INFO "INTERFACE LIST: Adding interface: %s  IP: %s Subnet: %s\n", dev->name, inet_ntoa(ifa->ifa_address), netmask);
#endif
		}
	}

	endfor_ifa(in_dev);
	read_unlock(&in_dev->lock);
	
	return routeDev;
}

static int __init ip_route_init()
{
	route_dev_t		*routeDev;
	struct net_device 	*dev;
	char *name = paramDevName;
	char	*str;
	int count = 0;

	if (paramDevName==NULL)
	{
		printk("You need to specify which device to use, ie:\n");
		printk("    insmod node paramDevName=ath0    \n");
       	return(-1);
	}

	init_packet_queue();

	aodv_netfilter_init();


	while(name)
	{
		str = strchr(name, ':');
		if(str)
		{
			(*str)='\0';
			str++;
		}
		read_lock(&dev_base_lock);
		read_lock(&inetdev_lock);
		for (dev = dev_base; dev!=NULL; dev = dev->next)
		{
			if(strcmp(dev->name, name)==0 )//|| strcmp(dev->name, "lo") == 0 )
			{
				routeDev = _create_ip_route_dev(dev);
				if(!routeDev)
				{
					printk("Net Device %s can not be used as route device!Maybe it is not a validate net device name or IP address not configed for it.\n", name);
					return (-ENXIO);
				}
				count ++;
				routeDev->ops = &ip_dev_ops;

				if(route_register_device(routeDev) )
				{
					printk("Route Device %s can not be registered in Mesh Route Engine!\n", routeDev->name);
					return (-ENXIO);
				}
				ROUTE_LIST_ADD_HEAD(&(routeDev->mylist), &ipRouteDevs);
			}
		}
		read_unlock(&inetdev_lock);
		read_unlock(&dev_base_lock);
		name = str;
//		str = strchr(name, ':');
	}
	
	if(count ==0)
	{
		printk("Unable to locate device: %s!\n", paramDevName );
		return (-ENXIO);
	}
	
	printk("%d IP Layer Route Device added into Mesh Route Engine\n", count);
	return 0;
}

static void __exit ip_route_cleanup()
{
	route_dev_t	*dev, *d2;

	ROUTE_LIST_FOR_EACH_SAFE(dev, d2, &ipRouteDevs, mylist)
	{
		route_unregister_device(dev);
		ROUTE_LIST_DEL(&(dev->mylist) );
		kfree(dev);
		dev = NULL;
	}

	printk("AODV : Unregister NetFilter hooks...\n");
	aodv_netfilter_destroy();

	cleanup_packet_queue();
}

module_init( ip_route_init);
module_exit( ip_route_cleanup );

MODULE_PARM(paramDevName,		"s");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Li Zhijie");
MODULE_DESCRIPTION("HWMP for Wireless Mesh MAC Layer");

