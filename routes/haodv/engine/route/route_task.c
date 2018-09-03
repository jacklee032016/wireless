/*
* $Id: route_task.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
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
static route_task_t *__aodv_task_get(route_node_t *node)
{
	route_task_t *task, *tmp;


	/* it will be delete by kernel thread, so with safety */
	ROUTE_SPIN_LOCK(node->taskLock);
	ROUTE_LIST_FOR_EACH_SAFE(task, tmp, node->tasks, list)
	{
		ROUTE_LIST_DEL(&(task->list));
		ROUTE_SPIN_UNLOCK(node->taskLock);
		return task;
	}
	
	ROUTE_SPIN_UNLOCK(node->taskLock);
	return NULL;    
}


static int __aodv_route_flush(route_node_t *node)
{
	mesh_route_t *route, *tmp;
	unsigned long  currtime = getcurrtime(node->epoch);
	unsigned long 	flags;
#if ROUTE_DEBUG	
	int count = 0;
#endif

	ROUTE_WRITE_LOCK(node->taskLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(route, tmp, node->routes, list)
	{
		if (time_before( route->lifetime, currtime) && (!route->selfRoute))
		{

			ROUTE_DPRINTF(node, ROUTE_DEBUG_ROUTE, "looking at route: %s\n",inet_ntoa(route->address.address));
			if ( route->routeValid )
			{
				_route_expire( route);
			}
			else
			{
//				aodv_delete_kroute( route);

//				ROUTE_WRITE_LOCK(node->routeLock, flags);
				ROUTE_LIST_DEL(&(route->list) );
				kfree(route);
				route = NULL;
#if ROUTE_DEBUG	
				count++;
#endif
//				ROUTE_WRITE_UNLOCK( node->routeLock, flags);
			}
		}
	}
	ROUTE_DPRINTF(node, ROUTE_DEBUG_ROUTE, "%d route is deleted because of timerout\n", count);
	ROUTE_WRITE_UNLOCK(node->taskLock, flags);
	return 0;
}


/* return the number of effected flood_id 
flush all the expired flood_id and add a timer for for CLEANUP with length of HELLO_INTERVAL 
*/
int __aodv_flood_flush(route_node_t	*node)
{
	route_flood_t 		*flood, *tmp;
	unsigned long  	curr_time = getcurrtime(node->epoch);
	int 				count = 0;
	unsigned long		flags;

	ROUTE_WRITE_LOCK(node->floodLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(flood, tmp, node->floods, list)
	{
		if (time_before( flood->lifetime, curr_time))
		{
			ROUTE_LIST_DEL( &(flood->list) );
			kfree( flood);
			flood = NULL;
			count++;
		}
	}
	ROUTE_WRITE_UNLOCK(node->floodLock, flags);

	node->mgmtOps->timer_insert(node, &node->address, ROUTE_MGMT_TASK_CLEANUP, HELLO_INTERVAL);
	
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
	route_node_t	*node = (route_node_t *)data;

	//Initalize the variables
	init_waitqueue_head(&aodv_wait);
	atomic_set(&kill_thread, 0);
	atomic_set(&aodv_is_dead, 0);

	lock_kernel();
	sprintf(current->comm, "meshd");
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

		while ((currentTask = __aodv_task_get(node)) != NULL)
		{

			//takes a different action depending on what type of event is recieved
			if(currentTask->type == ROUTE_TASK_PDU)
			{
				pdu_task_t *task = (pdu_task_t *)currentTask;
				route_dev_t *dev = task->header.dev;
				
				switch(task->header.subType )
				{
					case ROUTE_PDU_TASK_RREQ:
						ROUTE_DPRINTF(node, ROUTE_DEBUG_TASK, "TASK RX %s", "RREQ\n");
						dev->taskOps->rx_request(dev, task);
						break;

					case ROUTE_PDU_TASK_RREP:
						ROUTE_DPRINTF(node, ROUTE_DEBUG_TASK, "TASK RX %s", "RREP\n");
						dev->taskOps->rx_reply(dev, task);
						break;
					case ROUTE_PDU_TASK_RREP_ACK:
						ROUTE_DPRINTF(node, ROUTE_DEBUG_TASK, "TASK RX %s", "RREP ACK\n");
						dev->taskOps->rx_reply_ack(dev, task);
						break;

					case ROUTE_PDU_TASK_RERR:
						ROUTE_DPRINTF(node, ROUTE_DEBUG_TASK, "TASK RX %s", "RERR\n");
						dev->taskOps->rx_error(dev, task);
						break;
					default:
						break;
				}
				if(task->data)
					kfree( task->data);
			}
			else if(currentTask->type == ROUTE_TASK_MGMT)
			{
				switch(currentTask->subType)
				{
#if 1				
					//Cleanup  the Route Table and Flood ID queue
					case ROUTE_MGMT_TASK_CLEANUP:
						ROUTE_DPRINTF(node, ROUTE_DEBUG_TASK, "TASK %s", "CLEANUP\n");
						__aodv_flood_flush(node);
						__aodv_route_flush(node);
						break;
#endif						
					case ROUTE_MGMT_TASK_HELLO:
						ROUTE_DPRINTF(node, ROUTE_DEBUG_TASK, "TASK %s", "HELLO\n");
						node->protoOps->send_hello(node);
#ifdef ROUTE_SIGNAL
						aodv_iw_get_stats(node);
#endif
						break;
#if 1
					case ROUTE_MGMT_TASK_NEIGHBOR:
						ROUTE_DPRINTF(node, ROUTE_DEBUG_TASK, "TASK %s", "NEIGH\n");
			        		node->mgmtOps->neigh_delete(node, &currentTask->src );
			          		break;
					case ROUTE_MGMT_TASK_ROUTE_CLEANUP:
						ROUTE_DPRINTF(node, ROUTE_DEBUG_TASK, "TASK %s", "ROUTE CLEANUP\n");
						__aodv_route_flush(node);
						break;
#endif
					case ROUTE_MGMT_TASK_RESEND_RREQ:
					{
						route_dev_t *dev = currentTask->dev;
						ROUTE_DPRINTF(node, ROUTE_DEBUG_TASK, "TASK %s", "RESEND RREQ\n");
						if(dev )
						{
							dev->taskOps->send_request(dev, currentTask);
							/* retry task can not be freed here, so continue */
							continue;
						}	
						break;
					}	
					default:
						break;
				}	
			}
			else
				ROUTE_DPRINTF(node, ROUTE_DEBUG_ENGINE, "Task Type %d unknown", currentTask->type);
			
			kfree(currentTask);
			currentTask = NULL;
		}
	}

  exit:
	//Set the flag that shows you are dead
	atomic_set(&aodv_is_dead, 1);
}

