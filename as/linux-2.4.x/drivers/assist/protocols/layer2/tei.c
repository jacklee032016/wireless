/*
 * $Id: tei.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */
#include "layer2.h"
#include "helper.h"
#if 0
#include "debug.h"
#endif
#include <linux/random.h>

/* TEI management procedures, refer to specs vol.1 page 201-213
*/
#if 0
const char *tei_revision = "$Revision: 1.1.1.1 $";
#endif

/* TEI message type, refer specs Vol.1 P.213 table 5.2
* TEI message used when no multiple-frame mode avaiable, so it is transfer in MDL_UNIT DATA
* refer to page 211 
*/
#define ID_REQUEST	1
#define ID_ASSIGNED	2
#define ID_DENIED	3
#define ID_CHK_REQ	4
#define ID_CHK_RES	5
#define ID_REMOVE	6
#define ID_VERIFY	7

/* Management Entity Identifier, refer to specs vol.1 P.212 */
#define TEI_ENTITY_ID	0xf

static struct Fsm teifsm = {NULL, 0, 0, NULL, NULL};

enum
{
	ST_TEI_NOP,		/* normal work state */
	ST_TEI_IDREQ,
	ST_TEI_IDVERIFY,
};

#define TEI_STATE_COUNT (ST_TEI_IDVERIFY+1)

static char *strTeiState[] =
{
	"ST_TEI_NOP",
	"ST_TEI_IDREQ",
	"ST_TEI_IDVERIFY",
};

enum
{
	EV_IDREQ,
	EV_ASSIGN,
	EV_ASSIGN_REQ,
	EV_DENIED,
	EV_CHKREQ,
	EV_REMOVE,
	EV_VERIFY,
	EV_T202,
};

#define TEI_EVENT_COUNT (EV_T202+1)

static char *strTeiEvent[] =
{
	"EV_IDREQ",
	"EV_ASSIGN",
	"EV_ASSIGN_REQ",
	"EV_DENIED",
	"EV_CHKREQ",
	"EV_REMOVE",
	"EV_VERIFY",
	"EV_T202",
};

unsigned int random_ri(void)
{
	unsigned int x;

	get_random_bytes(&x, sizeof(x));
	return (x & 0xffff);
}

/* manager send out a MDL_FIND_TEI_REQ. In this case, boradcast TEI can not be used */
static teimgr_t *findtei(teimgr_t *tm, int tei)
{
	static teimgr_t *ptr = NULL;
	struct sk_buff *skb;

	if (tei == 127)
		return (NULL);
	skb = create_link_skb(MDL_FINDTEI | REQUEST, tei, sizeof(void), &ptr, 0);
	if (!skb)
		return (NULL);
	if (!tei_l2(tm->l2, skb))
		return(ptr);
	dev_kfree_skb(skb);
	return (NULL);
}

/* TEI manager use this creating TEI msg|request and send out in layer2
* All TEI msg are hold in MDL_UNITDATA when multiple-frame mode(connection) is not established
*/
static void put_tei_msg(teimgr_t *tm, u_char m_id, unsigned int ri, u_char tei)
{
	struct sk_buff *skb;
	u_char bp[8];

	bp[0] = (TEI_SAPI << 2);
	if (test_bit(FLG_LAPD_NET, &tm->l2->flag))
		bp[0] |= 2; /* CR:=1 for net command */
	bp[1] = (GROUP_TEI << 1) | 0x1;	/* 0x1 is EA bit, page 178 */
	bp[2] = UI;				/* UI frame before this byte */
	/*refer to page 212, figure 5.4 */
	bp[3] = TEI_ENTITY_ID;	/* layer management entity identity */
	/*RI : Reference Number, 2 bytes */
	bp[4] = ri >> 8;
	bp[5] = ri & 0xff;
	bp[6] = m_id;			/* message type */
	bp[7] = (tei << 1) | 1;		/* AI: action idicator , bit 0: this is final byte */
	skb = create_link_skb(MDL_UNITDATA | REQUEST, 0, 8, bp, 0);
	if (!skb)
	{
		printk(KERN_WARNING "AS_ISDN: No skb for TEI manager\n");
		return;
	}
	if (tei_l2(tm->l2, skb))
		dev_kfree_skb(skb);
}

