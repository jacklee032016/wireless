/*
 * $Id: core.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/config.h>
#include <linux/module.h>
/* added by lizhijie*/
#include <linux/init.h>

#include <linux/spinlock.h>
#include "core.h"
#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif
#ifdef CONFIG_SMP
#include <linux/smp_lock.h>
#endif

extern int  as_schedule_delay(wait_queue_head_t *q);

#include "as_isdn_version.h"

LIST_HEAD(AS_ISDN_objectlist);
rwlock_t		AS_ISDN_objects_lock = RW_LOCK_UNLOCKED;

int core_debug = \
	DEBUG_MGR_FUNC |\
	DEBUG_CORE_FUNC |\
	0;
/*\
	DEBUG_RDATA |\
	DEBUG_WDATA |\
	DEBUG_DUMMY_FUNC |\
	DEBUG_DEV_OP |\
	DEBUG_DEV_TIMER |\
*/

static u_char		entityarray[AS_ISDN_MAX_ENTITY/8];
static spinlock_t	entity_lock = SPIN_LOCK_UNLOCKED;

static int obj_id;

#ifdef MODULE
MODULE_AUTHOR("Li Zhijie <lizhijie@assistcn.com>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
MODULE_PARM(core_debug, "1i");
#endif

typedef struct _AS_ISDN_thread
{
	/* thread */
	struct task_struct		*thread;	 	/* task which point to current, eg. kernel thread itself */
	wait_queue_head_t		waitq;		/* wait_queue for kernel thread itself */
	struct semaphore		*notify;
	u_long				Flags;
	struct sk_buff_head	workq;
}AS_ISDN_thread_t;

/* flags for kernel thread AS_ISDNd */
#define AS_ISDN_TFLAGS_STARTED	1
#define AS_ISDN_TFLAGS_RMMOD		2
#define AS_ISDN_TFLAGS_ACTIV		3
#define AS_ISDN_TFLAGS_TEST		4

static AS_ISDN_thread_t	AS_ISDN_thread;

#if  __WITH_KERNEL_2_6_		
/* module and protocol which can be dynamic load in 2.6 */
static moditem_t modlist[] = 
{
	{"L1TES0", 		ISDN_PID_L1_TE_S0},
	{"L2LAPD", 		ISDN_PID_L2_LAPD},
	{"L2BX75", 		ISDN_PID_L2_B_X75SLP},
/*	
	{"L3UDSS1", 		ISDN_PID_L3_DSS1USER},
	{"L3UINS", 		ISDN_PID_L3_INS_USER},
*/	
	{"L2B_DTMF", 	ISDN_PID_L2_B_TRANSDTMF},
	{NULL, 			ISDN_PID_NONE}
};
#endif

/* 
 * kernel thread to do work which cannot be done in interrupt context 
 */
static int AS_ISDNd(void *data)
{
	AS_ISDN_thread_t	*hkt = data;

#ifdef CONFIG_SMP
	lock_kernel();
#endif

#if 0
	MAKEDAEMON("kISDNd");
#endif
	sigfillset(&current->blocked);
	hkt->thread = current;
#ifdef CONFIG_SMP
	unlock_kernel();
#endif
	printk( KERN_INFO "Assist ISDN kernel daemon started\n");

	test_and_set_bit(AS_ISDN_TFLAGS_STARTED, &hkt->Flags);

	for (;;) 
	{
		int		err;
		struct sk_buff	*skb;
		AS_ISDN_headext_t	*hhe;

		if (test_and_clear_bit(AS_ISDN_TFLAGS_RMMOD, &hkt->Flags))
			break;
		if (hkt->notify != NULL)
			up(hkt->notify);
#if 0		
		interruptible_sleep_on(&hkt->waitq);
#else
		as_schedule_delay(&hkt->waitq );
#endif
		if (test_and_clear_bit(AS_ISDN_TFLAGS_RMMOD, &hkt->Flags))
			break;

		while ((skb = skb_dequeue(&hkt->workq))) 
		{
			test_and_set_bit(AS_ISDN_TFLAGS_ACTIV, &hkt->Flags);
			err = -EINVAL;
			hhe=AS_ISDN_HEADEXT_P(skb);
			switch (hhe->addr)
			{
				case MGR_FUNCTION:/* data point to stack; skb->data point to pid and its pbuf */
					err=hhe->func.ctrl(hhe->data[0], hhe->prim, skb->data);
					if (err)
					{
						printk(KERN_WARNING "kISDNd: addr(%x) prim(%x) failed err(%x)MGR_FUNCTION\n", 	hhe->addr, hhe->prim, err);
					}
					else
					{
					
#if AS_ISDN_DEBUG_CORE_CTRL
						if (core_debug)
 							printk(KERN_ERR  "kISDNd: addr(%x) prim(%x) success\n", hhe->addr, hhe->prim);
#endif

						err--; /* to free skb */
					}
					break;
				case MGR_QUEUEIF:
					err = hhe->func.iff(hhe->data[0], skb);
					if (err)
					{
						printk(KERN_WARNING "kISDNd: addr(%x) prim(%x) failed err(%x) QueueIF\n", hhe->addr, hhe->prim, err);
					}
					break;
				default:
					int_error();
					printk(KERN_WARNING "kISDNd: addr(%x) prim(%x) unknown\n", hhe->addr, hhe->prim);
					err = -EINVAL;
					break;
			}
			if (err)
				kfree_skb(skb);
			test_and_clear_bit(AS_ISDN_TFLAGS_ACTIV, &hkt->Flags);
		}

		if (test_and_clear_bit(AS_ISDN_TFLAGS_TEST, &hkt->Flags))
			printk(KERN_ERR  "kISDNd: test event done\n");
	}
	
	printk(KERN_ERR  "kISDNd: daemon exit now\n");
	test_and_clear_bit(AS_ISDN_TFLAGS_STARTED, &hkt->Flags);
	test_and_clear_bit(AS_ISDN_TFLAGS_ACTIV, &hkt->Flags);
	discard_queue(&hkt->workq);
	hkt->thread = NULL;
	if (hkt->notify != NULL)
		up(hkt->notify);
	return(0);
}

AS_ISDNobject_t *get_object(int id)
{
	AS_ISDNobject_t	*obj;

	read_lock(&AS_ISDN_objects_lock);
	list_for_each_entry(obj, &AS_ISDN_objectlist, list)
	{
		if (obj->id == id)
		{
			read_unlock(&AS_ISDN_objects_lock);
			return(obj);
		}
	}	
	read_unlock(&AS_ISDN_objects_lock);
	return(NULL);
}

/* find object which can process 'protocol' */
static AS_ISDNobject_t *find_object(int protocol)
{
	AS_ISDNobject_t	*obj;
	int		err;

	read_lock(&AS_ISDN_objects_lock);
	list_for_each_entry(obj, &AS_ISDN_objectlist, list)
	{	/* used to dynamically added prococol in one AS_ISDNobject, such as hfc-multi*/
		err = obj->own_ctrl(NULL, MGR_HASPROTOCOL | REQUEST, &protocol);
		if (!err)
			goto unlock;
		if (err != -ENOPROTOOPT)
		{
			if (0 == AS_ISDN_HasProtocol(obj, protocol))
				goto unlock;
		}	
	}
	obj = NULL;
unlock:
	read_unlock(&AS_ISDN_objects_lock);
	return(obj);
}


#if  __WITH_KERNEL_2_6_		
/* meanful in 2.6 */
static AS_ISDNobject_t *find_object_module(int protocol)
{
#ifdef CONFIG_KMOD
	int		err;
#endif
	moditem_t	*m = modlist;
	AS_ISDNobject_t	*obj;

	while (m->name != NULL)
	{
		if (m->protocol == protocol)
		{
#ifdef CONFIG_KMOD
					
#if AS_ISDN_DEBUG_CORE_CTRL
			if (core_debug)
				printk(KERN_ERR  "find_object_module %s - trying to load\n", m->name);
#endif

			err=request_module(m->name);
					
#if AS_ISDN_DEBUG_CORE_CTRL
			if (core_debug)
				printk(KERN_ERR  "find_object_module: request_module(%s) returns(%d)\n", m->name, err);
#endif

#else
			printk(KERN_WARNING "not possible to autoload %s please try to load manually\n", m->name);
#endif
			if ((obj = find_object(protocol)))
				return(obj);
		}
		m++;
	}
					
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug)
		printk(KERN_ERR  "%s: no module for protocol %x found\n", __FUNCTION__, protocol);
#endif
	return(NULL);
}
#endif

