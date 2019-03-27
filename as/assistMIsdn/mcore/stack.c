/*
 * $Id: stack.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include "core.h"

LIST_HEAD(AS_ISDN_stacklist);
LIST_HEAD(AS_ISDN_instlist);

int get_stack_cnt(void)
{
	int cnt = 0;
	as_isdn_stack_t *st;

	list_for_each_entry(st, &AS_ISDN_stacklist, list)
		cnt++;
	return(cnt);
}

void get_stack_info(struct sk_buff *skb)
{
	as_isdn_head_t	*hp;
	as_isdn_stack_t	*cst, *st;
	stack_info_t	*si;
	as_isdn_layer_t	*lay;

	hp = AS_ISDN_HEAD_P(skb);
	st = get_stack4id(hp->addr);
	if (!st)
		hp->len = 0;
	else
	{
		si = (stack_info_t *)skb->data;
		memset(si, 0, sizeof(stack_info_t));
		si->id = st->id;
		si->extentions = st->extentions;
		if (st->mgr)
			si->mgr = st->mgr->id;
		else
			si->mgr = 0;
		
		memcpy(&si->pid, &st->pid, sizeof(AS_ISDN_pid_t));
		memcpy(&si->para, &st->para, sizeof(AS_ISDN_stPara_t));
		si->instcnt = 0;
		list_for_each_entry(lay, &st->layerlist, list)
		{
			if (lay->inst)
			{
				si->inst[si->instcnt] = lay->inst->id;
				si->instcnt++;
			}
		}
		si->childcnt = 0;
		
		list_for_each_entry(cst, &st->childlist, list)
		{
			si->child[si->childcnt] = cst->id;
			si->childcnt++;
		}
		hp->len = sizeof(stack_info_t);
		if (si->childcnt>2)
			hp->len += (si->childcnt-2)*sizeof(int);
	}
	skb_put(skb, hp->len);
}

/* stack ID: 0--127, master; 0x10 : child stack; 0x20 clone stack */
static int  get_free_stackid(as_isdn_stack_t *mst, int flag)
{
	u_int		id=1, found;
	as_isdn_stack_t	*st;

	if (!mst)
	{/* no master stack used, eg. this is a stack in the AS_ISDN_stacklist which id is below 127 */
		while(id<127)
		{
			found = 0;
			list_for_each_entry(st, &AS_ISDN_stacklist, list)
			{
				if (st->id == id)
				{
					found++;
					break;
				}
			}
			if (found)
				id++;
			else
				return(id);
		}
	}
	else if (flag & FLG_CLONE_STACK)
	{/* master stack with clone stack ID */
		id = mst->id | FLG_CLONE_STACK;
		while(id < CLONE_ID_MAX)
		{
			found = 0;
			id += CLONE_ID_INC;
			list_for_each_entry(st, &AS_ISDN_stacklist, list)
			{
				if (st->id == id)
				{
					found++;
					break;
				}
			}
			if (!found)
				return(id);
		}
	}
	else if (flag & FLG_CHILD_STACK)
	{
		id = mst->id | FLG_CHILD_STACK;
		while(id < CHILD_ID_MAX)
		{
			id += CHILD_ID_INC;
			found = 0;
			list_for_each_entry(st, &mst->childlist, list)
			{
				if (st->id == id)
				{
					found++;
					break;
				}
			}
			if (!found)
				return(id);
		}
	}
	return(0);
}

as_isdn_stack_t *get_stack4id(u_int id)
{
	as_isdn_stack_t *cst, *st;

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "get_stack4id(%x)\n", id);
#endif
	if (!id) /* 0 isn't a valid id */
		return(NULL);
	list_for_each_entry(st, &AS_ISDN_stacklist, list)
	{
		if (id == st->id)
			return(st);
		list_for_each_entry(cst, &st->childlist, list)
		{
			if (cst->id == id)
				return(cst);
		}
	}
	return(NULL);
}

/* return a layer object with assigned layermask from stack */
as_isdn_layer_t * getlayer4lay(as_isdn_stack_t *st, int layermask)
{
	as_isdn_layer_t	*layer;
	as_isdn_instance_t	*inst;

	if (!st)
	{
		int_error();
		return(NULL);
	}

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s : stack id %x layermask %x\n", __FUNCTION__, st->id, layermask);
#endif

	list_for_each_entry(layer, &st->layerlist, list)
	{
		inst = layer->inst;
		if(inst && (inst->pid.layermask & layermask))
			return(layer);
	}
	return(NULL);
}

