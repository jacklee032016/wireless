/*
* $Id: mesh_mgr_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
* Core services for Mesh MAC
*/

#include "mesh.h"

extern	struct file_operations swjtu_mesh_fops;

static int __sprintf_stats(char *buffer, MESH_DEVICE *dev)
{
	MESH_DEVICE_STATS *stats = (dev->get_stats ? dev->get_stats(dev): NULL);
	int size;
	
	if (stats)
		size = sprintf(buffer, "%6s:%8lu %7lu %4lu %4lu %4lu %5lu %10lu %9lu %8lu %7lu %4lu %4lu %4lu %5lu %7lu %10lu\n",
 		   dev->name,
		   stats->rx_bytes,
		   stats->rx_packets, stats->rx_errors,
		   stats->rx_dropped + stats->rx_missed_errors,
		   stats->rx_fifo_errors,
		   stats->rx_length_errors + stats->rx_over_errors
		   + stats->rx_crc_errors + stats->rx_frame_errors,
		   stats->rx_compressed, stats->multicast,
		   stats->tx_bytes,
		   stats->tx_packets, stats->tx_errors, stats->tx_dropped,
		   stats->tx_fifo_errors, stats->collisions,
		   stats->tx_carrier_errors + stats->tx_aborted_errors
		   + stats->tx_window_errors + stats->tx_heartbeat_errors,
		   stats->tx_compressed);
	else
		size = sprintf(buffer, "%6s: No statistics available.\n", dev->name);

	return size;
}

/*	/proc/mesh/dev  */
static int _mesh_dev_get_info(char *buffer, char **start, off_t offset, int length,int *eof,void *data)
{
	int len = 0;
	off_t begin = 0;
	off_t pos = 0;
	int size;
	MESH_DEVICE *dev;
	MESH_MGR	*mgr = (MESH_MGR *)data;//swjtu_get_mgr();


	size = sprintf(buffer, 
		"Inter-|   Receive                                                |  Transmit\n"
		" face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n");
	
	pos += size;
	len += size;
	
	MESH_READ_LOCK(&mgr->lock);

	MESH_LIST_FOR_EACH(dev, &(mgr->ports), node )
	{
		size = __sprintf_stats(buffer+len, dev);
		len += size;
		pos = begin + len;
				
		if (pos < offset)
		{
			len = 0;
			begin = pos;
		}

		if (pos > offset + length)
			break;
	}
	MESH_READ_UNLOCK(&mgr->lock);
		
	*start = buffer + (offset - begin);
	len -= (offset - begin);
	if (len > length)
		len = length;
	if (len < 0)
		len = 0;
	return len;
}

static mesh_proc_entry  _dev_proc =
{
	name	:	"dev",
	read		:	_mesh_dev_get_info,
	proc		:	NULL
};

int	mesh_register_protocol(MESH_MGR *mgr, MESH_NODE *myself)
{
	return 0;
}

int	swjtu_mesh_register_port( MESH_DEVICE *port)
{
	MESH_DEVICE	*tmp;
	int				index = 0;
	MESH_MGR 		*mgr = swjtu_get_mgr(); 
	unsigned long		flags;
	
	if(!port )
	{
		MESH_WARN_INFO("No MESH device is found\n");
		return -ENOENT;
	}

//	MESH_WRITE_LOCK_BH(mgr->rwLock);//, flags);
	MESH_LOCK(mgr);
	port->mgr = mgr;
	
	if(port->type == MESH_TYPE_TRANSFER && MESH_LIST_CHECK_EMPTY( &mgr->ports) )
	{/* MAC of MESH MGR(MESH node) are set as MAC address of first MESH TRANSFOR device */
		memcpy(mgr->id.meshAddr, port->dev_addr, ETH_ALEN );
	}

	MESH_LIST_FOR_EACH(tmp, &(mgr->ports), node )
	{
		if(!strcmp(tmp->name, port->name ) )
		{
			MESH_WARN_INFO("Mesh Device with name as %s has exist\n", port->name);
//			MESH_WRITE_UNLOCK_BH(mgr->rwLock);//, flags);
			MESH_UNLOCK(mgr);//, flags);
			return -EEXIST;
		}
		index ++;
	}

	MESH_LIST_ADD_HEAD(&(port->node), &(mgr->ports) );
//	MESH_WRITE_UNLOCK_BH(mgr->rwLock);//, flags);
	MESH_UNLOCK(mgr);//, flags);
		
	port->index = index+1;	/* so port->index is begin from 1: this number is used as device minor number */

	init_MUTEX(&(port->dev.inSema) );
	init_MUTEX(&(port->dev.outSema) );
	init_MUTEX(&(port->dev.ioctrlSema) );
	port->dev.index = port->index;


	port->fwdTable = mgr->fwdTable;
	swjtu_fwd_add_dev(mgr->fwdTable, port);
#if 0
	if( port->procEntry.read )
	{
//		sprintf(proc_name, "%s/%d", SWJTU_MESH_PROC_NAME, port->index);
		sprintf(proc_name, "%s/%s", SWJTU_MESH_PROC_NAME, port->name );
		port->procEntry.proc = create_proc_read_entry( proc_name, 0, NULL, port->procEntry.read, (void *)port );
		port->procEntry.proc->owner=THIS_MODULE;
	}
#else	
	MESH_PROC_CREATE_ENTRY(&port->procEntry, (void *)port);
#endif

	return 0;
}