/* send release_indicate to this object with instance parameter */
static void remove_object(AS_ISDNobject_t *obj)
{
	AS_ISDNstack_t *st, *nst;
	AS_ISDNlayer_t *layer, *nl;
	AS_ISDNinstance_t *inst;

	list_for_each_entry_safe(st, nst, &AS_ISDN_stacklist, list)
	{
		list_for_each_entry_safe(layer, nl, &st->layerlist, list)
		{
			inst = layer->inst;
			if (inst && inst->obj == obj)
				inst->obj->own_ctrl(st, MGR_RELEASE | INDICATION, inst);
		}
	}
}

/*just free skb : play role as AS_ISDNif */
static int dummy_if(AS_ISDNif_t *hif, struct sk_buff *skb)
{
	if (!skb)
	{
		printk(KERN_WARNING "%s: hif(%p) without skb\n",	__FUNCTION__, hif);
		return(-EINVAL);
	}
					
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_DUMMY_FUNC)
	{
		AS_ISDN_head_t	*hh = AS_ISDN_HEAD_P(skb);
		printk(KERN_ERR  "%s: hif(%p) skb(%p) len(%d) prim(%x)\n", __FUNCTION__, hif, skb, skb->len, hh->prim);
	}
#endif

	dev_kfree_skb_any(skb);
	return(0);
}