/* TEI manager send out a ID_REQUEST with TRI is broadcast TEI and start timer for this request 
* TEI ID_REQUEST is send by user side: U-->N
*/
static void tei_id_request(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;

	if (tm->l2->tei != -1) 
	{
//		tm->tei_m.printdebug(&tm->tei_m,"assign request for allready assigned tei %d",	tm->l2->tei);
		printk(KERN_WARNING "Assign request for allready assigned tei %d\n", tm->l2->tei);
		return;
	}
	tm->ri = random_ri();
	
#if AS_ISDN_DEBUG_L2_TEI
	if (tm->debug)
	{
//		tm->tei_m.printdebug(&tm->tei_m, "assign request ri %d", tm->ri);
		printk(KERN_DEBUG "assign request ri %d\n", tm->ri);
	}	
#endif
	
	put_tei_msg(tm, ID_REQUEST, tm->ri, 127);
	fsm_change_state(fi, ST_TEI_IDREQ);
	fsm_add_timer(&tm->t202, tm->T202, EV_T202, NULL, 1);
	tm->N202 = 3;
}

/* ID_ASSIGNED, N-->U: response for ID_REQUEST, refer page 205. figure 5.1 */
static void tei_assign_req(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;
	u_char *dp = arg;

	if (tm->l2->tei == -1)
	{/* assifned TEI value which send in Ai field in TEI msg, must be in 64-126*/
//		tm->tei_m.printdebug(&tm->tei_m, "net tei assign request without tei");
		printk(KERN_WARNING  "net tei assign request without tei\n");
		return;
	}
	tm->ri = ((unsigned int) *dp++ << 8);
	tm->ri += *dp++;

#if AS_ISDN_DEBUG_L2_TEI
	if (tm->debug)
	{
//		tm->tei_m.printdebug(&tm->tei_m, "net assign request ri %d teim %d", tm->ri, *dp);
		printk(KERN_DEBUG"net assign request ri %d teim %d\n", tm->ri, *dp);
	}
#endif

	/* is the assigned TEI value is not variable, eg. 88 from Network side ????*/
	put_tei_msg(tm, ID_ASSIGNED, tm->ri, tm->l2->tei);
	fsm_change_state(fi, ST_TEI_NOP);
}

static void tei_id_assign(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *otm, *tm = fi->userdata;
	struct sk_buff *skb;
	u_char *dp = arg;
	int ri, tei;

	ri = ((unsigned int) *dp++ << 8);
	ri += *dp++;
	dp++;
	tei = *dp >> 1;

#if AS_ISDN_DEBUG_L2_TEI
	if (tm->debug)
	{
//		tm->tei_m.printdebug(fi, "identity assign ri %d tei %d", ri, tei);
		printk(KERN_DEBUG "identity assign ri %d tei %d\n", ri, tei);
	}	
#endif

	if ((otm = findtei(tm, tei)))
	{	
		if (ri != otm->ri)
		{/* same tei is in use */

#if 1//AS_ISDN_DEBUG_L2_TEI
//			tm->tei_m.printdebug(fi, "possible duplicate assignment tei %d", tei);
			printk(KERN_WARNING "possible duplicate assignment tei %d", tei);
#endif
			skb = create_link_skb(MDL_ERROR | RESPONSE, 0, 0, NULL, 0);
			if (!skb)
				return;
			if (tei_l2(otm->l2, skb))
				dev_kfree_skb(skb);
		}
	}
	else if (ri == tm->ri)
	{
		fsm_del_timer(&tm->t202, 1);
		fsm_change_state(fi, ST_TEI_NOP);
		skb = create_link_skb(MDL_ASSIGN | REQUEST, tei, 0, NULL, 0);
		if (!skb)
			return;
		if (tei_l2(tm->l2, skb))
			dev_kfree_skb(skb);
//		cs->cardmsg(cs, MDL_ASSIGN | REQUEST, NULL);
	}
}

