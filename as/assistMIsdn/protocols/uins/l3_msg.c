/*
* $Id: l3_msg.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
* Common logic for L3 message handler 
* Li Zhijie, 2005.08.24
*/

#include "l3.h"

static int calc_msg_len(Q931_info_t *qi)
{
	int	i, cnt = 0;
	u_char	*buf = (u_char *)qi;
	u16	*v_ie;

	buf += L3_EXTRA_SIZE;
	if (qi->more_data)
		cnt++;
	if (qi->sending_complete)
		cnt++;
	if (qi->congestion_level)
		cnt++;
	
	v_ie = &qi->bearer_capability;
	for (i=0; i<32; i++)
	{
		if (v_ie[i])
			cnt += buf[v_ie[i] + 1] + 2;
	}
	
	return(cnt);
}


static int compose_msg(struct sk_buff *skb, Q931_info_t *qi)
{
	int	i, l;
	u_char	*p, *buf = (u_char *)qi;
	u16	*v_ie;

	buf += L3_EXTRA_SIZE;
	
	if (qi->more_data)
	{
		p = skb_put(skb, 1);
		*p = buf[qi->more_data];
	}
	if (qi->sending_complete)
	{
		p = skb_put(skb, 1);
		*p = buf[qi->sending_complete];
	}
	if (qi->congestion_level)
	{
		p = skb_put(skb, 1);
		*p = buf[qi->congestion_level];
	}
	
	v_ie = &qi->bearer_capability;
	for (i=0; i<32; i++)
	{
		if (v_ie[i])
		{
			l = buf[v_ie[i] + 1] +1;
			p = skb_put(skb, l + 1);
			*p++ = AS_ISDN_l3_pos2ie(i);
			memcpy(p, &buf[v_ie[i] + 1], l);
		}
	}
	
	return(0);
}


/* create a skb filled with MessageType, CallRef */
struct sk_buff *MsgStart(l3_process_t *pc, u_char mt, int len)
{
	struct sk_buff	*skb;
	int		lx = 4;
	u_char		*p;

	if (test_bit(FLG_CRLEN2, &pc->l3->Flag))
		lx++;

	if (pc->callref == -1) /* dummy cr */
		lx = 3;

	if (!(skb = alloc_stack_skb(len + lx, pc->l3->down_headerlen)))
		return(NULL);
	
	p = skb_put(skb, lx);
	*p++ = 8;			/* assigned Protocol Discriminator */
	
	if (lx == 3)			/* lx : length of CallRef */
		*p++ = 0;
	else if (lx == 5)
	{
		*p++ = 2;
		*p++ = (pc->callref >> 8)  ^ 0x80;
		*p++ = pc->callref & 0xff;
	}
	else
	{
		*p++ = 1;
		*p = pc->callref & 0xff;
		if (!(pc->callref & 0x8000))
			*p |= 0x80;
		p++;
	}

	*p = mt;
	return(skb);
}

int SendMsg(l3_process_t *pc, struct sk_buff *skb, int state)
{
	int		l;
	int		ret;
	struct sk_buff	*nskb;
	Q931_info_t	*qi;

	if (!skb)
		return(-EINVAL);
	qi = (Q931_info_t *)skb->data;
	l = calc_msg_len(qi);
	if (!(nskb = MsgStart(pc, qi->type, l)))
	{
		kfree_skb(skb);
		return(-ENOMEM);
	}
	
	if (l)
		compose_msg(nskb, qi);

	kfree_skb(skb);
	
	if (state != -1)
		newl3state(pc, state);
	
	if ((ret=l3_msg(pc->l3, DL_DATA | REQUEST, 0, 0, nskb)))
		kfree_skb(nskb);

	return(ret);
}


int l3ins_message(l3_process_t *pc, u_char mt)
{
	struct sk_buff	*skb;
	int		ret;

	if (!(skb = MsgStart(pc, mt, 0)))
		return(-ENOMEM);
	
	if ((ret=l3_msg(pc->l3, DL_DATA | REQUEST, 0, 0, skb)))
		kfree_skb(skb);

	return(ret);
}

void l3ins_message_cause(l3_process_t *pc, u_char mt, u_char cause)
{
	struct sk_buff	*skb;
	u_char		*p;
	int		ret;

//TRACE();	
	if (!(skb = MsgStart(pc, mt, 4)))
		return;
	p = skb_put(skb, 4);
	*p++ = IE_CAUSE;
	*p++ = 0x2;
	*p++ = 0x80 | CAUSE_LOC_USER;
	*p++ = 0x80 | cause;
	
//TRACE();	
	if ((ret=l3_msg(pc->l3, DL_DATA | REQUEST, 0, 0, skb)))
	{
//		TRACE();	
		kfree_skb(skb);
	}
//TRACE();	
	
}

void l3ins_status_send(l3_process_t *pc, u_char cause)
{
	struct sk_buff	*skb;
	u_char		*p;
	int		ret;

	if (!(skb = MsgStart(pc, MT_STATUS, 7)))
		return;
	
	p = skb_put(skb, 7);
	*p++ = IE_CAUSE;
	*p++ = 2;
	*p++ = 0x80 | CAUSE_LOC_USER;
	*p++ = 0x80 | cause;

	*p++ = IE_CALL_STATE;
	*p++ = 1;
	*p++ = pc->state & 0x3f;
	
	if ((ret=l3_msg(pc->l3, DL_DATA | REQUEST, 0, 0, skb)))
		kfree_skb(skb);
}

void l3ins_msg_without_setup(l3_process_t *pc, u_char cause)
{
	/* This routine is called if here was no SETUP made (checks in dss1up and in
	 * l3ins_setup) and a RELEASE_COMPLETE have to be sent with an error code
	 * MT_STATUS_ENQUIRE in the NULL state is handled too
	 */
	switch (cause)
	{
		case 81:		/* invalid callreference */
		case 88:		/* incomp destination */
		case 96:		/* mandory IE missing */
		case 100:	/* invalid IE contents */
		case 101:	/* incompatible Callstate */
			l3ins_message_cause(pc, MT_RELEASE_COMPLETE, cause);
			break;
		default:
			printk(KERN_ERR "AS_ISDN l3ins_msg_without_setup wrong cause %d\n", cause);
	}
	release_l3_process(pc);
}



