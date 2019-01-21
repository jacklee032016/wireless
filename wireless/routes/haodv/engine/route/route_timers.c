/*
* $Id: route_timers.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/

#include <linux/proc_fs.h>
#include "mesh_route.h"
#include "_route.h"

int _task_insert_from_timer(route_node_t *node, route_task_t * timer_task)
{
	if (!timer_task)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_TIMER, "Passed a Null %s", "task Task\n");
		return -ENOMEM;
	}

	node->mgmtOps->task_insert(node, timer_task);

	return 0;
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
static route_task_t *__route_timer_first_due(route_node_t *node, unsigned long currtime)
{
	route_task_t 		*task, *tmp;
	unsigned long 		flags;

	ROUTE_WRITE_LOCK(node->timerLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(task, tmp, node->timers, list)
	{
		if (time_before_eq(task->time, currtime))
		{
//		printk("%ld : %d\n", task->time, currtime );
			ROUTE_WRITE_UNLOCK(node->timerLock, flags);
			return task;
		}
	}

	ROUTE_WRITE_UNLOCK(node->timerLock, flags);
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
	route_node_t *node = (route_node_t *)data;

	currtime = getcurrtime(node->epoch);
	task = __route_timer_first_due(node, currtime);

	// While there is still events that has timed out
	while ( task != NULL)
	{/* into task queue */
		/* remove it from timer queue and send into task queue */
		/* has been locked in first_due */
//		ROUTE_WRITE_LOCK(node->timerLock, flags);
		ROUTE_LIST_DEL(&(task->list) );
//		ROUTE_WRITE_UNLOCK(node->timerLock, flags);
		node->mgmtOps->task_insert_timer(node, task);
		task = __route_timer_first_due(node, currtime);
	}
	node->mgmtOps->timers_update(node);
}


route_task_t *__route_timer_find_first(route_node_t *node)
{
	route_task_t *task = NULL;

	ROUTE_READ_LOCK(node->timerLock);
	ROUTE_LIST_FOR_EACH(task, node->timers, list)
	{
		break;
	}

	ROUTE_READ_UNLOCK(node->timerLock);
	return task;
}


void _timer_enqueue(route_node_t *node, route_task_t *timer)
{
	route_task_t		*task;
//    unsigned long  currtime = getcurrtime(node->epoch);
	unsigned long flags;

	ROUTE_READ_LOCK(node->timerLock);
	ROUTE_LIST_FOR_EACH(task, node->timers, list)
	{
		if( task->subType == timer->subType && task->id== timer->id )
		{
			ROUTE_DPRINTF(node, ROUTE_DEBUG_TIMER, "Timer with ID %d(%s) has been exist!\n", timer->id, inet_ntoa(timer->id));
			ROUTE_READ_UNLOCK(node->timerLock);
			return;
		}
	}
	ROUTE_READ_UNLOCK(node->timerLock);

	ROUTE_WRITE_LOCK(node->timerLock, flags);	
//	ROUTE_LIST_ADD_TAIL(&(timer->list), header);
	ROUTE_LIST_ADD_HEAD(&(timer->list), node->timers );
	ROUTE_WRITE_UNLOCK(node->timerLock, flags);	

}

void _timer_update(route_node_t *node )
{
	struct timeval delay_time;
	unsigned long  currtime;
	//    unsigned long  tv;
	unsigned long  remainder, numerator;
	unsigned long flags;
	route_task_t *first;

	delay_time.tv_sec = 0;
	delay_time.tv_usec = 0;

	ROUTE_WRITE_LOCK( node->timerLock, flags);
#if 0	
	if(ROUTE_LIST_CHECK_EMPTY(&timerQueue ) )
	{
		del_timer(&aodv_timer);
		ROUTE_IRQ_READ_UNLOCK(timerLock, flags);
		return;
	}
#endif

	first = __route_timer_find_first(node);
	if(first==0)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_TIMER, "Timer Queue %s", "is empty\n");
		return;
	}	
	//* Get the first time value
	currtime = getcurrtime(node->epoch);
	if (time_before( first->time, currtime))
	{
		// If the event has allready happend, set the timeout to              1 microsecond :-)
		delay_time.tv_sec = 0;
		delay_time.tv_usec = 1;
	}
	else
	{
		numerator = (first->time - currtime);
//		remainder = do_div(numerator, 1000);
		remainder = numerator%1000;
		numerator = numerator/1000;
		
//		delay_time.tv_sec = numerator;
		delay_time.tv_sec = numerator+node->epoch;
		delay_time.tv_usec = remainder * 1000;
	}
	
	if (!timer_pending( &(node->timer)) )
	{
		node->timer.function = __route_timer_queue_signal;
		node->timer.data = (long)node;
		node->timer.expires = jiffies + __tvtojiffies(&delay_time);
//		printk("timer sched in %u sec and %u milisec delay %u(%d)\n",delay_time.tv_sec, delay_time.tv_usec,node->timer.expires, jiffies);
		add_timer(&node->timer);
	}
	else
	{
		mod_timer(&(node->timer), jiffies + __tvtojiffies(&delay_time));
	}

	ROUTE_WRITE_UNLOCK( node->timerLock, flags);

	return;
}

