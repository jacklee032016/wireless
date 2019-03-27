/*
 * $Id: layer1.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * AS_ISDN_l1.c     common low level stuff for I.430 layer1
 */

#include <linux/config.h>
#include <linux/module.h>
#include "layer1.h"
#include "helper.h"
#if 0
#include "debug.h"
#endif

#include "as_isdn_version.h"

typedef struct _layer1
{
	struct list_head	list;
	u_long			Flags;
	struct FsmInst	l1m;
	struct FsmTimer 	timer;
	int				debug;
	int				delay;
	as_isdn_instance_t	inst;
} layer1_t;

/* l1 status_info */
typedef struct _status_info_l1
{
	int	len;
	int	typ;
	int	protocol;
	int	status;
	int	state;
	u_long	Flags;
	int	T3;
	int	delay;
	int	debug;
} status_info_l1_t;

static int asl1_debug = 0xFFFFFFFF;
static as_isdn_object_t isdnl1;

#define TIMER3_VALUE 7000

static struct Fsm l1fsm_b = {NULL, 0, 0, NULL, NULL};

static struct Fsm l1fsm_s = {NULL, 0, 0, NULL, NULL};

/* TE state, refer to specs vol. 1, page 20 */
enum
{
	ST_L1_F2,	/* sensing */
	ST_L1_F3,	/* deactivated*/
	ST_L1_F4,	/* awaiting signal  */
	ST_L1_F5,	/* Identifyinh input */
	ST_L1_F6,	/* Synchronized */
	ST_L1_F7,	/* Activated */
	ST_L1_F8,	/* Lost framing */
};

#define L1S_STATE_COUNT (ST_L1_F8+1)

static char *strL1SState[] =
{
	"ST_L1_F2",
	"ST_L1_F3",
	"ST_L1_F4",
	"ST_L1_F5",
	"ST_L1_F6",
	"ST_L1_F7",
	"ST_L1_F8",
};

#ifdef AS_ISDN_UINTERFACE
static struct Fsm l1fsm_u =
{NULL, 0, 0, NULL, NULL};

enum
{
	ST_L1_RESET,
	ST_L1_DEACT,
	ST_L1_SYNC2,
	ST_L1_TRANS,
};

#define L1U_STATE_COUNT (ST_L1_TRANS+1)

static char *strL1UState[] =
{
	"ST_L1_RESET",
	"ST_L1_DEACT",
	"ST_L1_SYNC2",
	"ST_L1_TRANS",
};
#endif

enum 
{
	ST_L1_NULL,
	ST_L1_WAIT_ACT,
	ST_L1_WAIT_DEACT,
	ST_L1_ACTIV,
};

#define L1B_STATE_COUNT (ST_L1_ACTIV+1)

static char *strL1BState[] =
{
	"ST_L1_NULL",
	"ST_L1_WAIT_ACT",
	"ST_L1_WAIT_DEACT",
	"ST_L1_ACTIV",
};

enum 
{
	EV_PH_ACTIVATE,
	EV_PH_DEACTIVATE,
	EV_RESET_IND,
	EV_DEACT_CNF,
	EV_DEACT_IND,
	EV_POWER_UP,
	EV_ANYSIG_IND, 
	EV_INFO2_IND,
	EV_INFO4_IND,
	EV_TIMER_DEACT,
	EV_TIMER_ACT,
	EV_TIMER3,
};

#define L1_EVENT_COUNT (EV_TIMER3 + 1)

static char *strL1Event[] =
{
	"EV_PH_ACTIVATE",
	"EV_PH_DEACTIVATE",
	"EV_RESET_IND",
	"EV_DEACT_CNF",
	"EV_DEACT_IND",
	"EV_POWER_UP",
	"EV_ANYSIG_IND", 
	"EV_INFO2_IND",
	"EV_INFO4_IND",
	"EV_TIMER_DEACT",
	"EV_TIMER_ACT",
	"EV_TIMER3",
};

#if AS_ISDN_DEBUG_FSM
static void l1m_debug(struct FsmInst *fi, char *fmt, ...)
{
	layer1_t *l1 = fi->userdata;
	logdata_t log;

	va_start(log.args, fmt);
	log.fmt = fmt;
	log.head = l1->inst.name;
	/* call central_manager export msg in console */
	l1->inst.obj->ctrl(&l1->inst, MGR_DEBUGDATA | REQUEST, &log);
	va_end(log.args);
}
#endif

