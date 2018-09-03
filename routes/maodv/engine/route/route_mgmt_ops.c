/*
* $Id: route_mgmt_ops.c,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
*/

#include <linux/proc_fs.h>
#include "mesh_route.h"
#include <linux/inetdevice.h>

#include "_route.h"

int _route_valid( mesh_route_t * route)
{
	int res = 0;
	if(!route->dev)
	{
		printk("Error : no device involved for this route, self route %d\n", route->self_route);
		return res;
	}
	
	if (time_after(route->lifetime,  getcurrtime(route->dev->engine->epoch)) && route->route_valid)
	{
		res = 1;
	}
	return res;
}

void _route_expire(mesh_route_t *route)
{
	//marks a route as expired
	if(!route->dev)
	{
		printk("Error : no device involved for this route, self route %d\n", route->self_route);
		return;
	}
	route->lifetime = (getcurrtime(route->dev->engine->epoch) + DELETE_PERIOD);
	route->seq++;
	route->route_valid = FALSE;
}

inline int _route_compare(mesh_route_t *route, u_int32_t destIp)
{
//	if ((route->ip & route->netmask) == (destIp & route->netmask))
	if ( route->ip  == destIp )
		return 1;
	else
		return 0;
}

int _engine_route_update(route_engine_t *engine, u_int32_t ip, u_int32_t next_hop_ip, u_int8_t metric, u_int32_t seq, struct net_device *dev)
{
	mesh_route_t *route;
	unsigned long  curr_time;

	/*lock table */
	route = engine->mgmtOps->route_lookup(engine, ip, ROUTE_TYPE_AODV);    /* Get eprev_route->next->prev = new_route;ntry from RT if there is one */

	if (!engine->mgmtOps->neigh_validate(engine, next_hop_ip))
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_ROUTE, "%s : Failed to update route: %s \n",engine->name,  inet_ntoa(ip));
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
		route = engine->mgmtOps->route_insert(engine, ip, ROUTE_TYPE_AODV);
		if (route == NULL)
			return -ENOMEM;
		route->ip = ip;
		route->dev = engine->dev_lookup_by_ip(engine, ip);
	}

	if (route->self_route)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_ROUTE, "updating a SELF-ROUTE!!! %s hopcount %d\n", inet_ntoa(next_hop_ip), metric);
		if (!route->route_valid)
			ROUTE_DPRINTF(engine, ROUTE_DEBUG_ROUTE, "because route was %s", "invalid!\n");

		if (!route->route_seq_valid)
			ROUTE_DPRINTF(engine, ROUTE_DEBUG_ROUTE, "because seq was %s", "invalid!\n");
		if (seq_greater(seq, route->seq))
			ROUTE_DPRINTF(engine, ROUTE_DEBUG_ROUTE, "because seq of route was %s", "lower!\n");
		if ((seq == route->seq) && (metric < route->metric))
			ROUTE_DPRINTF(engine, ROUTE_DEBUG_ROUTE, "becase seq same but hop %s", "lower!\n");
	}

	/* Update values in the RT entry */
	route->seq = seq;
	route->nextHop = next_hop_ip;
	route->metric = metric;
//	route->dev =   dev;
	route->route_valid = TRUE;
	route->route_seq_valid = TRUE;

/*
	aodv_delete_kroute(route);
	aodv_insert_kroute(route);

	ipq_send_ip(ip);
*/

	curr_time = getcurrtime(engine->epoch);  /* Get current time */
	/* Check if the lifetime in RT is valid, if not update it */

	route->lifetime =  curr_time +  ACTIVE_ROUTE_TIMEOUT;
	return 0;
}

