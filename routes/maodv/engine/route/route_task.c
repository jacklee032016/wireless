/*
* $Id: route_task.c,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
*/
#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif
 
#include <linux/ip.h>
#include <linux/udp.h>
#include "mesh_route.h"
#include "_route.h"

static wait_queue_head_t aodv_wait;

static atomic_t kill_thread;
static atomic_t aodv_is_dead;

#if ROUTE_DEBUG
static char *_task_name[] =
{
	"RREQ PDU",
	"RREP PDU",
	"RERR PDU",
	"RREP ACK PDU",

	"MGMT RESEND RREQ",
	"MGMT HELLO",
	"MGMT NEIGHBOR",
	"MGMT CLEANUP",
	"MGMT ROUTE CLEANUP"
	
};
#endif

/* remove task from queue and return, freed in aodvd thread, locked when free it */
static route_task_t *__aodv_task_get(route_engine_t *engine)
{
	route_task_t *task, *tmp;

	ROUTE_SPIN_LOCK(engine->taskLock);

	/* it will be delete by kernel thread, so with safety */
	ROUTE_LIST_FOR_EACH_SAFE(task, tmp, engine->tasks, list)
	{
		ROUTE_SPIN_UNLOCK(engine->taskLock);
		return task;
	}
	
	ROUTE_SPIN_UNLOCK(engine->taskLock);
	return NULL;    
}


