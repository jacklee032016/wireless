/*
 * $Id: mesh_fwd_db.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "_fwd.h"

static __inline__ unsigned long __timeout(MESH_FWD_TABLE *table)
{
	unsigned long timeout;

	timeout = jiffies - table->ageing_time;
	if( table->topology_change )
		timeout = jiffies - table->forward_delay;

	return timeout;
}

static __inline__ int __has_expired(MESH_FWD_TABLE *table, MESH_FWD_ITEM *item)
{
	if (!item->isStatic && time_before_eq( item->ageing_timer, __timeout(table)))
		return 1;

	return 0;
}

static __inline__ int __mac_hash(unsigned char *mac)
{
	unsigned long x;

	x = mac[0];
	x = (x << 2) ^ mac[1];
	x = (x << 2) ^ mac[2];
	x = (x << 2) ^ mac[3];
	x = (x << 2) ^ mac[4];
	x = (x << 2) ^ mac[5];

	x ^= x >> 8;

	return x & (MESH_FWD_HASH_SIZE - 1);
}

/* put this item into the header of hash */
static __inline__ void __hash_link(MESH_FWD_TABLE *table, MESH_FWD_ITEM *item, int hash)
{
	item->next_hash = table->hash[hash];
	if( item->next_hash != NULL)
		item->next_hash->pprev_hash = &item->next_hash;
	
	table->hash[hash] = item;
	item->pprev_hash = &table->hash[hash];
}

/* remove this item from the list of hash */
static __inline__ void __hash_unlink(MESH_FWD_ITEM *item)
{
	*(item->pprev_hash) = item->next_hash;
	if( item->next_hash != NULL)
		item->next_hash->pprev_hash = item->pprev_hash;
	
	item->next_hash = NULL;
	item->pprev_hash = NULL;
}

void mesh_fwd_db_put(MESH_FWD_ITEM *item)
{
	if (atomic_dec_and_test(&item->use_count))
		kfree(item);
}

/* change the MAC of Local item with dev */
void mesh_fwd_db_changeaddr(MESH_DEVICE *dev, unsigned char *newaddr)
{
	MESH_FWD_TABLE *table;
	int i;

	table = dev->fwdTable;
	MESH_WRITE_LOCK_BH(&table->hashRwLock);

	for (i=0;i<MESH_FWD_HASH_SIZE;i++)
	{
		MESH_FWD_ITEM *item;

		item = table->hash[i];
		while( item != NULL)
		{
			if( item->dest == dev && item->isLocal)
			{
				__hash_unlink(item);
				memcpy(item->addr.addr, newaddr, ETH_ALEN);

				/* MAC changed, so must be re-hash */
				__hash_link(table, item, __mac_hash(newaddr));
//				write_unlock_bh(&br->hash_lock);
				MESH_WRITE_UNLOCK_BH(&table->hashRwLock);
				return;
			}
			item = item->next_hash;
		}
	}
	
//	write_unlock_bh(&br->hash_lock);
	MESH_WRITE_UNLOCK_BH(table->hashRwLock);
}

void mesh_fwd_db_cleanup(MESH_FWD_TABLE *table)
{
	int i;
	unsigned long timeout;

	timeout = __timeout(table);

	MESH_WRITE_UNLOCK_BH(&table->hashRwLock);

	for (i=0;i<MESH_FWD_HASH_SIZE;i++)
	{
		MESH_FWD_ITEM *item;

		item = table->hash[i];
		
		while( item != NULL)
		{
			MESH_FWD_ITEM *g;

			g = item->next_hash;
			if (!item->isStatic && time_before_eq( item->ageing_timer, timeout))
			{
				__hash_unlink(item);
				/* mempry leap here ??? lzj */
				mesh_fwd_db_put(item );
			}
			item = g;
		}
	}
	
	MESH_WRITE_UNLOCK_BH(&table->hashRwLock);
}

void mesh_fwd_db_delete_by_dev(MESH_FWD_TABLE *table, MESH_DEVICE *dev)
{
	int i;

	MESH_WRITE_LOCK_BH(&table->hashRwLock);

	for (i=0;i<MESH_FWD_HASH_SIZE;i++)
	{
		MESH_FWD_ITEM *item;

		item = table->hash[i];

		while( item != NULL)
		{
			MESH_FWD_ITEM *g;

			g = item->next_hash;
			if( item->dest == dev)
			{
				__hash_unlink(item);
				mesh_fwd_db_put(item);
			}
			item = g;
		}
	}

	MESH_WRITE_UNLOCK_BH(&table->hashRwLock);
}

MESH_FWD_ITEM *mesh_fwd_db_get(MESH_FWD_TABLE *table, unsigned char *addr)
{
	MESH_FWD_ITEM *item;

	MESH_READ_LOCK_BH( &table->hashRwLock);

	item = table->hash[__mac_hash(addr)];
	while( item != NULL)
	{
		if (!memcmp(item->addr.addr, addr, ETH_ALEN))
		{
			if (!__has_expired(table, item) )
			{
				atomic_inc(&item->use_count);

				MESH_READ_UNLOCK_BH(&table->hashRwLock);
				return item;
			}

			MESH_READ_UNLOCK_BH(&table->hashRwLock);
			return NULL;
		}

		item = item->next_hash;
	}

	MESH_READ_UNLOCK_BH(&table->hashRwLock);

	return NULL;
}


