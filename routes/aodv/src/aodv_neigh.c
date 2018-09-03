/*
* $Id: aodv_neigh.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include <linux/ip.h>
#include <linux/udp.h>

#include "aodv.h"

#ifdef LINK_LIMIT
extern int g_link_limit;
#endif

aodv_neigh_t *aodv_neigh_find(aodv_t *aodv, u_int32_t target_ip)
{
	aodv_neigh_t 		*neigh; 

	AODV_READ_LOCK(aodv->neighLock);
	AODV_LIST_FOR_EACH(neigh, aodv->neighs, list)
	{
		if ( neigh->ip <= target_ip )
		{
			if ( neigh->ip == target_ip)
			{
				AODV_READ_UNLOCK(aodv->neighLock);
				return neigh;
			}
		}
	}
	AODV_READ_UNLOCK(aodv->neighLock);
	return NULL;
}

/* 1 : valid, 0 : */
int aodv_neigh_valid(aodv_t *aodv, u_int32_t target_ip)
{
	aodv_neigh_t * neigh;

	neigh = aodv_neigh_find(aodv, target_ip);
	if ( neigh)
	{
		if ( neigh->valid_link && time_before(getcurrtime(aodv->epoch), neigh->lifetime))
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

aodv_neigh_t *aodv_neigh_find_by_hw(aodv_t *aodv, char *hw_addr)
{
	aodv_neigh_t 		*neigh; 

	AODV_READ_LOCK(aodv->neighLock);
	AODV_LIST_FOR_EACH(neigh, aodv->neighs, list)
	{
		if (!memcmp(&( neigh->hw_addr), hw_addr, ETH_ALEN))
		{
			AODV_READ_UNLOCK(aodv->neighLock);
			return neigh;
		}	
	}
	
	AODV_READ_UNLOCK(aodv->neighLock);
	return NULL;
}

/* need to merge some logic in these 2 functions */
void aodv_neigh_update_link(aodv_t *aodv, char *hw_addr, u_int8_t link)
{
	aodv_neigh_t 		*neigh; 
	unsigned long		flags;

	AODV_WRITE_LOCK(aodv->neighLock, flags);
	AODV_LIST_FOR_EACH(neigh, aodv->neighs, list)
	{
		if (!memcmp(&( neigh->hw_addr), hw_addr, ETH_ALEN))
		{
			if (link)
				neigh->link = 0x100 - link;
			else
				neigh->link = 0;
			break;
		}
	}
	AODV_WRITE_UNLOCK(aodv->neighLock, flags);
}

int aodv_neigh_delete(aodv_t *aodv, u_int32_t ip)
{
	aodv_neigh_t 		*neigh, *tmp; 
	aodv_route_t		*route;
	unsigned long		flags;

	AODV_WRITE_LOCK(aodv->neighLock, flags);
	AODV_LIST_FOR_EACH_SAFE(neigh, tmp, aodv->neighs, list)
	{
		if ( neigh->ip == ip)
		{
			AODV_LIST_DEL(&(neigh->list) );

			kfree( neigh);
			neigh = NULL;
			aodv_timer_delete(aodv, ip, AODV_TASK_NEIGHBOR);
			aodv_timer_update(aodv);
			AODV_WRITE_UNLOCK( aodv->neighLock, flags);

			route = aodv_route_find(aodv, ip);
			if ( route && ( route->next_hop == route->ip))
			{
				aodv_delete_kroute(route );
				aodv_pdu_error_send(aodv, route->ip);
				if ( route->route_valid)
				{
					aodv_route_expire(aodv, route);
				}
				else
				{
					aodv_route_delete(aodv, route->ip);
				}
			}
			return 0;
		}
	}

	AODV_WRITE_UNLOCK( aodv->neighLock, flags);

	return -ENODATA;
}

aodv_neigh_t *aodv_neigh_insert(aodv_t *aodv, u_int32_t ip)
{
	aodv_neigh_t *neigh = NULL;
	unsigned long	flags;

#if AODV_DEBUG
	printk("AODV : create neight with IP : %s\n", inet_ntoa(ip));
#endif
	neigh = aodv_neigh_find(aodv, ip);
	if ( neigh )
	{
		printk(KERN_WARNING "AODV NEIGH : Creating a duplicate neighbor\n");
		return NULL;
	}

	if ((neigh = kmalloc(sizeof(aodv_neigh_t), GFP_ATOMIC)) == NULL)
	{  
		printk(KERN_WARNING "AODV NEIGHBOR : Can't allocate new entry\n");
		return NULL;
	}

	neigh->ip = ip;
	neigh->lifetime = 0;
	neigh->route_entry = NULL;
	neigh->link = 0;
	
#ifdef LINK_LIMIT
	neigh->valid_link = FALSE;
#else
	neigh->valid_link = TRUE;
#endif

	AODV_WRITE_LOCK( aodv->neighLock, flags);
	AODV_LIST_ADD_HEAD(&(neigh->list), aodv->neighs );
	AODV_WRITE_UNLOCK(aodv->neighLock, flags);

	return neigh;
}

void aodv_neigh_update_route(aodv_t *aodv, aodv_neigh_t *neigh, aodv_pdu_reply *reply)
{
	aodv_route_t *route;

	route = aodv_route_find(aodv, neigh->ip);
	if (route)
	{
/*				if (!tmp_route->route_valid)
				{
			    insert_kernel_route_entry(tmp_route->ip, tmp_route->next_hop, tmp_route->netmask,tmp_route->dev->name);
				}

				if (tmp_route->next_hop!=tmp_route->ip)
				{*/
		aodv_delete_kroute(route);
		aodv_insert_kroute(route);
				//}
	}
	else
	{	    
		printk(KERN_INFO "AODV: Creating route for neighbor: %s Link: %d\n", inet_ntoa( neigh->ip), neigh->link);
		route = aodv_route_create(aodv, neigh->ip, AODV_ROUTE_AODV);
		aodv_delete_kroute(route );
		aodv_insert_kroute( route);
	}

	route->lifetime = getcurrtime(aodv->epoch) + reply->lifetime;
	route->dev = neigh->dev;
	route->next_hop = neigh->ip;
	route->route_seq_valid = TRUE;
	route->route_valid = TRUE;
	route->seq = reply->dst_seq;

	neigh->route_entry = route;
	route->metric = find_metric( route->ip);
}

int aodv_neigh_update(aodv_t *aodv, aodv_neigh_t *neigh, aodv_pdu_reply *reply)
{
	if (neigh==NULL)
	{
		printk(KERN_WARNING "AODV: NULL neighbor passed!\n");
		return 0;
	}
#ifdef LINK_LIMIT
	if (neigh->link > (g_link_limit))
	{
		neigh->valid_link = TRUE;
	}
	/*if (tmp_neigh->link < (g_link_limit - 5))
	{
		tmp_neigh->valid_link = FALSE;
	}*/
#endif

	if (neigh->valid_link)
	{
		aodv_neigh_update_route(aodv, neigh, reply);
	}

	return 1;
}


