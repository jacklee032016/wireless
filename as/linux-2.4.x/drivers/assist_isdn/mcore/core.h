/*
 *  $Id: core.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/asISDNif.h>
#include "helper.h"
#ifdef AS_ISDN_MEMDEBUG
#include "memdbg.h"
#endif

#define AS_ISDN_DEV_MAJOR				198
#define AS_ISDN_MINOR_CORE			0
#define AS_ISDN_MINOR_RAW_MIN		128
#define AS_ISDN_MINOR_RAW_MAX		255

/* debugging */
#define DEBUG_CORE_FUNC		0x0001
#define DEBUG_DUMMY_FUNC		0x0002
#define DEBUG_DEV_OP			0x0100
#define DEBUG_MGR_FUNC		0x0200
#define DEBUG_DEV_TIMER		0x0400
#define DEBUG_RDATA			0x1000
#define DEBUG_WDATA			0x2000

/* from AS_ISDN_dev.c */

extern int		init_isdn_device(int);
extern int		free_isdn_device(void);
extern AS_ISDNdevice_t	*get_free_rawdevice(void);
extern int		free_device(AS_ISDNdevice_t *dev);

/* from AS_ISDN_stack.c */

extern struct list_head	AS_ISDN_stacklist;
extern struct list_head	AS_ISDN_instlist;

extern void		get_stack_info(struct sk_buff *);
extern int		get_stack_cnt(void);
extern AS_ISDNstack_t	*get_stack4id(u_int);
extern AS_ISDNstack_t	*new_stack(AS_ISDNstack_t *, AS_ISDNinstance_t *);
extern int		release_stack(AS_ISDNstack_t *);
extern int		do_for_all_layers(AS_ISDNstack_t *, u_int, void *);
extern int		change_stack_para(AS_ISDNstack_t *, u_int, AS_ISDN_stPara_t *);
extern void		release_stacks(AS_ISDNobject_t *);
extern int		copy_pid(AS_ISDN_pid_t *, AS_ISDN_pid_t *, u_char *);
extern int		set_stack(AS_ISDNstack_t *, AS_ISDN_pid_t *);
extern int		clear_stack(AS_ISDNstack_t *);
extern int		evaluate_stack_pids(AS_ISDNstack_t *, AS_ISDN_pid_t *);
extern AS_ISDNlayer_t	*getlayer4lay(AS_ISDNstack_t *, int);
extern AS_ISDNinstance_t	*get_instance(AS_ISDNstack_t *, int, int);

/* from AS_ISDN_core.c */

extern struct list_head	AS_ISDN_objectlist;
extern int core_debug;

extern int		register_layer(AS_ISDNstack_t *, AS_ISDNinstance_t *);
extern int		unregister_instance(AS_ISDNinstance_t *);
extern AS_ISDNinstance_t	*get_next_instance(AS_ISDNstack_t *, AS_ISDN_pid_t *);
extern AS_ISDNobject_t	*get_object(int);
extern AS_ISDNinstance_t	*get_instance4id(u_int);
extern int		isdn_alloc_entity(int *);
extern int		isdn_delete_entity(int);
