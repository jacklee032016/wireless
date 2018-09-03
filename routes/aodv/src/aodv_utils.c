/*
* $Id: aodv_utils.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include "aodv.h"

#ifdef AODV_SIGNAL
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
int aodv_iw_set_spy(aodv_t *aodv)
{
	int errno;
	int i;
	aodv_neigh_t *neigh;
	struct sockaddr iw_sa[IW_MAX_SPY];
	struct iwreq wrq;
	aodv_dev_t *dev;
	
#if WIRELESS_EXT > 15
	iw_handler handler;
	struct iw_request_info info;
#endif
    
//	mm_segment_t oldfs;
	AODV_LIST_FOR_EACH(dev, aodv->devs, list)
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

		AODV_LIST_FOR_EACH(neigh, aodv->neighs, list)
		{
			if ( dev->dev == neigh->dev)
			{
				if (i < IW_MAX_SPY)
				{
					memcpy((char *) &(iw_sa[i].sa_data), (char *) &(neigh->hw_addr), sizeof(struct sockaddr));
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

void aodv_iw_get_stats(aodv_t *aodv)
{
	int n, i, errno = 0;
	char buffer[(sizeof(struct iw_quality) + sizeof(struct sockaddr)) * IW_MAX_SPY];
	struct iwreq wrq;
	aodv_dev_t *dev;
	
#if WIRELESS_EXT > 15    
	iw_handler handler;
	struct iw_request_info info;   
#endif	
		
	struct sockaddr hwa[IW_MAX_SPY];
	struct iw_quality qual[IW_MAX_SPY];
 
	AODV_LIST_FOR_EACH(dev, aodv->devs, list)
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
				aodv_neigh_update_link(aodv, hwa[i].sa_data, (u_int8_t) qual[i].noise - qual[i].level);//level);// - 0x100);
			}
		}
	}
}

#endif

static struct sockaddr_in sin;
extern u_int32_t g_broadcast_ip;

int init_sock(struct socket *sock, u_int32_t ip, char *dev_name)
{
	int error;
	struct ifreq interface;
	mm_segment_t oldfs;

	//set the address we are sending from
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = ip;
	sin.sin_port = htons(AODVPORT);

	sock->sk->reuse = 1;
	sock->sk->allocation = GFP_ATOMIC;
	sock->sk->priority = GFP_ATOMIC;

	error = sock->ops->bind(sock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));
	strncpy(interface.ifr_ifrn.ifrn_name, dev_name, IFNAMSIZ);

	oldfs = get_fs();
	set_fs(KERNEL_DS);          //thank to Soyeon Anh and Dinesh Dharmaraju for spotting this bug!
	error = sock_setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (char *) &interface, sizeof(interface)) < 0;
	set_fs(oldfs);

	if (error < 0)
	{
		printk(KERN_ERR "Kernel AODV: Error, %d  binding socket. This means that some other \n", error);
		printk(KERN_ERR "        daemon is (or was a short time axgo) using port %i.\n", AODVPORT);
		return 0;
	}

	return 0;
}



int send_message(aodv_t *aodv, u_int32_t dst_ip, u_int8_t ttl, void *data, const size_t datalen, struct net_device *dev)
{
	struct msghdr msg;
	struct iovec iov;
	aodv_dev_t 	*aodvDev;
	aodv_route_t 	*route;
	mm_segment_t oldfs;
	unsigned long  curr_time;
	u_int32_t space;
	int len;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = dst_ip;
	sin.sin_port = htons((unsigned short) AODVPORT);

	//define the message we are going to be sending out
	msg.msg_name = (void *) &(sin);
	msg.msg_namelen = sizeof(sin);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
	msg.msg_iov->iov_len = (__kernel_size_t) datalen;
	msg.msg_iov->iov_base = (char *) data;

	if (ttl == 0)
		return 0;

	curr_time = getcurrtime(aodv->epoch);

	if (dev)
	{
		aodvDev =aodv_dev_find_by_net(aodv, dev);
	}
	else
	{
		route = aodv_route_find(aodv, dst_ip);
		if ( route == NULL)
		{
			printk(KERN_WARNING "AODV: Can't find route to: %s \n", inet_ntoa(dst_ip));
			return -EHOSTUNREACH;
		}
		aodvDev = aodv_dev_find_by_net( aodv, route->dev);
	}

	if ( aodvDev == NULL)
	{
		printk(KERN_WARNING "AODV: Error sending! Unable to find interface!\n");
		return -ENODEV;
	}

	space = sock_wspace(aodvDev->sock->sk);
	if (space < datalen)
	{
		printk(KERN_WARNING "AODV: Space: %d, Data: %d \n", space, datalen);
		return -ENOMEM;
	}

	aodvDev->sock->sk->broadcast = 0;
	aodvDev->sock->sk->protinfo.af_inet.ttl = ttl;
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	len = sock_sendmsg(aodvDev->sock, &msg,(size_t) datalen);
	if (len < 0)
	{
		printk(KERN_WARNING "AODV: Error sending! err no: %d, Dst: %s\n", len, inet_ntoa(dst_ip));
	}
	set_fs(oldfs);
	return 0;
}

int aodv_local_broadcast(aodv_t *aodv, u_int8_t ttl, void *data,const size_t datalen)
{
	struct msghdr 	msg;
	struct iovec 		iov;
	aodv_dev_t 		*aodvDev;
	int				length =0;
	mm_segment_t 	oldfs;
//	unsigned long  curr_time = getcurrtime(aodv->epoch);

	if (ttl < 1)
	{
		return 0;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = aodv->broadcastIp;
	sin.sin_port = htons((unsigned short) AODVPORT);

	//define the message we are going to be sending out
	msg.msg_name = (void *) &(sin);
	msg.msg_namelen = sizeof(sin);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	//    msg.msg_flags = MSG_NOSIGNAL;
	msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
	msg.msg_iov->iov_len = (__kernel_size_t) datalen;
	msg.msg_iov->iov_base = (char *) data;

	AODV_LIST_FOR_EACH(aodvDev, aodv->devs, list)
	{

		if( (aodvDev->sock) && (sock_wspace(aodvDev->sock->sk) >= msg.msg_iov->iov_len) )
		{
			aodvDev->sock->sk->broadcast = 1;
			aodvDev->sock->sk->protinfo.af_inet.ttl = ttl;
			oldfs = get_fs();
			set_fs(KERNEL_DS);

			length = sock_sendmsg(aodvDev->sock, &(msg), (size_t)msg.msg_iov->iov_len);
			if (length < 0)
				printk(KERN_WARNING "AODV: Error sending! err no: %d,on interface: %s\n", length, aodvDev->name);
			set_fs(oldfs);
		}

	}

	return length;
}

