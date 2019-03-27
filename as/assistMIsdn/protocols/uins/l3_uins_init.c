/*
 * $Id: l3_uins_init.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * Japan INS User side D-channel protocol
 */

#include <linux/module.h>
/* added by lizhijie*/
#include <linux/init.h>

#include "l3.h"

#include "as_isdn_version.h"

static int asl3_debug = 0xFFFFFFFF;

static as_isdn_object_t uins_obj;

extern int ins_fromup(as_isdn_if_t *hif, struct sk_buff *skb);
extern int ins_fromdown(as_isdn_if_t *hif, struct sk_buff *skb);
extern void init_l3(layer3_t *l3);
extern void release_l3(layer3_t *l3);


static void release_uins_layer3(layer3_t *l3)
{
	as_isdn_instance_t  *inst = &l3->inst;
	u_long		flags;

#if AS_ISDN_DEBUG_L3_DATA
	printk(KERN_ERR  "%s : refcnt %d l3(%p) inst(%p--%s)\n",uins_obj.name, uins_obj.refcnt, l3, inst, inst->name);
#endif

	spin_lock_irqsave(&l3->lock, flags);
	release_l3(l3);
	spin_unlock_irqrestore(&l3->lock, flags);
	
	if (inst->up.peer)
	{
		inst->up.peer->obj->ctrl(inst->up.peer, MGR_DISCONNECT | REQUEST, &inst->up);
	}
	if (inst->down.peer)
	{
		inst->down.peer->obj->ctrl(inst->down.peer, MGR_DISCONNECT | REQUEST, &inst->down);
	}
	
	list_del(&l3->list);
	uins_obj.ctrl(inst, MGR_UNREGLAYER | REQUEST, NULL);
	if (l3->entity != AS_ISDN_ENTITY_NONE)
		uins_obj.ctrl(inst, MGR_DELENTITY | REQUEST, (void *)l3->entity);
	
	kfree(l3);
}


static int new_uins_layer3(as_isdn_stack_t *st, AS_ISDN_pid_t *pid)
{
	layer3_t	*nl3;
	int		err;

	if (!st || !pid)
		return(-EINVAL);

	if (!(nl3 = kmalloc(sizeof(layer3_t), GFP_ATOMIC)))
	{
		printk(KERN_ERR "UINS-MGR : kmalloc layer3 failed\n");
		return(-ENOMEM);
	}
	memset(nl3, 0, sizeof(layer3_t));
	
	memcpy(&nl3->inst.pid, pid, sizeof(AS_ISDN_pid_t));
	nl3->debug = asl3_debug;

	AS_ISDN_init_instance(&nl3->inst, &uins_obj, nl3);

	if (!AS_ISDN_SetHandledPID(&uins_obj, &nl3->inst.pid))
	{
		int_error();
		return(-ENOPROTOOPT);
	}

	if ((pid->protocol[3] & ~ISDN_PID_FEATURE_MASK) != ISDN_PID_L3_INS_USER)
	{
		printk(KERN_ERR "UINS-MGR : create failed prt %x\n", pid->protocol[3]);
		kfree(nl3);
		return(-ENOPROTOOPT);
	}
	
	init_l3(nl3);

	if (pid->protocol[3] & ISDN_PID_L3_DF_PTP)
		test_and_set_bit(FLG_PTP, &nl3->Flag);
	if (pid->protocol[3] & ISDN_PID_L3_DF_EXTCID)
		test_and_set_bit(FLG_EXTCID, &nl3->Flag);
	if (pid->protocol[3] & ISDN_PID_L3_DF_CRLEN2)
		test_and_set_bit(FLG_CRLEN2, &nl3->Flag);
	if (!(nl3->global = kmalloc(sizeof(l3_process_t), GFP_ATOMIC)))
	{
		printk(KERN_ERR "UINS-MGR : can't get memory for dss1 global CR\n");
		release_l3(nl3);
		kfree(nl3);
		return(-ENOMEM);
	}
	nl3->global->state = 0;
	nl3->global->callref = 0;
	nl3->global->id = AS_ISDN_ID_GLOBAL;
	INIT_LIST_HEAD(&nl3->global->list);
	nl3->global->n303 = N303;
	nl3->global->l3 = nl3;
	nl3->global->t303skb = NULL;
	L3InitTimer(nl3->global, &nl3->global->timer);
	
	if (!(nl3->dummy = kmalloc(sizeof(l3_process_t), GFP_ATOMIC)))
	{
		printk(KERN_ERR "UINS-MGR : can't get memory for dss1 dummy CR\n");
		release_l3(nl3);
		kfree(nl3);
		return(-ENOMEM);
	}
	nl3->dummy->state = 0;
	nl3->dummy->callref = -1;
	nl3->dummy->id = AS_ISDN_ID_DUMMY;
	INIT_LIST_HEAD(&nl3->dummy->list);
	nl3->dummy->n303 = N303;
	nl3->dummy->l3 = nl3;
	nl3->dummy->t303skb = NULL;
	L3InitTimer(nl3->dummy, &nl3->dummy->timer);
	sprintf(nl3->inst.name, "UINS-%d", st->id);
	
	nl3->p_mgr = l3_process_mgr;
	
	list_add_tail(&nl3->list, &uins_obj.ilist);
	
	err = uins_obj.ctrl(&nl3->inst, MGR_NEWENTITY | REQUEST, NULL);
	if (err)
	{
		printk(KERN_WARNING "UINS-MGR : MGR_NEWENTITY REQUEST failed err(%x)\n", err);
	}

	err = uins_obj.ctrl(st, MGR_REGLAYER | INDICATION, &nl3->inst);
	if (err)
	{
		release_l3(nl3);
		list_del(&nl3->list);
		kfree(nl3);
	}
	else
	{
		AS_ISDN_stPara_t	stp;

	    	if (st->para.down_headerlen)
		    	nl3->down_headerlen = st->para.down_headerlen;
		stp.maxdatalen = 0;
		stp.up_headerlen = L3_EXTRA_SIZE;
		stp.down_headerlen = 0;
		uins_obj.ctrl(st, MGR_ADDSTPARA | REQUEST, &stp);
	}
	return(err);
}