/* find AS_ISDNinstance with assigned pid in stack*/
AS_ISDNinstance_t *get_next_instance(AS_ISDNstack_t *st, AS_ISDN_pid_t *pid)
{
	int		err;
	AS_ISDNinstance_t	*next;
	int		layer, proto;
	AS_ISDNobject_t	*obj;

	/* layer and protocol is decided by pid */
	layer = AS_ISDN_get_lowlayer(pid->layermask);
	proto = pid->protocol[layer];
	
	/* no instance in this stack with assigned layer(int) and protocol, if next is not null, 
	then instance of this layer and protocol has exist in this stack,  return at once */
	next = get_instance(st, layer, proto);
	
	if (!next)
	{/* no instance exist, then find object and use the found object to create a new instance(Layer) */	
		obj = find_object(proto);
#if  __WITH_KERNEL_2_6_		
		if (!obj)/* only in 2.6 */
			obj = find_object_module(proto);
#endif		
		if (!obj)
		{
					
#if AS_ISDN_DEBUG_CORE_CTRL
			if (core_debug)
				printk(KERN_WARNING "%s: no object found\n", __FUNCTION__);
#endif
			return(NULL);
		}
		/* use this object to create new Laer in this stack */
		err = obj->own_ctrl(st, MGR_NEWLAYER | REQUEST, pid);
		if (err)
		{
			printk(KERN_WARNING "%s: newlayer err(%d)\n", __FUNCTION__, err);
			return(NULL);
		}
		/* get and return the instance just created  */
		next = get_instance(st, layer, proto);
	}
	return(next);
}

static int sel_channel(AS_ISDNstack_t *st, channel_info_t *ci)
{
	int	err = -EINVAL;

	if (!ci)
		return(err);
					
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug)
		printk(KERN_ERR  "%s: st(%p) st->mgr(%p)\n", __FUNCTION__, st, st->mgr);
#endif

	if (st->mgr)
	{
		if (st->mgr->obj && st->mgr->obj->own_ctrl)
		{
			err = st->mgr->obj->own_ctrl(st->mgr, MGR_SELCHANNEL | REQUEST, ci);
					
#if AS_ISDN_DEBUG_CORE_CTRL
			if (core_debug)
				printk(KERN_ERR  "%s: MGR_SELCHANNEL(%d)\n", __FUNCTION__, err);
#endif

		}
		else
			int_error();
	}
	else
	{
		printk(KERN_WARNING "%s: no mgr st(%p)\n", __FUNCTION__, st);
	}

	if (err)
	{
		AS_ISDNstack_t	*cst;
		u_int		nr = 0;

		ci->st.p = NULL;
		if (!(ci->channel & (~CHANNEL_NUMBER)))
		{
			/* only number is set */
			struct list_head	*head;
			if (list_empty(&st->childlist))
			{
				if ((st->id & FLG_CLONE_STACK) && (st->childlist.prev != &st->childlist))
				{
					head = st->childlist.prev;
				}
				else
				{
					printk(KERN_WARNING "%s: invalid empty childlist (no clone) stid(%x) childlist(%p<-%p->%p)\n",
						__FUNCTION__, st->id, st->childlist.prev, &st->childlist, st->childlist.next);
					return(err);
				}
			}
			else
				head = &st->childlist;

			list_for_each_entry(cst, head, list)
			{
				nr++;
				if (nr == (ci->channel & 3))
				{
					ci->st.p = cst;
					return(0);
				}
			}
		}
	}
	return(err);
}