static mesh_route_t *_engine_route_find(route_engine_t *engine, unsigned int destIp, route_type_t type)
{
	mesh_route_t 		*route;

	unsigned long  curr_time = getcurrtime(engine->epoch);

	ROUTE_READ_LOCK(engine->routeLock);
	ROUTE_LIST_FOR_EACH(route, engine->routes, list)
	{
//		if( route->ip <= target_ip )
		{
			if (time_before( route->lifetime, curr_time) && (!route->self_route) && (route->route_valid))
			{
				_route_expire( route );
			}

			if ( _route_compare(route, destIp))
			{/* destIp belone to this route, so return this route */
#if 0//ROUTE_DEBUG			
				char msg[1024];
				int len = 0;
				len += sprintf(msg+len, "it looks like the route %s ",inet_ntoa( route->ip));
				len += sprintf(msg+len, "is equal to: %s\n",inet_ntoa(destIp));
				printk("%s", msg);
#endif				
				ROUTE_READ_UNLOCK(engine->routeLock);
				return route;
			}
		}
	}
	ROUTE_READ_UNLOCK(engine->routeLock);

	return NULL;//possible;
}


static mesh_route_t *_engine_route_create(route_engine_t *engine, uint32_t ip, route_type_t type)
{
	mesh_route_t *route;
	unsigned long	flags;

	/* Allocate memory for new entry */
	if ((route = (mesh_route_t *) kmalloc(sizeof(mesh_route_t), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV: Error getting memory for new route\n");
		return NULL;
	}

	route->self_route = FALSE;
	route->rreq_id = 0;
	route->metric = 0;
	route->seq = 0;
	route->ip = ip;
	route->nextHop = ip; /*lzj, 2006.03.31 */
//	route->nextHop = engine->myIp;
	route->dev = NULL;
	route->route_valid = FALSE;
	route->netmask = engine->broadcastIp;
	route->route_seq_valid = FALSE;
	route->type = type;

//	if (ip)
	{
		ROUTE_WRITE_LOCK(engine->routeLock, flags);
		ROUTE_LIST_ADD_HEAD( &(route->list), engine->routes );
		ROUTE_WRITE_UNLOCK( engine->routeLock, flags);
	}
	return route;
}

static int _engine_route_delete(route_engine_t *engine, u_int32_t target_ip)
{
	mesh_route_t *route, *tmp;
	unsigned long	flags;
	
	ROUTE_WRITE_LOCK(engine->routeLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(route, tmp, engine->routes, list)
	{
//		if( route->ip <= target_ip )
//		{
			if (route->ip == target_ip)
			{
				ROUTE_LIST_DEL(&(route->list) );
				ROUTE_WRITE_UNLOCK(engine->routeLock, flags);
				kfree(route);
				route = NULL;
				return 1;
			}
//		}	
	}
	ROUTE_WRITE_UNLOCK(engine->routeLock, flags);

	return 0;
}

static route_neigh_t *_engine_neigh_find(route_engine_t *engine, u_int32_t target_ip)
{
	route_neigh_t 		*neigh; 

	ROUTE_READ_LOCK(engine->neighLock);
	ROUTE_LIST_FOR_EACH(neigh, engine->neighs, list)
	{
		if ( neigh->ip <= target_ip )
		{
			if ( neigh->ip == target_ip)
			{
				ROUTE_READ_UNLOCK(engine->neighLock);
				return neigh;
			}
		}
	}
	ROUTE_READ_UNLOCK(engine->neighLock);
	return NULL;
}

static route_neigh_t *_engine_neigh_find_by_hw(route_engine_t *engine, char *hw_addr)
{
	route_neigh_t 		*neigh; 

	ROUTE_READ_LOCK(engine->neighLock);
	ROUTE_LIST_FOR_EACH(neigh, engine->neighs, list)
	{
		if (!memcmp(&( neigh->myHwAddr), hw_addr, ETH_ALEN))
		{
			ROUTE_READ_UNLOCK(engine->neighLock);
			return neigh;
		}	
	}
	
	ROUTE_READ_UNLOCK(engine->neighLock);
	return NULL;
}

/* 1 : valid, 0 : */
static int _engine_neigh_valid(route_engine_t *engine, u_int32_t target_ip)
{
	route_neigh_t * neigh;

	neigh = engine->mgmtOps->neigh_lookup_ip(engine, target_ip);
	if ( neigh)
	{
		if ( neigh->valid_link && time_before(getcurrtime(engine->epoch), neigh->lifetime))
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

/* need to merge some logic in these 2 functions */
static void _engine_neigh_update_link(route_engine_t *engine, char *hw_addr, u_int8_t link)
{
	route_neigh_t 		*neigh; 
	unsigned long		flags;

	ROUTE_WRITE_LOCK(engine->neighLock, flags);
	ROUTE_LIST_FOR_EACH(neigh, engine->neighs, list)
	{
		if (!memcmp(&( neigh->myHwAddr), hw_addr, ETH_ALEN))
		{
			if (link)
				neigh->link = 0x100 - link;
			else
				neigh->link = 0;
			break;
		}
	}
	ROUTE_WRITE_UNLOCK(engine->neighLock, flags);
}

static int _engine_neigh_delete(route_engine_t *engine, u_int32_t ip)
{
	route_neigh_t 		*neigh, *tmp; 
	mesh_route_t		*route;
	unsigned long		flags;

	ROUTE_WRITE_LOCK(engine->neighLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(neigh, tmp, engine->neighs, list)
	{
		if ( neigh->ip == ip)
		{
			ROUTE_LIST_DEL(&(neigh->list) );

			kfree( neigh);
			neigh = NULL;
			engine->mgmtOps->timer_delete(engine, ip, ROUTE_TASK_NEIGHBOR);
			engine->mgmtOps->timers_update(engine);
			ROUTE_WRITE_UNLOCK( engine->neighLock, flags);

			route = engine->mgmtOps->route_lookup(engine, ip, ROUTE_TYPE_AODV);
			if ( route && ( route->nextHop == route->ip))
			{
//				aodv_delete_kroute(route );
				engine->protoOps->send_error(engine, route->ip);
				if ( route->route_valid)
				{
					_route_expire(route);
				}
				else
				{
					engine->mgmtOps->route_delete(engine, route->ip);
				}
			}
			return 0;
		}
	}

	ROUTE_WRITE_UNLOCK( engine->neighLock, flags);

	return -ENODATA;
}

static route_neigh_t *_engine_neigh_insert(route_engine_t *engine, u_int32_t ip)
{
	route_neigh_t *neigh = NULL;
	unsigned long	flags;

	ROUTE_DPRINTF(engine, ROUTE_DEBUG_NEIGH, " create neight with IP : %s\n", inet_ntoa(ip));

	neigh = engine->mgmtOps->neigh_lookup_ip(engine, ip);
	if ( neigh )
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_NEIGH, " NEIGH : try to create a duplicate neighbor %s\n", inet_ntoa(ip));
		return NULL;
	}

	if ((neigh = kmalloc(sizeof(route_neigh_t), GFP_ATOMIC)) == NULL)
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

	ROUTE_WRITE_LOCK( engine->neighLock, flags);
	ROUTE_LIST_ADD_HEAD(&(neigh->list), engine->neighs );
	ROUTE_WRITE_UNLOCK(engine->neighLock, flags);

	return neigh;
}

static void _engine_neigh_update_route(route_engine_t *engine, route_neigh_t *neigh, aodv_pdu_reply *reply)
{
	mesh_route_t *route;

	route = engine->mgmtOps->route_lookup(engine, neigh->ip, ROUTE_TYPE_AODV);
	if (route)
	{
/*				if (!tmp_route->route_valid)
				{
			    insert_kernel_route_entry(tmp_route->ip, tmp_route->nextHop, tmp_route->netmask,tmp_route->dev->name);
				}

				if (tmp_route->nextHop!=tmp_route->ip)
				{*/
//		aodv_delete_kroute(route);
//		aodv_insert_kroute(route);
				//}
	}
	else
	{	    
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_NEIGH, "Creating route for neighbor: %s Link: %d\n", inet_ntoa( neigh->ip), neigh->link);
		route = engine->mgmtOps->route_insert(engine, neigh->ip, ROUTE_TYPE_AODV);
//		aodv_delete_kroute(route );
//		aodv_insert_kroute( route);
	}

	route->lifetime = getcurrtime(engine->epoch) + reply->lifetime;
	route->dev = neigh->dev;
	route->nextHop = neigh->ip;
	route->route_seq_valid = TRUE;
	route->route_valid = TRUE;
	route->seq = reply->dst_seq;

	neigh->route_entry = route;
	route->metric = find_metric( route->ip);
}

static int _engine_neigh_update(route_engine_t *engine, route_neigh_t *neigh, aodv_pdu_reply *reply)
{
	if (neigh==NULL)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_NEIGH, "AODV: NULL neighbor %s", "passed!\n");
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
		engine->mgmtOps->neigh_update_route(engine, neigh, reply);
	}

	return 1;
}

static unsigned long __tvtojiffies(struct timeval *value)
{
	unsigned long sec = (unsigned) value->tv_sec;
	unsigned long usec = (unsigned) value->tv_usec;

	if (sec > (ULONG_MAX / HZ))
		return ULONG_MAX;
	usec += 1000000 / HZ - 1;
	usec /= 1000000 / HZ;
	return HZ * sec + usec;
}

/* remove from timer queue and added into task queue, maybe delete , so softey is called */
static route_task_t *__route_timer_first_due(route_engine_t *engine, unsigned long currtime)
{
	route_task_t 		*task, *tmp;
	unsigned long 		flags;

	ROUTE_WRITE_LOCK(engine->timerLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(task, tmp, engine->timers, list)
	{
		/* If pqe's time is in teh interval */
		if (time_before_eq(task->time, currtime))
		{
//		printk("%ld : %d\n", task->time, currtime );
			ROUTE_WRITE_UNLOCK(engine->timerLock, flags);
			return task;
		}
	}

	ROUTE_WRITE_UNLOCK(engine->timerLock, flags);
	return NULL;
}

/* handler of kernel timer, in BH context, lock ???  */
void __route_timer_queue_signal(unsigned long data)
{
	route_task_t 	*task;
	unsigned long  currtime;
#if 0	
	unsigned long	flags;
#endif
	route_engine_t *engine = (route_engine_t *)data;

	currtime = getcurrtime(engine->epoch);
	task = __route_timer_first_due(engine, currtime);

	// While there is still events that has timed out
	while ( task != NULL)
	{/* into task queue */
		/* remove it from timer queue and send into task queue */
		/* has been locked in first_due */
//		ROUTE_WRITE_LOCK(engine->timerLock, flags);
		ROUTE_LIST_DEL(&(task->list) );
//		ROUTE_WRITE_UNLOCK(engine->timerLock, flags);
		engine->mgmtOps->task_insert_timer(engine, task);
		task = __route_timer_first_due(engine, currtime);
	}
	engine->mgmtOps->timers_update(engine);
}


static void __route_timer_enqueue(route_engine_t *engine, route_task_t *timer)
{
	route_task_t		*task;
//    unsigned long  currtime = getcurrtime(engine->epoch);
	unsigned long flags;

	ROUTE_READ_LOCK(engine->timerLock);
	ROUTE_LIST_FOR_EACH(task, engine->timers, list)
	{
		if( task->type == timer->type && task->id== timer->id )
		{
			ROUTE_DPRINTF(engine, ROUTE_DEBUG_TIMER, "Timer with ID %d(%s) has been exist!\n", timer->id, inet_ntoa(timer->id));
			ROUTE_READ_UNLOCK(engine->timerLock);
			return;
		}
	}
	ROUTE_READ_UNLOCK(engine->timerLock);

	ROUTE_WRITE_LOCK(engine->timerLock, flags);	
//	ROUTE_LIST_ADD_TAIL(&(timer->list), header);
	ROUTE_LIST_ADD_HEAD(&(timer->list), engine->timers );
	ROUTE_WRITE_UNLOCK(engine->timerLock, flags);	

#if 0
	while (tmp_timer != NULL && (time_after(new_timer->time, tmp_timer->time)))
	{
		//printk("%d is larger than %s type: %d time dif of %d \n", new_timer->type,inet_ntoa(tmp_timer->id),tmp_timer->type, new_timer->time-tmp_timer->time);
		prev_timer = tmp_timer;
		tmp_timer = tmp_timer->next;
	}
#endif
}


route_task_t *__route_timer_find_first(route_engine_t *engine)
{
	route_task_t *task = NULL;

	ROUTE_READ_LOCK(engine->timerLock);
	ROUTE_LIST_FOR_EACH(task, engine->timers, list)
	{
		break;
	}

	ROUTE_READ_UNLOCK(engine->timerLock);
	return task;
}

static void _route_timer_update(route_engine_t *engine )
{
	struct timeval delay_time;
	unsigned long  currtime;
	//    unsigned long  tv;
	unsigned long  remainder, numerator;
	unsigned long flags;
	route_task_t *first;

	delay_time.tv_sec = 0;
	delay_time.tv_usec = 0;

	ROUTE_WRITE_LOCK( engine->timerLock, flags);
#if 0	
	if(ROUTE_LIST_CHECK_EMPTY(&timerQueue ) )
	{
		del_timer(&aodv_timer);
		ROUTE_IRQ_READ_UNLOCK(timerLock, flags);
		return;
	}
#endif

	first = __route_timer_find_first(engine);
	if(first==0)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_TIMER, "Timer Queue %s", "is empty\n");
		return;
	}	
	//* Get the first time value
	currtime = getcurrtime(engine->epoch);
	if (time_before( first->time, currtime))
	{
		// If the event has allready happend, set the timeout to              1 microsecond :-)
		delay_time.tv_sec = 0;
		delay_time.tv_usec = 1;
	}
	else
	{
		// Set the timer to the actual seconds / microseconds from now
		//This is a fix for an error that occurs on ARM Linux Kernels because they do 64bits differently
		//Thanks to S. Peter Li for coming up with this fix!
		numerator = (first->time - currtime);
//		remainder = do_div(numerator, 1000);
		remainder = numerator%1000;
		numerator = numerator/1000;
		
//		delay_time.tv_sec = numerator;
		delay_time.tv_sec = numerator+engine->epoch;
		delay_time.tv_usec = remainder * 1000;
	}
	
	if (!timer_pending( &(engine->timer)) )
	{
		engine->timer.function = __route_timer_queue_signal;
		engine->timer.data = (long)engine;
		engine->timer.expires = jiffies + __tvtojiffies(&delay_time);
//		printk("timer sched in %u sec and %u milisec delay %u(%d)\n",delay_time.tv_sec, delay_time.tv_usec,engine->timer.expires, jiffies);
		add_timer(&engine->timer);
	}
	else
	{
		mod_timer(&(engine->timer), jiffies + __tvtojiffies(&delay_time));
	}

	ROUTE_WRITE_UNLOCK( engine->timerLock, flags);

	return;
}

