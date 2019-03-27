/*
 * $Id: l3_fsm.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * DL Link Control is process by this file and L3_LC_statemachine
 * DL Link Control is associated with a L3 management entity
 */
 
#include "l3.h"
#include "helper.h"


static struct Fsm l3fsm = {NULL, 0, 0, NULL, NULL};

/* L3 Link Control state */
enum
{
	ST_L3_LC_REL,
	ST_L3_LC_ESTAB_WAIT,
	ST_L3_LC_REL_DELAY, 
	ST_L3_LC_REL_WAIT,
	ST_L3_LC_ESTAB,
};

#define L3_STATE_COUNT (ST_L3_LC_ESTAB+1)

static char *strL3State[] =
{
	"ST_L3_LC_REL",
	"ST_L3_LC_ESTAB_WAIT",
	"ST_L3_LC_REL_DELAY",
	"ST_L3_LC_REL_WAIT",
	"ST_L3_LC_ESTAB",
};

enum
{
	EV_ESTABLISH_REQ,		/* rx DL_DATA|REQ when no DL_LC avaible or rx DL_ESTABLISH|REQ */
	EV_ESTABLISH_IND,		/* rx DL_ESTABLISH | INDICATION */
	EV_ESTABLISH_CNF,		/* rx DL_ESTABLISH | CONFIRM */
	EV_RELEASE_REQ,
	EV_RELEASE_CNF,
	EV_RELEASE_IND,
	EV_TIMEOUT,
};

#define L3_EVENT_COUNT (EV_TIMEOUT+1)

static char *strL3Event[] =
{
	"EV_ESTABLISH_REQ",
	"EV_ESTABLISH_IND",
	"EV_ESTABLISH_CNF",
	"EV_RELEASE_REQ",
	"EV_RELEASE_CNF",
	"EV_RELEASE_IND",
	"EV_TIMEOUT",
};


#define DREL_TIMER_VALUE 40000

#if 0
static void l3m_debug(struct FsmInst *fi, char *fmt, ...)
{
	layer3_t *l3 = fi->userdata;
	logdata_t log;

	va_start(log.args, fmt);
	log.fmt = fmt;
	log.head = l3->inst.name;
	l3->inst.obj->ctrl(&l3->inst, MGR_DEBUGDATA | REQUEST, &log);
	va_end(log.args);
}

static inline int my_l3_if_link(as_isdn_if_t *i, u_int prim, int dinfo, int len, void *arg, int reserve)
{
	struct sk_buff	*skb;
	int		err =0;

//		TRACE();
	if (!(skb = create_link_skb(prim, dinfo, len, arg, reserve)))

		return(-ENOMEM);
	if (!i)
	{
		err = -ENXIO;
	}	
	else
	{
//		TRACE();
		if (i->func  && skb)
		{
			err = i->func(i, skb);
		}
#if AS_ISDN_DEBUG_L3_DATA
		else
			TRACE();
		TRACE();
#endif		
	}	
	if (err)
	{
//		TRACE();
		kfree_skb(skb);
	}	
//	TRACE();
	return(err);
}

static inline int my_l3_if_newhead(as_isdn_if_t *i, u_int prim, int dinfo, struct sk_buff *skb)
{
//	TRACE();
	if (!i->func || !skb)
	{
//		TRACE();
		return(-ENXIO);
	}
	AS_ISDN_sethead(prim, dinfo, skb);
	
#if AS_ISDN_DEBUG_L3_DATA
	printk(KERN_ERR "kernel TRACE at %s->%s:%d! called %s --%s\n", __FILE__, __FUNCTION__, __LINE__,i->owner->name,  i->peer->name);
#endif

	return(i->func(i, skb));
}
#endif

/* down interface function */
static int l3down(layer3_t *l3, u_int prim, int dinfo, struct sk_buff *skb)
{
	int err = -EINVAL;

#if AS_ISDN_DEBUG_L3_DATA
	if( (l3==0) )
	{
		printk(KERN_ERR "kernel TRACE at %s->%s:%d!\n", __FILE__, __FUNCTION__, __LINE__);
		return -ENXIO;
	}

	if( (l3->inst.down.func == 0) )
	{
		printk(KERN_ERR "Function in %s is null\n", l3->inst.name );
		return -ENXIO;
	}	

	printk(KERN_ERR "kernel TRACE at %s->%s:%d : function of instance %s ,peer is %s\n", __FILE__, __FUNCTION__, __LINE__, l3->inst.down.owner->name , l3->inst.down.peer->name);
#endif

	if (!skb)
	{
		err = if_link(&l3->inst.down, prim, dinfo, 0, NULL, 0);
	}	
	else
	{
		err = if_newhead(&l3->inst.down, prim, dinfo, skb);
	}
	return(err);
}

