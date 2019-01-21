/*
 * $Id: udevice.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>
#include <linux/config.h>
#include <linux/timer.h>
/* added by lizhijie*/
#include <linux/init.h>

#include "core.h"

#define MAX_HEADER_LEN				4


#define AS_ISDN_DEV_NAME			"asisdn"


as_isdn_device_t		mainDev;	/* for D channel access, lizhijie, 2005.12.22 */

as_isdn_object_t		udev_obj;

int device_debug ;

// static int from_peer(as_isdn_if_t *, u_int, int, int, void *);
// static int to_peer(as_isdn_if_t *, u_int, int, int, void *);

#include <linux/slab.h>


/* sleep in user space until woken up. Equivilant of tsleep() in BSD */
int  as_schedule_delay(wait_queue_head_t *q)
{
	/* declare a wait_queue_t variable as 'wait', and add 'current' task into it */
	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue(q, &wait);
	current->state = TASK_INTERRUPTIBLE;

	if (!signal_pending(current)) 
		schedule();
	
	/* here, this task is waken up, and going to continue */
	current->state = TASK_RUNNING;
	remove_wait_queue(q, &wait);

	if (signal_pending(current)) 
		return -ERESTARTSYS;

	return(0);
}


devicelayer_t *get_devlayer(as_isdn_device_t *dev, int addr)
{
	devicelayer_t *dl;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: addr:%x\n", __FUNCTION__, addr);
#endif

	list_for_each_entry(dl, &dev->layerlist, list)
	{
//		if (device_debug & DEBUG_MGR_FUNC)
//			printk(KERN_ERR  "%s: dl(%p) iaddr:%x\n",
//				__FUNCTION__, dl, dl->iaddr);
		if ((u_int)dl->iaddr == (IF_IADDRMASK & addr))
			return(dl);
	}
	return(NULL);
}

static devicestack_t *get_devstack(as_isdn_device_t *dev, int addr)
{
	devicestack_t *ds;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "%s: addr:%x\n", __FUNCTION__, addr);
#endif
	
	list_for_each_entry(ds, &dev->stacklist, list)
	{
		if (ds->st && (ds->st->id == (u_int)addr))
			return(ds);
	}
	return(NULL);
}

/* release devicestack_t */
static int del_stack(devicestack_t *ds)
{
	as_isdn_device_t	*dev;

	if (!ds)
	{
		int_error();
		return(-EINVAL);
	}
	dev = ds->dev;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
	{
		printk(KERN_ERR  "%s: ds(%p) dev(%p)\n", 	__FUNCTION__, ds, dev);
	}
#endif

	if (!dev)
		return(-EINVAL);
	
	if (ds->st)
	{
		if (ds->extentions & EXT_STACK_CLONE)
			INIT_LIST_HEAD(&ds->st->childlist);
		udev_obj.ctrl(ds->st, MGR_DELSTACK | REQUEST, NULL);
	}
	
	list_del(&ds->list);
	kfree(ds);
	
	return(0);
}

int del_layer(devicelayer_t *dl)
{
	as_isdn_instance_t *inst = &dl->inst;
	as_isdn_device_t	*dev = dl->dev;
	int		i;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
	{
		printk(KERN_ERR  "%s: dl(%p) inst(%p) LM(%x) dev(%p)\n", 	__FUNCTION__, dl, inst, inst->pid.layermask, dev);
		printk(KERN_ERR  "%s: iaddr %x inst %s slave %p\n", __FUNCTION__, dl->iaddr, inst->name, dl->slave);
	}
#endif
	
	remove_if(dl, IF_UP);
	remove_if(dl, IF_DOWN);
	if (dl->slave)
	{
		if (dl->slave->obj)
			dl->slave->obj->own_ctrl(dl->slave,
				MGR_UNREGLAYER | REQUEST, NULL);
		else
			dl->slave = NULL; 
	}
	if (dl->lm_st && inst->st)
	{
		for (i=0; i<= MAX_LAYER_NR; i++) 
		{
			if (dl->lm_st & ISDN_LAYER(i))
			{
				inst->st->pid.protocol[i] = ISDN_PID_NONE;
			}
		}
		dl->lm_st = 0;
	}
	
	if (test_and_clear_bit(FLG_MGR_SETSTACK, &dl->Flags) && inst->st)
	{

#if AS_ISDN_DEBUG_CORE_DATA
		if (device_debug & DEBUG_MGR_FUNC)
			printk(KERN_ERR  "del_layer: CLEARSTACK id(%x)\n",	inst->st->id);
#endif
		
		udev_obj.ctrl(inst->st, MGR_CLEARSTACK | REQUEST, NULL);
	}

	if (inst->up.peer)
	{
		inst->up.peer->obj->ctrl(inst->up.peer,
			MGR_DISCONNECT | REQUEST, &inst->up);
	}

	if (inst->down.peer)
	{
		inst->down.peer->obj->ctrl(inst->down.peer,
			MGR_DISCONNECT | REQUEST, &inst->down);
	}

	dl->iaddr = 0;
	list_del(&dl->list);
	udev_obj.ctrl(inst, MGR_UNREGLAYER | REQUEST, NULL);

	if (test_and_clear_bit(FLG_MGR_OWNSTACK, &dl->Flags))
	{
		if (dl->inst.st)
		{
			del_stack(get_devstack(dev, dl->inst.st->id));
		}
	}
	
	kfree(dl);
	dl = NULL;
	return(0);
}


