/*
* $Id: route_init.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
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


mgmt_ops_t	node_mops =
{
	neigh_delete 			: 	_neigh_delete,
	neigh_insert			:	_neigh_insert,
	neigh_lookup			:	_neigh_lookup,
	neigh_lookup_mac		:	_neigh_lookup_mac,
	neigh_update_link		:	_neigh_update_link,
	neigh_update_route	:	_neigh_update_route,

	route_delete			:	_route_delete,
	route_insert			:	_route_insert,
	route_lookup			:	_route_lookup,
	route_update			:	_route_update,

	timer_insert			:	_timer_insert,
	timer_delete			:	_timer_delete,
	timers_update			:	_timer_update,
	timer_lookup			:	_timer_lookup,
	
	flood_insert			:	_flood_insert,
	flood_lookup			:	_flood_lookup,

	task_insert			:	_task_enqueue,
	task_insert_timer		:	_task_insert_from_timer,

	init					:	NULL,
	destroy 				:	_node_destroy
};

task_ops_t		taskOps =
{
	rx_request	:	_dev_rx_request,
	rx_reply		:	_dev_rx_reply,
	rx_reply_ack	:	_dev_rx_reply_ack,
	rx_hello		:	_dev_rx_hello,
	rx_error		:	_dev_rx_error,

	send_reply	:	_dev_send_reply,

	send_request	:	_dev_send_request

};


route_node_t		myAODV =
{
	debug		:	0, //0xFFFFFFFF,
		
	procs		:	proc_table,
	mgmtOps		:	&node_mops,
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
	route_node_t	*node = &myAODV;
	int 				aodv_pid;
	int 				res;

	sprintf(node->name, ROUTE_PROC_NAME );

	node->supportGateway = paramGateway;

	aodv_dir = proc_mkdir(ROUTE_PROC_NAME, NULL);
	aodv_dir->owner = THIS_MODULE;

	res = node_init( node, paramLocalSubnet);
	if(res != 0)
		return res;
	
	aodv_pid = kernel_thread((void *) &aodv_kthread, (void *)node, 0);
	
#ifdef ROUTE_SIGNAL
	aodv_iw_sock_init();
	ROUTE_DPRINTF(node, ROUTE_DEBUG_INIT, "A Maximum of: %d neighboring nodes' signals can be monitored\n",IW_MAX_SPY);
#endif

    
	ROUTE_DPRINTF(node, ROUTE_DEBUG_INIT, "Principal IP address - %s\n",inet_ntoa(node->address.address));
	if (node->supportGateway)
	{
		ROUTE_DPRINTF(node,ROUTE_DEBUG_INIT, "Set as %s Gateway\n", node->name );
	}

	mesh_sysctl_register(node);

	printk("______________________________________________________________________________________\n");    
	printk("\nAODV(Ad hoc On Demand Distance Vector) v1.0 --- Wireless Communications Lab, SWJTU.\n");
	printk("--------------------------------------------------------------------------------------\n");    
	
	return 0;
}


void __exit mesh_route_cleanup(void)
{
	route_node_t			*node = &myAODV;

	printk("%s :Shutting down...\n", node->name);

	remove_proc_entry(ROUTE_PROC_NAME , NULL);
	
#ifdef ROUTE_SIGNAL
	aodv_iw_sock_close();
#endif

	del_timer(&node->timer);
	ROUTE_DPRINTF(node, ROUTE_DEBUG_INIT, "Remove %s", "timer...\n");

	kill_aodv();
	ROUTE_DPRINTF(node, ROUTE_DEBUG_INIT, "Killed %s daemon thread...\n", node->name );

	node->mgmtOps->destroy(node);

	mesh_sysctl_unregister( node);
	
}

int route_register_device(route_dev_t *routeDev)
{
	route_node_t  *node = &myAODV;
	mesh_route_t  *route;
	unsigned long	flags;
	
	routeDev->node = node;
	routeDev->taskOps = &taskOps;
#if 1
	route = node->mgmtOps->route_insert(node, NULL, &routeDev->address, ROUTE_TYPE_KERNEL);
	if(route==NULL)
	{
		return (-ENOMEM);
	}
#endif
	routeDev->ops->init(routeDev);
#if 1	
	ADDRESS_ASSIGN( &route->address, &routeDev->address);
//	route->netmask = routeDev->netmask;// calculate_netmask(0); //ifa->ifa_mask;
	route->selfRoute = TRUE;
	route->seq = 1;
	route->oldSeq = ROUTE_SEQUENCE_UNKNOWN;
	route->reqId = 1;
	route->metric = 0;
	route->nextHop = NULL;//route->ip;
	route->lifetime = -1;
	route->routeValid = TRUE;
	route->routeSeqValid = 1;
	route->dev = routeDev;
//	routeDev->myroute = route;
#endif

	ROUTE_WRITE_LOCK(node->devLock, flags);
	if(ROUTE_LIST_CHECK_EMPTY(node->devs) )
	{/* is the first dev in this node */
		node->mgmtOps->timer_insert(node, &node->address, ROUTE_MGMT_TASK_HELLO, HELLO_INTERVAL);
		node->mgmtOps->timer_insert(node, &node->address, ROUTE_MGMT_TASK_CLEANUP, HELLO_INTERVAL);
	}	
	ROUTE_LIST_ADD_HEAD(&(routeDev->list) , node->devs );

	ROUTE_WRITE_UNLOCK(node->devLock, flags);

	return 0;
}

void route_unregister_device(route_dev_t *routeDev)
{
	route_node_t *node = &myAODV;
	route_dev_t	*dev, *d2;
	unsigned long	flags;

	node->mgmtOps->route_delete(node, &routeDev->address);
	
	/* remove all route of this device synchronized */
	if(ROUTE_LIST_CHECK_EMPTY(node->devs) )
	{
		/* cleanup flood queue and route table */
		node->mgmtOps->timer_insert(node, &routeDev->address, ROUTE_MGMT_TASK_CLEANUP, 0);
	}
	
	ROUTE_WRITE_LOCK(node->devLock, flags);
	ROUTE_LIST_FOR_EACH_SAFE(dev, d2, node->devs, list)
	{
		if(dev==routeDev)
		{
			ROUTE_LIST_DEL(&routeDev->list);
			routeDev->ops->destroy(routeDev);
		}
	}
	ROUTE_WRITE_UNLOCK(node->devLock, flags);
}

route_dev_t	*route_dev_lookup(struct net_device *dev)
{
	route_node_t *node = &myAODV;
	return node->dev_lookup(node, dev);
}

EXPORT_SYMBOL(route_register_device);
EXPORT_SYMBOL(route_unregister_device);
EXPORT_SYMBOL(route_dev_lookup);
EXPORT_SYMBOL(mesh_dump_pdu);

module_init( mesh_route_init);
module_exit( mesh_route_cleanup );

MODULE_PARM(paramGateway,		"i");
MODULE_PARM(paramLocalSubnet,	"s");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Li Zhijie");
MODULE_DESCRIPTION("HWMP for Wireless Mesh MAC Layer");