static int l3_newid(layer3_t *l3)
{
	int	id;

	id = l3->next_id++;
	if (id == 0x7fff)
		l3->next_id = 1;
	id |= (l3->entity << 16);
	return(id);
}


/* Call L3 manager's L3 process handler for DL_LC message used by L3 */
static void l3ml3p(layer3_t *l3, int pr)
{
	l3_process_t *p, *np;

	list_for_each_entry_safe(p, np, &l3->plist, list) 
		l3->p_mgr(p, pr, NULL);	/* point to l3_process_mgr */

	switch(pr)
	{
		case DL_ESTABLISH | INDICATION:
		case DL_ESTABLISH | CONFIRM:
			if_link(&l3->inst.up, DL_STATUS | INDICATION, SDL_ESTAB, 0, NULL, 0);
			break;
			
		case DL_RELEASE | INDICATION:
		case DL_RELEASE | CONFIRM:
			if_link(&l3->inst.up, DL_STATUS | INDICATION, SDL_REL, 0, NULL, 0);
			break;
	}
}

/****************** DL Link Connection handlers *****************/
/* activate the DL_LC : send out DL_ESTAB|REQUEST */
static void lc_activate(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	fsm_change_state(fi, ST_L3_LC_ESTAB_WAIT);
	l3down(l3, DL_ESTABLISH | REQUEST, 0, NULL);
}

static void lc_connect(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;
	struct sk_buff *skb;
	int dequeued = 0;

	fsm_change_state(fi, ST_L3_LC_ESTAB);
	while ((skb = skb_dequeue(&l3->squeue)))
	{
		if (l3down(l3, DL_DATA | REQUEST, l3_newid(l3), skb))
			dev_kfree_skb(skb);
		dequeued++;
	}

	if (list_empty(&l3->plist) &&  dequeued)
	{
	
#if AS_ISDN_DEBUG_L3_CTRL
		if (l3->debug)
		{
//			l3m_debug(fi, "lc_connect: release link");
			printk(KERN_DEBUG "lc_connect: release link\n");
		}
#endif		
		as_isdn_fsm_event(&l3->l3m, EV_RELEASE_REQ, NULL);
	}
	else
		l3ml3p(l3, DL_ESTABLISH | INDICATION);
}

/* rx DL_ESTABLISH | INDICATION when in state of ESTAB_WAIT */
static void lc_connected(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;
	struct sk_buff *skb;
	int dequeued = 0;

	fsm_del_timer(&l3->l3m_timer, 51);
	fsm_change_state(fi, ST_L3_LC_ESTAB);
	while ((skb = skb_dequeue(&l3->squeue)))
	{
		if (l3down(l3, DL_DATA | REQUEST, l3_newid(l3), skb))
			dev_kfree_skb(skb);
		dequeued++;
	}

	if (list_empty(&l3->plist) &&  dequeued)
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (l3->debug)
		{
//			l3m_debug(fi, "lc_connected: release link");
			printk(KERN_DEBUG "lc_connected: release link\n");
		}
#endif		
		as_isdn_fsm_event(&l3->l3m, EV_RELEASE_REQ, NULL);
	}
	else
		l3ml3p(l3, DL_ESTABLISH | CONFIRM);
}

static void lc_start_delay(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	fsm_change_state(fi, ST_L3_LC_REL_DELAY);
	fsm_add_timer(&l3->l3m_timer, DREL_TIMER_VALUE, EV_TIMEOUT, NULL, 50);
}

static void lc_release_req(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	if (test_bit(FLG_L2BLOCK, &l3->Flag))
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (l3->debug)
		{
//			l3m_debug(fi, "lc_release_req: l2 blocked");
			printk(KERN_DEBUG "lc_release_req: l2 blocked\n");
		}	
#endif		
		/* restart release timer */
		fsm_add_timer(&l3->l3m_timer, DREL_TIMER_VALUE, EV_TIMEOUT, NULL, 51);
	}
	else
	{
		fsm_change_state(fi, ST_L3_LC_REL_WAIT);
		l3down(l3, DL_RELEASE | REQUEST, 0, NULL);
	}
}

