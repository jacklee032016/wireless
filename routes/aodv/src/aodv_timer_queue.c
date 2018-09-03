/*
* $Id: aodv_timer_queue.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include "aodv.h"
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
static aodv_task_t *__aodv_timer_first_due(aodv_t *aodv, unsigned long currtime)
{
	aodv_task_t 		*task, *tmp;
	unsigned long 		flags;

	AODV_WRITE_LOCK(aodv->timerLock, flags);
	AODV_LIST_FOR_EACH_SAFE(task, tmp, aodv->timers, list)
	{
		/* If pqe's time is in teh interval */
		if (time_before_eq(task->time, currtime))
		{
//		printk("%ld : %d\n", task->time, currtime );
			AODV_WRITE_UNLOCK(aodv->timerLock, flags);
			return task;
		}
	}

	AODV_WRITE_UNLOCK(aodv->timerLock, flags);
	return NULL;
}

/* handler of kernel timer, in BH context, lock ???  */
void __aodv_timer_queue_signal(unsigned long data)
{
	aodv_task_t 	*task;
	unsigned long  currtime;
	unsigned long	flags;
	aodv_t *aodv = (aodv_t *)data;

	currtime = getcurrtime(aodv->epoch);
	task = __aodv_timer_first_due(aodv, currtime);

	// While there is still events that has timed out
	while ( task != NULL)
	{/* into task queue */
		/* remove it from timer queue and send into task queue */
		/* has been locked in first_due */
//		AODV_WRITE_LOCK(aodv->timerLock, flags);
		AODV_LIST_DEL(&(task->list) );
//		AODV_WRITE_UNLOCK(aodv->timerLock, flags);
		aodv_task_insert_from_timer(aodv, task);
		task = __aodv_timer_first_due(aodv, currtime);
	}
	aodv_timer_update(aodv);
}


static void __aodv_timer_enqueue(aodv_t *aodv, aodv_task_t *timer)
{
	aodv_task_t		*task;
//    unsigned long  currtime = getcurrtime(aodv->epoch);
	unsigned long flags;

	AODV_READ_LOCK(aodv->timerLock);
	AODV_LIST_FOR_EACH(task, aodv->timers, list)
	{
		if( task->type == timer->type && task->id== timer->id )
		{
			printk("Timer with ID %d has been exist!\n", timer->id);
			AODV_READ_UNLOCK(aodv->timerLock);
			return;
		}
	}
	AODV_READ_UNLOCK(aodv->timerLock);

	AODV_WRITE_LOCK(aodv->timerLock, flags);	
//	AODV_LIST_ADD_TAIL(&(timer->list), header);
	AODV_LIST_ADD_HEAD(&(timer->list), aodv->timers );
	AODV_WRITE_UNLOCK(aodv->timerLock, flags);	

#if 0
	while (tmp_timer != NULL && (time_after(new_timer->time, tmp_timer->time)))
	{
		//printk("%d is larger than %s type: %d time dif of %d \n", new_timer->type,inet_ntoa(tmp_timer->id),tmp_timer->type, new_timer->time-tmp_timer->time);
		prev_timer = tmp_timer;
		tmp_timer = tmp_timer->next;
	}
#endif
}


aodv_task_t *__aodv_timer_find_first(aodv_t *aodv)
{
	aodv_task_t *task = NULL;
	unsigned long flags;

	AODV_READ_LOCK(aodv->timerLock);
	AODV_LIST_FOR_EACH(task, aodv->timers, list)
	{
		break;
	}

	AODV_READ_UNLOCK(aodv->timerLock);
	return task;
}

void aodv_timer_update(aodv_t *aodv )
{
	struct timeval delay_time;
	unsigned long  currtime;
	//    unsigned long  tv;
	unsigned long  remainder, numerator;
	unsigned long flags;
	aodv_task_t *first;

	delay_time.tv_sec = 0;
	delay_time.tv_usec = 0;

	AODV_WRITE_LOCK( aodv->timerLock, flags);
#if 0	
	if(AODV_LIST_CHECK_EMPTY(&timerQueue ) )
	{
		del_timer(&aodv_timer);
		AODV_IRQ_READ_UNLOCK(timerLock, flags);
		return;
	}
#endif

	first = __aodv_timer_find_first(aodv);
	if(first==0)
	{
		printk("Timer Queue is empty\n");
		return;
	}	
	//* Get the first time value
	currtime = getcurrtime(aodv->epoch);
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
		delay_time.tv_sec = numerator+aodv->epoch;
		delay_time.tv_usec = remainder * 1000;
	}
	
	if (!timer_pending( &(aodv->timer)) )
	{
		aodv->timer.function = __aodv_timer_queue_signal;
		aodv->timer.data = (long)aodv;
		aodv->timer.expires = jiffies + __tvtojiffies(&delay_time);
//		printk("timer sched in %u sec and %u milisec delay %u(%d)\n",delay_time.tv_sec, delay_time.tv_usec,aodv->timer.expires, jiffies);
		add_timer(&aodv->timer);
	}
	else
	{
		mod_timer(&(aodv->timer), jiffies + __tvtojiffies(&delay_time));
	}

	AODV_WRITE_UNLOCK( aodv->timerLock, flags);

	return;
}

