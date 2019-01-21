/*
* $Id: aodv_route.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include "packet_queue.h"
#include <net/route.h>
#include <linux/socket.h>
#include <linux/in.h>

#include "aodv.h"

int aodv_route_valid(aodv_t *aodv, aodv_route_t * route)
{
	if (time_after(route->lifetime,  getcurrtime(aodv->epoch)) && route->route_valid)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void aodv_route_expire(aodv_t *aodv, aodv_route_t * route)
{
	//marks a route as expired
	route->lifetime = (getcurrtime(aodv->epoch) + DELETE_PERIOD);
	route->seq++;
	route->route_valid = FALSE;
}


int aodv_route_delete(aodv_t *aodv, u_int32_t target_ip)
{
	aodv_route_t *route, *tmp;
	unsigned long	flags;
	
	AODV_WRITE_LOCK(aodv->routeLock, flags);
	AODV_LIST_FOR_EACH_SAFE(route, tmp, aodv->routes, list)
	{
//		if( route->ip <= target_ip )
//		{
			if (route->ip == target_ip)
			{
				AODV_LIST_DEL(&(route->list) );
				AODV_WRITE_UNLOCK(aodv->routeLock, flags);
				kfree(route);
				route = NULL;
				return 1;
			}
//		}	
	}
	AODV_WRITE_UNLOCK(aodv->routeLock, flags);

	return 0;
}

aodv_route_t *aodv_route_create(aodv_t *aodv, uint32_t ip,aodv_route_type_t	 type)
{
	aodv_route_t *route;
	unsigned long	flags;

	/* Allocate memory for new entry */
	if ((route = (aodv_route_t *) kmalloc(sizeof(aodv_route_t), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV: Error getting memory for new route\n");
		return NULL;
	}

	route->self_route = FALSE;
	route->rreq_id = 0;
	route->metric = 0;
	route->seq = 0;
	route->ip = ip;
	route->next_hop = ip; /*lzj, 2006.03.31 */
//	route->next_hop = aodv->myIp;
	route->dev = NULL;
	route->route_valid = FALSE;
	route->netmask = aodv->broadcastIp;
	route->route_seq_valid = FALSE;
	route->type = type;

//	if (ip)
	{
		AODV_WRITE_LOCK(aodv->routeLock, flags);
		AODV_LIST_ADD_HEAD( &(route->list), aodv->routes );
		AODV_WRITE_UNLOCK( aodv->routeLock, flags);
	}
	return route;
}

int aodv_route_update(aodv_t *aodv, u_int32_t ip, u_int32_t next_hop_ip, u_int8_t metric, u_int32_t seq, struct net_device *dev)
{
	aodv_route_t *route;
	unsigned long  curr_time;

	/*lock table */
	route = aodv_route_find(aodv, ip);    /* Get eprev_route->next->prev = new_route;ntry from RT if there is one */

	if (!aodv_neigh_valid(aodv, next_hop_ip))
	{
		printk(KERN_INFO "AODV: Failed to update route: %s \n", inet_ntoa(ip));
		return 0;
	}

	if ( route && seq_greater( route->seq, seq))
	{
		return 0;
	}

	if ( route && route->route_valid)
	{
		if ((seq == route->seq) && (metric >= route->metric))
		{
			return 0;
		}
	}

	if (route == NULL)
	{
		route = aodv_route_create(aodv, ip, AODV_ROUTE_AODV);
		if (route == NULL)
			return -ENOMEM;
		route->ip = ip;
	}

	if (route->self_route)
	{
		printk("updating a SELF-ROUTE!!! %s hopcount %d\n", inet_ntoa(next_hop_ip), metric);
		if (!route->route_valid)
			printk("because route was invalid!\n");

		if (!route->route_seq_valid)
			printk("because seq was invalid!\n");
		if (seq_greater(seq, route->seq))
			printk("because seq of route was lower!\n");
		if ((seq == route->seq) && (metric < route->metric))
			printk("becase seq same but hop lower!\n");
	}

	/* Update values in the RT entry */
	route->seq = seq;
	route->next_hop = next_hop_ip;
	route->metric = metric;
	route->dev =   dev;
	route->route_valid = TRUE;
	route->route_seq_valid = TRUE;

	aodv_delete_kroute(route);
	aodv_insert_kroute(route);

	ipq_send_ip(ip);

	curr_time = getcurrtime(aodv->epoch);  /* Get current time */
	/* Check if the lifetime in RT is valid, if not update it */

	route->lifetime =  curr_time +  ACTIVE_ROUTE_TIMEOUT;
	return 0;
}

inline int aodv_route_compare(aodv_route_t * route, u_int32_t destIp)
{
//	if ((route->ip & route->netmask) == (destIp & route->netmask))
	if ( route->ip  == destIp )
		return 1;
	else
		return 0;
}

aodv_route_t *aodv_route_find(aodv_t *aodv, unsigned long destIp)
{
	aodv_route_t 		*route;

	unsigned long  curr_time = getcurrtime(aodv->epoch);

	AODV_READ_LOCK(aodv->routeLock);
	AODV_LIST_FOR_EACH(route, aodv->routes, list)
	{
//		if( route->ip <= target_ip )
		{
			if (time_before( route->lifetime, curr_time) && (!route->self_route) && (route->route_valid))
			{
				aodv_route_expire(aodv, route );
			}

			if ( aodv_route_compare(route, destIp))
			{/* destIp belone to this route, so return this route */
#if 0//AODV_DEBUG			
				char msg[1024];
				int len = 0;
				len += sprintf(msg+len, "it looks like the route %s ",inet_ntoa( route->ip));
				len += sprintf(msg+len, "is equal to: %s\n",inet_ntoa(destIp));
				printk("%s", msg);
#endif				
				AODV_READ_UNLOCK(aodv->routeLock);
				return route;
			}
		}
	}
	AODV_READ_UNLOCK(aodv->routeLock);

	return NULL;//possible;
}

int find_metric(u_int32_t tmp_ip)
{
	return 1;
}


int aodv_pdu_error_send(aodv_t *aodv, u_int32_t brk_dst_ip)
{
	aodv_route_t 		*route;
	
	aodv_pdu_error 	*errpdu;
	int 				errSize;
	rerr_route 		*dead = NULL;
	aodv_dest_t 		*errDest = NULL;
	void 			*data;

	rerr_route 		*badRoutes = NULL;
	rerr_route 		*errRoute = NULL;

	int numRoutes = 0;

	//    int rerrhdr_created = 0;
	AODV_READ_LOCK(aodv->routeLock);
	AODV_LIST_FOR_EACH(route, aodv->routes, list)
	{
		if (( route->next_hop == brk_dst_ip)  && !route->self_route) //&& valid_aodv_route(tmp_route)
		{
			if (( errRoute = (rerr_route *) kmalloc(sizeof(rerr_route), GFP_ATOMIC)) == NULL)
			{
				printk(KERN_WARNING "AODV: RERR Can't allocate new entry\n");
				AODV_READ_UNLOCK(aodv->routeLock);
				return 0;
			}
			
			errRoute->next = badRoutes;
			badRoutes = errRoute;
			
			errRoute->ip = route->ip;
			errRoute->seq = htonl( route->seq);
			numRoutes++;
			if ( route->route_valid)
				aodv_route_expire(aodv, route);
			printk(KERN_INFO "RERR: Broken link as next hop for - %s \n", inet_ntoa( route->ip));
		}
		//move on to the next entry
	}
	
	if( numRoutes == 0)
	{
		AODV_READ_UNLOCK(aodv->routeLock);
		return 0;
	}	
	
	errSize = sizeof(aodv_pdu_error) + (sizeof(aodv_dest_t)*numRoutes);
	if ((data = kmalloc(errSize, GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV: Error creating packet to send RERR\n");
		AODV_READ_UNLOCK(aodv->routeLock);
		return -ENOMEM;
	}

	errpdu = (aodv_pdu_error *) data;
	errpdu->type = 3;
	errpdu->dst_count = numRoutes;
	errpdu->reserved = 0;
	errpdu->n = 0;

	//add in the routes that have broken...
	errDest = (aodv_dest_t *) ((void *)data + sizeof(aodv_pdu_error));
	while ( errRoute)
	{
		errDest->ip = errRoute->ip;
		errDest->seq = errRoute->seq;
		dead = errRoute;     
		errRoute = errRoute->next;
		kfree(dead );
		errDest = (void *)errDest + sizeof(aodv_dest_t);
	}

	aodv_local_broadcast(aodv,NET_DIAMETER, data, errSize);
	kfree(data);
	AODV_READ_UNLOCK(aodv->routeLock);
	return 0;
}

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


int aodv_insert_kroute(aodv_route_t *route)
{
	struct rtentry *krtentry;
	mm_segment_t oldfs;
	int error;

	if (( krtentry = __create_kroute_entry(route->ip, route->next_hop, route->netmask, route->dev->name )) == NULL)
	{
		return -1;
	}
	
	oldfs = get_fs();
	set_fs(get_ds());
	error = ip_rt_ioctl(SIOCADDRT, (char *) krtentry);
	set_fs(oldfs);

	if (error<0)
	{
#if AODV_DEBUG	
		printk("error %d trying to insert route dest IP %s ",error, inet_ntoa(route->ip) );
		printk("Next Hop %s ", inet_ntoa(route->next_hop) );
		printk("Netmask %s, Device %s\n", inet_ntoa(route->netmask),  route->dev->name);
#endif

	}
	kfree(krtentry);

	return 0;
}

int aodv_delete_kroute(aodv_route_t *route)
{
	struct rtentry *krtentry;
	mm_segment_t oldfs;
	int error;

	if (( krtentry = __create_kroute_entry( route->ip, 0, route->netmask, NULL)) == NULL)
	{
		printk("error : trying to delete route %s ", inet_ntoa(route->ip) );
		printk("through %s\n", inet_ntoa(route->next_hop));
		return 1;
	}

	oldfs = get_fs();
	set_fs(KERNEL_DS);
//	while (ip_rt_ioctl(SIOCDELRT, (char *) krtentry) > 0);
	error = ip_rt_ioctl(SIOCDELRT, (char *) krtentry);
	set_fs(oldfs);

	if (error<0)
	{
#if AODV_DEBUG	
		printk("AODV : error %d trying to delete route %s ", error, inet_ntoa(route->ip) );
		printk("through %s\n", inet_ntoa(route->next_hop));
#endif		
	}	
	kfree( krtentry);
	return 0;
}