/* get instance in a stack with assigned layer number and protocol
* only one instance exist in stack with assigned layermask and protocol 
*/
as_isdn_instance_t *get_instance(as_isdn_stack_t *st, int layer_nr, int protocol)
{
	as_isdn_layer_t	*layer;
	as_isdn_instance_t	*inst=NULL;

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "get_instance st(%p) lnr(%d) prot(%x)\n", st, layer_nr, protocol);
#endif

	if (!st)
	{
		int_error();
		return(NULL);
	}
	if ((layer_nr<0) || (layer_nr>MAX_LAYER_NR))
	{
		int_errtxt("lnr %d", layer_nr);
		return(NULL);
	}
	
	list_for_each_entry(layer, &st->layerlist, list)
	{
		inst = layer->inst;
		if (inst)
		{
#if AS_ISDN_DEBUG_CORE_CTRL
			if (core_debug & DEBUG_CORE_FUNC)
				printk(KERN_ERR  "get_instance inst(%p, %x) lm %x/%x prot %x/%x\n",
					inst, inst->id, inst->pid.layermask, ISDN_LAYER(layer_nr), inst->pid.protocol[layer_nr], protocol);
#endif
			if ((inst->pid.layermask & ISDN_LAYER(layer_nr)) &&
				(inst->pid.protocol[layer_nr] == protocol))
				goto out;
			inst = NULL;
		}
		if (list_empty(&layer->list))
		{
			int_errtxt("deadloop layer %p", layer);
			return(NULL);
		}
	}
out:
	return(inst);
}

/* get instance with assigned id */
as_isdn_instance_t *get_instance4id(u_int id)
{
	as_isdn_instance_t *inst;

	list_for_each_entry(inst, &AS_ISDN_instlist, list)
		if (inst->id == id)
			return(inst);
	return(NULL);
}

/* layer is composed of instance, so layermask of layer is decided by lay->inst->pid->layermask */
int get_layermask(as_isdn_layer_t *layer)
{
	int mask = 0;

	if (layer->inst)
		mask |= layer->inst->pid.layermask;
	return(mask);
}

/* insert the layer into stack in the position of layermask, eg. layer in stack->layerlist is order by layermask decreased */
int insertlayer(as_isdn_stack_t *st, as_isdn_layer_t *layer, int layermask)
{
	as_isdn_layer_t *item;
	
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s(%p, %p, %x)\n",	__FUNCTION__, st, layer, layermask);  
#endif

	if (!st || !layer)
	{
		int_error();
		return(-EINVAL);
	}

	if (list_empty(&st->layerlist))
	{
		list_add(&layer->list, &st->layerlist);
	}
	else
	{
		list_for_each_entry(item, &st->layerlist, list)
		{
			if (layermask < get_layermask(item))
			{
				list_add_tail(&layer->list, &item->list); 
				return(0);
			}
		}
		list_add_tail(&layer->list, &st->layerlist);
	}
	return(0);
}

/* create a new master/child stack, assign stack-mgr is instance and inst->stack is this new stack */
as_isdn_stack_t *new_stack(as_isdn_stack_t *master, as_isdn_instance_t *inst)
{
	as_isdn_stack_t *newst;

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "create %s stack inst(%p)\n", master ? "child" : "master", inst);
#endif

	if (!(newst = kmalloc(sizeof(as_isdn_stack_t), GFP_ATOMIC)))
	{
		printk(KERN_ERR "kmalloc asISDN_stack failed\n");
		return(NULL);
	}
	memset(newst, 0, sizeof(as_isdn_stack_t));
	INIT_LIST_HEAD(&newst->layerlist);
	INIT_LIST_HEAD(&newst->childlist);

	if (!master)
	{
		if (inst && inst->st)
		{/* CLONE, inst->st is not null, refer new_devstack */
			newst->id = get_free_stackid(inst->st, FLG_CLONE_STACK);
		}
		else
		{
			newst->id = get_free_stackid(NULL, 0);
		}
	}
	else
	{
		newst->id = get_free_stackid(master, FLG_CHILD_STACK);
	}
	newst->mgr = inst;

	if (master)
	{
		list_add_tail(&newst->list, &master->childlist);
	}
	else
	{
		list_add_tail(&newst->list, &AS_ISDN_stacklist);
	}
	
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "Stack id %x added\n", newst->id);
#endif

	if (inst)
		inst->st = newst;
	return(newst);
}

