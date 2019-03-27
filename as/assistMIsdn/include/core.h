/*
 *  $Id: core.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include <linux/slab.h>
#include <linux/string.h>
#include "asISDN.h"
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

#define FLG_MGR_SETSTACK			1
#define FLG_MGR_OWNSTACK			2

#define FLG_MGR_TIMER_INIT			1
#define FLG_MGR_TIMER_RUNING		2


/* from AS_ISDN_dev.c */

extern int		init_isdn_device(int);

/* from AS_ISDN_stack.c */

extern struct list_head	AS_ISDN_stacklist;
extern struct list_head	AS_ISDN_instlist;

extern void		get_stack_info(struct sk_buff *);
extern int		get_stack_cnt(void);
extern as_isdn_stack_t	*get_stack4id(u_int);
extern as_isdn_stack_t	*new_stack(as_isdn_stack_t *, as_isdn_instance_t *);
extern int		release_stack(as_isdn_stack_t *);
extern int		do_for_all_layers(as_isdn_stack_t *, u_int, void *);
extern int		change_stack_para(as_isdn_stack_t *, u_int, AS_ISDN_stPara_t *);
extern void		release_stacks(as_isdn_object_t *);
extern int		copy_pid(AS_ISDN_pid_t *, AS_ISDN_pid_t *, u_char *);
extern int		set_stack(as_isdn_stack_t *, AS_ISDN_pid_t *);
extern int		clear_stack(as_isdn_stack_t *);
extern int		evaluate_stack_pids(as_isdn_stack_t *, AS_ISDN_pid_t *);
extern as_isdn_layer_t	*getlayer4lay(as_isdn_stack_t *, int);
extern as_isdn_instance_t	*get_instance(as_isdn_stack_t *, int, int);

/* from AS_ISDN_core.c */

extern struct list_head	AS_ISDN_objectlist;
extern int core_debug;

extern int		register_layer(as_isdn_stack_t *, as_isdn_instance_t *);
extern int		unregister_instance(as_isdn_instance_t *);

extern as_isdn_instance_t	*get_next_instance(as_isdn_stack_t *, AS_ISDN_pid_t *);
extern as_isdn_object_t	*get_object(int);
extern as_isdn_instance_t	*get_instance4id(u_int);
extern int		isdn_alloc_entity(int *);
extern int		isdn_delete_entity(int);

extern as_isdn_object_t		udev_obj;
extern int 				device_debug ;
extern as_isdn_device_t		mainDev;
extern struct file_operations as_isdn_fops;

/* sleep in user space until woken up. Equivilant of tsleep() in BSD */
int  as_schedule_delay(wait_queue_head_t *q);

int dev_remove_timer(as_isdn_device_t *dev, u_int id);
void dev_free_timer(as_isdn_timer_t *ht);
int dev_del_timer(as_isdn_device_t *dev, u_int id);
int dev_add_timer(as_isdn_device_t *dev, as_isdn_head_t *hp);
int dev_init_timer(as_isdn_device_t *dev, u_int id);
void dev_expire_timer(as_isdn_timer_t *ht);

int del_layer(devicelayer_t *dl);

int connect_if_req(as_isdn_device_t *dev, struct sk_buff *skb);
int set_if_req(as_isdn_device_t *dev, struct sk_buff *skb);
int add_if_req(as_isdn_device_t *dev, struct sk_buff *skb);
int del_if_req(as_isdn_device_t *dev, u_int addr);
int remove_if(devicelayer_t *dl, int stat);

int  as_isdn_rdata_frame(as_isdn_device_t *dev, struct sk_buff *skb);
devicelayer_t *get_devlayer(as_isdn_device_t *dev, int addr);

void as_rawdev_free_all(void);
int from_up_down(as_isdn_if_t *hif, struct sk_buff *skb);

