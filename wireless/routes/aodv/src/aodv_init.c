/*
* $Id: aodv_init.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <asm/delay.h>
#include "packet_queue.h"

#include "aodv.h"

extern	aodv_proc_entry  proc_table[];

#ifdef LINK_LIMIT
int		g_link_limit = 15;
#endif


char 	*paramLocalSubnet;
char 	*paramDevName;
u_int8_t 	paramGateway = 1;


static AODV_LIST_HEAD( routeList );
static rwlock_t 	routeLock = RW_LOCK_UNLOCKED;

static AODV_LIST_HEAD( neighList);
static rwlock_t 	neighLock = RW_LOCK_UNLOCKED;

static AODV_LIST_HEAD(devList);

static AODV_LIST_HEAD(floodQueue);
static rwlock_t 	floodLock = RW_LOCK_UNLOCKED;

static AODV_LIST_HEAD(timerQueue);
static rwlock_t 	timerLock = RW_LOCK_UNLOCKED;


static spinlock_t  	taskLock = SPIN_LOCK_UNLOCKED;
static AODV_LIST_HEAD(taskQueue);


aodv_t		myAODV =
{
	devs			:	&devList,
	
	routes		:	&routeList,
	routeLock	:	&routeLock,
	
	neighs		:	&neighList,
	neighLock	:	&neighLock,	

	tasks		:	&taskQueue,
	taskLock		:	&taskLock,
	
	timers		:	&timerQueue,
	timerLock	:	&timerLock,
	
	floods		:	&floodQueue,
	floodLock	:	&floodLock,
};


static void __destroy_aodv(aodv_t *aodv)
{
	aodv_task_t	*task;
	aodv_dev_t 	*aodvDev, *d2;
	aodv_flood_t 	*flood, *f2;
	aodv_route_t 	*route, *r2;
	unsigned long	flags;
	int 			count = 0;
//	del_timer(&(aodv->timer) );
	
	AODV_SPIN_LOCK(aodv->taskLock);
	AODV_LIST_FOR_EACH(task, aodv->tasks, list)
	{
		AODV_LIST_DEL(&(task->list ));
		kfree( task->data);
		task->data = NULL;
		kfree( task);
		task = NULL;
	}
	printk("AODV : Removed Tasks...\n");
	AODV_SPIN_UNLOCK(aodv->taskLock);
	
	AODV_WRITE_LOCK(aodv->floodLock, flags);
	AODV_LIST_FOR_EACH_SAFE(flood, f2, aodv->floods, list )
	{
		AODV_LIST_DEL(&(flood->list) );
		kfree( flood);
		flood = NULL;
		count++;
	}
	AODV_WRITE_UNLOCK(aodv->floodLock, flags);
	printk("AODV : Removed Floods...\n");

	AODV_WRITE_LOCK(aodv->routeLock, flags);
	AODV_LIST_FOR_EACH_SAFE(route, r2, aodv->routes, list)
	{
		AODV_LIST_DEL( &(route->list));

		if(route->type == AODV_ROUTE_AODV )
			aodv_delete_kroute(route);
		kfree( route);
		route = NULL;
	}
	AODV_WRITE_UNLOCK(aodv->routeLock, flags);
	printk("AODV : remove AODV routes...\n");

	AODV_LIST_FOR_EACH_SAFE(aodvDev, d2, aodv->devs, list)
	{
		AODV_LIST_DEL(&aodvDev->list);
		sock_release(aodvDev->sock);
		kfree(aodvDev);
	}
	printk("AODV : Removed Device...\n");
}

static int __init_aodv(aodv_t *aodv)
{
	char* 			str;
	u_int32_t 		localSubnetIp=0;
	u_int32_t 		localSubnetMask=0;
	u_int32_t			lo;
	
	aodv_route_t 		*route;
	
	if (paramDevName==NULL)
	{
		printk("You need to specify which device to use, ie:\n");
		printk("    insmod aodv paramDevName=ath0    \n");
       	return(-1);
	}
	sprintf(aodv->devName, paramDevName );

	aodv->supportGateway = paramGateway;

	inet_aton("255.255.255.255",&(aodv->broadcastIp) );
	
	aodv->epoch = getAodvEpoch();
	init_timer(&(aodv->timer) );

#if 0
	inet_aton("127.0.0.1", &lo);
	route = aodv_route_create(aodv, lo, AODV_ROUTE_KERNEL);
	route->next_hop = lo;
	route->metric = 0;
	route->self_route = TRUE;
	route->route_valid = TRUE;
	route->seq = 0;
#endif

	if (paramLocalSubnet==NULL)
		return -ENXIO;
	else
	{
		str = strchr(paramLocalSubnet,'/');
		if ( str)
		{
			(*str)='\0';
			str++;
#if AODV_DEBUG			
			printk("Adding Local Routed Subnet %s/%s to the routing table...\n",paramLocalSubnet, str);
#endif
			inet_aton(paramLocalSubnet, &localSubnetIp);
			inet_aton(str, &localSubnetMask);

			route = aodv_route_create(aodv, localSubnetIp, AODV_ROUTE_AODV);
			route->route_valid = TRUE;
			route->route_seq_valid = TRUE;
			route->netmask = localSubnetMask;
			route->self_route = TRUE;
			route->metric = 1;
			route->seq = 1;
			route->rreq_id = 1;
			
			aodv->myRoute = route;
			aodv->myIp = route->ip;
		}
	}

	if (!aodv_dev_init(aodv))
	{
		__destroy_aodv( aodv);
		return (-EPERM);
	}

	return 0;
}

static int aodv_init(void)
{
	struct proc_dir_entry *aodv_dir;
	aodv_proc_entry 	*proc = proc_table;
	unsigned char 	proc_name[32];
	aodv_t			*aodv = &myAODV;
	int 				aodv_pid;
	int 				res;

	res = __init_aodv( aodv);
	if(res != 0)
		return res;
	
	init_packet_queue();

#ifdef AODV_SIGNAL
	aodv_iw_sock_init();
#if AODV_DEBUG
	printk("AODV : A Maximum of: %d neighboring nodes' signals can be monitored\n",IW_MAX_SPY);
#endif
#endif

	aodv_netfilter_init();
    
#if AODV_DEBUG
	printk("AODV : Principal IP address - %s\n",inet_ntoa(aodv->myIp));
	if (aodv->supportGateway)
	{
		printk("AODV : Set as AODV Gateway\n");
	}
#endif


	aodv_pid = kernel_thread((void *) &aodv_kthread, (void *)aodv, 0);
	
	aodv_dir = proc_mkdir(AODV_PROC_NAME, NULL);
	aodv_dir->owner = THIS_MODULE;

	while( proc->read )
	{
		sprintf(proc_name, "%s/%s", AODV_PROC_NAME, proc->name);
		proc->proc = create_proc_read_entry( proc_name, 0, NULL, proc->read, (void *)aodv);
		proc->proc->owner=THIS_MODULE;
		proc++;
	}

	printk("______________________________________________________________________________________\n");    
	printk("\nAODV(Ad hoc On Demand Distance Vector) v1.0 --- Wireless Communications Lab, SWJTU.\n");
	printk("--------------------------------------------------------------------------------------\n");    
	
	aodv_timer_insert(aodv, AODV_TASK_HELLO, HELLO_INTERVAL, aodv->myIp);
	aodv_timer_insert(aodv, AODV_TASK_CLEANUP, HELLO_INTERVAL, aodv->myIp);
	return 0;
}


void aodv_cleanup(void)
{
	aodv_proc_entry 	*proc = proc_table;
	unsigned char 	proc_name[32];
	aodv_t			*aodv = &myAODV;

	printk("AODV : Shutting down...\n");
	while( proc->read )
	{
		sprintf(proc_name, "%s/%s", AODV_PROC_NAME, proc->name);
		remove_proc_entry(proc_name, NULL);
		proc++;
	}

	remove_proc_entry(AODV_PROC_NAME , NULL);

	aodv_netfilter_destroy();
	printk("AODV : Unregister NetFilter hooks...\n");
//	close_sock();
	
#ifdef AODV_SIGNAL
	aodv_iw_sock_close();
#endif
	printk("AODV : Close sockets...\n");

	del_timer(&aodv->timer);
	printk("AODV : Remove timer...\n");

	kill_aodv();
	printk("AODV : Killed AODV daemon thread...\n");

	__destroy_aodv( aodv);

}

module_init( aodv_init);
module_exit( aodv_cleanup );

MODULE_PARM(paramDevName,		"s");
MODULE_PARM(paramGateway,		"i");
MODULE_PARM(paramLocalSubnet,	"s");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Li Zhijie");
MODULE_DESCRIPTION("AODV for Wireless MAC Layer");