static int disconnect_if(AS_ISDNinstance_t *inst, u_int prim, AS_ISDNif_t *hif)
{
	int	err = 0;

	if (hif)
	{
		hif->stat = IF_NOACTIV;
		hif->func = dummy_if;
		hif->peer = NULL;
		hif->fdata = NULL;
	}
	if (inst)
		err = inst->obj->own_ctrl(inst, prim, hif);
	return(err);
}

static int add_if(AS_ISDNinstance_t *inst, u_int prim, AS_ISDNif_t *hif)
{
	AS_ISDNif_t *myif;

	if (!inst)
		return(-EINVAL);
	if (!hif)
		return(-EINVAL);
	if (hif->stat & IF_UP)
	{
		myif = &inst->down;
	}
	else if (hif->stat & IF_DOWN)
	{
		myif = &inst->up;
	}
	else
		return(-EINVAL);

	while(myif->clone)
		myif = myif->clone;

	myif->clone = hif;
	hif->predecessor = myif;
	inst->obj->own_ctrl(inst, prim, hif);
	return(0);
}

static char tmpbuf[4096];

static int debugout(AS_ISDNinstance_t *inst, logdata_t *log)
{
	char *p = tmpbuf;

	if (log->head && *log->head)
		p += sprintf(p,"%s ", log->head);
	else
		p += sprintf(p,"%s ", inst->obj->name);
	
	p += vsprintf(p, log->fmt, log->args);
	printk(KERN_ERR  "%s\n", tmpbuf);
	return(0);
}

/* check raw device exist : 0, Yes, <0, not exist */
static int get_hdevice(AS_ISDNdevice_t **dev, int *typ)
{
	if (!dev)
		return(-EINVAL);
	
	if (!typ)
		return(-EINVAL);
	
	if (*typ == AS_ISDN_RAW_DEVICE)
	{
		*dev = get_free_rawdevice();
		if (!(*dev))
			return(-ENODEV);
		return(0);
	}
	return(-EINVAL);
}

static int mgr_queue(void *data, u_int prim, struct sk_buff *skb)
{
	AS_ISDN_headext_t *hhe = AS_ISDN_HEADEXT_P(skb);

	hhe->addr = prim;
	skb_queue_tail(&AS_ISDN_thread.workq, skb);
	wake_up_interruptible(&AS_ISDN_thread.waitq);
	return(0);
}

static int central_manager(void *, u_int, void *);

static int set_stack_req(AS_ISDNstack_t *st, AS_ISDN_pid_t *pid)
{
	struct sk_buff	*skb;
	AS_ISDN_headext_t	*hhe;
	AS_ISDN_pid_t	*npid;
	u_char		*pbuf = NULL;

	skb = alloc_skb(sizeof(AS_ISDN_pid_t) + pid->maxplen, GFP_ATOMIC);
	hhe = AS_ISDN_HEADEXT_P(skb);
	hhe->prim = MGR_SETSTACK_NW | REQUEST;
	hhe->addr = MGR_FUNCTION;
	hhe->data[0] = st;
	npid = (AS_ISDN_pid_t *)skb_put(skb, sizeof(AS_ISDN_pid_t));
	if (pid->pbuf)
		pbuf = skb_put(skb, pid->maxplen);
	copy_pid(npid, pid, pbuf);
	
	hhe->func.ctrl = central_manager;
	skb_queue_tail(&AS_ISDN_thread.workq, skb);
	wake_up_interruptible(&AS_ISDN_thread.waitq);

	return(0);
}