/* create skb for primitive and call up interface with this skb */
static int l1up(layer1_t *l1, u_int prim, int dinfo, int len, void *arg)
{
	return(if_link(&l1->inst.up, prim, dinfo, len, arg, 0));
}

static int l1down(layer1_t *l1, u_int prim, int dinfo, int len, void *arg)
{
	return(if_link(&l1->inst.down, prim, dinfo, len, arg, 0));
}

static void l1_reset(struct FsmInst *fi, int event, void *arg)
{
	fsm_change_state(fi, ST_L1_F3);
}

static void l1_deact_cnf(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	fsm_change_state(fi, ST_L1_F3);
	if (test_bit(FLG_L1_ACTIVATING, &l1->Flags))
		l1down(l1, PH_CONTROL | REQUEST, HW_POWERUP, 0, NULL);
}

static void l1_deact_req_s(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	fsm_change_state(fi, ST_L1_F3);
	fsm_restart_timer(&l1->timer, 550, EV_TIMER_DEACT, NULL, 2);
	test_and_set_bit(FLG_L1_DEACTTIMER, &l1->Flags);
}

static void l1_power_up_s(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	if (test_bit(FLG_L1_ACTIVATING, &l1->Flags))
	{
		fsm_change_state(fi, ST_L1_F4);
		l1down(l1, PH_SIGNAL | REQUEST, INFO3_P8, 0, NULL);
		fsm_restart_timer(&l1->timer, TIMER3_VALUE, EV_TIMER3, NULL, 2);
		test_and_set_bit(FLG_L1_T3RUN, &l1->Flags);
	}
	else
		fsm_change_state(fi, ST_L1_F3);
}

static void l1_go_F5(struct FsmInst *fi, int event, void *arg)
{
	fsm_change_state(fi, ST_L1_F5);
}

static void l1_go_F8(struct FsmInst *fi, int event, void *arg)
{
	fsm_change_state(fi, ST_L1_F8);
}

static void l1_info2_ind(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

#ifdef AS_ISDN_UINTERFACE
	if (test_bit(FLG_L1_UINT, &l1->Flags))
		fsm_change_state(fi, ST_L1_SYNC2);
	else
#endif
		fsm_change_state(fi, ST_L1_F6);
	l1down(l1, PH_SIGNAL | REQUEST, INFO3_P8, 0, NULL);
}

static void l1_info4_ind(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

#ifdef AS_ISDN_UINTERFACE
	if (test_bit(FLG_L1_UINT, &l1->Flags))
		fsm_change_state(fi, ST_L1_TRANS);
	else
#endif
		fsm_change_state(fi, ST_L1_F7);

	l1down(l1, PH_SIGNAL | REQUEST, INFO3_P8, 0, NULL);
	if (test_and_clear_bit(FLG_L1_DEACTTIMER, &l1->Flags))
		fsm_del_timer(&l1->timer, 4);
	if (!test_bit(FLG_L1_ACTIVATED, &l1->Flags))
	{
		if (test_and_clear_bit(FLG_L1_T3RUN, &l1->Flags))
			fsm_del_timer(&l1->timer, 3);
		fsm_restart_timer(&l1->timer, 110, EV_TIMER_ACT, NULL, 2);
		test_and_set_bit(FLG_L1_ACTTIMER, &l1->Flags);
	}
}

static void l1_timer3(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;
	u_int db = HW_D_NOBLOCKED;

	test_and_clear_bit(FLG_L1_T3RUN, &l1->Flags);	
	if (test_and_clear_bit(FLG_L1_ACTIVATING, &l1->Flags))
	{
		if (test_and_clear_bit(FLG_L1_DBLOCKED, &l1->Flags))
			l1up(l1, PH_CONTROL | INDICATION, 0, 4, &db);
		l1up(l1, PH_DEACTIVATE | INDICATION, 0, 0, NULL);
	}
#ifdef AS_ISDN_UINTERFACE
	if (!test_bit(FLG_L1_UINT, &l1->Flags))
#endif
	if (l1->l1m.state != ST_L1_F6)
	{
		fsm_change_state(fi, ST_L1_F3);
		l1down(l1, PH_CONTROL | REQUEST, HW_POWERUP, 0, NULL);
	}
}