#ifdef MODULE
MODULE_AUTHOR("Li Zhijie <lizhijie@assistcn.com>");
	#ifdef MODULE_LICENSE
	MODULE_LICENSE("GPL");
	#endif
MODULE_PARM(asl3_debug, "1i");
#endif

/* handler for AS_ISDNobject instance */
static int uins_manager(void *data, u_int prim, void *arg)
{
	as_isdn_instance_t *inst = data;
	layer3_t *l3l;

	if (!data)
		return(-EINVAL);

#if AS_ISDN_DEBUG_L3_DATA
	if (asl3_debug & 0x1000)
		printk(KERN_ERR  "UINS-MGR : data:%p(inst:%s) prim:%x arg:%p--%s\n", data, inst->name, prim, arg, inst->name);
#endif

	list_for_each_entry(l3l, &uins_obj.ilist, list)
	{
		if (&l3l->inst == inst)
			break;
	}
	if (&l3l->list == &uins_obj.ilist)
		l3l = NULL;
	if (prim == (MGR_NEWLAYER | REQUEST))
		return(new_uins_layer3(data, arg));

	if (!l3l)
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (asl3_debug & 0x1)
			printk(KERN_WARNING "UINS-MGR : prim(%x) no instance\n", prim);
#endif
		return(-EINVAL);
	}

	switch(prim)
	{
		case MGR_NEWENTITY | CONFIRM:
			l3l->entity = (int)arg;
			break;

		case MGR_ADDSTPARA | INDICATION:
			l3l->down_headerlen = ((AS_ISDN_stPara_t *)arg)->down_headerlen;
		case MGR_CLRSTPARA | INDICATION:
			break;

		case MGR_CONNECT | REQUEST:
			return(AS_ISDN_ConnectIF(inst, arg));

		case MGR_SETIF | REQUEST:
		case MGR_SETIF | INDICATION:
			/* inst: owner; arg : peer's hif */
			return(AS_ISDN_SetIF(inst, arg, prim, ins_fromup, ins_fromdown, l3l));
			
		case MGR_DISCONNECT | REQUEST:
		case MGR_DISCONNECT | INDICATION:
			return(AS_ISDN_DisConnectIF(inst, arg));

		case MGR_RELEASE | INDICATION:
		case MGR_UNREGLAYER | REQUEST:
#if AS_ISDN_DEBUG_L3_DATA
			if (asl3_debug & 0x1000)
				printk(KERN_ERR  "UINS-MGR : release_uins_layer3 id %x\n", l3l->inst.st->id);
#endif
			release_uins_layer3(l3l);
			break;

		PRIM_NOT_HANDLED(MGR_CTRLREADY | INDICATION);
		default:
			if (asl3_debug & 0x1)
				printk(KERN_WARNING "UINS-MGR : prim %x not handled\n", prim);
			return(-EINVAL);
	}
	return(0);
}

int UINS_Init(void)
{
	int err;

	printk(KERN_ERR AS_VERSION_INFO("UserINS Rev") );
#ifdef MODULE
	uins_obj.owner = THIS_MODULE;
#endif
	INIT_LIST_HEAD(&uins_obj.ilist);
	uins_obj.name = "UINS";
	uins_obj.DPROTO.protocol[3] = ISDN_PID_L3_INS_USER |ISDN_PID_L3_DF_PTP ;
	uins_obj.own_ctrl = uins_manager;

	AS_ISDNl3New();

	if ((err = as_isdn_register(&uins_obj)))
	{
		printk(KERN_ERR "Can't register %s error(%d)\n", uins_obj.name, err);
		AS_ISDNl3Free();
	}
	
	return(err);
}

#ifdef MODULE
void UINS_cleanup(void)
{
	int err;
	layer3_t	*l3, *next;

	if ((err = as_isdn_unregister(&uins_obj)))
	{
		printk(KERN_ERR "%s : Can't unregister %s error(%d)\n", uins_obj.name, uins_obj.name, err);
	}

	if (!list_empty(&uins_obj.ilist))
	{
		printk(KERN_WARNING "%s : list not empty\n" , uins_obj.name);
		list_for_each_entry_safe(l3, next, &uins_obj.ilist, list)
			release_uins_layer3(l3);
	}
	
	AS_ISDNl3Free();

	printk(KERN_INFO "Release " AS_VERSION_INFO("UserINS Rev") );
}

module_init(UINS_Init);
module_exit(UINS_cleanup);
#endif
