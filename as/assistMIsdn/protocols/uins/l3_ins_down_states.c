/* 
* $Id: l3_ins_down_states.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
* Common logic for used in fromup interface
* Li Zhijie, 2005.08.24
*/

#include "l3.h"

//#include "l3_ins_state.h"
void l3ins_dummy(l3_process_t *pc, u_char pr, void *arg);
void l3ins_restart(l3_process_t *pc, u_char pr, void *arg);
void l3ins_status(l3_process_t *pc, u_char pr, void *arg);
void l3ins_facility(l3_process_t *pc, u_char pr, void *arg);


void l3ins_disconnect_req(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	Q931_info_t	*qi;
	u_char		*p;

	StopAllL3Timer(pc);
	if (arg)
	{
		qi = (Q931_info_t *)skb->data;
		if (!qi->cause)
		{
			qi->cause = skb->len - L3_EXTRA_SIZE;
			p = skb_put(skb, 4);
			*p++ = IE_CAUSE;
			*p++ = 2;
			*p++ = 0x80 | CAUSE_LOC_USER;
			*p++ = 0x80 | CAUSE_NORMALUNSPECIFIED;
		}
		SendMsg(pc, arg, 11);
	}
	else
	{
		newl3state(pc, 11);
		l3ins_message_cause(pc, MT_DISCONNECT, CAUSE_NORMALUNSPECIFIED);
	}
	L3AddTimer(&pc->timer, T305, CC_T305);
}

static void l3ins_setup_req(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = skb_clone(arg, GFP_ATOMIC);

	if (!SendMsg(pc, arg, 1))
	{
		L3DelTimer(&pc->timer);
		L3AddTimer(&pc->timer, T303, CC_T303);
		pc->t303skb = skb;
	}
	else
		dev_kfree_skb(skb);
}

static void l3ins_connect_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (!pc->bc)
	{
#if AS_ISDN_DEBUG_L3_STATE
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "D-chan connect for waiting call");
			printk(KERN_DEBUG "D-chan connect for waiting call\n");
		}	
#endif		
		l3ins_disconnect_req(pc, pr, NULL);
		return;
	}

	if (arg)
	{
		SendMsg(pc, arg, 8);
	}
	else
	{
		newl3state(pc, 8);
		l3ins_message(pc, MT_CONNECT);
	}
	L3DelTimer(&pc->timer);
	L3AddTimer(&pc->timer, T313, CC_T313);
}

static void l3ins_release_cmpl_req(l3_process_t *pc, u_char pr, void *arg)
{
	StopAllL3Timer(pc);
	if (arg)
	{
		SendMsg(pc, arg, 0);
	}
	else
	{
		newl3state(pc, 0);
		l3ins_message(pc, MT_RELEASE_COMPLETE);
	}
	AS_ISDN_l3up(pc, CC_RELEASE_COMPLETE | CONFIRM, NULL);
	release_l3_process(pc);
}

static void l3ins_alert_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (arg)
	{
		SendMsg(pc, arg, 7);
	}
	else
	{
		newl3state(pc, 7);
		l3ins_message(pc, MT_ALERTING);
	}
	L3DelTimer(&pc->timer);
}

static void l3ins_proceed_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (arg)
	{
		SendMsg(pc, arg, 9);
	}
	else
	{
		newl3state(pc, 9);
		l3ins_message(pc, MT_CALL_PROCEEDING);
	}
	L3DelTimer(&pc->timer);
}

static void l3ins_setup_ack_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (arg)
	{
		SendMsg(pc, arg, 25);
	}
	else
	{
		newl3state(pc, 25);
		l3ins_message(pc, MT_SETUP_ACKNOWLEDGE);
	}
	L3DelTimer(&pc->timer);
	L3AddTimer(&pc->timer, T302, CC_T302);
}

static void l3ins_suspend_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (arg)
	{
		SendMsg(pc, arg, 15);
	}
	else
	{
		newl3state(pc, 15);
		l3ins_message(pc, MT_SUSPEND);
	}
	L3AddTimer(&pc->timer, T319, CC_T319);
}

static void l3ins_resume_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (arg)
	{
		SendMsg(pc, arg, 17);
	}
	else
	{
		newl3state(pc, 17);
		l3ins_message(pc, MT_RESUME);
	}
	L3AddTimer(&pc->timer, T318, CC_T318);
}

static void l3ins_status_enq_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (arg)
		dev_kfree_skb(arg);
	l3ins_message(pc, MT_STATUS_ENQUIRY);
}

static void l3ins_information_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (pc->state == 2)
	{
		L3DelTimer(&pc->timer);
		L3AddTimer(&pc->timer, T304, CC_T304);
	}

	if (arg)
	{
		SendMsg(pc, arg, -1);
	}
}