static int __aodv_route_flush(route_engine_t *engine)
{
	mesh_route_t *route, *tmp;
	unsigned long  currtime = getcurrtime(engine->epoch);
	unsigned long 	flags;
#if ROUTE_DEBUG	
	int count = 0;
#endif

	ROUTE_WRITE_LOCK(engine->taskLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(route, tmp, engine->routes, list)
	{
		if (time_before( route->lifetime, currtime) && (!route->self_route))
		{

			ROUTE_DPRINTF(engine, ROUTE_DEBUG_ROUTE, "looking at route: %s\n",inet_ntoa(route->ip));
			if ( route->route_valid )
			{
				_route_expire( route);
			}
			else
			{
//				aodv_delete_kroute( route);

//				ROUTE_WRITE_LOCK(engine->routeLock, flags);
				ROUTE_LIST_DEL(&(route->list) );
				kfree(route);
				route = NULL;
				count++;
//				ROUTE_WRITE_UNLOCK( engine->routeLock, flags);
			}
		}
	}
	ROUTE_DPRINTF(engine, ROUTE_DEBUG_ROUTE, "%d route is deleted because of timerout\n", count);
	ROUTE_WRITE_UNLOCK(engine->taskLock, flags);
	return 0;
}


/* return the number of effected flood_id 
flush all the expired flood_id and add a timer for for CLEANUP with length of HELLO_INTERVAL 
*/
int __aodv_flood_flush(route_engine_t	*engine)
{
	route_flood_t 		*flood, *tmp;
	unsigned long  	curr_time = getcurrtime(engine->epoch);
	int 				count = 0;
	unsigned long		flags;

	ROUTE_WRITE_LOCK(engine->floodLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(flood, tmp, engine->floods, list)
	{
		if (time_before( flood->lifetime, curr_time))
		{
			ROUTE_LIST_DEL( &(flood->list) );
			kfree( flood);
			flood = NULL;
			count++;
		}
	}
	ROUTE_WRITE_UNLOCK(engine->floodLock, flags);

	engine->mgmtOps->timer_insert(engine, ROUTE_TASK_CLEANUP, engine->myIp, HELLO_INTERVAL);
	
	return count;
}

/* wakeup kernel thread when a task has enqueue task queue */
void kick_aodv()
{
	wake_up_interruptible(&aodv_wait);
}

void kill_aodv()
{
	wait_queue_head_t queue;	/* wait queue for rmmod process */
	init_waitqueue_head(&queue);

	//sets a flag letting the thread know it should die
	//wait for the thread to set flag saying it is dead

        //lower semaphore for the thread
	atomic_set(&kill_thread, 1);
	wake_up_interruptible(&aodv_wait);
	interruptible_sleep_on_timeout(&queue, HZ);	/* block rmmod process tempate */
}

void aodv_kthread(void *data)
{
	route_task_t 		*currentTask;
	route_engine_t	*engine = (route_engine_t *)data;

	//Initalize the variables
	init_waitqueue_head(&aodv_wait);
	atomic_set(&kill_thread, 0);
	atomic_set(&aodv_is_dead, 0);

	lock_kernel();
	sprintf(current->comm, "mrouted");
	exit_mm(current);
	unlock_kernel();

	//add_wait_queue_exclusive(event_socket->sk->sleep,&(aodv_wait));
	//	add_wait_queue(&(aodv_wait),event_socket->sk->sleep);

	for (;;)
	{
		if (atomic_read(&kill_thread))
		{
			goto exit;
		}

		//goto sleep until we recieve an interupt
		interruptible_sleep_on(&aodv_wait);

		if (atomic_read(&kill_thread))
		{
			goto exit;
		}

		while ((currentTask = __aodv_task_get(engine)) != NULL)
		{

			//takes a different action depending on what type of event is recieved
			switch (currentTask->type)
			{

				case ROUTE_TASK_RREQ:
					ROUTE_DPRINTF(engine, ROUTE_DEBUG_TASK, "TASK %s", "RREQ\n");
					
					engine->protoOps->rx_request(engine, currentTask);
					kfree(currentTask->data);
					break;

				case ROUTE_TASK_RREP:
					ROUTE_DPRINTF(engine, ROUTE_DEBUG_TASK, "TASK %s", "RREP\n");
					engine->protoOps->rx_reply(engine, currentTask);
					kfree(currentTask->data);
					break;
				case ROUTE_TASK_RREP_ACK:
					ROUTE_DPRINTF(engine, ROUTE_DEBUG_TASK, "TASK %s", "RREP ACK\n");
					engine->protoOps->rx_reply_ack(engine, currentTask);
					kfree(currentTask->data);
					break;

				case ROUTE_TASK_RERR:
					ROUTE_DPRINTF(engine, ROUTE_DEBUG_TASK, "TASK %s", "RERR\n");
					engine->protoOps->rx_error(engine, currentTask);
					kfree(currentTask->data);
					break;
				//Cleanup  the Route Table and Flood ID queue
				case ROUTE_TASK_CLEANUP:
					ROUTE_DPRINTF(engine, ROUTE_DEBUG_TASK, "TASK %s", "CLEANUP\n");
					__aodv_flood_flush(engine);
					__aodv_route_flush(engine);
					break;
				case ROUTE_TASK_HELLO:
					ROUTE_DPRINTF(engine, ROUTE_DEBUG_TASK, "TASK %s", "HELLO\n");
					engine->protoOps->send_hello(engine);
#ifdef ROUTE_SIGNAL
					aodv_iw_get_stats(engine);
#endif
					break;
				case ROUTE_TASK_NEIGHBOR:
					ROUTE_DPRINTF(engine, ROUTE_DEBUG_TASK, "TASK %s", "NEIGH\n");
		        		engine->mgmtOps->neigh_delete(engine, currentTask->src_ip);
		          		break;
				case ROUTE_TASK_ROUTE_CLEANUP:
					ROUTE_DPRINTF(engine, ROUTE_DEBUG_TASK, "TASK %s", "ROUTE CLEANUP\n");
					__aodv_route_flush(engine);
					break;

				case ROUTE_TASK_RESEND_RREQ:
					ROUTE_DPRINTF(engine, ROUTE_DEBUG_TASK, "TASK %s", "RESEND RREQ\n");
					engine->protoOps->resend_request(engine, currentTask);
					break;
				default:
					break;
			}
			ROUTE_SPIN_LOCK(engine->taskLock);
			ROUTE_LIST_DEL(&(currentTask->list));
			ROUTE_SPIN_UNLOCK(engine->taskLock);
			kfree(currentTask);
			currentTask = NULL;
		}
	}

  exit:
	//Set the flag that shows you are dead
	atomic_set(&aodv_is_dead, 1);
}

/* create a task for both task queue and timer queue */
route_task_t *route_task_create(route_engine_t *engine, route_task_type_t type)
{
	route_task_t *task;

	task = (route_task_t *) kmalloc(sizeof(route_task_t), GFP_ATOMIC);
	if ( task == NULL)
	{
		printk(KERN_WARNING "ERROR : Not enough memory to create Event Queue Entry\n");
		return NULL;
	}

	task->time = getcurrtime(engine->epoch);
	task->type = type;
	task->src_ip = 0;
	task->dst_ip = 0;
	task->ttl = 0;
	task->retries = 0;
	task->data = NULL;
	task->data_len = 0;
	task->dev = NULL;

	return task;
}

EXPORT_SYMBOL(route_task_create);