/* network side, use it in check procedure to check dup */
static void tei_id_test_dup(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *otm, *tm = fi->userdata;
	u_char *dp = arg;
	int tei, ri;

	ri = ((unsigned int) *dp++ << 8);
	ri += *dp++;
	dp++;
	tei = *dp >> 1;
	
#if AS_ISDN_DEBUG_L2_TEI
	if (tm->debug)
	{
//		tm->tei_m.printdebug(fi, "foreign identity assign ri %d tei %d", ri, tei);
		printk(KERN_DEBUG "foreign identity assign ri %d tei %d\n", ri, tei);
	}	
#endif
	if ((otm = findtei(tm, tei)))
	{	/* same tei is in use */
		if (ri != otm->ri)
		{	/* and it wasn't our request */
//			tm->tei_m.printdebug(fi, "possible duplicate assignment tei %d", tei);
			printk(KERN_WARNING"possible duplicate assignment tei %d\n", tei);
			as_isdn_fsm_event(&otm->tei_m, EV_VERIFY, NULL);
		}
	} 
}

static void tei_id_denied(struct FsmInst *fi, int event, void *arg)
{
#if AS_ISDN_DEBUG_L2_TEI
	teimgr_t *tm = fi->userdata;
#endif
	u_char *dp = arg;
	int ri, tei;

	ri = ((unsigned int) *dp++ << 8);
	ri += *dp++;
	dp++;
	tei = *dp >> 1;
#if AS_ISDN_DEBUG_L2_TEI
	if (tm->debug)
	{
//		tm->tei_m.printdebug(fi, "identity denied ri %d tei %d", ri, tei);
		printk(KERN_DEBUG "identity denied ri %d tei %d\n", ri, tei);
	}	
#endif
}

static void tei_id_chk_req(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;
	u_char *dp = arg;
	int tei;

	tei = *(dp+3) >> 1;
#if AS_ISDN_DEBUG_L2_TEI
	if (tm->debug)
	{
//		tm->tei_m.printdebug(fi, "identity check req tei %d", tei);
		printk(KERN_DEBUG "identity check req tei %d\n", tei);
	}	
#endif
	if ((tm->l2->tei != -1) && ((tei == GROUP_TEI) || (tei == tm->l2->tei)))
	{
		fsm_del_timer(&tm->t202, 4);
		fsm_change_state(&tm->tei_m, ST_TEI_NOP);
		/* ID check response : U-->N */
		put_tei_msg(tm, ID_CHK_RES, random_ri(), tm->l2->tei);
	}
}

static void tei_id_remove(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;
	u_char *dp = arg;
	struct sk_buff *skb;
	int tei;

	tei = *(dp+3) >> 1;
#if AS_ISDN_DEBUG_L2_TEI
	if (tm->debug)
	{
//		tm->tei_m.printdebug(fi, "identity remove tei %d", tei);
		printk(KERN_DEBUG  "identity remove tei %d\n", tei);
	}	
#endif
	if ((tm->l2->tei != -1) && ((tei == GROUP_TEI) || (tei == tm->l2->tei)))
	{
		fsm_del_timer(&tm->t202, 5);
		fsm_change_state(&tm->tei_m, ST_TEI_NOP);
		skb = create_link_skb(MDL_REMOVE | REQUEST, 0, 0, NULL, 0);
		if (!skb)
			return;
		if (tei_l2(tm->l2, skb))
			dev_kfree_skb(skb);
//		cs->cardmsg(cs, MDL_REMOVE | REQUEST, NULL);
	}
}

