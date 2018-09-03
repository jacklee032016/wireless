/*
 * $Id: mesh_fwd_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "_fwd.h"

static int __mesh_fwd_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
#define READ_MAX		10
	MESH_FWD_TABLE		*table = (MESH_FWD_TABLE *)data;
	MESH_FWD_ITEM		*item;
	int 				length = 0;
	unsigned long  	remainder = 0, numerator = 0;
	static int			position = 0;
	char				buf[READ_MAX*sizeof( struct __mesh_fwd_entry)];
	int				count = 0, i;
	struct __mesh_fwd_entry *ent;
	
	if(!table)
		return 0;

	length += sprintf(buffer+length, "\n%s\n---------------------------------------------------------------------------------\n", table->name);
	length += sprintf(buffer+length, "        MAC        |   Device    |   isLocal    |   isStatic    |   Aging time \n");
	length += sprintf(buffer+length, "---------------------------------------------------------------------------------\n");

	count = mesh_fwd_db_get_entries(table, buf, READ_MAX, position );
	if(count < READ_MAX)
		position = 0;
	else
		position += count;

	ent = (struct __mesh_fwd_entry *)buf;

	for(i=0; i< count; i++)
	{
		
		length += sprintf(buffer+length," %02x:%02x:%02x:%02x:%02x:%02x \t%s \t\t%s \t\t%s \t\t%d\n", 
			ent->mac_addr[0], ent->mac_addr[1],ent->mac_addr[2],ent->mac_addr[3],ent->mac_addr[4],ent->mac_addr[5],
			ent->devName,(ent->isLocal)?"Yes":"No", (ent->isStatic)?"Yes":"No", ent->ageing_timer_value);
		ent++;
	}

	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n\n");

	*buffer_location = buffer + offset;
	length -= offset;
	if (length > buffer_length)
		length = buffer_length;
	else if (length < 0)
		length = 0;

	return length;
}


#if 0
static int br_initial_port_cost(struct net_device *dev)
{
	if (!strncmp(dev->name, "lec", 3))
		return 7;

	if (!strncmp(dev->name, "eth", 3))
		return 100;			/* FIXME handle 100Mbps */

	if (!strncmp(dev->name, "plip", 4))
		return 2500;

	return 100;
}
#endif

MESH_FWD_TABLE *swjtu_fwd_init_table(MESH_MGR *mgr, char *name)
{
	MESH_FWD_TABLE *table;

//	MESH_MALLOC(table, MESH_FWD_TABLE, sizeof(MESH_FWD_TABLE), MESH_M_NOWAIT|MESH_M_ZERO, "MESH FWD TABLE");
	MESH_MALLOC(table, MESH_FWD_TABLE, sizeof(MESH_FWD_TABLE), MESH_M_ZERO, "MESH FWD TABLE");
	if (table == NULL)
		return NULL;

	table->mgr = mgr;
//	strncpy(table->name, name, MESHNAMSIZ);
	sprintf(table->name,"%s", name);

	table->lock = RW_LOCK_UNLOCKED;
	table->hashRwLock = RW_LOCK_UNLOCKED;

#if WITH_BR_PORT
	table->bridge_id.prio[0] = 0x80;
	table->bridge_id.prio[1] = 0x00;
	memset(table->bridge_id.addr, 0, ETH_ALEN);

	table->designated_root = table->bridge_id;
#endif

	table->stp_enabled = 0;
	table->root_path_cost = 0;
	table->root_port = 0;
	table->bridge_max_age = table->max_age = 20 * HZ;
	table->bridge_hello_time = table->hello_time = 2 * HZ;
	table->bridge_forward_delay = table->forward_delay = 15 * HZ;
	table->topology_change = 0;
	table->topology_change_detected = 0;
	
	fwd_timer_clear(&table->hello_timer);
	fwd_timer_clear(&table->tcn_timer);
	fwd_timer_clear(&table->topology_change_timer);

	table->ageing_time = 300 * HZ;
	table->gc_interval = 4 * HZ;

	table->procEntry.read = __mesh_fwd_proc;
	sprintf(table->procEntry.name, "%s", table->name ); 

//	MESH_PROC_CREATE_ENTRY(&table->procEntry, (void *)table );

	return table;
}

int swjtu_fwd_add_dev(MESH_FWD_TABLE *table, MESH_DEVICE *dev)
{

#if 0
	if (dev->flags & IFF_LOOPBACK || dev->type != ARPHRD_ETHER)
		return -EINVAL;
#endif

//	if (dev->hard_start_xmit == br_dev_xmit)
//		return -ELOOP;

	mesh_dev_hold(dev);
//	write_lock_bh(&br->lock);
	MESH_WRITE_LOCK_BH(&table->lock);
#if 0
	if ((p = new_nbp(br, dev)) == NULL) {
		write_unlock_bh(&br->lock);
		dev_put(dev);
		return -EXFULL;
	}
#endif

//	dev_set_promiscuity(dev, 1);

//	br_stp_recalculate_bridge_id(br);
	swjtu_mesh_fwd_db_insert(table, dev, dev->dev_addr, 1, 1 );
	
//	if (dev->flags & MESHF_UP )
//		br_stp_enable_port(p);
	MESH_WRITE_UNLOCK_BH( &table->lock);

	return 0;
}

/* remove fwd item from fwd table and decrease the reference count */
int swjtu_fwd_del_dev(MESH_FWD_TABLE *table, MESH_DEVICE *dev)
{
	int retval = 0;

//	br_write_lock_bh(BR_NETPROTO_LOCK);
//	write_lock(&br->lock);
	MESH_WRITE_UNLOCK_BH( &table->lock);
//	br_stp_disable_port(table);

	mesh_fwd_db_delete_by_dev(table, dev);
//	dev_put(dev);
	__mesh_dev_put(dev);

//	br_stp_recalculate_bridge_id(table);

	MESH_WRITE_UNLOCK_BH( &table->lock);
//	write_unlock(&br->lock);
//	br_write_unlock_bh(BR_NETPROTO_LOCK);

	return retval;
}

