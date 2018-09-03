/*
* $Id: mesh_mgr_mgmt.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "mesh.h"


#define	MESH_DEBUG_OPTIONS	0xFFFFFFFF
#if 0
\
	MESH_DEBUG_INIT |MESH_DEBUG_PACKET|	\
	MESH_DEBUG_PACKET|MESH_DEBUG_PORT|	\
	MESH_DEBUG_IOCTL
#endif

int 	meshdev_max_backlog = 300;
int 	weight_p = 64;            /* old backlog weight */
/* These numbers are selected based on intuition and some
 * experimentatiom, if you have more scientific way of doing this
 * please go ahead and fix things.
 */
int	no_cong_thresh = 10;
int	no_cong = 20;
int	lo_cong = 100;
int	mod_cong = 290;

static  mesh_rw_lock_t	mgrLock = RW_LOCK_UNLOCKED;

static int __mesh_mgr_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	MESH_MGR		*mgr = (MESH_MGR *)data;
	MESH_DEVICE	*port;
	int 				length = 0;
	unsigned long  	remainder = 0, numerator = 0;
	
	if(! mgr)
		return 0;

	length += sprintf(buffer+length, "\n%s\n---------------------------------------------------------------------------------\n", mgr->name);
	length += sprintf(buffer+length, "        Device      |   MAC    |    Valid    |  Lifetime \n");
	length += sprintf(buffer+length, "---------------------------------------------------------------------------------\n");

	MESH_READ_LOCK(mgr->rwLock);
	MESH_LIST_FOR_EACH( port, &(mgr->ports), node)
	{
		{
			length += sprintf(buffer+length," %s  \t\t%s\n", port->name, swjtu_mesh_mac_sprintf(port->dev_addr ));
		}
	}
	MESH_READ_UNLOCK( mgr->rwLock );

	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n\n");

	*buffer_location = buffer + offset;
	length -= offset;
	if (length > buffer_length)
		length = buffer_length;
	else if (length < 0)
		length = 0;

	return length;
}


static mesh_proc_entry	_mgrProc =
{
	name	:	"meshMgr",
	read		:	__mesh_mgr_proc,
	proc		:	NULL
};

static MESH_MGR	_meshMgr =
{
	isInitted	:	0,	
	name	:	"meshMgr",	
	debug	:	MESH_DEBUG_INIT|MESH_DEBUG_PACKET,

	mgrProc	:	&_mgrProc,
	
	myself	:	NULL
};

struct softmesh_data		softmesh_datas[NR_CPUS] __cacheline_aligned;
struct meshif_rx_stats 		meshdev_rx_stat[NR_CPUS];

int mesh_fwd_handle_frame(struct sk_buff *skb, MESH_DEVICE *dev, MESH_PACKET *packet);

static MESH_PACKET mesh_fwd_packet_type =
{
//	type:	__constant_htons(0),
	type:	__constant_htons(ETH_P_ALL),			/* all default packet are process */
	func:	mesh_fwd_handle_frame,
	data:	(void *)NULL, 					/* understand shared skbs */
};


MESH_MGR	*swjtu_get_mgr(void )
{
	MESH_MGR *mgr = &_meshMgr;

	if(!mgr->isInitted )
	{
		MESH_NODE 	*myself;
		MESH_FWD_TABLE	*fwdTable;

		fwdTable = swjtu_fwd_init_table(mgr, SWJTU_MESH_NAME_FWD );
		assert(fwdTable);

		mgr->fwdTable = fwdTable;
		mgr->isInitted = 1;
		mgr->debug = MESH_DEBUG_OPTIONS;
		mgr->portal = NULL;
		
		init_MUTEX(&(mgr->dev.inSema) );
		init_MUTEX(&(mgr->dev.outSema) );
		init_MUTEX(&(mgr->dev.ioctrlSema) );
		mgr->do_ioctl = swjtu_mgr_ioctl;
		mgr->dev.index = SWJTU_DEVICE_MINOR_MGR;

		mgr->add_portal = swjtu_create_portal;
		mgr->remove_portal = swjtu_remove_portal;
		memcpy(mgr->id.meshAddr, SWJTU_MESH_DEFAULT_MESHID, sizeof(SWJTU_MESH_DEFAULT_MESHID) );

		MESH_LIST_INIT_LIST_HEAD(&(mgr->neighbors) );
		MESH_LIST_INIT_LIST_HEAD(&(mgr->ports) );

		mgr->rwLock = &mgrLock;
		MESH_LOCK_INIT(mgr, "MGR");

		MESH_TASK_INIT(&mgr->rxTask, swjtu_mesh_rx_action, mgr);
		MESH_TASK_INIT(&mgr->txTask, swjtu_mesh_tx_action, mgr);

		mgr->ptype_all = NULL;
		
		MESH_MALLOC(myself, (MESH_NODE *), sizeof(MESH_NODE), MESH_M_NOWAIT|MESH_M_ZERO, "MESH NODE");
		if(!myself )
		{
			return NULL;//-ENOMEM;
		}
		mgr->myself = myself;

#if 1
		swjtu_mesh_register_packet(&mesh_fwd_packet_type );
#endif

	}
	
	return mgr;
}