static void tei_id_verify(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;

#if AS_ISDN_DEBUG_L2_TEI
	if (tm->debug)
	{
//		tm->tei_m.printdebug(fi, "id verify request for tei %d", tm->l2->tei);
		printk(KERN_DEBUG "id verify request for tei %d\n", tm->l2->tei);
	}	
#endif	
	put_tei_msg(tm, ID_VERIFY, 0, tm->l2->tei);
	fsm_change_state(&tm->tei_m, ST_TEI_IDVERIFY);
	fsm_add_timer(&tm->t202, tm->T202, EV_T202, NULL, 2);
	tm->N202 = 2;
}

static void tei_id_request_timeout(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;
	struct sk_buff *skb;

	if (--tm->N202)
	{
		tm->ri = random_ri();
#if AS_ISDN_DEBUG_L2_TEI
		if (tm->debug)
		{
//			tm->tei_m.printdebug(fi, "assign req(%d) ri %d",4 - tm->N202, tm->ri);
			printk(KERN_DEBUG"assign req(%d) ri %d\n",4 - tm->N202, tm->ri);
		}	
#endif
		put_tei_msg(tm, ID_REQUEST, tm->ri, 127);
		fsm_add_timer(&tm->t202, tm->T202, EV_T202, NULL, 3);
	}
	else
	{
//		tm->tei_m.printdebug(fi, "assign req failed");
		printk(KERN_WARNING"assign req failed\n");
		skb = create_link_skb(MDL_ERROR | REQUEST, 0, 0, NULL, 0);
		if (!skb)
			return;
		if (tei_l2(tm->l2, skb))
			dev_kfree_skb(skb);
//		cs->cardmsg(cs, MDL_REMOVE | REQUEST, NULL);
		fsm_change_state(fi, ST_TEI_NOP);
	}
}

/* timeout */
static void tei_id_verify_timeout(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;
	struct sk_buff *skb;

	if (--tm->N202)
	{
#if AS_ISDN_DEBUG_L2_TEI
		if (tm->debug)
		{
//			tm->tei_m.printdebug(fi,"id verify req(%d) for tei %d", 3 - tm->N202, tm->l2->tei);
			printk(KERN_DEBUG "id verify req(%d) for tei %d\n", 3 - tm->N202, tm->l2->tei);
		}	
#endif

		put_tei_msg(tm, ID_VERIFY, 0, tm->l2->tei);
		fsm_add_timer(&tm->t202, tm->T202, EV_T202, NULL, 4);
	}
	else
	{
//		tm->tei_m.printdebug(fi, "verify req for tei %d failed", tm->l2->tei);
		printk(KERN_WARNING"verify req for tei %d failed\n", tm->l2->tei);
		skb = create_link_skb(MDL_REMOVE | REQUEST, 0, 0, NULL, 0);
		if (!skb)
			return;
		if (tei_l2(tm->l2, skb))
			dev_kfree_skb(skb);
//		cs->cardmsg(cs, MDL_REMOVE | REQUEST, NULL);
		fsm_change_state(fi, ST_TEI_NOP);
	}
}