static __inline__ void __possibly_replace(MESH_FWD_ITEM *item, MESH_DEVICE *source, int isLocal)
{
	if (!item->isStatic || isLocal)
	{
		item->dest = source;
		item->isLocal = isLocal;
		item->isStatic = isLocal;
		item->ageing_timer = jiffies;
	}
}

void swjtu_mesh_fwd_db_insert(MESH_FWD_TABLE *table, MESH_DEVICE *source, unsigned char *addr, int isLocal, int isStatic )
{
	MESH_FWD_ITEM *item;
	int hash;

	hash = __mac_hash(addr);

	MESH_DEBUG_INFO( MESH_DEBUG_FWD, "Hash of %s is %d\n", source->name, hash);

	MESH_WRITE_LOCK_BH( &table->hashRwLock);
	item = table->hash[hash];

	while (item != NULL)
	{
		if (!memcmp(item->addr.addr, addr, ETH_ALEN) )
		{
			/* attempt to update an entry for a local interface */
			if (item->isLocal)
			{
				if (isLocal)
					MESH_DEBUG_INFO( MESH_DEBUG_FWD, 
						"%s: attempt to add interface with same source address.\n", source->name);
#if 0
				else if (net_ratelimit()) 
					MESH_WARN_INFO( "%s: received packet with own address as source address\n", source->name);
#endif
				goto out;
			}

			__possibly_replace(item, source, isLocal);
			goto out;
		}

		item = item->next_hash;
	}

	MESH_MALLOC(item, MESH_FWD_ITEM, sizeof(MESH_FWD_ITEM), MESH_M_NOWAIT|MESH_M_ZERO,"FWD ITEM" );
	if( item == NULL) 
		goto out;

	memcpy(item->addr.addr, addr, ETH_ALEN);

	atomic_set(& item->use_count, 1);
	item->dest = source;
	item->isLocal = isLocal;
	item->isStatic = isStatic;
	item->ageing_timer = jiffies;

	__hash_link(table, item, hash);

 out:
	MESH_WRITE_UNLOCK_BH(table->hashRwLock);
	
}


static __inline__ void __copy_fdb(struct __mesh_fwd_entry *ent, MESH_FWD_ITEM *item)
{
	memset(ent, 0, sizeof(struct __mesh_fwd_entry) );
	memcpy(ent->mac_addr, item->addr.addr, ETH_ALEN);
	
//	ent->port_no = item->dest? item->dest->port_no:0;
	sprintf(ent->devName, "%s", item->dest->name );
	ent->port_no = item->dest? item->dest->index:0;
	ent->isLocal = item->isLocal;
	ent->ageing_timer_value = 0;
	if (!item->isStatic)
		ent->ageing_timer_value = jiffies - item->ageing_timer;
}

int mesh_fwd_db_get_entries(MESH_FWD_TABLE *table, unsigned char *_buf, int maxnum, int offset)
{
	int i;
	int num;
	struct __mesh_fwd_entry *walk;

	num = 0;
	walk = (struct __mesh_fwd_entry *)_buf;

	MESH_READ_LOCK_BH( &table->hashRwLock);
	
	for (i=0;i<MESH_FWD_HASH_SIZE;i++)
	{
		MESH_FWD_ITEM *item;

		item = table->hash[i];
		
		while( item != NULL && num < maxnum)
		{
			struct __mesh_fwd_entry ent;
			int err = 0;
			MESH_FWD_ITEM *g;
			MESH_FWD_ITEM **pp; 
#if 0
			if (__has_expired(table, item) )
			{
				item = item->next_hash;
				continue;
			}
#endif
			if (offset)
			{/* skip n items */
				offset--;
				item = item->next_hash;
				continue;
			}

			__copy_fdb(&ent, item);

			MESH_DEBUG_INFO( MESH_DEBUG_FWD, "Hash of %s(%s) is %d\n",swjtu_mesh_mac_sprintf(item->addr.addr), item->dest->name, i );

//			atomic_inc(&item->use_count);
#if 0			
			MESH_READ_UNLOCK_BH(&table->hashRwLock);

			err = copy_to_user(walk, &ent, sizeof(struct __mesh_fwd_entry));
#else
			memcpy(walk, &ent, sizeof(struct __mesh_fwd_entry));
#endif
#if 0
			MESH_READ_LOCK_BH(&table->hashRwLock);
#endif

			g = item->next_hash;
			pp = item->pprev_hash;
//			mesh_fwd_db_put( item);

			if (err)
				goto out_fault;

			if (g == NULL && pp == NULL)
				goto out_disappeared;

			num++;
			walk++;

			item = g;
		}
	}

 out:
 	MESH_READ_UNLOCK_BH(&table->hashRwLock);
	return num;

 out_disappeared:
	num = -EAGAIN;
	goto out;

 out_fault:
	num = -EFAULT;
	goto out;
}