static void lc_release_ind(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	fsm_del_timer(&l3->l3m_timer, 52);
	fsm_change_state(fi, ST_L3_LC_REL);
	discard_queue(&l3->squeue);
	l3ml3p(l3, DL_RELEASE | INDICATION);
}

/* rx RELEASE_CONFORM from peer, this is the last step in DL_LC */
static void lc_release_cnf(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	fsm_change_state(fi, ST_L3_LC_REL);
	discard_queue(&l3->squeue);
	l3ml3p(l3, DL_RELEASE | CONFIRM);
}


/* *INDENT-OFF* */
static struct FsmNode L3FnList[] =
{
	{ST_L3_LC_REL,			EV_ESTABLISH_REQ,	lc_activate},
	{ST_L3_LC_REL,			EV_ESTABLISH_IND,	lc_connect},
	{ST_L3_LC_REL,			EV_ESTABLISH_CNF,	lc_connect},
	{ST_L3_LC_ESTAB_WAIT,	EV_ESTABLISH_CNF,	lc_connected},
	{ST_L3_LC_ESTAB_WAIT,	EV_RELEASE_REQ,		lc_start_delay},
	{ST_L3_LC_ESTAB_WAIT,	EV_RELEASE_IND,		lc_release_ind},
	{ST_L3_LC_ESTAB,		EV_RELEASE_IND,		lc_release_ind},
	{ST_L3_LC_ESTAB,		EV_RELEASE_REQ,		lc_start_delay},
	{ST_L3_LC_REL_DELAY, 	EV_RELEASE_IND,		lc_release_ind},
	{ST_L3_LC_REL_DELAY,	EV_ESTABLISH_REQ,	lc_connected},
	{ST_L3_LC_REL_DELAY,	EV_TIMEOUT,			lc_release_req},
	{ST_L3_LC_REL_WAIT,		EV_RELEASE_CNF,		lc_release_cnf},
	{ST_L3_LC_REL_WAIT,		EV_ESTABLISH_REQ,	lc_activate},
};
/* *INDENT-ON* */

#define L3_FN_COUNT (sizeof(L3FnList)/sizeof(struct FsmNode))

void AS_ISDNl3New(void)
{
	l3fsm.state_count = L3_STATE_COUNT;
	l3fsm.event_count = L3_EVENT_COUNT;
	l3fsm.strEvent = strL3Event;
	l3fsm.strState = strL3State;
	as_isdn_fsm_new(&l3fsm, L3FnList, L3_FN_COUNT);
}

void AS_ISDNl3Free(void)
{
	as_isdn_fsm_free(&l3fsm);
}

void init_l3(layer3_t *l3)
{
	INIT_LIST_HEAD(&l3->plist);
	l3->global = NULL;
	l3->dummy = NULL;
	l3->entity = AS_ISDN_ENTITY_NONE;
	l3->next_id = 1;
	spin_lock_init(&l3->lock);
	skb_queue_head_init(&l3->squeue);
	
	l3->l3m.fsm = &l3fsm;
	l3->l3m.state = ST_L3_LC_REL;
#if AS_ISDN_DEBUG_FSM
	l3->l3m.debug = l3->debug;
#endif
	l3->l3m.userdata = l3;
	l3->l3m.userint = 0;
#if AS_ISDN_DEBUG_FSM
	l3->l3m.printdebug = NULL; //l3m_debug;
#endif	
	fsm_init_timer(&l3->l3m, &l3->l3m_timer);
}

void release_l3(layer3_t *l3)
{
	l3_process_t *p, *np;

#if AS_ISDN_DEBUG_L3_INIT
	if (l3->l3m.debug)
	{
		printk(KERN_ERR  "release_l3(%p) plist(%s) global(%p) dummy(%p)\n", l3, list_empty(&l3->plist) ? "no" : "yes", l3->global, l3->dummy);
	}	
#endif

	list_for_each_entry_safe(p, np, &l3->plist, list)
		release_l3_process(p);

	if (l3->global)
	{
		StopAllL3Timer(l3->global);
		kfree(l3->global);
		l3->global = NULL;
	}

	if (l3->dummy)
	{
		StopAllL3Timer(l3->dummy);
		kfree(l3->dummy);
		l3->dummy = NULL;
	}
	
	fsm_del_timer(&l3->l3m_timer, 54);
	discard_queue(&l3->squeue);
}