static void l1_timer_act(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	test_and_clear_bit(FLG_L1_ACTTIMER, &l1->Flags);
	test_and_set_bit(FLG_L1_ACTIVATED, &l1->Flags);
	if (test_and_clear_bit(FLG_L1_ACTIVATING, &l1->Flags))
		l1up(l1, PH_ACTIVATE | CONFIRM, 0, 0, NULL);
	else
		l1up(l1, PH_ACTIVATE | INDICATION, 0, 0, NULL);
}

static void l1_timer_deact(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;
	u_int db = HW_D_NOBLOCKED;

	test_and_clear_bit(FLG_L1_DEACTTIMER, &l1->Flags);
	test_and_clear_bit(FLG_L1_ACTIVATED, &l1->Flags);
	if (test_and_clear_bit(FLG_L1_DBLOCKED, &l1->Flags))
		l1up(l1, PH_CONTROL | INDICATION, 0, 4, &db);
	l1up(l1, PH_DEACTIVATE | INDICATION, 0, 0, NULL);
	l1down(l1, PH_CONTROL | REQUEST, HW_DEACTIVATE, 0, NULL);
}

static void l1_activate_s(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	l1down(l1, PH_CONTROL | REQUEST, HW_RESET, 0, NULL);
}

static void l1_activate_no(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;
	u_int db = HW_D_NOBLOCKED;

	if ((!test_bit(FLG_L1_DEACTTIMER, &l1->Flags)) && (!test_bit(FLG_L1_T3RUN, &l1->Flags)))
	{
		test_and_clear_bit(FLG_L1_ACTIVATING, &l1->Flags);
		if (test_and_clear_bit(FLG_L1_DBLOCKED, &l1->Flags))
			l1up(l1, PH_CONTROL | INDICATION, 0, 4, &db);
		l1up(l1, PH_DEACTIVATE | INDICATION, 0, 0, NULL);
	}
}

static struct FsmNode L1SFnList[] =
{
	{ST_L1_F3, EV_PH_ACTIVATE, l1_activate_s},
	{ST_L1_F6, EV_PH_ACTIVATE, l1_activate_no},
	{ST_L1_F8, EV_PH_ACTIVATE, l1_activate_no},
	{ST_L1_F3, EV_RESET_IND, l1_reset},
	{ST_L1_F4, EV_RESET_IND, l1_reset},
	{ST_L1_F5, EV_RESET_IND, l1_reset},
	{ST_L1_F6, EV_RESET_IND, l1_reset},
	{ST_L1_F7, EV_RESET_IND, l1_reset},
	{ST_L1_F8, EV_RESET_IND, l1_reset},
	{ST_L1_F3, EV_DEACT_CNF, l1_deact_cnf},
	{ST_L1_F4, EV_DEACT_CNF, l1_deact_cnf},
	{ST_L1_F5, EV_DEACT_CNF, l1_deact_cnf},
	{ST_L1_F6, EV_DEACT_CNF, l1_deact_cnf},
	{ST_L1_F7, EV_DEACT_CNF, l1_deact_cnf},
	{ST_L1_F8, EV_DEACT_CNF, l1_deact_cnf},
	{ST_L1_F6, EV_DEACT_IND, l1_deact_req_s},
	{ST_L1_F7, EV_DEACT_IND, l1_deact_req_s},
	{ST_L1_F8, EV_DEACT_IND, l1_deact_req_s},
	{ST_L1_F3, EV_POWER_UP,  l1_power_up_s},
	{ST_L1_F4, EV_ANYSIG_IND,l1_go_F5},
	{ST_L1_F6, EV_ANYSIG_IND,l1_go_F8},
	{ST_L1_F7, EV_ANYSIG_IND,l1_go_F8},
	{ST_L1_F3, EV_INFO2_IND, l1_info2_ind},
	{ST_L1_F4, EV_INFO2_IND, l1_info2_ind},
	{ST_L1_F5, EV_INFO2_IND, l1_info2_ind},
	{ST_L1_F7, EV_INFO2_IND, l1_info2_ind},
	{ST_L1_F8, EV_INFO2_IND, l1_info2_ind},
	{ST_L1_F3, EV_INFO4_IND, l1_info4_ind},
	{ST_L1_F4, EV_INFO4_IND, l1_info4_ind},
	{ST_L1_F5, EV_INFO4_IND, l1_info4_ind},
	{ST_L1_F6, EV_INFO4_IND, l1_info4_ind},
	{ST_L1_F8, EV_INFO4_IND, l1_info4_ind},
	{ST_L1_F3, EV_TIMER3, l1_timer3},
	{ST_L1_F4, EV_TIMER3, l1_timer3},
	{ST_L1_F5, EV_TIMER3, l1_timer3},
	{ST_L1_F6, EV_TIMER3, l1_timer3},
	{ST_L1_F8, EV_TIMER3, l1_timer3},
	{ST_L1_F7, EV_TIMER_ACT, l1_timer_act},
	{ST_L1_F3, EV_TIMER_DEACT, l1_timer_deact},
	{ST_L1_F4, EV_TIMER_DEACT, l1_timer_deact},
	{ST_L1_F5, EV_TIMER_DEACT, l1_timer_deact},
	{ST_L1_F6, EV_TIMER_DEACT, l1_timer_deact},
	{ST_L1_F7, EV_TIMER_DEACT, l1_timer_deact},
	{ST_L1_F8, EV_TIMER_DEACT, l1_timer_deact},
};