/* insert local task : cleanup and hello */
int aodv_timer_insert(aodv_t *aodv, aodv_task_type_t type, u_int32_t delay, u_int32_t ip)
{
	aodv_task_t *task;
//	unsigned long flags;
	unsigned long cur;

	task = aodv_task_create(aodv, type);
	if ( task == NULL)
	{
		printk(KERN_WARNING "AODV: Error allocating timer!\n");
		return -ENOMEM;
	}
	task->src_ip = ip;
	task->dst_ip = ip;
	task->id = ip;

	cur = getcurrtime(aodv->epoch);
	task->time = cur + delay;

//	printk( "curtime :%ld %ld delay %d\n", task->time, cur, delay );
	__aodv_timer_enqueue(aodv, task);

	aodv_timer_update(aodv);
	return 0;
}

int aodv_timer_insert_request(aodv_t *aodv, aodv_pdu_request * rreq, u_int8_t retries)
{
	aodv_task_t *timer;
//	unsigned long flags;

	if (!( timer = aodv_task_create(aodv, AODV_TASK_RESEND_RREQ)))
	{
		printk(KERN_WARNING "AODV: Error allocating timer!\n");
		return -ENOMEM;
	}

	timer->src_ip = rreq->src_ip;
	timer->dst_ip = rreq->dst_ip;
	timer->id = rreq->dst_ip;
	timer->retries = retries;
	timer->ttl = 30;
	timer->time = getcurrtime(aodv->epoch) + ((2 ^ (RREQ_RETRIES - retries)) * NET_TRAVERSAL_TIME);

	__aodv_timer_enqueue(aodv, timer);
	return 0;
}


aodv_task_t *aodv_timer_find(aodv_t *aodv, u_int32_t id, aodv_task_type_t type)
{
	aodv_task_t *task;

	AODV_READ_LOCK(aodv->timerLock);
	AODV_LIST_FOR_EACH(task, aodv->timers, list)
	{
		if (( task->id == id) && ( task->type == type))
		{
			AODV_READ_UNLOCK(aodv->timerLock);
			return task;
		}
	}

	AODV_READ_UNLOCK(aodv->timerLock);

	return NULL;
}

void aodv_timer_delete(aodv_t *aodv, u_int32_t id, aodv_task_type_t type)
{
	aodv_task_t *task, *tmp;
	unsigned long flags;

//	AODV_READ_LOCK( aodv->timerLock, flags);

	AODV_LIST_FOR_EACH_SAFE(task, tmp, aodv->timers, list)
	{
		if ((task->id == id) && (task->type == type))
		{
			AODV_WRITE_LOCK(aodv->timerLock, flags);
			AODV_LIST_DEL(&(task->list));
			AODV_WRITE_UNLOCK(aodv->timerLock, flags);
			kfree( task->data);
			task->data = NULL;
			kfree( task);
			task = NULL;
			
		}
	}

//	AODV_READ_UNLOCK( aodv->timerLock);
}

aodv_flood_t *aodv_flood_find(aodv_t *aodv, u_int32_t src_ip, u_int32_t id)
{
	aodv_flood_t *flood;

	unsigned long curr;

	AODV_READ_LOCK(aodv->floodLock);

	if(AODV_LIST_CHECK_EMPTY(aodv->floods) )
	{
		AODV_READ_UNLOCK(aodv->floodLock);
		return NULL;
	}

	curr = getcurrtime(aodv->epoch);
	AODV_LIST_FOR_EACH(flood, aodv->floods, list )
	{
		if (time_before(flood->lifetime, curr))
		{
			AODV_READ_UNLOCK(aodv->floodLock);
			return NULL;
		}

		//if there is a match and it is still valid
		if ((src_ip == flood->src_ip) && (id == flood->id))
		{
			AODV_READ_UNLOCK(aodv->floodLock);
			return  flood;
		}
	}

	AODV_READ_UNLOCK(aodv->floodLock);
	return NULL;
}

/* append in the header of queue */
int aodv_flood_insert(aodv_t *aodv, u_int32_t src_ip, u_int32_t dst_ip, u_int32_t id, unsigned long  lifetime)
{
	aodv_flood_t *flood;
	unsigned long	flags;

	AODV_READ_LOCK(aodv->floodLock);
	AODV_LIST_FOR_EACH(flood, aodv->floods, list)
	{
		if(flood->src_ip == src_ip && flood->id==id)
		{
			printk("Flood with ID %d has exist\n", id);
			AODV_READ_UNLOCK( aodv->floodLock);
			return -EEXIST;
		}
	}
	AODV_READ_UNLOCK( aodv->floodLock);
	
	/* Allocate memory for the new entry */
	if ((flood = (aodv_flood_t *) kmalloc(sizeof(aodv_flood_t), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_WARNING "AODV : Not enough memory to create Flood in queue\n");
		return -ENOMEM;
	}

	/* Fill in the information in the new entry */
	flood->src_ip = src_ip;
	flood->dst_ip = dst_ip;
	flood->id = id;
	flood->lifetime = lifetime;

	AODV_WRITE_LOCK(aodv->floodLock, flags);
	AODV_LIST_ADD_HEAD(&(flood->list), aodv->floods);
	AODV_WRITE_UNLOCK(aodv->floodLock, flags);

	return 0;
}

