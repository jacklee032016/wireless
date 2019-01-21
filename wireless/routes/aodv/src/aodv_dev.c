/*
* $Id: aodv_dev.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/

#include <linux/types.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/string.h>

#include "aodv.h"

static aodv_dev_t *__create_aodv_dev(aodv_t *aodv, struct net_device *netdev, struct in_ifaddr *ifa)
{
	aodv_dev_t 	*dev;
	aodv_route_t 	*route;

	if ((dev = (aodv_dev_t *) kmalloc(sizeof(aodv_dev_t), GFP_ATOMIC)) == NULL)
	{
		/* Couldn't create a new entry in the routing table */
		printk(KERN_WARNING "AODV: Not enough memory to create Route Table Entry\n");
		return NULL;
	}

	route = aodv_route_create(aodv, ifa->ifa_address, AODV_ROUTE_KERNEL);
	if(route==NULL)
	{
		kfree(dev);
		dev = NULL;
		return NULL;
	}

#if AODV_DEBUG
	printk(KERN_INFO "AODV device IP :  %s\n",  inet_ntoa(ifa->ifa_address));
#endif
	route->ip = ifa->ifa_address;
	//route->netmask = calculate_netmask(0); //ifa->ifa_mask;
	route->self_route = TRUE;
	route->seq = 1;
	route->old_seq = AODV_SEQUENCE_UNKNOWN;
	route->rreq_id = 1;
	route->metric = 0;
	route->next_hop = route->ip;
	route->lifetime = -1;
	route->route_valid = TRUE;
	route->route_seq_valid = 1;
	route->dev = netdev;
	
	dev->ip = route->ip;
	dev->netmask = ifa->ifa_mask;
	dev->route_entry = route;
	dev->dev = netdev;
	strncpy(dev->name, netdev->name, strlen(netdev->name)>IFNAMSIZ?IFNAMSIZ:strlen(netdev->name) );

	aodv_insert_kroute( route);

	return dev;
}

static int __insert_aodv_dev(aodv_t *aodv, struct net_device *dev)
{
	aodv_dev_t 		*aodvDev;
	int success=0, error=0;
	struct in_ifaddr *ifa;

	struct in_device *in_dev;
	char netmask[16];

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
#if AODV_DEBUG	
	printk("%s is added\r\n", dev->name);
#endif
//		if ((strcmp(aodv->devName,"")!=0) && (strncmp(dev->name, aodv->devName, IFNAMSIZ) == 0))
		{
			if (ifa==NULL)
			{
				return success;
			}

			aodvDev  = __create_aodv_dev(aodv, dev, ifa);
			strcpy(netmask, inet_ntoa(aodvDev->netmask & aodvDev->ip));
#if AODV_DEBUG			
			printk(KERN_INFO "INTERFACE LIST: Adding interface: %s  IP: %s Subnet: %s\n", dev->name, inet_ntoa(ifa->ifa_address), netmask);
#endif

			strncpy(aodvDev->name, dev->name, IFNAMSIZ);
			
			aodv->myRoute = aodvDev->route_entry;

			AODV_LIST_ADD_HEAD(&(aodvDev->list) , aodv->devs );
			
			error = sock_create(PF_INET, SOCK_DGRAM, 0, &(aodvDev->sock));
			if (error < 0)
			{
				kfree(aodvDev);
				read_unlock(&in_dev->lock);
				printk(KERN_ERR "Error during creation of socket; terminating, %d\n", error);
				return error;
			}

			init_sock(aodvDev->sock, aodvDev->ip, dev->name);
			success++;
		}
	}

	endfor_ifa(in_dev);
	read_unlock(&in_dev->lock);
	return success;
}

/* lookup net_device and create route for our AODV */
int aodv_dev_init(aodv_t *aodv)
{
	struct net_device *dev;
	int count = 0;
	char		*str, *name =aodv->devName;

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
				count += __insert_aodv_dev(aodv, dev);
		}
		read_unlock(&inetdev_lock);
		read_unlock(&dev_base_lock);
		name = str;
//		str = strchr(name, ':');
	}
	if(count ==0)
	{
		printk("Unable to locate device: %s!\n", aodv->devName );
		return 0;
	}
	
	return count;
}

/*  find the first ip address associated with this device */
static u_int32_t find_dev_ip(struct net_device * dev)
{
	struct in_device *indev;

	//make sure we get a valid DEV
	if (dev == NULL)
	{
		printk(KERN_WARNING "AODV: FIND_DEV_IP gotta NULL DEV! ");
		return -EFAULT;
	}

	//make sure that dev has an IP section
	if (dev->ip_ptr == NULL)
	{
		printk(KERN_WARNING "AODV: FIND_DEV_IP gotta NULL ip_ptr!! ");
		return -EFAULT;
	}

	indev = (struct in_device *) dev->ip_ptr;
	if ( indev && (indev->ifa_list != NULL))
		return ( indev->ifa_list->ifa_address);
	else
		return 0;
}

/* why not use the net_device data member in AODV_DEVICE */
aodv_dev_t *aodv_dev_find_by_net(aodv_t *aodv, struct net_device * dev)
{
	aodv_dev_t 		*aodvDev;
	struct in_device 	*indev;
	u_int32_t 		ip;

	if (dev == NULL)
	{
		printk(KERN_WARNING "AODV: FIND_INTERFACE_BY_DEV gotta NULL DEV! \n");
		return NULL;
	}
	if (dev->ip_ptr == NULL)
	{
		printk(KERN_WARNING "AODV: FIND_INTERFACE_BY_DEV gotta NULL ip_ptr!! \n");
		return NULL;
	}
	
	indev = (struct in_device *) dev->ip_ptr;
	if ( indev->ifa_list == NULL)
	{
		printk(KERN_WARNING "AODV: FIND_INTERFACE_BY_DEV gotta NULL ifa_list!! \n");
		return NULL;
	}
	ip = indev->ifa_list->ifa_address;

	AODV_LIST_FOR_EACH(aodvDev, aodv->devs, list)
	{
		if ( aodvDev->ip == (u_int32_t) ip)
			return aodvDev;
	}

	printk(KERN_WARNING "AODV: Failed search for matching interface for: %s which has an ip of: %s\n", dev->name, inet_ntoa(ip));
	return NULL;
}


/* check destIp is in one local subnet of my aodv device */
int aodv_dev_local_check(aodv_t *aodv, u_int32_t destIp)
{
	aodv_dev_t 		*aodvDev;

	AODV_LIST_FOR_EACH(aodvDev, aodv->devs, list)
	{
		if ((aodvDev->ip & aodvDev->netmask)  == (aodvDev->netmask & destIp ))
			return 1;
	}

	return 0;
}