#define L1S_FN_COUNT (sizeof(L1SFnList)/sizeof(struct FsmNode))

#ifdef AS_ISDN_UINTERFACE
static void l1_deact_req_u(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	fsm_change_state(fi, ST_L1_RESET);
	fsm_restart_timer(&l1->timer, 550, EV_TIMER_DEACT, NULL, 2);
	test_and_set_bit(FLG_L1_DEACTTIMER, &l1->Flags);
	l1down(l1, PH_CONTROL | REQUEST, HW_POWERUP, 0, NULL);
}

static void l1_power_up_u(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	fsm_restart_timer(&l1->timer, TIMER3_VALUE, EV_TIMER3, NULL, 2);
	test_and_set_bit(FLG_L1_T3RUN, &l1->Flags);
}

static void l1_info0_ind(struct FsmInst *fi, int event, void *arg)
{
	fsm_change_state(fi, ST_L1_DEACT);
}

static void l1_activate_u(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	l1down(l1, PH_SIGNAL | REQUEST, INFO1, 0, NULL);
}

static struct FsmNode L1UFnList[] =
{
	{ST_L1_RESET, EV_DEACT_IND, l1_deact_req_u},
	{ST_L1_DEACT, EV_DEACT_IND, l1_deact_req_u},
	{ST_L1_SYNC2, EV_DEACT_IND, l1_deact_req_u},
	{ST_L1_TRANS, EV_DEACT_IND, l1_deact_req_u},
	{ST_L1_DEACT, EV_PH_ACTIVATE, l1_activate_u},
	{ST_L1_DEACT, EV_POWER_UP, l1_power_up_u},
	{ST_L1_DEACT, EV_INFO2_IND, l1_info2_ind},
	{ST_L1_TRANS, EV_INFO2_IND, l1_info2_ind},
	{ST_L1_RESET, EV_DEACT_CNF, l1_info0_ind},
	{ST_L1_DEACT, EV_INFO4_IND, l1_info4_ind},
	{ST_L1_SYNC2, EV_INFO4_IND, l1_info4_ind},
	{ST_L1_RESET, EV_INFO4_IND, l1_info4_ind},
	{ST_L1_DEACT, EV_TIMER3, l1_timer3},
	{ST_L1_SYNC2, EV_TIMER3, l1_timer3},
	{ST_L1_TRANS, EV_TIMER_ACT, l1_timer_act},
	{ST_L1_DEACT, EV_TIMER_DEACT, l1_timer_deact},
	{ST_L1_SYNC2, EV_TIMER_DEACT, l1_timer_deact},
	{ST_L1_RESET, EV_TIMER_DEACT, l1_timer_deact},
};

#define L1U_FN_COUNT (sizeof(L1UFnList)/sizeof(struct FsmNode))

#endif

/* B state machine */
static void l1b_activate(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	fsm_change_state(fi, ST_L1_WAIT_ACT);
	fsm_restart_timer(&l1->timer, l1->delay, EV_TIMER_ACT, NULL, 2);
}

static void l1b_deactivate(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	fsm_change_state(fi, ST_L1_WAIT_DEACT);
	fsm_restart_timer(&l1->timer, 10, EV_TIMER_DEACT, NULL, 2);
}