/* release all layer of this stack
*  before release operation, primitive is executed by instance in all layers
*  this stack object is not released
*/
static int release_layers(as_isdn_stack_t *st, u_int prim)
{
	as_isdn_instance_t *inst;
	as_isdn_layer_t    *layer, *nl;
	int		cnt = 0;

	list_for_each_entry_safe(layer, nl, &st->layerlist, list)
	{
//TRACE();		
		inst = layer->inst;
//TRACE();		
		if (inst)
		{
#if AS_ISDN_DEBUG_CORE_CTRL
			if (core_debug & DEBUG_CORE_FUNC)
				printk(KERN_ERR   "%s: st(%p) inst(%p):%x %s lm(%x)\n", 	__FUNCTION__, st, inst, inst->id,	inst->name, inst->pid.layermask);
#endif

//TRACE();		
			inst->obj->own_ctrl(inst, prim, NULL);
		}
//TRACE();	
#if 0
		/* move to unregister_instance for dynamic instance management ,lizhijie, 2005.09.09 */
		list_del(&layer->list);
		kfree(layer);
		layer = NULL;
#endif

//TRACE();		
		if (cnt++ > 1000)
		{
			int_errtxt("release_layers endless loop st(%p)", st);
			return(-EINVAL);
		}
//TRACE();		
	}
//TRACE();		
	return(0);
}

/* primitive is executed in all layers of this stack */
int do_for_all_layers(as_isdn_stack_t *st, u_int prim, void *arg)
{
	as_isdn_instance_t *inst;
	as_isdn_layer_t    *layer, *nl;
	int		cnt = 0;

	if (!st)
	{
		int_error();
		return(-EINVAL);
	}

	list_for_each_entry_safe(layer, nl, &st->layerlist, list)
	{
		inst = layer->inst;
		if (inst)
		{
#if AS_ISDN_DEBUG_CORE_CTRL
			if (core_debug & DEBUG_CORE_FUNC)
				printk(KERN_ERR   "%s: st(%p) inst(%p):%x %s prim(%x) arg(%p)\n", __FUNCTION__, st, inst, inst->id, inst->name, prim, arg);
#endif

			inst->obj->own_ctrl(inst, prim, arg);
		}
		if (cnt++ > 1000)
		{
			int_errtxt("do_for_all_layers endless loop st(%p)", st);
			return(-EINVAL);
		}
	}
	return(0);
}

/* add or clear stack->para, then make all layers of this stack execute this primitive  */
int change_stack_para(as_isdn_stack_t *st, u_int prim, AS_ISDN_stPara_t *stpara)
{
	int	changed = 0;

	if (!st)
	{
		int_error();
		return(-EINVAL);
	}

	if (prim == (MGR_ADDSTPARA | REQUEST))
	{
		if (!stpara)
		{
			int_error();
			return(-EINVAL);
		}
		prim = MGR_ADDSTPARA | INDICATION;
		if (stpara->maxdatalen > 0 && stpara->maxdatalen < st->para.maxdatalen)
		{
			changed++;
			st->para.maxdatalen = stpara->maxdatalen;
		}
		if (stpara->up_headerlen > st->para.up_headerlen)
		{
			changed++;
			st->para.up_headerlen = stpara->up_headerlen;
		}
		if (stpara->down_headerlen > st->para.down_headerlen)
		{
			changed++;
			st->para.down_headerlen = stpara->down_headerlen;
		}
		if (!changed)
			return(0);
		stpara = &st->para;
	}
	else if (prim == (MGR_CLRSTPARA | REQUEST))
	{
		prim = MGR_CLRSTPARA | INDICATION;
		memset(&st->para, 0, sizeof(AS_ISDN_stPara_t));
		stpara = NULL;
	}
	
	return(do_for_all_layers(st, prim, stpara));
}