void release_l3_process(l3_process_t *p)
{
	layer3_t *l3;

	if (!p)
		return;
	l3 = p->l3;
//TRACE();	
	AS_ISDN_l3up(p, CC_RELEASE_CR | INDICATION, NULL);
//TRACE();	
	list_del(&p->list);
//TRACE();	
	StopAllL3Timer(p);
//TRACE();	
	kfree(p);
//TRACE();	
	if (list_empty(&l3->plist) && !test_bit(FLG_PTP, &l3->Flag))
	{
#if AS_ISDN_DEBUG_L3_INIT
		if (l3->debug)
		{
//			l3_debug(l3, "release_l3_process: last process");
			printk(KERN_DEBUG "release_l3_process: last process\n");
		}	
#endif		
//TRACE();	
		if (!skb_queue_len(&l3->squeue))
		{
//TRACE();
#if AS_ISDN_DEBUG_L3_INIT
			if (l3->debug)
			{
//				l3_debug(l3, "release_l3_process: release link");
				printk(KERN_DEBUG "release_l3_process: release link\n");
			}
#endif			
			as_isdn_fsm_event(&l3->l3m, EV_RELEASE_REQ, NULL);
		}
#if AS_ISDN_DEBUG_L3_INIT
		else
		{
//TRACE();	
			if (l3->debug)
			{
//				l3_debug(l3, "release_l3_process: not release link");
				printk(KERN_WARNING "release_l3_process: not release link\n");
			}	
		}
#endif		
	}
};



/* l3 msg handler : mainly handle l2(DL) primitive needed by L3, down interface for L3 
* This is handler for the purpose of DL_LC by L3 layer management entity when no DL_LC usable
*/
int l3_msg(layer3_t *l3, u_int pr, int dinfo, int len, void *arg)
{
//TRACE();	
	switch (pr)
	{
		case (DL_DATA | REQUEST):
//TRACE();	
#if 1
			if ( l3->l3m.state == ST_L3_LC_ESTAB)
			{/* DL_LC availble, so send data directly */
//TRACE();	
				return(l3down(l3, pr, l3_newid(l3), arg));
			}
			else
#endif				
			{/* else, keep L3msg in layer3_t->sendQueue and stimulate stateMachine with event ESTABLISH_REQ */
				struct sk_buff *skb = arg;
//TRACE();	
 //				printk(KERN_ERR  "%s: queue skb %p len(%d)\n", __FUNCTION__, skb, skb->len);
				skb_queue_tail(&l3->squeue, skb);
//TRACE();	
				as_isdn_fsm_event(&l3->l3m, EV_ESTABLISH_REQ, NULL); 
			}

//TRACE();	
			break;
			
		case (DL_STATUS | REQUEST):
			/* bounce DL_STATUS from REQ to Confirm */
//TRACE();	
			if_link(&l3->inst.up, DL_STATUS | CONFIRM, (l3->l3m.state==ST_L3_LC_ESTAB)?SDL_ESTAB:SDL_REL, 0, NULL, 0);
			break;
			
		case (DL_ESTABLISH | REQUEST):
			/* stimulate stateMachine with event ESTABLISH_REQ */
//TRACE();	
			as_isdn_fsm_event(&l3->l3m, EV_ESTABLISH_REQ, NULL);
			break;
			
		case (DL_ESTABLISH | CONFIRM):
//TRACE();	
			as_isdn_fsm_event(&l3->l3m, EV_ESTABLISH_CNF, NULL);
			break;
			
		case (DL_ESTABLISH | INDICATION):
//TRACE();	
			as_isdn_fsm_event(&l3->l3m, EV_ESTABLISH_IND, NULL);
			break;
			
		case (DL_RELEASE | INDICATION):
//TRACE();	
			as_isdn_fsm_event(&l3->l3m, EV_RELEASE_IND, NULL);
			break;
			
		case (DL_RELEASE | CONFIRM):
//TRACE();	
			as_isdn_fsm_event(&l3->l3m, EV_RELEASE_CNF, NULL);
			break;
			
		case (DL_RELEASE | REQUEST):
//TRACE();	
			as_isdn_fsm_event(&l3->l3m, EV_RELEASE_REQ, NULL);
			break;
	}
//TRACE();	
	return(0);
}