int isdn_alloc_entity(int *entity)
{
	u_long	flags;

	spin_lock_irqsave(&entity_lock, flags);
	*entity = 1;
	while(*entity < AS_ISDN_MAX_ENTITY)
	{
		if (!test_and_set_bit(*entity, (u_long *)&entityarray[0]))
			break;
		(*entity)++;
	}
	spin_unlock_irqrestore(&entity_lock, flags);
	if (*entity == AS_ISDN_MAX_ENTITY)
		return(-EBUSY);
	return(0);
}

int isdn_delete_entity(int entity)
{
	u_long	flags;
	int	ret = 0;

	spin_lock_irqsave(&entity_lock, flags);
	if (!test_and_clear_bit(entity, (u_long *)&entityarray[0]))
	{
		printk(KERN_WARNING "AS_ISDN: del_entity(%d) but entity not allocated\n", entity);
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(&entity_lock, flags);
	return(ret);
}

static int new_entity(AS_ISDNinstance_t *inst)
{
	int	entity;
	int	ret;

	if (!inst)
		return(-EINVAL);
	
	ret = isdn_alloc_entity(&entity);
	if (ret)
	{
		printk(KERN_WARNING "AS_ISDN: no more entity available(max %d)\n", AS_ISDN_MAX_ENTITY);
		return(ret);
	}
	ret = inst->obj->own_ctrl(inst, MGR_NEWENTITY | CONFIRM, (void *)entity);
	if (ret)
		isdn_delete_entity(entity);

	return(ret);
}

static int central_manager(void *data, u_int prim, void *arg)
{
	AS_ISDNstack_t *st = data;

	switch(prim)
	{/* primitive with parameters */
		case MGR_NEWSTACK | REQUEST:
			if (!(st = new_stack(data, arg)))
				return(-EINVAL);
			return(0);

		case MGR_NEWENTITY | REQUEST:
			return(new_entity(data));

		case MGR_DELENTITY | REQUEST:
		case MGR_DELENTITY | INDICATION:
			return(isdn_delete_entity((int)arg));

		case MGR_REGLAYER | INDICATION:
			return(register_layer(st, arg));

		case MGR_REGLAYER | REQUEST:
			if (!register_layer(st, arg))
			{/* success*/
				AS_ISDNinstance_t *inst = arg;
				return(inst->obj->own_ctrl(arg, MGR_REGLAYER | CONFIRM, NULL));
			}
			return(-EINVAL);

		case MGR_UNREGLAYER | REQUEST:
			return(unregister_instance(data));

		case MGR_DISCONNECT | REQUEST:
		case MGR_DISCONNECT | INDICATION:
			return(disconnect_if(data, prim, arg));

		case MGR_GETDEVICE | REQUEST:
			return(get_hdevice(data, arg));

		case MGR_DELDEVICE | REQUEST:
			return(free_device(data));

		case MGR_QUEUEIF | REQUEST:
			return(mgr_queue(data, MGR_QUEUEIF, arg));
	}
	
	if (!data)
		return(-EINVAL);

	switch(prim)
	{
		case MGR_SETSTACK | REQUEST:
			/* can sleep in case of module reload */
			return(set_stack_req(st, arg));

		case MGR_SETSTACK_NW | REQUEST:
			return(set_stack(st, arg));

		case MGR_CLEARSTACK | REQUEST:
			return(clear_stack(st));

		case MGR_DELSTACK | REQUEST:
			return(release_stack(st));

		case MGR_SELCHANNEL | REQUEST:
			return(sel_channel(st, arg));

		case MGR_ADDIF | REQUEST:
			return(add_if(data, prim, arg));

		case MGR_CTRLREADY | INDICATION:
			return(do_for_all_layers(st, prim, arg));

		case MGR_ADDSTPARA | REQUEST:
		case MGR_CLRSTPARA | REQUEST:
			return(change_stack_para(st, prim, arg));

		case MGR_CONNECT | REQUEST:
			return(AS_ISDN_ConnectIF(data, arg));

		case MGR_EVALSTACK  | REQUEST:
			return(evaluate_stack_pids(data, arg));

		case MGR_GLOBALOPT | REQUEST:
		case MGR_LOADFIRM | REQUEST:
			if (st->mgr && st->mgr->obj && st->mgr->obj->own_ctrl)
				return(st->mgr->obj->own_ctrl(st->mgr, prim, arg));
			break;

		case MGR_DEBUGDATA | REQUEST:
			return(debugout(data, arg));

		default:
			if (core_debug)
				printk(KERN_WARNING "manager prim %x not handled\n", prim);
			break;
	}
	return(-EINVAL);
}

int as_isdn_register(AS_ISDNobject_t *obj)
{
	u_long	flags;

	if (!obj)
		return(-EINVAL);
	write_lock_irqsave(&AS_ISDN_objects_lock, flags);
	obj->id = obj_id++;
	list_add_tail(&obj->list, &AS_ISDN_objectlist);
	write_unlock_irqrestore(&AS_ISDN_objects_lock, flags);
	obj->ctrl = central_manager;
	
	// register_prop
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug)
		printk(KERN_ERR  "register %s id %x\n", obj->name, obj->id);
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "register: obj(%p)\n", obj);
#endif

	return(0);
}

