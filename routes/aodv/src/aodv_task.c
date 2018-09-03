/*
* $Id: aodv_task.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include <linux/ip.h>
#include <linux/udp.h>
#include "aodv.h"

static wait_queue_head_t aodv_wait;

static atomic_t kill_thread;
static atomic_t aodv_is_dead;


/* remove task from queue and return, freed in aodvd thread, locked when free it */
static aodv_task_t *__aodv_task_get(aodv_t *aodv)
{
	aodv_task_t *task, *tmp;

	AODV_SPIN_LOCK(aodv->taskLock);

	/* it will be delete by kernel thread, so with safety */
	AODV_LIST_FOR_EACH_SAFE(task, tmp, aodv->tasks, list)
	{
		AODV_SPIN_UNLOCK(aodv->taskLock);
		return task;
	}
	
	AODV_SPIN_UNLOCK(aodv->taskLock);
	return NULL;    
}


static int __aodv_route_flush(aodv_t *aodv)
{
	aodv_route_t *route, *tmp;
	unsigned long  currtime = getcurrtime(aodv->epoch);
	unsigned long 	flags;

	AODV_WRITE_LOCK(aodv->taskLock, flags);
	AODV_LIST_FOR_EACH_SAFE(route, tmp, aodv->routes, list)
	{
		if (time_before( route->lifetime, currtime) && (!route->self_route))
		{
#if AODV_DEBUG		
			printk("looking at route: %s\n",inet_ntoa(route->ip));
#endif
			if ( route->route_valid )
			{
				aodv_route_expire(aodv, route);
			}
			else
			{
				aodv_delete_kroute( route);
//				AODV_WRITE_LOCK(aodv->routeLock, flags);
				AODV_LIST_DEL(&(route->list) );
				kfree(route);
				route = NULL;
//				AODV_WRITE_UNLOCK( aodv->routeLock, flags);
			}
		}
	}

	AODV_WRITE_UNLOCK(aodv->taskLock, flags);
	return 0;
}


/* return the number of effected flood_id 
flush all the expired flood_id and add a timer for for CLEANUP with length of HELLO_INTERVAL 
*/
int __aodv_flood_flush(aodv_t	*aodv)
{
	aodv_flood_t 		*flood, *tmp;
	unsigned long  	curr_time = getcurrtime(aodv->epoch);
	int 				count = 0;
	unsigned long		flags;

	AODV_WRITE_LOCK(aodv->floodLock, flags);
	AODV_LIST_FOR_EACH_SAFE(flood, tmp, aodv->floods, list)
	{
		if (time_before( flood->lifetime, curr_time))
		{
			AODV_LIST_DEL( &(flood->list) );
			kfree( flood);
			flood = NULL;
			count++;
		}
	}
	AODV_WRITE_UNLOCK(aodv->floodLock, flags);

	aodv_timer_insert(aodv, AODV_TASK_CLEANUP, HELLO_INTERVAL, aodv->myIp);
	
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
	aodv_task_t 	*currentTask;
	aodv_t		*aodv = (aodv_t *)data;

	//Initalize the variables
	init_waitqueue_head(&aodv_wait);
	atomic_set(&kill_thread, 0);
	atomic_set(&aodv_is_dead, 0);

	lock_kernel();
	sprintf(current->comm, "aodvd");
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

		while ((currentTask = __aodv_task_get(aodv)) != NULL)
		{

			//takes a different action depending on what type of event is recieved
			switch (currentTask->type)
			{

				case AODV_TASK_RREQ:
#if AODV_DEBUG					
					printk("AODV RREQ TASK\n");
#endif
					aodv_pdu_request_rx(aodv, currentTask);
					kfree(currentTask->data);
					break;

				case AODV_TASK_RREP:
#if AODV_DEBUG					
					printk("AODV RREP TASK\n");
#endif
					aodv_pdu_reply_rx(aodv, currentTask);
					kfree(currentTask->data);
					break;
				case AODV_TASK_RREP_ACK:
#if AODV_DEBUG					
					printk("AODV RREP ACK TASK\n");
#endif
					aodv_pdu_reply_ack_rx(aodv, currentTask);
					kfree(currentTask->data);
					break;

				case AODV_TASK_RERR:
#if AODV_DEBUG					
					printk("AODV RERR TASK\n");
#endif
					aodv_pdu_error_rx(aodv, currentTask);
					kfree(currentTask->data);
					break;
				//Cleanup  the Route Table and Flood ID queue
				case AODV_TASK_CLEANUP:
#if AODV_DEBUG					
					printk("AODV CLEANUP TASK\n");
#endif
					__aodv_flood_flush(aodv);
					__aodv_route_flush(aodv);
					break;
				case AODV_TASK_HELLO:
#if AODV_DEBUG					
					printk("AODV HELLO TASK\n");
#endif
					aodv_pdu_hello_send(aodv);
#ifdef AODV_SIGNAL
					aodv_iw_get_stats(aodv);
#endif
					break;
				case AODV_TASK_NEIGHBOR:
#if AODV_DEBUG					
					printk("AODV NEIGH TASK\n");
#endif
		        		aodv_neigh_delete(aodv, currentTask->src_ip);
		          		break;
				case AODV_TASK_ROUTE_CLEANUP:
#if AODV_DEBUG					
					printk("AODV ROUTE CLEANUP TASK\n");
#endif
					__aodv_route_flush(aodv);
					break;

				case AODV_TASK_RESEND_RREQ:
#if AODV_DEBUG					
					printk("AODV RESEND RREQ TASK\n");
#endif
					aodv_pdu_request_resend_ttl(aodv, currentTask);
					break;
				default:
					break;
			}
			AODV_SPIN_LOCK(aodv->taskLock);
			AODV_LIST_DEL(&(currentTask->list));
			AODV_SPIN_UNLOCK(aodv->taskLock);
			kfree(currentTask);
			currentTask = NULL;
		}
	}

  exit:
	//Set the flag that shows you are dead
	atomic_set(&aodv_is_dead, 1);
}