static int set_if(devicelayer_t *dl, u_int prim, as_isdn_if_t *hif)
{
	int err = 0;

	err = AS_ISDN_SetIF(&dl->inst, hif, prim, from_up_down, from_up_down, dl);
	return(err);
}

static int udev_manager(void *data, u_int prim, void *arg)
{
	as_isdn_instance_t *inst = data;
	as_isdn_device_t	*dev = &mainDev;
	devicelayer_t	*dl;
	int		err = -EINVAL;

#if AS_ISDN_DEBUG_CORE_DATA
	if (device_debug & DEBUG_MGR_FUNC)
		printk(KERN_ERR  "DEV-MGR : data:%p(inst:%s) prim:%x arg:%p\n", data,inst->name, prim, arg);
#endif

	if (!data)
		return(-EINVAL);
//	read_lock(&AS_ISDN_device_lock);

#if 0
	list_for_each_entry(dev, &AS_ISDN_devicelist, list)
	{
#endif

		list_for_each_entry(dl, &dev->layerlist, list)
		{
			if (&dl->inst == inst)
			{
				err = 0;
				break;
			}
		}
	if (err)
	{
		printk(KERN_WARNING "DEV-MGR : prim %x without device layer\n", prim);
		goto out;
	}
#if 0		
		if (!err)
			break;
	}
	if (err)
	{
		printk(KERN_WARNING "DEV-MGR : prim %x without device layer\n", prim);
		goto out;
	}
#endif

	switch(prim)
	{
		case MGR_CONNECT | REQUEST:
			err = AS_ISDN_ConnectIF(inst, arg);
			break;

		case MGR_SETIF | REQUEST:
		case MGR_SETIF | INDICATION:
			err = set_if(dl, prim, arg);
			break;

		case MGR_DISCONNECT | REQUEST:
		case MGR_DISCONNECT | INDICATION:
			err = AS_ISDN_DisConnectIF(inst, arg);
			break;

		case MGR_RELEASE | INDICATION:
#if AS_ISDN_DEBUG_CORE_DATA
			if (device_debug & DEBUG_MGR_FUNC)
				printk(KERN_ERR  "DEV-MGR : release_dev id %x\n", dl->inst.st->id);
#endif
			del_layer(dl);
			err = 0;
			break;
			
		default:
			printk(KERN_WARNING "DEV-MGR :  prim %x not handled\n", prim);
			err = -EINVAL;
			break;
	}
out:
	
//	read_unlock(&AS_ISDN_device_lock);
	return(err);
}

static void _init_main_device(void)
{
	as_isdn_device_t  *dev = &mainDev;

	memset(dev, 0, sizeof(as_isdn_device_t));
	dev->minor = 0;
	init_waitqueue_head(&dev->rport.procq);
	init_waitqueue_head(&dev->wport.procq);
		
	skb_queue_head_init(&dev->rport.queue);
	skb_queue_head_init(&dev->wport.queue);
		
	init_MUTEX(&dev->in_sema);
	init_MUTEX(&dev->out_sema);
	init_MUTEX(&dev->ioctrl_sema);
		
	INIT_LIST_HEAD(&dev->layerlist);
	INIT_LIST_HEAD(&dev->stacklist);
	INIT_LIST_HEAD(&dev->timerlist);
	INIT_LIST_HEAD(&dev->entitylist);
}

/* object for udevice is used in every layers of B and D channels */
int init_isdn_device (int debug)
{
	int err,i;

	udev_obj.name = "ISDN-Dev";
	for (i=0; i<=MAX_LAYER_NR; i++)
	{
		udev_obj.DPROTO.protocol[i] = ISDN_PID_ANY;
		udev_obj.BPROTO.protocol[i] = ISDN_PID_ANY;
	}
	
	INIT_LIST_HEAD(&udev_obj.ilist);
	udev_obj.own_ctrl = udev_manager;
	device_debug = debug;
	if (register_chrdev(AS_ISDN_DEV_MAJOR, AS_ISDN_DEV_NAME, &as_isdn_fops))
	{
		printk(KERN_WARNING "AS ISDN: Could not register devices %s\n", AS_ISDN_DEV_NAME);
		return(-EIO);
	}

	_init_main_device();
	
	if ((err = as_isdn_register(&udev_obj)))
	{
		printk(KERN_ERR "Can't register %s error(%d)\n", udev_obj.name, err);
	}
	return(err);
}

int free_isdn_device(void)
{
	int 		err = 0;

	as_rawdev_free_all();
		
	if ((err = as_isdn_unregister(&udev_obj)))
	{
		printk(KERN_ERR "Can't unregister %s (%d)\n", udev_obj.name, err);
	}
	if ((err = unregister_chrdev(AS_ISDN_DEV_MAJOR, AS_ISDN_DEV_NAME)))
	{
		printk(KERN_WARNING "AS ISDN: devices %s busy on remove\n",AS_ISDN_DEV_NAME);
	}
	return(err);
}