/* insert local task : cleanup and hello */
static int _route_timer_insert(route_engine_t *engine, route_task_type_t type, u_int32_t ip, u_int32_t delay)
{
	route_task_t *task;
//	unsigned long flags;
	unsigned long cur;

	task = route_task_create(engine, type);
	if ( task == NULL)
	{
		printk(KERN_WARNING "AODV: Error allocating timer!\n");
		return -ENOMEM;
	}
	task->src_ip = ip;
	task->dst_ip = ip;
	task->id = ip;

	cur = getcurrtime(engine->epoch);
	task->time = cur + delay;

//	printk( "curtime :%ld %ld delay %d\n", task->time, cur, delay );
	__route_timer_enqueue(engine, task);

	engine->mgmtOps->timers_update(engine);
	return 0;
}

static int _route_timer_insert_request(route_engine_t *engine, aodv_pdu_request * rreq, u_int8_t retries)
{
	route_task_t *timer;
//	unsigned long flags;

	if (!( timer = route_task_create(engine, ROUTE_TASK_RESEND_RREQ)))
	{
		printk(KERN_WARNING "AODV: Error allocating timer!\n");
		return -ENOMEM;
	}

	timer->src_ip = rreq->src_ip;
	timer->dst_ip = rreq->dst_ip;
	timer->id = rreq->dst_ip;
	timer->retries = retries;
	timer->ttl = 30;
	timer->time = getcurrtime(engine->epoch) + ((2 ^ (RREQ_RETRIES - retries)) * NET_TRAVERSAL_TIME);

	__route_timer_enqueue(engine, timer);
	return 0;
}