int as_isdn_unregister(AS_ISDNobject_t *obj)
{
	u_long	flags;

	if (!obj)
		return(-EINVAL);
					
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug)
		printk(KERN_ERR  "unregister %s %d refs\n", obj->name, obj->refcnt);
#endif

	if (obj->DPROTO.protocol[0])
		release_stacks(obj);
	else
		remove_object(obj);

	write_lock_irqsave(&AS_ISDN_objects_lock, flags);
	list_del(&obj->list);
	write_unlock_irqrestore(&AS_ISDN_objects_lock, flags);
					
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "as_isdn_unregister: objectlist(%p<-%p->%p)\n", AS_ISDN_objectlist.prev, &AS_ISDN_objectlist, AS_ISDN_objectlist.next);
#endif

	return(0);
}

int  as_isdn_init(void)
{
	DECLARE_MUTEX_LOCKED(sem);
	int err;

	printk(KERN_INFO AS_VERSION_INFO("Core Engine") );

#ifdef AS_ISDN_MEMDEBUG
	err = __mid_init();
	if (err)
		return(err);
#endif

	err = init_isdn_device(core_debug);
	if (err)
		return(err);
	
	init_waitqueue_head(&AS_ISDN_thread.waitq);
	skb_queue_head_init(&AS_ISDN_thread.workq);
	AS_ISDN_thread.notify = &sem;
	kernel_thread(AS_ISDNd, (void *)&AS_ISDN_thread, 0);
	down(&sem);
	AS_ISDN_thread.notify = NULL;
	test_and_set_bit(AS_ISDN_TFLAGS_TEST, &AS_ISDN_thread.Flags);
	wake_up_interruptible(&AS_ISDN_thread.waitq);
	return(err);
}

void as_isdn_cleanup(void)
{
	DECLARE_MUTEX_LOCKED(sem);
	AS_ISDNstack_t *st, *nst;

	free_isdn_device();
	if (!list_empty(&AS_ISDN_objectlist))
	{
		printk(KERN_WARNING "core objects not empty\n");
	}
	
	if (!list_empty(&AS_ISDN_stacklist))
	{
		printk(KERN_WARNING "core stacklist not empty\n");
		list_for_each_entry_safe(st, nst, &AS_ISDN_stacklist, list) 
		{
			printk(KERN_WARNING "core st %x still in list\n",	st->id);
			if (list_empty(&st->list))
			{
				printk(KERN_WARNING "core st == next\n");
				break;
			}
		}
	}
	if (AS_ISDN_thread.thread)
	{
		/* abort AS_ISDNd kernel thread */
		AS_ISDN_thread.notify = &sem;
		test_and_set_bit(AS_ISDN_TFLAGS_RMMOD, &AS_ISDN_thread.Flags);
		wake_up_interruptible(&AS_ISDN_thread.waitq);
		down(&sem);
		AS_ISDN_thread.notify = NULL;
	}
#ifdef AS_ISDN_MEMDEBUG
	__mid_cleanup();
#endif
	printk(KERN_INFO "Release " AS_VERSION_INFO("Core Engine") );
}

module_init(as_isdn_init);
module_exit(as_isdn_cleanup);

EXPORT_SYMBOL(as_isdn_register);
EXPORT_SYMBOL(as_isdn_unregister);

extern int as_isdn_rdata_raw_data(AS_ISDNif_t *hif,  int prim, struct sk_buff *skb);

EXPORT_SYMBOL(as_isdn_rdata_raw_data);