/* release all child-stack and associated layers, then layerList, finally free stack */
int release_stack(as_isdn_stack_t *st)
{
	int err;
	as_isdn_stack_t *cst, *nst;

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s: st(%p)\n", __FUNCTION__, st);
#endif

	list_for_each_entry_safe(cst, nst, &st->childlist, list)
	{
#if AS_ISDN_DEBUG_CORE_CTRL
		if (core_debug & DEBUG_CORE_FUNC)
			printk(KERN_ERR  "%s: cst(%p)\n", __FUNCTION__, cst);
#endif
		if ((err = release_layers(cst, MGR_RELEASE | INDICATION)))
		{
			printk(KERN_WARNING "release_stack child err(%d)\n", err);
			return(err);
		}
		list_del(&cst->list);
		kfree(cst);
	}
	
	if ((err = release_layers(st, MGR_RELEASE | INDICATION)))
	{
		printk(KERN_WARNING "release_stack err(%d)\n", err);
		return(err);
	}
	
	list_del(&st->list);
	kfree(st);
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s: stacklist(%p<-%p->%p)\n", __FUNCTION__, AS_ISDN_stacklist.prev, &AS_ISDN_stacklist, AS_ISDN_stacklist.next);
#endif
	return(0);
}

/* in system stacklist, if any layer of this stack contains this object, then it will be free 
* eg.  object can be used in many stack or many layer in system 
* this object is not released 
*/
void release_stacks(as_isdn_object_t *obj)
{
	as_isdn_stack_t *st, *tmp;
	as_isdn_layer_t *layer, *ltmp;
	int rel;

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s: obj(%p) %s\n",	__FUNCTION__, obj, obj->name);
#endif

	list_for_each_entry_safe(st, tmp, &AS_ISDN_stacklist, list)
	{
		rel = 0;
#if AS_ISDN_DEBUG_CORE_CTRL
		if (core_debug & DEBUG_CORE_FUNC)
			printk(KERN_ERR  "%s: st(%p)\n", __FUNCTION__, st);
#endif
		list_for_each_entry_safe(layer, ltmp, &st->layerlist, list)
		{
#if AS_ISDN_DEBUG_CORE_CTRL
			if (core_debug & DEBUG_CORE_FUNC)
				printk(KERN_ERR  "%s: layer(%p) inst(%p)\n", __FUNCTION__, layer, layer->inst);
#endif
			if (layer->inst && layer->inst->obj == obj)
				rel++;
		}		
		if (rel)
			release_stack(st);
	}
	
#if AS_ISDN_DEBUG_CORE_CTRL
	if (obj->refcnt)
		printk(KERN_WARNING "release_stacks obj %s refcnt is %d\n", obj->name, obj->refcnt);
#endif
}


static void get_free_instid(as_isdn_stack_t *st, as_isdn_instance_t *inst)
{
	as_isdn_instance_t *il;

	inst->id = AS_ISDN_get_lowlayer(inst->pid.layermask)<<20;
	inst->id |= FLG_INSTANCE;
	if (st)
	{
		inst->id |= st->id;
	}
	else
	{
		list_for_each_entry(il, &AS_ISDN_instlist, list)
		{
			if (il->id == inst->id)
			{
				if ((inst->id & IF_INSTMASK) >= INST_ID_MAX)
				{
					inst->id = 0;
					return;
				}
				inst->id += INST_ID_INC;
				il = list_entry(AS_ISDN_instlist.next, as_isdn_instance_t, list);
			}
		}
	}
}