static route_task_t *_route_timer_find(route_engine_t *engine, u_int32_t id, route_task_type_t type)
{
	route_task_t *task;

	ROUTE_READ_LOCK(engine->timerLock);
	ROUTE_LIST_FOR_EACH(task, engine->timers, list)
	{
		if (( task->id == id) && ( task->type == type))
		{
			ROUTE_READ_UNLOCK(engine->timerLock);
			return task;
		}
	}

	ROUTE_READ_UNLOCK(engine->timerLock);

	return NULL;
}

static void _route_timer_delete(route_engine_t *engine, route_task_type_t type, u_int32_t id)
{
	route_task_t *task, *tmp;
	unsigned long flags;

//	ROUTE_READ_LOCK( engine->timerLock, flags);

	ROUTE_LIST_FOR_EACH_SAFE(task, tmp, engine->timers, list)
	{
		if ((task->id == id) && (task->type == type))
		{
			ROUTE_WRITE_LOCK(engine->timerLock, flags);
			ROUTE_LIST_DEL(&(task->list));
			ROUTE_WRITE_UNLOCK(engine->timerLock, flags);
			kfree( task->data);
			task->data = NULL;
			kfree( task);
			task = NULL;
			
		}
	}

//	ROUTE_READ_UNLOCK( engine->timerLock);
}

