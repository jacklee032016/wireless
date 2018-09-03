/*
* $Id: linux_compat.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/
#ifndef	__LINUX_COMPAT_H__
#define	__LINUX_COMPAT_H__

#define	MESH_M_NOWAIT		0x0001		/* do not block */
#define	MESH_M_WAITOK		0x0002		/* ok to block */
#define	MESH_M_ZERO			0x0100		/* bzero the allocation */


#define	MESH_MALLOC(_ptr, cast, _size, _flags, _msg ) \
 	(_ptr) = kmalloc((_size), (_flags) & MESH_M_NOWAIT ? GFP_ATOMIC : GFP_KERNEL ) ; \
	if (  (_ptr) && ( (_flags) & MESH_M_ZERO)  )	\
		memset((_ptr), 0, (_size) ); \
	else if (!(_ptr) ) 			\
		printk("memory allocated for %s failed\n", _msg)

#define __user		

#define MESH_LIST_HEAD							LIST_HEAD				/* init list head with a variable */
#define MESH_LIST_NODE							struct list_head

#define MESH_LIST_INIT_LIST_HEAD 				INIT_LIST_HEAD			/* init list header with a pointer */
#define MESH_LIST_FOR_EACH						list_for_each_entry
#define MESH_LIST_FOR_EACH_SAFE				list_for_each_entry_safe			/* 4 parameters */
#define MESH_LIST_ENTRY						list_entry
#define MESH_LIST_ADD_HEAD					list_add					/* stack : FILO */
#define MESH_LIST_ADD_TAIL						list_add_tail				/* queue : FIFO */
#define MESH_LIST_DEL							list_del

#define MESH_LIST_CHECK_EMPTY					list_empty


typedef		rwlock_t 				mesh_rw_lock_t;

#define  MESH_READ_LOCK( lock) \
	read_lock(lock )
#define	MESH_READ_UNLOCK(lock) \
	read_unlock(lock)

#define  MESH_READ_LOCK_BH( lock) \
	read_lock_bh(lock )
#define	MESH_READ_UNLOCK_BH(lock) \
	read_unlock_bh(lock)

#define	MESH_WRITE_LOCK( lock, flags) \
	write_lock_irqsave(lock, flags)

#define	MESH_WRITE_UNLOCK(lock, flags) \
	write_unlock_irqrestore(lock, flags)

#define	MESH_WRITE_LOCK_BH(lock)	\
	write_lock_bh( lock);

#define	MESH_WRITE_UNLOCK_BH(lock)	\
	write_unlock_bh( lock);

typedef spinlock_t mesh_lock_t;

#define	MESH_LOCK_INIT(obj, _name)	spin_lock_init(&(obj)->lock)
#define	MESH_LOCK_DESTROY(obj)
#define	MESH_LOCK(obj)				spin_lock_bh(&(obj)->lock)
#define	MESH_UNLOCK(obj)			spin_unlock_bh(&(obj)->lock)

/* task defined as tasklet */
typedef	struct tasklet_struct	MESH_TASK;

#define	MESH_TASK_INIT(name, function, data)	\
		tasklet_init((name),(function),(unsigned long)(data))

//DECLARE_TASKLET(name, function, data)	
		
/* data is not used when TASK is implemented with tasklet(not taskqueue in IMMEDIATE queue) */
#define	MESH_TASK_SCHEDULE(name, data)	\
		tasklet_schedule((name))

#define	SWJTU_MESH_DEBUG

#ifdef SWJTU_MESH_DEBUG
#define assert(expr) \
        if(!(expr)){					\
		printk(KERN_ERR "Assertion failed! %s,%s,%s,line=%d\n",	\
		#expr,__FILE__,__FUNCTION__,__LINE__);		\
        }
#else
#define	assert(expr)			do {} while (0)
#endif

#define	MESH_PROC_CREATE_ENTRY( procEntry, data) \
do{	char	name[32]; \
		if((procEntry)->read){sprintf(name, "%s/%s", SWJTU_MESH_PROC_NAME, (procEntry)->name ); \
		(procEntry)->proc = create_proc_read_entry(name, 0, NULL, (procEntry)->read, (void *)data); \
		(procEntry)->proc->owner=THIS_MODULE;	\
	}\
}while(0)
	
#endif