/* change TEI msg send in MDL_UNITDATA into event and drive state machine */		
static int tei_ph_data_ind(teimgr_t *tm, int dtyp, struct sk_buff *skb)
{
	u_char *dp;
	int mt;
	int ret = -EINVAL;

	if (!skb)
		return(ret);

	/* P2P(no TEI assignment procedure needed) and layer2_t is not for network side
	(TE mode, not NT mode), so no TEI procedure need, return directly  */
	if (test_bit(FLG_FIXED_TEI, &tm->l2->flag) &&	!test_bit(FLG_LAPD_NET, &tm->l2->flag))
		return(ret);
	
	if (skb->len < 8)
	{
//		tm->tei_m.printdebug(&tm->tei_m, "short mgr frame %ld/8", skb->len);
//		printk(KERN_WARNING"short mgr frame %ld/8\n", skb->len);
		printk(KERN_WARNING"short mgr frame %d/8\n", skb->len);
		return(ret);
	}
	
	dp = skb->data + 2;
	if ((*dp & 0xef) != UI)
	{/* must be MDL_INIT DATA */
//		tm->tei_m.printdebug(&tm->tei_m, "mgr frame is not ui %x", *dp);
		printk(KERN_WARNING "mgr frame is not ui %x\n", *dp);
		return(ret);
	}
	dp++;
	
	if (*dp++ != TEI_ENTITY_ID)
	{/* wrong management entity identifier, ignore */
		dp--;
//		tm->tei_m.printdebug(&tm->tei_m, "tei handler wrong entity id %x", *dp);
		printk(KERN_WARNING  "tei handler wrong entity id %x\n", *dp);
		return(ret);
	}

		mt = *(dp+2);
#if AS_ISDN_DEBUG_L2_TEI
		if (tm->debug)
		{
//			tm->tei_m.printdebug(&tm->tei_m, "tei handler mt %x", mt);
			printk(KERN_DEBUG  "tei handler mt %x\n", mt);
		}	
#endif

	/* change TEI msg send in MDL_UNIT DATA into event */		
		if (mt == ID_ASSIGNED)
			as_isdn_fsm_event(&tm->tei_m, EV_ASSIGN, dp);
		else if (mt == ID_DENIED)
			as_isdn_fsm_event(&tm->tei_m, EV_DENIED, dp);
		else if (mt == ID_CHK_REQ)
			as_isdn_fsm_event(&tm->tei_m, EV_CHKREQ, dp);
		else if (mt == ID_REMOVE)
			as_isdn_fsm_event(&tm->tei_m, EV_REMOVE, dp);
		else if (mt == ID_REQUEST && test_bit(FLG_LAPD_NET, &tm->l2->flag))
			/* ID_REQUEST is handled only when I'm in network side(NT mode) */
			as_isdn_fsm_event(&tm->tei_m, EV_ASSIGN_REQ, dp);
		else
		{
//			tm->tei_m.printdebug(&tm->tei_m, "tei handler wrong mt %x", mt);
			printk(KERN_DEBUG "tei handler wrong mt %x", mt);
			return(ret);
		}

	dev_kfree_skb(skb);
	return(0);
}

/* sink of MDL_** for TEI manager */
int l2_tei(teimgr_t *tm, struct sk_buff *skb)
{
	as_isdn_head_t	*hh;
	int		ret = -EINVAL;

	if (!tm || !skb)
		return(ret);

	hh = AS_ISDN_HEAD_P(skb);
#if AS_ISDN_DEBUG_L2_TEI
	if (tm->debug)
	{
		printk(KERN_ERR  "%s: prim(%x)\n", __FUNCTION__, hh->prim);
	}	
#endif

	switch(hh->prim)
	{
		case (MDL_UNITDATA | INDICATION):
			/* All TEI msg are hold in MDL_UNITDATA */
			return(tei_ph_data_ind(tm, hh->dinfo, skb));

		case (MDL_ASSIGN | INDICATION):
			if (test_bit(FLG_FIXED_TEI, &tm->l2->flag))
			{/* fixed TEI */
#if AS_ISDN_DEBUG_L2_TEI
				if (tm->debug)
				{
//					tm->tei_m.printdebug(&tm->tei_m, "fixed assign tei %d", tm->l2->tei);
					printk(KERN_DEBUG "fixed assign tei %d", tm->l2->tei);
				}	
#endif
				skb_trim(skb, 0);
				AS_ISDN_sethead(MDL_ASSIGN | REQUEST, tm->l2->tei, skb);
				if (!tei_l2(tm->l2, skb))
					return(0);
				//			cs->cardmsg(cs, MDL_ASSIGN | REQUEST, NULL);
			}
			else
				as_isdn_fsm_event(&tm->tei_m, EV_IDREQ, NULL);

			break;
			
		case (MDL_ERROR | INDICATION):
			if (!test_bit(FLG_FIXED_TEI, &tm->l2->flag))
				as_isdn_fsm_event(&tm->tei_m, EV_VERIFY, NULL);
			break;
	}
	dev_kfree_skb(skb);
	return(0);
}