/* insert local task : cleanup and hello */
int _timer_insert(route_node_t *node, aodv_address_t *address, task_subtype_t subtype, u_int32_t delay)
{
	route_task_t *task;
//	unsigned long flags;
	unsigned long cur;

	MALLOC(task, (route_task_t *), sizeof(route_task_t), M_NOWAIT|M_ZERO, "MGMT TASK" );
	if ( task == NULL)
	{
		return -ENOMEM;
	}

	ADDRESS_ASSIGN((&task->src),  address );
	ADDRESS_ASSIGN((&task->dest), address );
	task->id = node->address.address;	//???
	task->type = ROUTE_TASK_MGMT;
	task->subType = subtype;

	cur = getcurrtime(node->epoch);
	task->time = cur + delay;

//	printk( "curtime :%ld %ld delay %d\n", task->time, cur, delay );
	_timer_enqueue(node, task);

	node->mgmtOps->timers_update(node);
	return 0;
}

route_task_t *_timer_lookup(route_node_t *node, u_int32_t id, task_subtype_t type)
{
	route_task_t *task;

	ROUTE_READ_LOCK(node->timerLock);
	ROUTE_LIST_FOR_EACH(task, node->timers, list)
	{
		if (( task->id == id) && ( task->subType == type))
		{
			ROUTE_READ_UNLOCK(node->timerLock);
			return task;
		}
	}

	ROUTE_READ_UNLOCK(node->timerLock);

	return NULL;
}

void _timer_delete(route_node_t *node,aodv_address_t *address,  task_subtype_t subtype, u_int32_t id)
{
	route_task_t *task, *tmp;
	unsigned long flags;

//	ROUTE_READ_LOCK( node->timerLock, flags);

	ROUTE_LIST_FOR_EACH_SAFE(task, tmp, node->timers, list)
	{
		if ((task->id == id) && (task->subType == subtype))
		{
			ROUTE_WRITE_LOCK(node->timerLock, flags);
			ROUTE_LIST_DEL(&(task->list));
			ROUTE_WRITE_UNLOCK(node->timerLock, flags);
//			kfree( task->data);
//			task->data = NULL;
			kfree( task);
			task = NULL;
			
		}
	}

//	ROUTE_READ_UNLOCK( node->timerLock);
}

route_flood_t *_flood_lookup(route_node_t *node, aodv_address_t *origAdd, u_int32_t reqId)
{
	route_flood_t *flood;

	unsigned long curr;

	ROUTE_READ_LOCK(node->floodLock);

	if(ROUTE_LIST_CHECK_EMPTY(node->floods) )
	{
		ROUTE_READ_UNLOCK(node->floodLock);
		return NULL;
	}

	curr = getcurrtime(node->epoch);
	ROUTE_LIST_FOR_EACH(flood, node->floods, list )
	{
		if (time_before(flood->lifetime, curr))
		{
			ROUTE_READ_UNLOCK(node->floodLock);
			return NULL;
		}

		//if there is a match and it is still valid
//		if ((src_ip == flood->src) && (id == flood->id))
		if ( ADDRESS_EQUAL( origAdd, &flood->orig ) && ( reqId == flood->reqId) )
		{
			ROUTE_READ_UNLOCK(node->floodLock);
			return  flood;
		}
	}

	ROUTE_READ_UNLOCK(node->floodLock);
	return NULL;
}