static route_flood_t *_route_flood_find(route_engine_t *engine, u_int32_t src_ip, u_int32_t id)
{
	route_flood_t *flood;

	unsigned long curr;

	ROUTE_READ_LOCK(engine->floodLock);

	if(ROUTE_LIST_CHECK_EMPTY(engine->floods) )
	{
		ROUTE_READ_UNLOCK(engine->floodLock);
		return NULL;
	}

	curr = getcurrtime(engine->epoch);
	ROUTE_LIST_FOR_EACH(flood, engine->floods, list )
	{
		if (time_before(flood->lifetime, curr))
		{
			ROUTE_READ_UNLOCK(engine->floodLock);
			return NULL;
		}

		//if there is a match and it is still valid
		if ((src_ip == flood->src_ip) && (id == flood->id))
		{
			ROUTE_READ_UNLOCK(engine->floodLock);
			return  flood;
		}
	}

	ROUTE_READ_UNLOCK(engine->floodLock);
	return NULL;
}

/* append in the header of queue */
static int _route_flood_insert(route_engine_t *engine, u_int32_t src_ip, u_int32_t dst_ip, u_int32_t id, unsigned long  lifetime)
{
	route_flood_t *flood;
	unsigned long	flags;

	ROUTE_READ_LOCK(engine->floodLock);
	ROUTE_LIST_FOR_EACH(flood, engine->floods, list)
	{
		if(flood->src_ip == src_ip && flood->id==id)
		{
			ROUTE_DPRINTF(engine, ROUTE_DEBUG_FLOOD, "Flood with ID %d(%s) has exist\n", id, inet_ntoa(id) );
			ROUTE_READ_UNLOCK( engine->floodLock);
			return -EEXIST;
		}
	}
	ROUTE_READ_UNLOCK( engine->floodLock);
	
	/* Allocate memory for the new entry */
	if ((flood = (route_flood_t *) kmalloc(sizeof(route_flood_t), GFP_ATOMIC)) == NULL)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_FLOOD, "Not enough memory to create Flood %s", "in queue\n");
		return -ENOMEM;
	}

	/* Fill in the information in the new entry */
	flood->src_ip = src_ip;
	flood->dst_ip = dst_ip;
	flood->id = id;
	flood->lifetime = lifetime;

	ROUTE_WRITE_LOCK(engine->floodLock, flags);
	ROUTE_LIST_ADD_HEAD(&(flood->list), engine->floods);
	ROUTE_WRITE_UNLOCK(engine->floodLock, flags);

	return 0;
}