static void l1b_timer_act(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	fsm_change_state(fi, ST_L1_ACTIV);
	l1up(l1, PH_ACTIVATE | CONFIRM, 0, 0, NULL);
}

static void l1b_timer_deact(struct FsmInst *fi, int event, void *arg)
{
	layer1_t *l1 = fi->userdata;

	fsm_change_state(fi, ST_L1_NULL);
	l1up(l1, PH_DEACTIVATE | CONFIRM, 0, 0, NULL);
}

static struct FsmNode L1BFnList[] =
{
	{ST_L1_NULL, EV_PH_ACTIVATE, l1b_activate},
	{ST_L1_WAIT_ACT, EV_TIMER_ACT, l1b_timer_act},
	{ST_L1_ACTIV, EV_PH_DEACTIVATE, l1b_deactivate},
	{ST_L1_WAIT_DEACT, EV_TIMER_DEACT, l1b_timer_deact},
};

#define L1B_FN_COUNT (sizeof(L1BFnList)/sizeof(struct FsmNode))

/* Physical layer active SAP provide to layer2 :layer call it to unitilize the service of l1
so ,only PHY request is defined here */
static int l1from_up(as_isdn_if_t *hif, struct sk_buff *skb)
{
	layer1_t	*l1;
	as_isdn_head_t	*hh;
	int		err = 0;

//	TRACE();
	if (!hif || !hif->fdata || !skb)
		return(-EINVAL);
	l1 = hif->fdata;
	hh = AS_ISDN_HEAD_P(skb);

	switch(hh->prim)
	{
		case (PH_DATA | REQUEST):
		case (PH_CONTROL | REQUEST):
			if (l1->inst.down.func)
				return(l1->inst.down.func(&l1->inst.down,	skb));
			else
				err = -ENXIO;
			break;
		case (PH_ACTIVATE | REQUEST):
			if (test_bit(FLG_L1_ACTIVATED, &l1->Flags))
				l1up(l1, PH_ACTIVATE | CONFIRM, 0, 0, NULL);
			else
			{
				test_and_set_bit(FLG_L1_ACTIVATING, &l1->Flags);
				as_isdn_fsm_event(&l1->l1m, EV_PH_ACTIVATE, NULL);
			}
			break;
		case (MDL_FINDTEI | REQUEST):
			/* call DL manager entity to find TEI ??? */
			if (l1->inst.up.func)
				return(l1->inst.up.func(&l1->inst.up, skb));
			else
				err = -ENXIO;
			break;
		default:
		{
#if 0			
			if (l1->debug)
				AS_ISDN_debug(l1->inst.st->id, NULL, "l1from_up msg %x unhandled", hh->prim);
#endif			
			printk(KERN_WARNING "Layer 1(StackId=0x%x) : msg %x from_up unhandled\n", l1->inst.st->id, hh->prim);
			err = -EINVAL;
			break;
		}	
	}
	if (!err)
		dev_kfree_skb(skb);
	return(err);
}