void	swjtu_mesh_unregister_port(MESH_DEVICE *port)
{
//	MESH_DEVICE	*tmp;
//	int				index = 0;
	unsigned long		flags;
	char				proc_name[SWJTU_MESH_NAME_LENGTH];
	MESH_MGR 		*mgr = swjtu_get_mgr(); 
	
	if(!port )
	{
		MESH_WARN_INFO("No MESH device is found when unregister mesh port\n");
		return ;//-ENOENT;
	}

	swjtu_fwd_del_dev(mgr->fwdTable, port);
	if( port->procEntry.read )
	{
		sprintf(proc_name, "%s/%d", SWJTU_MESH_PROC_NAME, port->index);
		remove_proc_entry( proc_name, NULL);
	}

//	MESH_WRITE_LOCK_BH(mgr->rwLock);//, flags);
	MESH_LOCK(mgr);//, flags);
	MESH_LIST_DEL(&(port->node) );
//	MESH_WRITE_UNLOCK_BH(mgr->rwLock);//, flags);
	MESH_UNLOCK(mgr);//, flags);
	
	return;
}

static int __init init_mesh(void)
{
	MESH_MGR 			*mgr = swjtu_get_mgr();
	mesh_proc_entry		*proc = mgr->mgrProc;
	struct proc_dir_entry 	*meshRootDir;
	int	i;

	if(!mgr)
		return -EINVAL;
	
	/*Initialise the packet receive queues.	 */
	for (i = 0; i < NR_CPUS; i++)
	{
		struct softmesh_data *queue;

		queue = &softmesh_datas[i];
		skb_queue_head_init(&queue->input_pkt_queue);
		queue->throttle = 0;
		queue->cng_level = 0;
		queue->avg_blog = 10; /* arbitrary non-zero */
		queue->completion_queue = NULL;
		MESH_LIST_INIT_LIST_HEAD(&queue->poll_list);
		set_bit(__MESH_LINK_STATE_START, &queue->blog_dev.state);
		queue->blog_dev.weight = weight_p;
		queue->blog_dev.poll = swjtu_mesh_process_backlog;
		
		sprintf(queue->blog_dev.name, "%s", "backLog");
		
		queue->blog_dev.mgr = mgr;
		
		atomic_set(&queue->blog_dev.refcnt, 1);
	}

	swjtu_mesh_sysctl_register( mgr);
	
	if (register_chrdev(SWJTU_MESH_DEV_MAJOR, SWJTU_MESH_DEV_NAME, &swjtu_mesh_fops))
	{
		MESH_WARN_INFO("Could not register devices %s\n", SWJTU_MESH_DEV_NAME);
		return(-EIO);
	}

	meshRootDir = proc_mkdir(SWJTU_MESH_PROC_NAME, NULL);
	meshRootDir->owner = THIS_MODULE;

#if 0
	sprintf(proc_name, "%s/%s", SWJTU_MESH_PROC_NAME, mgr->name);
	proc->proc = create_proc_read_entry( proc_name, 0, NULL, proc->read, (void *)mgr);
	proc->proc->owner=THIS_MODULE;
#else
	MESH_PROC_CREATE_ENTRY(proc, (void *)mgr);
#endif

	MESH_PROC_CREATE_ENTRY(&_dev_proc, (void *)mgr );

	MESH_PROC_CREATE_ENTRY(&mgr->fwdTable->procEntry, (void *)mgr->fwdTable );

	MESH_DEBUG_INFO(MESH_DEBUG_INIT,"(MESH) : %s\n", MESH_VERSION);
	return 0;
}


static void __exit exit_mesh(void)
{
	MESH_MGR		*mgr = swjtu_get_mgr();
	MESH_DEVICE	*port, *tmp;
	int 				err = 0;

	remove_proc_entry(SWJTU_MESH_PROC_NAME, NULL);

	MESH_LOCK(mgr);
	MESH_LIST_FOR_EACH_SAFE(port, tmp, &(mgr->ports), node)
	{
		swjtu_mesh_unregister_port( port);
		MESH_LIST_DEL( &(port->node) );
	}
	MESH_UNLOCK( mgr);
		
	if ((err = unregister_chrdev(SWJTU_MESH_DEV_MAJOR, SWJTU_MESH_DEV_NAME)))
	{
		MESH_WARN_INFO("devices %s busy on remove\n",SWJTU_MESH_DEV_NAME);
	}

	MESH_DEBUG_INFO( MESH_DEBUG_INIT, "driver unloaded\n" );
	swjtu_mesh_sysctl_unregister( mgr);
	
	return ;
}


module_init(init_mesh);
module_exit(exit_mesh);


MODULE_AUTHOR("Zhijie Li");
MODULE_DESCRIPTION("Mesh Core Module");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Wireless Communication Lab,SWJTU");
#endif


EXPORT_SYMBOL(swjtu_mesh_register_port);
EXPORT_SYMBOL(swjtu_mesh_unregister_port);
EXPORT_SYMBOL(meshw_ioctl_process);

EXPORT_SYMBOL(swjtu_get_mgr);

EXPORT_SYMBOL(swjtu_mesh_fwd_db_insert);

EXPORT_SYMBOL(swjtu_meshif_rx);

EXPORT_SYMBOL(swjtu_mesh_mac_sprintf);
EXPORT_SYMBOL(swjtu_mesh_dump_rawpkt);

