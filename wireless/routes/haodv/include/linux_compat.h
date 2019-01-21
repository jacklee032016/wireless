/*
* $Id: linux_compat.h,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/
#ifndef __LINUX_COMPAT_H__
#define __LINUX_COMPAT_H__
/*
 * flags to malloc.
 */
#define	M_NOWAIT	0x0001		/* do not block */
#define	M_WAITOK	0x0002		/* ok to block */
#define	M_ZERO		0x0100		/* bzero the allocation */


#define	MALLOC(_ptr, cast, _size, _flags, _msg ) \
 	(_ptr) = kmalloc((_size), (_flags) & M_NOWAIT ? GFP_ATOMIC : GFP_KERNEL ) ; \
	if (  (_ptr) && ( (_flags) & M_ZERO)  )	\
		memset((_ptr), 0, (_size) ); \
	else if (!(_ptr) ) 			\
		printk("memory allocated for %s failed\n", _msg)
/*
(cast) 
*/
	


#define TRUE								1
#define FALSE 							0

#define ROUTE_LIST_HEAD				LIST_HEAD
#define ROUTE_LIST_NODE				struct list_head
#define ROUTE_LIST_FOR_EACH			list_for_each_entry
#define ROUTE_LIST_FOR_EACH_SAFE		list_for_each_entry_safe
#define ROUTE_LIST_ENTRY				list_entry
#define ROUTE_LIST_ADD_HEAD			list_add				/* stack : FILO */
#define ROUTE_LIST_ADD_TAIL			list_add_tail			/* queue : FIFO */
#define ROUTE_LIST_DEL					list_del
//#define ROUTE_LIST_DEL				
#define ROUTE_LIST_CHECK_EMPTY		list_empty

#endif