static void l3ins_notify_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (arg)
	{
		SendMsg(pc, arg, -1);
	}
}

static void l3ins_progress_req(l3_process_t *pc, u_char pr, void *arg)
{
	if (arg)
	{
		SendMsg(pc, arg, -1);
	}
}


void l3ins_release_req(l3_process_t *pc, u_char pr, void *arg)
{
	StopAllL3Timer(pc);
	if (arg)
	{
		SendMsg(pc, arg, 19);
	}
	else
	{
		newl3state(pc, 19);
		l3ins_message(pc, MT_RELEASE);
	}
	L3AddTimer(&pc->timer, T308, CC_T308_1);
}

/***************************************************************
State Machine for : global CallRef|Call(l3_process_t)|Down|Up 
***************************************************************/
/* pass data/primitive down to low layer */
struct stateentry downstatelist[] =
{
	{SBIT(0),			CC_SETUP | REQUEST, 					l3ins_setup_req},
	{SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) |
		SBIT(10) | SBIT(11) | SBIT(12) | SBIT(15) | SBIT(25),
						CC_INFORMATION | REQUEST, 			l3ins_information_req},
	{ALL_STATES,			CC_NOTIFY | REQUEST, 				l3ins_notify_req},
	{SBIT(10),			CC_PROGRESS | REQUEST, 				l3ins_progress_req},
	{SBIT(0),			CC_RESUME | REQUEST, 				l3ins_resume_req},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(6) | SBIT(7) |
		SBIT(8) | SBIT(9) | SBIT(10) | SBIT(25),
						CC_DISCONNECT | REQUEST, 			l3ins_disconnect_req},
	{SBIT(11) | SBIT(12),	CC_RELEASE | REQUEST, 				l3ins_release_req},
	{ALL_STATES,			CC_RESTART | REQUEST, 				l3ins_restart},
	{SBIT(6) | SBIT(25),	CC_SETUP | RESPONSE, 				l3ins_release_cmpl_req},
	{ALL_STATES,			CC_RELEASE_COMPLETE | REQUEST, 		l3ins_release_cmpl_req},
	{SBIT(6) | SBIT(25),	CC_PROCEEDING | REQUEST, 			l3ins_proceed_req},
	{SBIT(6),			CC_SETUP_ACKNOWLEDGE | REQUEST, 	l3ins_setup_ack_req},
	{SBIT(25),			CC_SETUP_ACKNOWLEDGE | REQUEST, 	l3ins_dummy},
	{SBIT(6) | SBIT(9) | SBIT(25),
						CC_ALERTING | REQUEST, 				l3ins_alert_req},
	{SBIT(6) | SBIT(7) | SBIT(9) | SBIT(25),
						CC_CONNECT | REQUEST, 				l3ins_connect_req},
	{SBIT(10),			CC_SUSPEND | REQUEST, 				l3ins_suspend_req},
	{ALL_STATES,			CC_STATUS_ENQUIRY | REQUEST, 		l3ins_status_enq_req},
};

#define DOWNSLLEN \
	(sizeof(downstatelist) / sizeof(struct stateentry))

/* used in fromup interface : pass msg down */
void l3ins_down_handler( l3_process_t *proc, struct sk_buff *skb)
{
	as_isdn_head_t	*hh;
	int i;
	
	hh = AS_ISDN_HEAD_P(skb);
	
	for (i = 0; i < DOWNSLLEN; i++)
	{
		if ((hh->prim == downstatelist[i].primitive) && ((1 << proc->state) & downstatelist[i].state))
			break;
	}	

	if (i == DOWNSLLEN)
	{
#if AS_ISDN_DEBUG_L3_STATE
		if (proc->l3->debug & L3_DEB_STATE)
		{
//			l3_debug(proc->l3, "uins down state %d prim %#x unhandled", proc->state, hh->prim);
			printk(KERN_ERR  "INS USER down state %d prim %#x unhandled", proc->state, hh->prim);
		}
#endif
		dev_kfree_skb(skb);
	}
	else
	{
#if AS_ISDN_DEBUG_L3_STATE
		if (proc->l3->debug & L3_DEB_STATE)
		{
//			l3_debug(proc->l3, "INS USER down state %d prim %#x para len %d\n", proc->state, hh->prim, skb->len);
			printk(KERN_DEBUG "INS USER down state %d prim %#x para len %d\n", proc->state, hh->prim, skb->len);
		}
#endif
		/* pass data/primitive down to low layer */
		if (skb->len)
			downstatelist[i].rout(proc, hh->prim, skb);
		else
		{
			downstatelist[i].rout(proc, hh->prim, NULL);
			dev_kfree_skb(skb);
		}
	}
}