/* append this task in header of task queue*/
int _task_enqueue(route_engine_t *engine, route_task_t *task)
{
	ROUTE_SPIN_LOCK(engine->taskLock);

	ROUTE_LIST_ADD_TAIL(&(task->list), engine->tasks);
//	ROUTE_LIST_ADD_HEAD(&(task->list), engine->tasks);

	ROUTE_SPIN_UNLOCK(engine->taskLock);

	kick_aodv();

	return 0;
}

static int _task_insert_from_timer(route_engine_t *engine, route_task_t * timer_task)
{
	if (!timer_task)
	{
		ROUTE_DPRINTF(engine, ROUTE_DEBUG_TIMER, "Passed a Null %s", "task Task\n");
		return -ENOMEM;
	}

	engine->mgmtOps->task_insert(engine, timer_task);

	return 0;
}

static void _engine_destroy(route_engine_t *engine)
{
	route_task_t	*task;
	route_dev_t 	*routeDev, *d2;
	route_flood_t 	*flood, *f2;
	mesh_route_t 	*route, *r2;
	unsigned long	flags;
	int 			count = 0;
	unsigned char 	proc_name[32];
//	del_timer(&(engine->timer) );
	route_proc_entry 	*proc = engine->procs;
	
	ROUTE_SPIN_LOCK(engine->taskLock);
	ROUTE_LIST_FOR_EACH(task, engine->tasks, list)
	{
		ROUTE_LIST_DEL(&(task->list ));
		kfree( task->data);
		task->data = NULL;
		kfree( task);
		task = NULL;
	}

	ROUTE_DPRINTF(engine, ROUTE_DEBUG_INIT, "Removed %s", "Tasks...\n");
	ROUTE_SPIN_UNLOCK(engine->taskLock);
	
	ROUTE_WRITE_LOCK(engine->floodLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(flood, f2, engine->floods, list )
	{
		ROUTE_LIST_DEL(&(flood->list) );
		kfree( flood);
		flood = NULL;
		count++;
	}
	ROUTE_WRITE_UNLOCK(engine->floodLock, flags);

	ROUTE_DPRINTF(engine, ROUTE_DEBUG_INIT, "Removed %s", "Floods...\n");

	ROUTE_WRITE_LOCK(engine->routeLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(route, r2, engine->routes, list)
	{
		ROUTE_LIST_DEL( &(route->list));

//		if(route->type == ROUTE_ROUTE_AODV )
//			aodv_delete_kroute(route);
		kfree( route);
		route = NULL;
	}
	ROUTE_WRITE_UNLOCK(engine->routeLock, flags);
	ROUTE_DPRINTF(engine, ROUTE_DEBUG_INIT, "remove %s", "routes...\n");

	ROUTE_LIST_FOR_EACH_SAFE(routeDev, d2, engine->devs, list)
	{
		ROUTE_LIST_DEL(&routeDev->list);
//		routeDev->destroy(routeDev);
		kfree(routeDev);
	}
	ROUTE_DPRINTF(engine, ROUTE_DEBUG_INIT, "Removed %s" ,"Device...\n");
	
	while( proc->read )
	{
		sprintf(proc_name, "%s/%s", ROUTE_PROC_NAME, proc->name);
		remove_proc_entry(proc_name, NULL);
		proc++;
	}
}

mgmt_ops_t	engine_mops =
{
	neigh_delete 			: 	_engine_neigh_delete,
	neigh_insert			:	_engine_neigh_insert,
	neigh_lookup_ip		:	_engine_neigh_find,
	neigh_lookup_mac		:	_engine_neigh_find_by_hw,
	neigh_validate 		:	_engine_neigh_valid,
	neigh_update_link		:	 _engine_neigh_update_link,
	neigh_update_route	:	_engine_neigh_update_route,

	timer_delete			:	_route_timer_delete,
	timer_insert			:	_route_timer_insert,
	timer_insert_request	:	_route_timer_insert_request,
	timers_update			:	_route_timer_update,
	timer_lookup			:	_route_timer_find,
	
	flood_insert			:	_route_flood_insert,
	flood_lookup			:	_route_flood_find,

	route_delete			:	_engine_route_delete,
	route_insert			:	_engine_route_create,
	route_lookup			:	_engine_route_find,
	route_update			:	_engine_route_update,

	task_insert			:	_task_enqueue,
	task_insert_timer		:	_task_insert_from_timer,

	init					:	NULL,
	destroy 				:	_engine_destroy
};

