/*
* $Id: route_iw_utils.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/
 
#include "mesh_route.h"

#ifdef ROUTE_SIGNAL
#include <net/iw_handler.h>
#include <linux/wireless.h>
static struct socket *iw_sock;

void aodv_iw_sock_init(void)
{
	int error;

	error = sock_create(AF_INET, SOCK_DGRAM, 0, &iw_sock);
	if (error < 0)
	{
		printk(KERN_ERR "Error during creation of socket; terminating, %d\n", error);
	}
}

void aodv_iw_sock_close(void)
{
	sock_release(iw_sock);
}

/* wireless extension */
int aodv_iw_set_spy(route_node_t *node)
{
	int errno;
	int i;
	route_neigh_t *neigh;
	struct sockaddr iw_sa[IW_MAX_SPY];
	struct iwreq wrq;
	route_dev_t *dev;
	
#if WIRELESS_EXT > 15
	iw_handler handler;
	struct iw_request_info info;
#endif
    
//	mm_segment_t oldfs;
	ROUTE_LIST_FOR_EACH(dev, node->devs, list)
	{
		break;
	}
	if( dev == NULL)
		return 1;
	
#if WIRELESS_EXT > 15
	if( dev->dev->wireless_handlers )
#else
	if (( dev->dev->get_wireless_stats!=NULL) && ( dev->dev->do_ioctl!=NULL))
#endif
	{
		i = 0;

		ROUTE_LIST_FOR_EACH(neigh, node->neighs, list)
		{
			if ( dev->dev == neigh->dev)
			{
				if (i < IW_MAX_SPY)
				{
					memcpy((char *) &(iw_sa[i].sa_data), (char *) &(neigh->myHwAddr), sizeof(struct sockaddr));
					i++;
					neigh->link = 0;
				}
				else
				{
					neigh->link = 0;
				}
			}
		}

		strncpy(wrq.ifr_name, dev->name, IFNAMSIZ);
		wrq.u.data.pointer = (caddr_t) & (iw_sa);
		wrq.u.data.length = i;
		wrq.u.data.flags = 0;

#if WIRELESS_EXT > 15
		info.cmd = SIOCSIWSPY;
		info.flags = 0;

		handler = dev->dev->wireless_handlers->standard[SIOCSIWSPY - SIOCIWFIRST];
		if (handler)
		{
			errno = handler( dev->dev, &info, &(wrq.u),(char *) iw_sa);
			if (errno<0)
				printk(KERN_WARNING "AODV: Error with SIOCSIWSPY: %d\n", errno);
		}
#else
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		errno = dev->dev->do_ioctl( dev->dev, (struct ifreq *) &wrq, SIOCSIWSPY);
		set_fs(oldfs);
		if (errno < 0)
			printk(KERN_WARNING "AODV: Error with SIOCSIWSPY: %d\n", errno);
#endif
	}

	return 0;
}

void aodv_iw_get_stats(route_node_t *node)
{
	int n, i, errno = 0;
	char buffer[(sizeof(struct iw_quality) + sizeof(struct sockaddr)) * IW_MAX_SPY];
	struct iwreq wrq;
	route_dev_t *dev;
	
#if WIRELESS_EXT > 15    
	iw_handler handler;
	struct iw_request_info info;   
#endif	
		
	struct sockaddr hwa[IW_MAX_SPY];
	struct iw_quality qual[IW_MAX_SPY];

trace;
 	ROUTE_READ_LOCK(node->devLock);
	ROUTE_LIST_FOR_EACH(dev, node->devs, list)
	{
#if WIRELESS_EXT > 15
		if(dev->dev->wireless_handlers )
#else
		if (( dev->dev->get_wireless_stats!=NULL) && ( dev->dev->do_ioctl!=NULL))
#endif		
		{
			strncpy(wrq.ifr_name, dev->name, IFNAMSIZ);
			wrq.u.data.pointer = (caddr_t) buffer;
			wrq.u.data.length = 0;
			wrq.u.data.flags = 0;

#if WIRELESS_EXT > 15
			info.cmd = SIOCGIWSPY;
			info.flags = 0;
			handler = dev->dev->wireless_handlers->standard[SIOCGIWSPY - SIOCIWFIRST];
			if (handler)
			{
				errno = handler( dev->dev, &info, &(wrq.u), buffer);
				if (errno<0)
					return;
			}
#else
			oldfs = get_fs();
			set_fs(KERNEL_DS);
			errno = dev->dev->do_ioctl( dev->dev,(struct ifreq * ) &wrq,SIOCGIWSPY );
			set_fs(oldfs);

			if (errno < 0)
				return;
#endif	          	    			

			n = wrq.u.data.length;
			memcpy(hwa, buffer, n * sizeof(struct sockaddr));
			memcpy(qual, buffer + n * sizeof(struct sockaddr), n * sizeof(struct iw_quality));
			for (i = 0; i < n; i++)
			{
				aodv_neigh_update_link(node, hwa[i].sa_data, (u_int8_t) qual[i].noise - qual[i].level);//level);// - 0x100);
			}
		}
	}
	ROUTE_READ_UNLOCK(node->devLock);
	
}

#endif