/* function for down layer peer, L0 here. so only confirm/indication is handled by this function 
Data passed here by D Channel BottomHalf : skb is freed by DChannel BH */
static int l1from_down(as_isdn_if_t *hif,  struct sk_buff *skb)
{
	layer1_t	*l1;
	as_isdn_head_t	*hh;
	int		err = 0;

	if (!hif || !hif->fdata || !skb)
		return(-EINVAL);
	
//	TRACE();
	l1 = hif->fdata;
	hh = AS_ISDN_HEAD_P(skb);
	/* PH_DATA_* must be transfer to upper layers, so up->func provided by up-peer is called */
	if (hh->prim == PH_DATA_IND)
	{
		if (test_bit(FLG_L1_ACTTIMER, &l1->Flags))
			as_isdn_fsm_event(&l1->l1m, EV_TIMER_ACT, NULL);
		if (l1->inst.up.func)
		{
//			TRACE();
#if AS_ISDN_DEBUG_L1_DATA
			printk(KERN_ERR "instance %s up instance is %s\n", l1->inst.name, (l1->inst.up.owner)? l1->inst.up.peer->name:"NULL" );
#endif
			return(l1->inst.up.func(&l1->inst.up, skb));
		}
		else
		{
//			TRACE();
			err = -ENXIO;
		}
	}
	else if (hh->prim == PH_DATA_CNF)
	{
		if (l1->inst.up.func)
			return(l1->inst.up.func(&l1->inst.up, skb));
		else
			err = -ENXIO;
	}
	/* layer 1 play role of PHY_Control/SIGNAL_Manager, so no up_interface provided by up-peer is needed */
	else if (hh->prim == (PH_CONTROL | INDICATION))
	{
		if (hh->dinfo == HW_RESET)
			as_isdn_fsm_event(&l1->l1m, EV_RESET_IND, NULL);
		else if (hh->dinfo == HW_DEACTIVATE)
			as_isdn_fsm_event(&l1->l1m, EV_DEACT_IND, NULL);
		else if (hh->dinfo == HW_POWERUP)
			as_isdn_fsm_event(&l1->l1m, EV_POWER_UP, NULL);
		else
//		else if (l1->debug)
		{
//			AS_ISDN_debug(l1->inst.st->id, NULL, "l1from_down ctrl ind %x unhandled", hh->dinfo);
			printk(KERN_ERR "Layer1(StackId=0x%x) : IND %x from_down unhandled",l1->inst.st->id , hh->dinfo);
		}	
	}
	else if (hh->prim == (PH_CONTROL | CONFIRM))
	{
		if (hh->dinfo == HW_DEACTIVATE) 
			as_isdn_fsm_event(&l1->l1m, EV_DEACT_CNF, NULL);
		else
//		else if (l1->debug)
		{
//			AS_ISDN_debug(l1->inst.st->id, NULL, "l1from_down ctrl cnf %x unhandled", hh->dinfo);
			printk(KERN_ERR "Layer1(StackId=0x%x) : CNF %x from_down unhandled",l1->inst.st->id , hh->dinfo);
		}	
	}
	else if (hh->prim == (PH_SIGNAL | INDICATION))
	{/* no PH_SIGNAL_CONFIRM exist in specs */
		if (hh->dinfo == ANYSIGNAL)
			as_isdn_fsm_event(&l1->l1m, EV_ANYSIG_IND, NULL);
		else if (hh->dinfo == LOSTFRAMING)
			as_isdn_fsm_event(&l1->l1m, EV_ANYSIG_IND, NULL);
		else if (hh->dinfo == INFO2)
			as_isdn_fsm_event(&l1->l1m, EV_INFO2_IND, NULL);
		else if (hh->dinfo == INFO4_P8)
			as_isdn_fsm_event(&l1->l1m, EV_INFO4_IND, NULL);
		else if (hh->dinfo == INFO4_P10)
			as_isdn_fsm_event(&l1->l1m, EV_INFO4_IND, NULL);
		else
//		else if (l1->debug)
		{
//			AS_ISDN_debug(l1->inst.st->id, NULL, "l1from_down sig %x unhandled", hh->dinfo);
			printk(KERN_ERR "Layer1(StackId=0x%x) : SIGNAL %x from_down unhandled",l1->inst.st->id , hh->dinfo);
		}	
	}
	else
	{
//		if (l1->debug)
//			AS_ISDN_debug(l1->inst.st->id, NULL,	"l1from_down msg %x unhandled", hh->prim);
		printk(KERN_ERR "Layer1(StackId=0x%x) : msg %x from_down unhandled",l1->inst.st->id , hh->prim);
		err = -EINVAL;
	}
	
//	if (!err)	modify as following, lizhijie, 2005.09.19 
//		dev_kfree_skb(skb);
#if 0
	if(err)
	{
		dev_kfree_skb(skb);
		err = 0;
	}
#endif	
	return(err);
}

static void release_l1(layer1_t *l1)
{
	as_isdn_instance_t	*inst = &l1->inst;

	fsm_del_timer(&l1->timer, 0);
	/* ask peer dis-connect(release) our up and down interfaces */
	if (inst->up.peer)
	{
		inst->up.peer->obj->ctrl(inst->up.peer,MGR_DISCONNECT | REQUEST, &inst->up);
	}
	if (inst->down.peer)
	{
		inst->down.peer->obj->ctrl(inst->down.peer, MGR_DISCONNECT | REQUEST, &inst->down);
	}
	list_del(&l1->list);
	isdnl1.ctrl(inst, MGR_UNREGLAYER | REQUEST, NULL);
	
	kfree(l1);/* remove all data contains instance */
}