MESH_DEVICE *swjtu_get_port_from_mgr(int index)
{
	MESH_MGR  *mgr = swjtu_get_mgr();
	MESH_DEVICE	*port;
//	unsigned long		flags;

//	MESH_LOCK(mgr);
	MESH_READ_LOCK(mgr->rwLock);
	MESH_LIST_FOR_EACH(port, &(mgr->ports), node)
	{
		if(port->index == index)
		{
//			MESH_UNLOCK( mgr);
			MESH_READ_UNLOCK(mgr->rwLock);
			return port;
		}	
	}

//	MESH_UNLOCK(mgr);
	MESH_READ_UNLOCK(mgr->rwLock );
	return NULL;
}


MESH_DEVICE *swjtu_get_port_by_name(char *devname)
{
	MESH_MGR  *mgr = swjtu_get_mgr();
	MESH_DEVICE	*port;

	if(!devname || !strlen(devname) )
	{
		MESH_WARN_INFO("No name is provided when get MESH DEVICE\n");
		return NULL;
	}
	
	MESH_READ_LOCK(mgr->rwLock);
	MESH_LIST_FOR_EACH(port, &(mgr->ports), node)
	{
		if( !strcmp(port->name, devname) )
		{
			MESH_READ_UNLOCK(mgr->rwLock);
			return port;
		}	
	}

	MESH_READ_UNLOCK(mgr->rwLock );
	return NULL;
}

void swjtu_mesh_register_packet(MESH_PACKET *pt)
{
	MESH_MGR	*mgr;
	int hash;
	unsigned long flags;

	mgr = swjtu_get_mgr();

	MESH_WRITE_LOCK(mgr->rwLock, flags);
//	br_write_lock_bh(BR_NETPROTO_LOCK);

#ifdef CONFIG_NET_FASTROUTE
	/* Hack to detect packet socket */
	if ((pt->data) && ((int)(pt->data)!=1))
	{
		netdev_fastroute_obstacles++;
		dev_clear_fastroute(pt->dev);
	}
#endif

	if (pt->type == htons(ETH_P_ALL))
	{
//		netdev_nit++;
		pt->next= mgr->ptype_all;
		mgr->ptype_all = pt;
	}
	else
	{
		hash=ntohs(pt->type)&15;
		pt->next = mgr->ptype_base[hash];
		mgr->ptype_base[hash] = pt;
	}

//	br_write_unlock_bh(BR_NETPROTO_LOCK);
	MESH_WRITE_UNLOCK(mgr->rwLock, flags);
}


void swjtu_mesh_unregister_packet(MESH_PACKET *pt)
{
	MESH_PACKET **pt1;
	MESH_MGR	*mgr = swjtu_get_mgr();
	unsigned long	flags;

//	br_write_lock_bh(BR_NETPROTO_LOCK);
	MESH_WRITE_LOCK(mgr->rwLock, flags);

	if (pt->type == htons(ETH_P_ALL))
	{
//		netdev_nit--;
		pt1=&(mgr->ptype_all);
	}
	else
	{
		pt1=&(mgr->ptype_base[ntohs(pt->type)&15]);
	}

	for (; (*pt1) != NULL; pt1 = &((*pt1)->next))
	{
		if (pt == (*pt1))
		{
			*pt1 = pt->next;
#ifdef CONFIG_NET_FASTROUTE
			if (pt->data)
				netdev_fastroute_obstacles--;
#endif
//			br_write_unlock_bh(BR_NETPROTO_LOCK);
			MESH_WRITE_UNLOCK(mgr->rwLock, flags );
			return;
		}
	}
//	br_write_unlock_bh(BR_NETPROTO_LOCK);
	MESH_WRITE_UNLOCK(mgr->rwLock, flags );
	MESH_WARN_INFO( "remove mesh packet : %p not found.\n", pt);
}

