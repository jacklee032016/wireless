/*
* $Id: route_init.c,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
*/

#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <asm/delay.h>

#include "mesh_route.h"
#include "_route.h"

#ifdef LINK_LIMIT
int		g_link_limit = 15;
#endif


char 	*paramLocalSubnet;
u_int8_t 	paramGateway = 1;


static ROUTE_LIST_HEAD( routeList );
static rwlock_t 	routeLock = RW_LOCK_UNLOCKED;

static ROUTE_LIST_HEAD( neighList);
static rwlock_t 	neighLock = RW_LOCK_UNLOCKED;

static ROUTE_LIST_HEAD(devList);
static rwlock_t 	devLock = RW_LOCK_UNLOCKED;

static ROUTE_LIST_HEAD(floodQueue);
static rwlock_t 	floodLock = RW_LOCK_UNLOCKED;

static ROUTE_LIST_HEAD(timerQueue);
static rwlock_t 	timerLock = RW_LOCK_UNLOCKED;


static spinlock_t  	taskLock = SPIN_LOCK_UNLOCKED;
static ROUTE_LIST_HEAD(taskQueue);


route_engine_t		myAODV =
{
	debug		:	0xFFFFFFFF,
		
	procs		:	proc_table,
	mgmtOps		:	&engine_mops,
	protoOps		:	&aodv_ops,

	devs			:	&devList,
	devLock		:	&devLock,
	
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

static int __init mesh_route_init(void)
{
	struct proc_dir_entry *aodv_dir;
	route_engine_t	*engine = &myAODV;
	int 				aodv_pid;
	int 				res;

	sprintf(engine->name, ROUTE_PROC_NAME );

	engine->supportGateway = paramGateway;

	aodv_dir = proc_mkdir(ROUTE_PROC_NAME, NULL);
	aodv_dir->owner = THIS_MODULE;

	res = engine_init( engine, paramLocalSubnet);
	if(res != 0)
		return res;
	
	aodv_pid = kernel_thread((void *) &aodv_kthread, (void *)engine, 0);
	
#ifdef ROUTE_SIGNAL
	aodv_iw_sock_init();
	ROUTE_DPRINTF(engine, ROUTE_DEBUG_INIT, "A Maximum of: %d neighboring nodes' signals can be monitored\n",IW_MAX_SPY);
#endif

    
	ROUTE_DPRINTF(engine, ROUTE_DEBUG_INIT, "Principal IP address - %s\n",inet_ntoa(engine->myIp));
	if (engine->supportGateway)
	{
		ROUTE_DPRINTF(engine,ROUTE_DEBUG_INIT, "Set as %s Gateway\n", engine->name );
	}


	printk("______________________________________________________________________________________\n");    
	printk("\nAODV(Ad hoc On Demand Distance Vector) v1.0 --- Wireless Communications Lab, SWJTU.\n");
	printk("--------------------------------------------------------------------------------------\n");    
	
	return 0;
}


void __exit mesh_route_cleanup(void)
{
	route_engine_t			*engine = &myAODV;

	printk("%s :Shutting down...\n", engine->name);

	remove_proc_entry(ROUTE_PROC_NAME , NULL);
	
#ifdef ROUTE_SIGNAL
	aodv_iw_sock_close();
#endif

	del_timer(&engine->timer);
	ROUTE_DPRINTF(engine, ROUTE_DEBUG_INIT, "Remove %s", "timer...\n");

	kill_aodv();
	ROUTE_DPRINTF(engine, ROUTE_DEBUG_INIT, "Killed %s daemon thread...\n", engine->name );

	engine->mgmtOps->destroy(engine);;
}

int route_register_device(route_dev_t *routeDev)
{
	route_engine_t  *engine = &myAODV;
	mesh_route_t  *route;
	unsigned long	flags;
	
	routeDev->engine = engine;

	route = engine->mgmtOps->route_insert(engine, routeDev->ip, ROUTE_TYPE_KERNEL);
	if(route==NULL)
	{
		return (-ENOMEM);
	}

	routeDev->ops->init(routeDev);
	
	route->ip = routeDev->ip;
//	route->netmask = routeDev->netmask;// calculate_netmask(0); //ifa->ifa_mask;
	route->self_route = TRUE;
	route->seq = 1;
	route->old_seq = ROUTE_SEQUENCE_UNKNOWN;
	route->rreq_id = 1;
	route->metric = 0;
	route->nextHop = route->ip;
	route->lifetime = -1;
	route->route_valid = TRUE;
	route->route_seq_valid = 1;
	route->dev = routeDev;
	routeDev->myroute = route;
	
	ROUTE_WRITE_LOCK(engine->devLock, flags);
	if(ROUTE_LIST_CHECK_EMPTY(engine->devs) )
	{
		engine->mgmtOps->timer_insert(engine, ROUTE_TASK_HELLO, engine->myIp, HELLO_INTERVAL);
		engine->mgmtOps->timer_insert(engine, ROUTE_TASK_CLEANUP, engine->myIp, HELLO_INTERVAL);
	}	
	ROUTE_LIST_ADD_HEAD(&(routeDev->list) , engine->devs );

	ROUTE_WRITE_UNLOCK(engine->devLock, flags);

	return 0;
}

void route_unregister_device(route_dev_t *routeDev)
{
	route_engine_t *engine = &myAODV;
	route_dev_t	*dev, *d2;
	unsigned long	flags;

	engine->mgmtOps->route_delete(engine, routeDev->myroute->ip);
	
	ROUTE_WRITE_LOCK(engine->devLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(dev, d2, engine->devs, list)
	{
		if(dev==routeDev)
		{
			ROUTE_LIST_DEL(&routeDev->list);
			routeDev->ops->destroy(routeDev);
		}
	}
	if(ROUTE_LIST_CHECK_EMPTY(engine->devs) )
	{
		/* cleanup flood queue and route table */
		engine->mgmtOps->timer_insert(engine, ROUTE_TASK_CLEANUP, engine->myIp, 0);
	}
	ROUTE_WRITE_UNLOCK(engine->devLock, flags);
}

route_dev_t	*route_dev_lookup(struct net_device *dev)
{
	route_engine_t *engine = &myAODV;
	return engine->dev_lookup(engine, dev);
}

EXPORT_SYMBOL(route_register_device);
EXPORT_SYMBOL(route_unregister_device);
EXPORT_SYMBOL(route_dev_lookup);

module_init( mesh_route_init);
module_exit( mesh_route_cleanup );

MODULE_PARM(paramGateway,		"i");
MODULE_PARM(paramLocalSubnet,	"s");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Li Zhijie");
MODULE_DESCRIPTION("HWMP for Wireless Mesh MAC Layer");