static int new_l1(as_isdn_stack_t *st, AS_ISDN_pid_t *pid)
{
	layer1_t *nl1;
	int err;

	if (!st || !pid)
		return(-EINVAL);
	if (!(nl1 = kmalloc(sizeof(layer1_t), GFP_ATOMIC)))
	{
		printk(KERN_ERR "ISDNL1-MGR : kmalloc layer1_t failed\n");
		return(-ENOMEM);
	}
	memset(nl1, 0, sizeof(layer1_t));
	
	memcpy(&nl1->inst.pid, pid, sizeof(AS_ISDN_pid_t));
	AS_ISDN_init_instance(&nl1->inst, &isdnl1, nl1);
	/* check whether this pid can be support by isdnl1 */
	if (!AS_ISDN_SetHandledPID(&isdnl1, &nl1->inst.pid))
	{
		int_error();
		return(-ENOPROTOOPT);
	}

	switch(pid->protocol[1])
	{
		case ISDN_PID_L1_TE_S0:
			sprintf(nl1->inst.name, "l1TES0-%d", st->id);
			nl1->l1m.fsm = &l1fsm_s;
			nl1->l1m.state = ST_L1_F3;
			nl1->Flags = 0;
			break;
			
		default:
			printk(KERN_ERR "ISDNL1-MGR : layer1 object create failed prt %x\n", pid->protocol[1]);
			kfree(nl1);
			return(-ENOPROTOOPT);
	}
	
	nl1->debug = asl1_debug;
#if AS_ISDN_DEBUG_FSM
	nl1->l1m.debug = asl1_debug;
#endif
	nl1->l1m.userdata = nl1;
	nl1->l1m.userint = 0;
#if AS_ISDN_DEBUG_FSM
	nl1->l1m.printdebug = l1m_debug;
#endif
	fsm_init_timer(&nl1->l1m, &nl1->timer);
	list_add_tail(&nl1->list, &isdnl1.ilist);
	
	err = isdnl1.ctrl(st, MGR_REGLAYER | INDICATION, &nl1->inst);
	if (err)
	{
		fsm_del_timer(&nl1->timer, 0);
		list_del(&nl1->list);
		kfree(nl1);
	}
	return(err);
}

static int l1_status(layer1_t *l1, status_info_l1_t *si)
{

	if (!si)
		return(-EINVAL);
	
	memset(si, 0, sizeof(status_info_l1_t));
	si->len = sizeof(status_info_l1_t) - 2*sizeof(int);
	si->typ = STATUS_INFO_L1;
	si->protocol = l1->inst.pid.protocol[1];
	if (test_bit(FLG_L1_ACTIVATED, &l1->Flags))
		si->status = 1;
	si->state = l1->l1m.state;
	si->Flags = l1->Flags;
	si->T3 = TIMER3_VALUE;
	si->delay = l1->delay;
	si->debug = l1->debug;
	return(0);
}


#ifdef MODULE
MODULE_AUTHOR("Li Zhijie <lizhijie@assistcn.com>");
MODULE_PARM(asl1_debug, "1i");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#define Isdnl1Init init_module
#endif

static int l1_manager(void *data, u_int prim, void *arg)
{
	as_isdn_instance_t	*inst = data;
	layer1_t	*l1l;
	int		err = -EINVAL;

	if (!data)
		return(err);
	
#if AS_ISDN_DEBUG_L1_INIT
	if (asl1_debug & 0x10000)
		printk(KERN_ERR  "ISDNL1-MGR : data:%p(inst:%s) prim:(%x) arg:(%p)--%s\n", data, inst->name, prim, arg, inst->name);
#endif
	
	list_for_each_entry(l1l, &isdnl1.ilist, list)
	{/* check the instance param refer to myself : layer 1 */
		if (&l1l->inst == inst)
		{
			err = 0;
			break;
		}
	}
	if (err && (prim != (MGR_NEWLAYER | REQUEST)))
	{
		printk(KERN_WARNING "ISDNL1-MGR : connect no instance\n");
		return(err);
	}
	
	switch(prim)
	{
		case MGR_NEWLAYER | REQUEST:
			/* only in this case, instance can be null,eg param 'inst' is not myself 
			* this is first step in using layer 1 after driver loaded	*/
			err = new_l1(data, arg);
			break;
		case MGR_CONNECT | REQUEST:
			err = AS_ISDN_ConnectIF(inst, arg);
			break;

		case MGR_SETIF | REQUEST:	/* setif command from upper layer */
		case MGR_SETIF | INDICATION:	/* setif command from down layer */
			err = AS_ISDN_SetIF(inst, arg, prim, l1from_up, l1from_down, l1l);
			break;

		case MGR_DISCONNECT | REQUEST:
		case MGR_DISCONNECT | INDICATION:
			err = AS_ISDN_DisConnectIF(inst, arg);
			break;

		case MGR_UNREGLAYER | REQUEST:
		case MGR_RELEASE | INDICATION:

#if AS_ISDN_DEBUG_L1_CTRL
			printk(KERN_ERR  "ISDNL1-MGR : release_l1 id %x\n", l1l->inst.st->id);
#endif
			release_l1(l1l);
			break;

		case MGR_STATUS | REQUEST:
			err = l1_status(l1l, arg);
			break;

		PRIM_NOT_HANDLED(MGR_CTRLREADY|INDICATION);
		PRIM_NOT_HANDLED(MGR_ADDSTPARA|INDICATION);
		default:
			printk(KERN_WARNING "ISDNL1-MGR : prim %x not handled\n", prim);
			err = -EINVAL;
			break;
	}
	return(err);
}