/* clear or construct layer object in stack(latermask is decided by inst->pid),
* this layer is represented by this instance object
*/
int register_layer(as_isdn_stack_t *st, as_isdn_instance_t *inst)
{
	as_isdn_layer_t	*layer = NULL;
	int		refinc = 0;

	if (!inst)
		return(-EINVAL);

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s:st(%p) inst(%p/%p/%s) lmask(%x) id(%x)\n", __FUNCTION__, st, inst, inst->obj, inst->name, inst->pid.layermask, inst->id);
#endif

	if (inst->id)
	{ /* allready registered */
		if (inst->st || !st)
		{
			int_errtxt("register duplicate %08x %p %p", inst->id, inst->st, st);
			return(-EBUSY);
		}
	}
	
	if (st)
	{
		if ((layer = getlayer4lay(st, inst->pid.layermask)))
		{/* there is a layer with layermask exist already in this stack */
			if (layer->inst)
			{
				int_errtxt("stack %08x has layer %08x(Inst:%s[%s]) : layermask is %08x", st->id, layer->inst->id,layer->inst->name, inst->name, inst->pid.layermask);
				return(-EBUSY);
			}
		}
		else if (!(layer = kmalloc(sizeof(as_isdn_layer_t), GFP_ATOMIC)))
		{/* else, construct a new layer object */
			int_errtxt("no mem for layer %x", inst->pid.layermask);
			return(-ENOMEM);
		}
		/* clear this layer object even it is come from stack */
		memset(layer, 0, sizeof(as_isdn_layer_t));
		insertlayer(st, layer, inst->pid.layermask);
		layer->inst = inst;	/* this layer represent this instance */	
	}
	
	if (!inst->id)
		refinc++;
	/* assigned id for this instance */
	get_free_instid(st, inst);

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s: inst(%p/%p) id(%x)%s\n", __FUNCTION__, inst, inst->obj, inst->id, refinc ? " changed" : "");
#endif

	if (!inst->id)
	{/* now id has assigned to instance */
		int_errtxt("no free inst->id for layer %x", inst->pid.layermask);
		if (st && layer)
		{
			list_del(&layer->list);
			kfree(layer);
		}
		return(-EINVAL);
	}
	inst->st = st;
	if (refinc)
		inst->obj->refcnt++;
	
	list_add_tail(&inst->list, &AS_ISDN_instlist);
	return(0);
}

/* dis-associated reference relationship between this instance and stack/layer in this stack
* inst->stack; stack->mgr; stack->layer->inst
*/
int unregister_instance(as_isdn_instance_t *inst)
{
	as_isdn_layer_t *layer;
	int err = 0;

	if (!inst)
		return(-EINVAL);

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s: st(%p) inst(%p:%s):%x lay(%x)\n", __FUNCTION__, inst->st, inst, inst->name, inst->id, inst->pid.layermask);
#endif

	if (inst->st)
	{
		if ((layer = getlayer4lay(inst->st, inst->pid.layermask)))
		{
#if AS_ISDN_DEBUG_CORE_CTRL
			if (core_debug & DEBUG_CORE_FUNC)
				printk(KERN_ERR  "%s: layer(%p)->inst(%p:%s)\n",	__FUNCTION__, layer, layer->inst, layer->inst->name);
#endif
/* remove layer here, from release_layers */
			layer->inst = NULL;
			list_del(&layer->list);
			kfree(layer);
			layer = NULL;

		}
		else
		{
			printk(KERN_WARNING "%s: no layer found for instance(%s), stack(%x)\n", __FUNCTION__, inst->name, inst->st->id );
			err = -ENODEV;
		}
		if (inst->st && (inst->st->mgr != inst))
			inst->st = NULL;
	}
	list_del_init(&inst->list);
	inst->id = 0;
	inst->obj->refcnt--;
#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s : instlist(%p<-%p->%p)\n", __FUNCTION__, AS_ISDN_instlist.prev, &AS_ISDN_instlist, AS_ISDN_instlist.next);
#endif
	return(0);
}

/* param 'pbuf' is used as dest dpid->pbuf and copy all fields of src pid */
int copy_pid(AS_ISDN_pid_t *dpid, AS_ISDN_pid_t *spid, u_char *pbuf)
{
	u_int	i, off;

	memcpy(dpid, spid, sizeof(AS_ISDN_pid_t));
	if (spid->pbuf)
	{
		if (!pbuf)
		{
			int_error();
			return(-ENOMEM);
		}
		dpid->pbuf = pbuf;
		memcpy(dpid->pbuf, spid->pbuf, spid->maxplen);

		for (i = 0; i <= MAX_LAYER_NR; i++)
		{
			if (spid->param[i])
			{
				off = (u_int)(spid->param[i] - spid->pbuf);
				dpid->param[i] = dpid->pbuf + off;
			}
		}
	}
	return(0);
}

