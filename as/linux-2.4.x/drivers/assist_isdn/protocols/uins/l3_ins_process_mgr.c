/* 
* $Id: l3_ins_process_mgr.c,v 1.1.1.1 2006/11/29 08:55:14 lizhijie Exp $
* handler for all l3_process_t  
* used only in l3_process_mgr.c 
* Li Zhijie, 2005.08.24
*/

#include "l3.h"

extern void l3ins_restart(l3_process_t *pc, u_char pr, void *arg);
extern void l3ins_disconnect_req(l3_process_t *pc, u_char pr, void *arg);

static void l3ins_t302(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
	newl3state(pc, 11);
	l3ins_message_cause(pc, MT_DISCONNECT, CAUSE_INVALID_NUMBER);
	AS_ISDN_l3up(pc, CC_TIMEOUT | INDICATION, NULL);
}

static void l3ins_t303(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
	if (pc->n303 > 0)
	{
		pc->n303--;
		if (pc->t303skb)
		{
			struct sk_buff	*skb;
			if (pc->n303 > 0)
			{
				skb = skb_clone(pc->t303skb, GFP_ATOMIC);
			}
			else
			{
				skb = pc->t303skb;
				pc->t303skb = NULL;
			}
			if (skb)
				SendMsg(pc, skb, -1);
		}
		L3AddTimer(&pc->timer, T303, CC_T303);
		return;
	}
	if (pc->t303skb)
		kfree_skb(pc->t303skb);
	pc->t303skb = NULL;
	l3ins_message_cause(pc, MT_RELEASE_COMPLETE, CAUSE_TIMER_EXPIRED);
	AS_ISDN_l3up(pc, CC_TIMEOUT | INDICATION, NULL);
	release_l3_process(pc);
}

static void l3ins_t304(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
	newl3state(pc, 11);
	l3ins_message_cause(pc, MT_DISCONNECT, CAUSE_TIMER_EXPIRED);
	AS_ISDN_l3up(pc, CC_TIMEOUT | INDICATION, NULL);
}

static void l3ins_t305(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
#if 0
	if (pc->cause != NO_CAUSE)
		cause = pc->cause;
#endif
	newl3state(pc, 19);
	l3ins_message_cause(pc, MT_RELEASE, CAUSE_NORMALUNSPECIFIED);
	L3AddTimer(&pc->timer, T308, CC_T308_1);
}

static void l3ins_t310(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
	newl3state(pc, 11);
	l3ins_message_cause(pc, MT_DISCONNECT, CAUSE_TIMER_EXPIRED);
	AS_ISDN_l3up(pc, CC_TIMEOUT | INDICATION, NULL);
}

static void l3ins_t313(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
	newl3state(pc, 11);
	l3ins_message_cause(pc, MT_DISCONNECT, CAUSE_TIMER_EXPIRED);
	AS_ISDN_l3up(pc, CC_TIMEOUT | INDICATION, NULL);
}

static void l3ins_t308_1(l3_process_t *pc, u_char pr, void *arg)
{
	newl3state(pc, 19);
	L3DelTimer(&pc->timer);
	l3ins_message(pc, MT_RELEASE);
	L3AddTimer(&pc->timer, T308, CC_T308_2);
}

static void l3ins_t308_2(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
	AS_ISDN_l3up(pc, CC_TIMEOUT | INDICATION, NULL);
	release_l3_process(pc);
}

static void l3ins_t318(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
#if 0
	pc->cause = 102;	/* Timer expiry */
	pc->para.loc = 0;	/* local */
#endif
	AS_ISDN_l3up(pc, CC_RESUME_REJECT | INDICATION, NULL);
	newl3state(pc, 19);
	l3ins_message(pc, MT_RELEASE);
	L3AddTimer(&pc->timer, T308, CC_T308_1);
}

static void l3ins_t319(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
#if 0
	pc->cause = 102;	/* Timer expiry */
	pc->para.loc = 0;	/* local */
#endif
	AS_ISDN_l3up(pc, CC_SUSPEND_REJECT | INDICATION, NULL);
	newl3state(pc, 10);
}