/* create a task for both task queue and timer queue */
aodv_task_t *aodv_task_create(aodv_t *aodv, aodv_task_type_t type)
{
	aodv_task_t *task;

	task = (aodv_task_t *) kmalloc(sizeof(aodv_task_t), GFP_ATOMIC);
	if ( task == NULL)
	{
		printk(KERN_WARNING "AODV: Not enough memory to create Event Queue Entry\n");
		return NULL;
	}

	task->time = getcurrtime(aodv->epoch);
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

/* append this task in header of task queue*/
int __aodv_task_enqueue(aodv_t *aodv, aodv_task_t *task)
{
	AODV_SPIN_LOCK(aodv->taskLock);

	AODV_LIST_ADD_TAIL(&(task->list), aodv->tasks);
//	AODV_LIST_ADD_HEAD(&(task->list), aodv->tasks);

	AODV_SPIN_UNLOCK(aodv->taskLock);

	kick_aodv();

	return 0;
}

/* create a task from UDP AODV PDU */
int aodv_task_insert(aodv_t *aodv, aodv_task_type_t  type, struct sk_buff *packet)
{
	aodv_task_t *task;
	struct iphdr *ip;

	int start_point = sizeof(struct udphdr) + sizeof(struct iphdr);

	task = aodv_task_create(aodv, type);
	if (!task)
	{
		printk(KERN_WARNING "AODV: Not enough memory to create Task\n");
		return -ENOMEM;
	}

	if (type < AODV_TASK_RESEND_RREQ)
	{
		ip = packet->nh.iph;
		task->src_ip = ip->saddr;
		task->dst_ip = ip->daddr;
		task->ttl = ip->ttl;
		task->dev = packet->dev;
		task->data_len = packet->len - start_point;

		//create space for the data and copy it there
		task->data = kmalloc(task->data_len, GFP_ATOMIC);
		if (!task->data)
		{
			kfree(task);
			printk(KERN_WARNING "AODV: Not enough memory to create Event Queue Data Entry\n");
			return -ENOMEM;
		}

		memcpy(task->data, packet->data + start_point, task->data_len);
	}

	switch (type)
	{
		case AODV_TASK_RREP:
			memcpy(&(task->src_hw_addr), &(packet->mac.ethernet->h_source), sizeof(unsigned char) * ETH_ALEN);
			break;
		default:
			break;
	}

	__aodv_task_enqueue(aodv, task);
	
	return 0;
}

int aodv_task_insert_from_timer(aodv_t *aodv, aodv_task_t * timer_task)
{
	if (!timer_task)
	{
		printk(KERN_WARNING "AODV: Passed a Null task Task\n");
		return -ENOMEM;
	}

	__aodv_task_enqueue(aodv, timer_task);

	return 0;
}

