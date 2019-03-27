/*
 * $Id: fsm.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
//#include <linux/isdn_compat.h>
#ifdef NEED_JIFFIES_INCLUDE
#include <linux/jiffies.h>
#endif
#include <linux/string.h>
#include "fsm.h"

#define FSM_TIMER_DEBUG 0

void as_isdn_fsm_new(struct Fsm *fsm, struct FsmNode *fnlist, int fncount)
{
	int i;

	fsm->jumpmatrix = (FSMFNPTR *)kmalloc(sizeof (FSMFNPTR) * fsm->state_count * fsm->event_count, GFP_KERNEL);
	memset(fsm->jumpmatrix, 0, sizeof (FSMFNPTR) * fsm->state_count * fsm->event_count);

	for (i = 0; i < fncount; i++) 
		if ((fnlist[i].state>=fsm->state_count) || (fnlist[i].event>=fsm->event_count))
		{
			printk(KERN_ERR "as_isdn_fsm_new Error line %d st(%ld/%ld) ev(%ld/%ld)\n",
				i,(long)fnlist[i].state,(long)fsm->state_count, (long)fnlist[i].event,(long)fsm->event_count);
		}
		else		
			fsm->jumpmatrix[fsm->state_count * fnlist[i].event + fnlist[i].state] = (FSMFNPTR) fnlist[i].routine;
}

void as_isdn_fsm_free(struct Fsm *fsm)
{
	kfree((void *) fsm->jumpmatrix);
}

int as_isdn_fsm_event(struct FsmInst *fi, int event, void *arg)
{
	FSMFNPTR r;

	if ((fi->state>=fi->fsm->state_count) || (event >= fi->fsm->event_count))
	{
		printk(KERN_ERR "as_isdn_fsm_event Error st(%ld/%ld) ev(%d/%ld)\n",
			(long)fi->state,(long)fi->fsm->state_count,event,(long)fi->fsm->event_count);
		return(1);
	}
	
	r = fi->fsm->jumpmatrix[fi->fsm->state_count * event + fi->state];
	if (r)
	{
#if AS_ISDN_DEBUG_FSM
		if (fi->debug)
			fi->printdebug(fi, "State %s Event %s", fi->fsm->strState[fi->state],fi->fsm->strEvent[event]);
#endif		
		r(fi, event, arg);
		return (0);
	}
	else
	{
#if AS_ISDN_DEBUG_FSM
		if (fi->debug)
			fi->printdebug(fi, "State %s Event %s no action",fi->fsm->strState[fi->state], fi->fsm->strEvent[event]);
#endif
		return (!0);
	}
}

void fsm_change_state(struct FsmInst *fi, int newstate)
{
	fi->state = newstate;
#if AS_ISDN_DEBUG_FSM
	if (fi->debug)
		fi->printdebug(fi, "ChangeState %s",	fi->fsm->strState[newstate]);
#endif	
}

static void FsmExpireTimer(struct FsmTimer *ft)
{
#if AS_ISDN_DEBUG_FSM
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "FsmExpireTimer %lx", (long) ft);
#endif
#endif
	as_isdn_fsm_event(ft->fi, ft->event, ft->arg);
}

/* associated a FsmTimer with a FsmInst */
void fsm_init_timer(struct FsmInst *fi, struct FsmTimer *ft)
{
	ft->fi = fi;
	ft->tl.function = (void *) FsmExpireTimer;
	ft->tl.data = (long) ft;
#if AS_ISDN_DEBUG_FSM
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "fsm_init_timer %lx", (long) ft);
#endif
#endif
	init_timer(&ft->tl);
}

void fsm_del_timer(struct FsmTimer *ft, int where)
{
#if AS_ISDN_DEBUG_FSM
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "fsm_del_timer %lx %d", (long) ft, where);
#endif
#endif
	del_timer(&ft->tl);
}

int fsm_add_timer(struct FsmTimer *ft, int millisec, int event, void *arg, int where)
{
#if AS_ISDN_DEBUG_FSM
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "fsm_add_timer %lx %d %d",	(long) ft, millisec, where);
#endif
#endif
	if (timer_pending(&ft->tl))
	{
		printk(KERN_WARNING "fsm_add_timer: timer already active!\n");
#if AS_ISDN_DEBUG_FSM
		ft->fi->printdebug(ft->fi, "fsm_add_timer already active!");
#endif
		return -1;
	}
	init_timer(&ft->tl);
	ft->event = event;
	ft->arg = arg;
	ft->tl.expires = jiffies + (millisec * HZ) / 1000;
	add_timer(&ft->tl);
	return 0;
}

void fsm_restart_timer(struct FsmTimer *ft,
	    int millisec, int event, void *arg, int where)
{
#if AS_ISDN_DEBUG_FSM
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "fsm_restart_timer %lx %d %d", (long) ft, millisec, where);
#endif
#endif
	if (timer_pending(&ft->tl))
		del_timer(&ft->tl);
	init_timer(&ft->tl);
	ft->event = event;
	ft->arg = arg;
	ft->tl.expires = jiffies + (millisec * HZ) / 1000;
	add_timer(&ft->tl);
}

EXPORT_SYMBOL(as_isdn_fsm_new);
EXPORT_SYMBOL(as_isdn_fsm_free);
EXPORT_SYMBOL(as_isdn_fsm_event);
EXPORT_SYMBOL(fsm_change_state);
EXPORT_SYMBOL(fsm_init_timer);
EXPORT_SYMBOL(fsm_add_timer);
EXPORT_SYMBOL(fsm_restart_timer);
EXPORT_SYMBOL(fsm_del_timer);