#if 0
static void tei_debug(struct FsmInst *fi, char *fmt, ...)
{
	teimgr_t	*tm = fi->userdata;
	logdata_t	log;
	char		head[24];

	va_start(log.args, fmt);
	sprintf(head,"tei %s", tm->l2->inst.name);
	log.fmt = fmt;
	log.head = head;
	tm->l2->inst.obj->ctrl(&tm->l2->inst, MGR_DEBUGDATA | REQUEST, &log);
	va_end(log.args);
}
#endif

static struct FsmNode TeiFnList[] =
{
	{ST_TEI_NOP, 		EV_IDREQ, 		tei_id_request},
	{ST_TEI_NOP, 		EV_ASSIGN, 		tei_id_test_dup},
	{ST_TEI_NOP,			EV_ASSIGN_REQ, 	tei_assign_req},
	{ST_TEI_NOP, 		EV_VERIFY, 		tei_id_verify},
	{ST_TEI_NOP, 		EV_REMOVE, 		tei_id_remove},
	{ST_TEI_NOP, 		EV_CHKREQ, 		tei_id_chk_req},
	{ST_TEI_IDREQ, 		EV_T202, 		tei_id_request_timeout},
	{ST_TEI_IDREQ, 		EV_ASSIGN, 		tei_id_assign},
	{ST_TEI_IDREQ, 		EV_DENIED, 		tei_id_denied},
	{ST_TEI_IDVERIFY, 	EV_T202, 		tei_id_verify_timeout},
	{ST_TEI_IDVERIFY, 	EV_REMOVE, 		tei_id_remove},
	{ST_TEI_IDVERIFY, 	EV_CHKREQ, 		tei_id_chk_req},
};

#define TEI_FN_COUNT (sizeof(TeiFnList)/sizeof(struct FsmNode))

void release_tei(teimgr_t *tm)
{
	fsm_del_timer(&tm->t202, 1);
	kfree(tm);
}

/* create TEI manager in layer2_t */
int create_teimgr(layer2_t *l2)
{
	teimgr_t *ntei;

	if (!l2)
	{
		printk(KERN_ERR "create_tei no layer2\n");
		return(-EINVAL);
	}

	if (!(ntei = kmalloc(sizeof(teimgr_t), GFP_ATOMIC)))
	{
		printk(KERN_ERR "kmalloc teimgr failed\n");
		return(-ENOMEM);
	}
	memset(ntei, 0, sizeof(teimgr_t));
	ntei->l2 = l2;
	ntei->T202 = 2000;	/* T202  2000 milliseconds, refer to specs page 231 */
	ntei->debug = l2->debug;
#if AS_ISDN_DEBUG_FSM
	ntei->tei_m.debug = l2->debug;
#endif
	ntei->tei_m.userdata = ntei;
#if AS_ISDN_DEBUG_FSM
//	ntei->tei_m.printdebug = tei_debug;
	ntei->tei_m.printdebug = NULL;
#endif
	/* NT&TE modes can be supported by the same state machine */
	if (test_bit(FLG_LAPD_NET, &l2->flag))
	{
		ntei->tei_m.fsm = &teifsm;
		ntei->tei_m.state = ST_TEI_NOP;
	}
	else
	{
		ntei->tei_m.fsm = &teifsm;
		ntei->tei_m.state = ST_TEI_NOP;
	}
	fsm_init_timer(&ntei->tei_m, &ntei->t202);
	l2->tm = ntei;
	return(0);
}

int TEIInit(void)
{
	teifsm.state_count = TEI_STATE_COUNT;
	teifsm.event_count = TEI_EVENT_COUNT;
	teifsm.strEvent = strTeiEvent;
	teifsm.strState = strTeiState;
	
	as_isdn_fsm_new(&teifsm, TeiFnList, TEI_FN_COUNT);
	return(0);
}

void TEIFree(void)
{
	as_isdn_fsm_free(&teifsm);
}
