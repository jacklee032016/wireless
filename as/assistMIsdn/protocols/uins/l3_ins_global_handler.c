/* 
* $Id: l3_ins_global_handler.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
* handler for Global CallReference which is only usedin fromdown interface
* Li Zhijie, 2005.08.24
*/

#include "l3.h"

extern void l3ins_status(l3_process_t *pc, u_char pr, void *arg);

static void l3ins_global_restart(l3_process_t *pc, u_char pr, void *arg)
{
	u_char		*p, ri, ch = 0, chan = 0;
	struct sk_buff	*skb = arg;
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	l3_process_t	*up, *n;

//TRACE();	
//	newl3state(pc, 2);
	L3DelTimer(&pc->timer);
	if (qi->restart_ind)
	{
		p = skb->data;
		p += L3_EXTRA_SIZE + qi->restart_ind;
		p++;
		ri = p[1];
#if AS_ISDN_DEBUG_L3_CTRL
//		l3_debug(pc->l3, "Restart %x", ri);
		printk(KERN_DEBUG "Restart %x\n", ri);
#endif
	}
	else
	{
#if AS_ISDN_DEBUG_L3_CTRL
//		l3_debug(pc->l3, "Restart without restart IE");
		printk(KERN_WARNING "Restart without restart IE\n");
#endif
		ri = 0x86;
	}

	if (qi->channel_id)
	{
		p = skb->data;
		p += L3_EXTRA_SIZE + qi->channel_id;
		p++;
		chan = p[1] & 3;
		ch = p[1];
#if AS_ISDN_DEBUG_L3_CTRL
		if (pc->l3->debug)
		{
//			l3_debug(pc->l3, "Restart for channel %d", chan);
			printk(KERN_DEBUG "Restart for channel %d\n", chan);
		}	
#endif
	}

	list_for_each_entry_safe(up, n, &pc->l3->plist, list)
	{
		if ((ri & 7) == 7)
			l3_process_mgr(up, CC_RESTART | REQUEST, NULL);
		else if (up->bc == chan)
			AS_ISDN_l3up(up, CC_RESTART | REQUEST, NULL);
	}
	dev_kfree_skb(skb);
	skb = MsgStart(pc, MT_RESTART_ACKNOWLEDGE, chan ? 6 : 3);
	p = skb_put(skb, chan ? 6 : 3);
	if (chan)
	{
		*p++ = IE_CHANNEL_ID;
		*p++ = 1;
		*p++ = ch | 0x80;
	}
	*p++ = IE_RESTART_IND;
	*p++ = 1;
	*p++ = ri;
	if (l3_msg(pc->l3, DL_DATA | REQUEST, 0, 0, skb))
		kfree_skb(skb);
}

/* state machine for global CallRef */
static struct stateentry globalmes_list[] =
{
	{ALL_STATES, 	MT_STATUS, 		l3ins_status},
	{SBIT(0),		MT_RESTART, 	l3ins_global_restart},
/*	{SBIT(1),		MT_RESTART_ACKNOWLEDGE, l3ins_restart_ack},
*/
};

#define GLOBALM_LEN \
	(sizeof(globalmes_list) / sizeof(struct stateentry))


/* handler for global CallRef */
void global_handler(layer3_t *l3, u_int mt, struct sk_buff *skb)
{
	u_int		i;
	l3_process_t	*proc = l3->global;

//TRACE();	
	proc->callref = skb->data[2]; /* cr flag */
	for (i = 0; i < GLOBALM_LEN; i++)
	{
		if ((mt == globalmes_list[i].primitive) && ((1 << proc->state) & globalmes_list[i].state))
			break;
	}	
	
	if (i == GLOBALM_LEN)
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (l3->debug & L3_DEB_STATE)
		{
//			l3_debug(l3, "INS USER global state %d mt %x unhandled", proc->state, mt);
			printk(KERN_WARNING "INS USER global state %d mt %x unhandled\n", proc->state, mt);
		}
#endif
		l3ins_status_send(proc, CAUSE_INVALID_CALLREF);
		dev_kfree_skb(skb);
	}
	else
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (l3->debug & L3_DEB_STATE)
		{
//			l3_debug(l3, "INS USER global %d mt %x", proc->state, mt);
			printk(KERN_DEBUG "INS USER global %d mt %x\n", proc->state, mt);
		}
#endif
		globalmes_list[i].rout(proc, mt, skb);
	}
}