/* used in kernel thread context */
int set_stack(as_isdn_stack_t *st, AS_ISDN_pid_t *pid)
{
	int 		err;
	u_char		*pbuf = NULL;
	as_isdn_instance_t	*inst;
	as_isdn_layer_t	*hl, *hln;

	if (!st || !pid)
	{
		int_error();
		return(-EINVAL);
	}
	if (!st->mgr || !st->mgr->obj || !st->mgr->obj->ctrl)
	{
		int_error();
		return(-EINVAL);
	}

	if (pid->pbuf)
		pbuf = kmalloc(pid->maxplen, GFP_ATOMIC);
	err = copy_pid(&st->pid, pid, pbuf);
	if (err)
		return(err);	
	memcpy(&st->mgr->pid, &st->pid, sizeof(AS_ISDN_pid_t));

	/* check protocol in obj and inst->pid */
	if (!AS_ISDN_SetHandledPID(st->mgr->obj, &st->mgr->pid))
	{
		int_error();
		return(-ENOPROTOOPT);
	}
	else
	{/* if there are same protocol for them, then remove and continue */
		AS_ISDN_RemoveUsedPID(pid, &st->mgr->pid);
	}

	/* send this primitive to centrel_manager, then transfer primitive MGR_REGLAYER_CONFIRM to ASHFC  */
	err = st->mgr->obj->ctrl(st, MGR_REGLAYER | REQUEST, st->mgr);
	if (err)
	{
		int_error();
		return(err);
	}

	/* create layer_t for every protocols defined in pid :
	check the result of REG_LAYER */
	while (pid->layermask)
	{
		inst = get_next_instance(st, pid);
		if (!inst)
		{
			int_error();
			st->mgr->obj->ctrl(st, MGR_CLEARSTACK| REQUEST, NULL);
			return(-ENOPROTOOPT);
		}
		AS_ISDN_RemoveUsedPID(pid, &inst->pid);
	}
	
	list_for_each_entry_safe(hl, hln, &st->layerlist, list)
	{
#if AS_ISDN_DEBUG_CORE_CTRL
		printk(KERN_INFO "%s : layer's instance %s(ID %x)\n", __FUNCTION__, hl->inst->name,hl->inst->id );
#endif

		if (hl->list.next == &st->layerlist)
		{/* layer has been in st->layerlist */
#if AS_ISDN_DEBUG_CORE_CTRL
			printk(KERN_INFO "%s : layer's instance %s(ID %x): BREAK (connect to %s)\n", __FUNCTION__, hl->inst->name,hl->inst->id, hln->inst->name );
#endif
			break;
		}

		if (!hl->inst)
		{
			int_error();
			return(-EINVAL);
		}
		if (!hl->inst->obj)
		{
			int_error();
			return(-EINVAL);
		}
		if (!hl->inst->obj->own_ctrl)
		{
			int_error();
			return(-EINVAL);
		}
		
#if AS_ISDN_DEBUG_CORE_CTRL
		printk(KERN_INFO "%s : layer's instance %s(ID %x)--MGR_CONNECT (to %s)\n", __FUNCTION__, hl->inst->name,hl->inst->id, hln->inst->name );
#endif
		/* hl is low layer instance, hln is high layer instance */
		hl->inst->obj->own_ctrl(hl->inst, MGR_CONNECT | REQUEST, hln->inst);
	}
	st->mgr->obj->own_ctrl(st->mgr, MGR_SETSTACK |CONFIRM, NULL);
	return(0);
}

/* clear stack->pid and stack->para
 * and call release_layers to make all object execure MGR_UNREGLAYER|REQ
 * stack object is not freed 
*/
int clear_stack(as_isdn_stack_t *st)
{

	if (!st)
		return(-EINVAL);

#if AS_ISDN_DEBUG_CORE_CTRL
	if (core_debug & DEBUG_CORE_FUNC)
		printk(KERN_ERR  "%s: st(%p)\n", __FUNCTION__, st);
#endif

	if (st->pid.pbuf)
		kfree(st->pid.pbuf);
	memset(&st->pid, 0, sizeof(AS_ISDN_pid_t));
	memset(&st->para, 0, sizeof(AS_ISDN_stPara_t));
	
	return(release_layers(st, MGR_UNREGLAYER | REQUEST));
}