/* append in the header of queue */
int _flood_insert(route_node_t *node, aodv_address_t *orig, aodv_address_t *dest, u_int32_t reqId, unsigned long  lifetime)
{
	route_flood_t *flood;
	unsigned long	flags;

	ROUTE_READ_LOCK(node->floodLock);
	ROUTE_LIST_FOR_EACH(flood, node->floods, list)
	{
//		if(flood->src_ip == src_ip && flood->id==id)
		if( ADDRESS_EQUAL(&flood->orig, orig) && flood->reqId==reqId)
		{
			ROUTE_DPRINTF(node, ROUTE_DEBUG_FLOOD, "Flood with ID %d(%s) has exist\n", reqId, inet_ntoa(reqId) );
			ROUTE_READ_UNLOCK( node->floodLock);
			return -EEXIST;
		}
	}
	ROUTE_READ_UNLOCK( node->floodLock);
	
	/* Allocate memory for the new entry */
	if ((flood = (route_flood_t *) kmalloc(sizeof(route_flood_t), GFP_ATOMIC)) == NULL)
	{
		ROUTE_DPRINTF(node, ROUTE_DEBUG_FLOOD, "Not enough memory to create Flood %s", "in queue\n");
		return -ENOMEM;
	}

	/* Fill in the information in the new entry */
	ADDRESS_ASSIGN(&flood->orig, orig );
	ADDRESS_ASSIGN(&flood->dest, dest );
	flood->reqId = reqId;
	flood->lifetime = lifetime;

	ROUTE_WRITE_LOCK(node->floodLock, flags);
	ROUTE_LIST_ADD_HEAD(&(flood->list), node->floods);
	ROUTE_WRITE_UNLOCK(node->floodLock, flags);

	return 0;
}


/* append this task in header of task queue*/
int _task_enqueue(route_node_t *node, route_task_t *task)
{
	ROUTE_SPIN_LOCK(node->taskLock);

	ROUTE_LIST_ADD_TAIL(&(task->list), node->tasks);
//	ROUTE_LIST_ADD_HEAD(&(task->list), node->tasks);

	ROUTE_SPIN_UNLOCK(node->taskLock);

	kick_aodv();

	return 0;
}

void _node_destroy(route_node_t *node)
{
	route_task_t	*task;
	route_dev_t 	*routeDev, *d2;
	route_flood_t 	*flood, *f2;
	mesh_route_t 	*route, *r2;
	unsigned long	flags;
	int 			count = 0;
	unsigned char 	proc_name[32];
//	del_timer(&(node->timer) );
	route_proc_entry 	*proc = node->procs;
	
	ROUTE_SPIN_LOCK(node->taskLock);
	ROUTE_LIST_FOR_EACH(task, node->tasks, list)
	{
		ROUTE_LIST_DEL(&(task->list ));
		if(task->type == ROUTE_TASK_PDU)
		{
			pdu_task_t *pdu = (pdu_task_t *)task;
			kfree( pdu->data);
			pdu->data = NULL;
		}	
		kfree( task);
		task = NULL;
	}
	ROUTE_SPIN_UNLOCK(node->taskLock);

	ROUTE_DPRINTF(node, ROUTE_DEBUG_INIT, "Removed %s", "Tasks...\n");
	
	ROUTE_WRITE_LOCK(node->floodLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(flood, f2, node->floods, list )
	{
		ROUTE_LIST_DEL(&(flood->list) );
		kfree( flood);
		flood = NULL;
		count++;
	}
	ROUTE_WRITE_UNLOCK(node->floodLock, flags);

	ROUTE_DPRINTF(node, ROUTE_DEBUG_INIT, "Removed %s", "Floods...\n");

	ROUTE_WRITE_LOCK(node->routeLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(route, r2, node->routes, list)
	{
		ROUTE_LIST_DEL( &(route->list));

//		if(route->type == ROUTE_ROUTE_AODV )
//			aodv_delete_kroute(route);
		kfree( route);
		route = NULL;
	}
	ROUTE_WRITE_UNLOCK(node->routeLock, flags);
	ROUTE_DPRINTF(node, ROUTE_DEBUG_INIT, "remove %s", "routes...\n");

	ROUTE_WRITE_LOCK(node->devLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(routeDev, d2, node->devs, list)
	{
		ROUTE_LIST_DEL(&routeDev->list);
//		routeDev->destroy(routeDev);
		kfree(routeDev);
	}
	ROUTE_WRITE_UNLOCK(node->devLock, flags);
	ROUTE_DPRINTF(node, ROUTE_DEBUG_INIT, "Removed %s" ,"Device...\n");
	
	while( proc->read )
	{
		sprintf(proc_name, "%s/%s", ROUTE_PROC_NAME, proc->name);
		remove_proc_entry(proc_name, NULL);
		proc++;
	}
}