int Isdnl1Init(void)
{
	int err;

	printk(KERN_ERR  AS_VERSION_INFO("L1 Driver") );
#ifdef MODULE
	isdnl1.owner = THIS_MODULE;
#endif
	isdnl1.name = "ISDNL1";
	/* only for D channel T/S interface, Layer1 TE mode protocol */
	isdnl1.DPROTO.protocol[1] = ISDN_PID_L1_TE_S0;
	isdnl1.own_ctrl = l1_manager;
	INIT_LIST_HEAD(&isdnl1.ilist);
	
#ifdef AS_ISDN_UINTERFACE
	isdnl1.DPROTO.protocol[1] |= ISDN_PID_L1_TE_U;
	l1fsm_u.state_count = L1U_STATE_COUNT;
	l1fsm_u.event_count = L1_EVENT_COUNT;
	l1fsm_u.strEvent = strL1Event;
	l1fsm_u.strState = strL1UState;
	as_isdn_fsm_new(&l1fsm_u, L1UFnList, L1U_FN_COUNT);
#endif

	/* S/T state machine */
	l1fsm_s.state_count = L1S_STATE_COUNT;
	l1fsm_s.event_count = L1_EVENT_COUNT;
	l1fsm_s.strEvent = strL1Event;
	l1fsm_s.strState = strL1SState;
	as_isdn_fsm_new(&l1fsm_s, L1SFnList, L1S_FN_COUNT);

	/* B?? B Channel */
	l1fsm_b.state_count = L1B_STATE_COUNT;
	l1fsm_b.event_count = L1_EVENT_COUNT;
	l1fsm_b.strEvent = strL1Event;
	l1fsm_b.strState = strL1BState;
	as_isdn_fsm_new(&l1fsm_b, L1BFnList, L1B_FN_COUNT);
	
	if ((err = as_isdn_register(&isdnl1)))
	{
		printk(KERN_ERR "Can't register %s error(%d)\n", isdnl1.name, err);
#ifdef AS_ISDN_UINTERFACE
		as_isdn_fsm_free(&l1fsm_u);
#endif
		as_isdn_fsm_free(&l1fsm_s);
		as_isdn_fsm_free(&l1fsm_b);
	}
	return(err);
}

#ifdef MODULE
void cleanup_module(void)
{
	int 		err;
	layer1_t	*l1, *nl1;

	if ((err = as_isdn_unregister(&isdnl1)))
	{
		printk(KERN_ERR "Can't unregister ISDN layer 1 %s error(%d)\n", isdnl1.name, err);
	}
	if(!list_empty(&isdnl1.ilist))
	{
		printk(KERN_WARNING "AS_ISDNl1 inst list not empty\n");
		list_for_each_entry_safe(l1, nl1, &isdnl1.ilist, list)
			release_l1(l1);
	}
#ifdef AS_ISDN_UINTERFACE
	as_isdn_fsm_free(&l1fsm_u);
#endif
	as_isdn_fsm_free(&l1fsm_s);
	as_isdn_fsm_free(&l1fsm_b);
	
	printk(KERN_INFO "Release " AS_VERSION_INFO("L1 Driver") );
}
#endif
