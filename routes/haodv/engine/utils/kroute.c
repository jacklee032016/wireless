/*
* $Id: kroute.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/

#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/spinlock.h>
#include <linux/rtnetlink.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <net/route.h>
#include <net/sock.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#include "mesh_route.h"

static struct rtentry *__create_kroute_entry(u_int32_t dst_ip, u_int32_t gw_ip, u_int32_t genmask_ip, char *interf)
{
	struct rtentry *krtentry;
	static struct sockaddr_in dst;
	static struct sockaddr_in gw;
	static struct sockaddr_in genmask;

	if (( krtentry = kmalloc(sizeof(struct rtentry), GFP_ATOMIC)) == NULL)
	{
		printk("KRTABLE: Gen- Error generating a route entry\n");
		return NULL;            /* Malloc failed */
	}

	dst.sin_family = AF_INET;
	gw.sin_family = AF_INET;
	genmask.sin_family = AF_INET;

	//    dst.sin_addr.s_addr = dst_ip;
	dst.sin_addr.s_addr = dst_ip & genmask_ip;

	//JPA
	/*    if (gw_ip == dst_ip)
	gw.sin_addr.s_addr = 0;
	else*/
	gw.sin_addr.s_addr = gw_ip;
	//gw.sin_addr.s_addr = 0;

	//JPA
	//    genmask.sin_addr.s_addr=g_broadcast_ip;
	genmask.sin_addr.s_addr = genmask_ip;

	//JPA
	krtentry->rt_flags = RTF_UP | RTF_HOST | RTF_GATEWAY;
	/*    new_rtentry->rt_flags = RTF_UP;
	//JPA
	if (gw_ip != dst_ip)
	new_rtentry->rt_flags |= RTF_GATEWAY;
	*/

	krtentry->rt_metric = 0;
	krtentry->rt_dev = interf;

	/* bug ???,lzj */
	krtentry->rt_dst = *(struct sockaddr *) &dst;
	krtentry->rt_gateway = *(struct sockaddr *) &gw;
	krtentry->rt_genmask = *(struct sockaddr *) &genmask;

	return krtentry;
}


int aodv_insert_kroute(mesh_route_t *route)
{
	struct rtentry *krtentry;
	mm_segment_t oldfs;
	int error;

	if (( krtentry = __create_kroute_entry(route->address.address, route->nextHop->address.address, route->netmask, route->dev->name )) == NULL)
	{
		return -1;
	}
	
	oldfs = get_fs();
	set_fs(get_ds());
	error = ip_rt_ioctl(SIOCADDRT, (char *) krtentry);
	set_fs(oldfs);

	if (error<0)
	{
#if ROUTE_DEBUG	
		printk("error %d trying to insert route dest IP %s ",error, inet_ntoa(route->address.address) );
		printk("Next Hop %s ", inet_ntoa(route->nextHop->address.address) );
		printk("Netmask %s, Device %s\n", inet_ntoa(route->netmask),  route->dev->name);
#endif

	}
	kfree(krtentry);

	return 0;
}

int aodv_delete_kroute(mesh_route_t *route)
{
	struct rtentry *krtentry;
	mm_segment_t oldfs;
	int error;

	if (( krtentry = __create_kroute_entry( route->address.address, 0, route->netmask, NULL)) == NULL)
	{
		printk("error : trying to delete route %s ", inet_ntoa(route->address.address) );
		printk("through %s\n", inet_ntoa(route->nextHop->address.address));
		return 1;
	}

	oldfs = get_fs();
	set_fs(KERNEL_DS);
//	while (ip_rt_ioctl(SIOCDELRT, (char *) krtentry) > 0);
	error = ip_rt_ioctl(SIOCDELRT, (char *) krtentry);
	set_fs(oldfs);

	if (error<0)
	{
#if ROUTE_DEBUG	
		printk("AODV : error %d trying to delete route %s ", error, inet_ntoa(route->address.address) );
		printk("through %s\n", inet_ntoa(route->nextHop->address.address));
#endif		
	}	
	kfree( krtentry);
	return 0;
}