static void l3ins_dl_reset(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*nskb, *skb = alloc_skb(L3_EXTRA_SIZE + 10, GFP_ATOMIC);
	Q931_info_t	*qi;
	u_char		*p;

	if (!skb)
		return;
	qi = (Q931_info_t *)skb_put(skb, L3_EXTRA_SIZE);
	AS_ISDN_initQ931_info(qi);
	qi->type = MT_DISCONNECT;
	qi->cause = 1;
	p = skb_put(skb, 5);
	p++;
	*p++ = IE_CAUSE;
	*p++ = 2;
	*p++ = 0x80 | CAUSE_LOC_USER;
	*p++ = 0x80 | CAUSE_TEMPORARY_FAILURE;
	nskb = skb_clone(skb, GFP_ATOMIC);
	l3ins_disconnect_req(pc, pr, skb);
	if (nskb) 
	{
		if (AS_ISDN_l3up(pc, CC_DISCONNECT | REQUEST, nskb))
			dev_kfree_skb(nskb);
	}
}

static void l3ins_dl_release(l3_process_t *pc, u_char pr, void *arg)
{
	newl3state(pc, 0);
#if 0
        pc->cause = 0x1b;          /* Destination out of order */
        pc->para.loc = 0;
#endif
	release_l3_process(pc);
}

static void l3ins_dl_reestablish(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);
	L3AddTimer(&pc->timer, T309, CC_T309);
	l3_msg(pc->l3, DL_ESTABLISH | REQUEST, 0, 0, NULL);
}
 
static void l3ins_dl_reest_status(l3_process_t *pc, u_char pr, void *arg)
{
	L3DelTimer(&pc->timer);

	l3ins_status_send(pc, CAUSE_NORMALUNSPECIFIED);
}

static void l3ins_reset(l3_process_t *pc, u_char pr, void *arg)
{
	release_l3_process(pc);
}



/* state machine for Call(l3_process_t) used by L3 manager(layer3_t) */
struct stateentry manstatelist[] =
{/*    state                           primitive                              handler */
	{SBIT(2),		DL_ESTABLISH | INDICATION, 	l3ins_dl_reset},
	{SBIT(10),		DL_ESTABLISH | CONFIRM, 		l3ins_dl_reest_status},
	{SBIT(10),		DL_RELEASE | INDICATION, 	l3ins_dl_reestablish},
	{ALL_STATES,		DL_RELEASE | INDICATION, 	l3ins_dl_release},
	{SBIT(25),		CC_T302, 					l3ins_t302},
	{SBIT(1),		CC_T303, 					l3ins_t303},
	{SBIT(2),		CC_T304, 					l3ins_t304},
	{SBIT(3),		CC_T310, 					l3ins_t310},
	{SBIT(8),		CC_T313, 					l3ins_t313},
	{SBIT(11),		CC_T305, 					l3ins_t305},
	{SBIT(15),		CC_T319, 					l3ins_t319},
	{SBIT(17),		CC_T318, 					l3ins_t318},
	{SBIT(19),		CC_T308_1, 					l3ins_t308_1},
	{SBIT(19),		CC_T308_2, 					l3ins_t308_2},
	{SBIT(10),		CC_T309, 					l3ins_dl_release},
	{SBIT(6),		CC_TCTRL, 					l3ins_reset},
	{ALL_STATES,		CC_RESTART | REQUEST, 		l3ins_restart},
};

#define MANSLLEN \
        (sizeof(manstatelist) / sizeof(struct stateentry))


/* layer manager entry point 
* when call (l3_process_t) is in state xx and receive primitive, 
* the manager handle this call with primitive and arg
*/
int l3_process_mgr(l3_process_t *proc, u_int pr, void *arg)
{
	u_int	i;
 
	if (!proc)
	{
		printk(KERN_ERR "AS_ISDN l3_process_mgr without proc pr=%04x\n", pr);
		return(-EINVAL);
	}

	for (i = 0; i < MANSLLEN; i++)
	{
		if ((pr == manstatelist[i].primitive) && ((1 << proc->state) & manstatelist[i].state))
			break;
	}	

	if (i == MANSLLEN)
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (proc->l3->debug & L3_DEB_STATE)
		{
//			l3_debug(proc->l3, "cr %d l3_process_mgr state %d prim %#x unhandled", proc->callref & 0x7fff, proc->state, pr);
			printk(KERN_WARNING "cr %d l3_process_mgr state %d prim %#x unhandled\n", proc->callref & 0x7fff, proc->state, pr);
		}
#endif
	}
	else
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (proc->l3->debug & L3_DEB_STATE)
		{
//			l3_debug(proc->l3, "cr %d l3_process_mgr state %d prim %#x", proc->callref & 0x7fff, proc->state, pr);
			printk(KERN_DEBUG "cr %d l3_process_mgr state %d prim %#x\n", proc->callref & 0x7fff, proc->state, pr);
		}
#endif
		/* when primitive and state are same between manstatelist and parameters */
		manstatelist[i].rout(proc, pr, arg);
	}

	return(0);
}