static int test_stack_protocol(as_isdn_stack_t *st, u_int l1prot, u_int l2prot, u_int l3prot)
{
	int		cnt = MAX_LAYER_NR + 1, ret = 1;
	AS_ISDN_pid_t	pid;
	as_isdn_instance_t	*inst;
	
	clear_stack(st);
	memset(&pid, 0, sizeof(AS_ISDN_pid_t));
	pid.layermask = ISDN_LAYER(1);
	if (!(((l2prot == 2) || (l2prot == 0x40)) && (l3prot == 1)))
		pid.layermask |= ISDN_LAYER(2);
	if (!(l3prot == 1))
		pid.layermask |= ISDN_LAYER(3);
	
	pid.protocol[1] = l1prot | ISDN_PID_LAYER(1) | ISDN_PID_BCHANNEL_BIT;
	if (pid.layermask & ISDN_LAYER(2))
		pid.protocol[2] = l2prot | ISDN_PID_LAYER(2) | ISDN_PID_BCHANNEL_BIT;
	if (pid.layermask & ISDN_LAYER(3))
		pid.protocol[3] = l3prot | ISDN_PID_LAYER(3) | ISDN_PID_BCHANNEL_BIT;
	copy_pid(&st->pid, &pid, NULL);
	memcpy(&st->mgr->pid, &pid, sizeof(AS_ISDN_pid_t));

	if (!AS_ISDN_SetHandledPID(st->mgr->obj, &st->mgr->pid))
	{
		memset(&st->pid, 0, sizeof(AS_ISDN_pid_t));
		return(-ENOPROTOOPT);
	}
	else
	{
		AS_ISDN_RemoveUsedPID(&pid, &st->mgr->pid);
	}
	if (!pid.layermask)
	{
		memset(&st->pid, 0, sizeof(AS_ISDN_pid_t));
		return(0);
	}
	ret = st->mgr->obj->ctrl(st, MGR_REGLAYER | REQUEST, st->mgr);
	if (ret)
	{
		clear_stack(st);
		return(ret);
	}
	
	while (pid.layermask && cnt--)
	{
		inst = get_next_instance(st, &pid);
		if (!inst)
		{
			st->mgr->obj->ctrl(st, MGR_CLEARSTACK| REQUEST, NULL);
			return(-ENOPROTOOPT);
		}
		AS_ISDN_RemoveUsedPID(&pid, &inst->pid);
	}
	if (!cnt)
		ret = -ENOPROTOOPT;
	clear_stack(st);
	return(ret);
}

static u_int	validL1pid4L2[ISDN_PID_IDX_MAX + 1] = 
{
			0x022d,
			0x03ff,
			0x0000,
			0x0000,
			0x0010,
			0x022d,
			0x03ff,
			0x0380,
			0x022d,
			0x022d,
			0x022d,
			0x01c6,
			0x0000,
};

static u_int	validL2pid4L3[ISDN_PID_IDX_MAX + 1] = 
{
			0x1fff,
			0x0000,
			0x0101,
			0x0101,
			0x0010,
			0x0010,
			0x0000,
			0x00c0,
			0x0000,
};

int evaluate_stack_pids(as_isdn_stack_t *st, AS_ISDN_pid_t *pid)
{
	int 		err;
	AS_ISDN_pid_t	pidmask;
	u_int		l1bitm, l2bitm, l3bitm;
	u_int		l1idx, l2idx, l3idx;

	if (!st || !pid) 
	{
		int_error();
		return(-EINVAL);
	}
	if (!st->mgr || !st->mgr->obj || !st->mgr->obj->ctrl)
	{
		int_error();
		return(-EINVAL);
	}
	
	copy_pid(&pidmask, pid, NULL);
	memset(pid, 0, sizeof(AS_ISDN_pid_t));

	for (l1idx=0; l1idx <= ISDN_PID_IDX_MAX; l1idx++)
	{
		l1bitm = 1 << l1idx;
		if (!(pidmask.protocol[1] & l1bitm))
			continue;
		
		for (l2idx=0; l2idx <= ISDN_PID_IDX_MAX; l2idx++)
		{
			l2bitm = 1 << l2idx;
			if (!(pidmask.protocol[2] & l2bitm))
				continue;
			if (!(validL1pid4L2[l2idx] & l1bitm))
				continue;
			for (l3idx=0; l3idx <= ISDN_PID_IDX_MAX; l3idx++)
			{
				err = 1;
				l3bitm = 1 << l3idx;
				if (!(pidmask.protocol[3] & l3bitm))
					continue;
				if (!(validL2pid4L3[l3idx] & l2bitm))
					continue;
				err = test_stack_protocol(st, l1bitm, l2bitm, l3bitm);
				if (!err)
				{
					pid->protocol[3] |= l3bitm;
					pid->protocol[2] |= l2bitm;
					pid->protocol[1] |= l1bitm;
				}
			}
		}
	}
	return(0);
}
