/*
 * $Id: l3_uins_if.c,v 1.1.1.1 2006/11/29 08:55:14 lizhijie Exp $
 */

#include "l3.h"

extern void global_handler(layer3_t *l3, u_int mt, struct sk_buff *skb);
extern void l3ins_down_handler( l3_process_t *proc, struct sk_buff *skb);
extern void l3ins_data_handler(Q931_info_t *qi, l3_process_t *proc, struct sk_buff *skb);

extern void l3ins_facility(l3_process_t *pc, u_char pr, void *arg);

/* parse the L3 message in skb and create a new Q931_info_t in skb to hold the Q931 info */
static int parseQ931(struct sk_buff *skb)
{
	Q931_info_t	*qi;
	int			l, codeset, maincodeset;
	int			len, iep/*position of IE */, pos = 0, cnt = 0;
	u16			*ie, cr;
	u_char		mt, *p = skb->data;

	if (skb->len < 3)
		return(-1);
	
	p++;			/* protocol discriminator */
	l = (*p++) & 0xf;	/* len of CallRef */
	if (l>2)
		return(-2);
	if (l)
		cr = *p++;
	else
		cr = 0;
	
	if (l == 2)
	{
		cr <<= 8;
		cr |= *p++;
	}
	else if (l == 1)
	{/* when CallRf len=1 */
		if (cr & 0x80)
		{
			cr |= 0x8000;
			cr &= 0xFF7F;
		}
	}
	
	mt = *p;		/*msgType*/
	if ((u_long)p & 1)
		pos = 1;
	else
		pos = 0;
	skb_pull(skb, (p - skb->data) - pos);
	len = skb->len;
	p = skb->data;
	if (skb_headroom(skb) < (int)L3_EXTRA_SIZE/*length of Q931*/)
	{
		int_error();
		return(-3);
	}

	/* allocated extra space for Q931_info_t as skb and return pointer to this new extra data */
	qi = (Q931_info_t *)skb_push(skb, L3_EXTRA_SIZE);
	AS_ISDN_initQ931_info(qi);

	qi->type = mt;
	qi->crlen = l;
	qi->cr = cr;
	pos++;
	codeset = maincodeset = 0;
	ie = &qi->bearer_capability;
#if 0
	/* commented for codeset 6 support */
	while (pos < len)
	{
		/* check whether this IE is Shift */
		/*locking shift procedure, refer to Specs. vol.3.P.70. Clause 4.5.3*/
		if ((p[pos] & 0xf0) == 0x90)	/* shift identifier */
		{
			codeset = p[pos] & 0x07;	/* codeset Identifier */
			if (!(p[pos] & 0x08))		/* bit 4 is 0 : locking shift*/
				maincodeset = codeset;
			pos++;
			continue;
		}

printk(KERN_ERR" codeset=%d\r\n" ,codeset);
		/* when codeset 0 is active; not support codeset=6(network specific IE) until now */
		if (codeset == 0) /* Q931 IE : codeset 0  */
		{
			if (p[pos] & 0x80)
			{ /* single octet IE */
				if (p[pos] == IE_MORE_DATA)
					qi->more_data = pos;
				else if (p[pos] == IE_COMPLETE)
					qi->sending_complete = pos;
				else if ((p[pos] & 0xf0) == IE_CONGESTION)
					qi->congestion_level = pos;
				cnt++;
				pos++;
			}
			else
			{/* change position of IE in L3 message into Q931_info_t */
				iep = AS_ISDN_l3_ie2pos(p[pos]);
				if ((pos+1) >= len)
					return(-4);
				l = p[pos+1]; 			/* length of this IE */
				if ((pos+l+1) >= len)
					return(-5);
				
				if (iep>=0)
				{
					if (!ie[iep])
						ie[iep] = pos;	/* in the position of Q931_info_t keep the L3 message's position info of this IE*/
				}
				pos += l + 2;			/* length of IR + lengthByte + TypeByte */
				cnt++;
			}
		}
		codeset = maincodeset;
	}
#endif
	while (pos < len)
	{
		/* check whether this IE is Shift */
		/*locking shift procedure, refer to Specs. vol.3.P.70. Clause 4.5.3*/
		if ((p[pos] & 0xf0) == 0x90)	/* shift identifier */
		{
			codeset = p[pos] & 0x07;	/* codeset Identifier */
			if (!(p[pos] & 0x08))		/* bit 4 is 0 : locking shift*/
				maincodeset = codeset;
			pos++;
			continue;
		}

//		printk(KERN_ERR" codeset=%d\r\n" ,codeset);
		/* when codeset 0 is active; not support codeset=6(network specific IE) until now */
		if (codeset == 0) /* Q931 IE : codeset 0  */
		{
			if (p[pos] & 0x80)
			{ /* single octet IE */
				if (p[pos] == IE_MORE_DATA)
					qi->more_data = pos;
				else if (p[pos] == IE_COMPLETE)
					qi->sending_complete = pos;
				else if ((p[pos] & 0xf0) == IE_CONGESTION)
					qi->congestion_level = pos;
				cnt++;
				pos++;
			}
			else
			{/* change position of IE in L3 message into Q931_info_t */
				iep = AS_ISDN_l3_ie2pos(p[pos]);
				if ((pos+1) >= len)
					return(-4);
				l = p[pos+1]; 			/* length of this IE */
				if ((pos+l+1) >= len)
					return(-5);
				
				if (iep>=0)
				{
					if (!ie[iep])
						ie[iep] = pos;	/* in the position of Q931_info_t keep the L3 message's position info of this IE*/
				}
				pos += l + 2;			/* length of IR + lengthByte + TypeByte */
				cnt++;
			}
		}
		else
		{/* added codeset 6 support for INS PBX, lizhijie,2005.12.13
		* refer to vol. 3, p.70 and vol.4, p.359
		* PBX send out IE of advice of charge in codeset 6 : '0x96(codeset 6), 0x01(IE type),0x02(len), 0x82(total charge),...' 
		*/
//			printk(KERN_ERR"0x%x:0x%x:0x%x:0x%x", p[pos], p[pos+1], p[pos+2], p[pos+3]);
			l = p[pos+1];
			pos += l+2;
//			printk(KERN_ERR"pos =%d  len =%d", pos, len );
		}
		codeset = maincodeset;
	}
	
	return(cnt);
}


/* verify if a message type exists and contain no IE error when data is received, eg. from down to up 
* Only NT-->USER signal is support, so it is only support TE mode
*/
static int l3ins_check_messagetype_validity(l3_process_t *pc, int mt, void *arg)
{/* Only TE mode */
	switch (mt)
	{
		case MT_ALERTING:
		case MT_CALL_PROCEEDING:
		case MT_CONNECT:
		case MT_CONNECT_ACKNOWLEDGE:
		case MT_DISCONNECT:
		case MT_INFORMATION:
		case MT_FACILITY:
		case MT_NOTIFY:
		case MT_PROGRESS:
		case MT_RELEASE:
		case MT_RELEASE_COMPLETE:
		case MT_SETUP:
		case MT_SETUP_ACKNOWLEDGE:
		case MT_RESUME_ACKNOWLEDGE:
		case MT_RESUME_REJECT:
		case MT_SUSPEND_ACKNOWLEDGE:
		case MT_SUSPEND_REJECT:
		case MT_USER_INFORMATION:
		case MT_RESTART:
		case MT_RESTART_ACKNOWLEDGE:
		case MT_CONGESTION_CONTROL:
		case MT_STATUS:
		case MT_STATUS_ENQUIRY:
			
#if AS_ISDN_DEBUG_L3_DATA
			if (pc->l3->debug & L3_DEB_CHECK)
			{
//				l3_debug(pc->l3, "%s : mt(%x) OK",__FUNCTION__,  mt);
				printk(KERN_DEBUG  "%s : mt(%x) OK",__FUNCTION__,  mt);
			}	
#endif
			break;
			
		case MT_RESUME: /* RESUME only in user->net */
		case MT_SUSPEND: /* SUSPEND only in user->net */
		default:
#if AS_ISDN_DEBUG_L3_DATA
			if (pc->l3->debug & (L3_DEB_CHECK | L3_DEB_WARN))
			{
//				l3_debug(pc->l3, "%s mt(%x) fail",__FUNCTION__,  mt);
				printk(KERN_WARNING "%s mt(%x) fail",__FUNCTION__,  mt);
			}	
#endif
			l3ins_status_send(pc, CAUSE_MT_NOTIMPLEMENTED);
			return(1);
			
	}
	return(0);
}

/* All L3 message is encapsulated in skb->data */
int ins_fromdown(AS_ISDNif_t *hif, struct sk_buff *skb)
{
	layer3_t			*l3;
	int				cause, callState, ret = -EINVAL;
	char				*ptr;
	l3_process_t		*proc;
	AS_ISDN_head_t	*hh;
	Q931_info_t		*qi;
	u_long			flags;

	if (!hif || !skb)
		return(ret);

	l3 = hif->fdata;
	hh = AS_ISDN_HEAD_P(skb);
#if AS_ISDN_DEBUG_L3_DATA
//	if (asl3_debug)
		printk(KERN_ERR  "%s: prim(%x)\n", __FUNCTION__, hh->prim);
#endif
	if (!l3)
		return(ret);

//	TRACE();

	spin_lock_irqsave(&l3->lock, flags);
	switch (hh->prim)
	{
		case (DL_DATA | INDICATION):
		case (DL_UNITDATA | INDICATION):
			/* used as L3 messages received */
//			TRACE();
			break;
			
		case (DL_ESTABLISH | CONFIRM):
		case (DL_ESTABLISH | INDICATION):
		case (DL_RELEASE | INDICATION):
		case (DL_RELEASE | CONFIRM):
			l3_msg(l3, hh->prim, hh->dinfo, 0, NULL);
			dev_kfree_skb(skb);
			spin_unlock_irqrestore(&l3->lock, flags);
			return(0);
			break;
			
		case (DL_DATA | CONFIRM):
		case (DL_UNITDATA | CONFIRM):
			/* no handle needed for these primitives */
			dev_kfree_skb(skb);
			spin_unlock_irqrestore(&l3->lock, flags);
			return(0);
			break;
			
		default:
			printk(KERN_WARNING "%s : unknown pr=%04x\n", l3->inst.name, hh->prim);
			spin_unlock_irqrestore(&l3->lock, flags);
			return(-EINVAL);
	}

//	TRACE();

	/******** following is only for DL_DATA|UNITDATA|INDICATION, eg. 
	All L2 Msg send from peer is encapsulated in these primitives ***********/
	if (skb->len < 3)
	{
//		l3_debug(l3, "UINS up frame too short(%d)", skb->len);
		printk(KERN_WARNING "INS USER up frame too short(%d)\n", skb->len);
		dev_kfree_skb(skb);
		spin_unlock_irqrestore(&l3->lock, flags);
		return(0);
	}

	if (skb->data[0] != PROTO_DIS_EURO)
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (l3->debug & L3_DEB_PROTERR)
		{
//			l3_debug(l3, "%s : FROMDOWN %sunexpected discriminator %x message len %d", l3->inst.name, (hh->prim == (DL_DATA | INDICATION)) ? " " : "(broadcast) ", skb->data[0], skb->len);
			printk(KERN_WARNING "%s : FROMDOWN %sunexpected discriminator %x message len %d\n", l3->inst.name, (hh->prim == (DL_DATA | INDICATION)) ? " " : "(broadcast) ", skb->data[0], skb->len);
		}
#endif
		dev_kfree_skb(skb);
		spin_unlock_irqrestore(&l3->lock, flags);
		return(0);
	}
	
//	TRACE();
	ret = parseQ931(skb);
	if (ret < 0)
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (l3->debug & L3_DEB_PROTERR)
		{
//			l3_debug(l3, "dss1up: parse IE error %d", ret);
			printk(KERN_WARNING "INS USER up: parse IE error %d\n", ret);
		}	
#endif
		printk(KERN_WARNING "%s : FROMDOWN parse IE error %d\n", l3->inst.name, ret);
		dev_kfree_skb(skb);
		spin_unlock_irqrestore(&l3->lock, flags);
		return(0);
	}
	
//	TRACE();
	qi = (Q931_info_t *)skb->data;
	ptr = skb->data;
	ptr += L3_EXTRA_SIZE;
#if AS_ISDN_DEBUG_L3_DATA
	if (l3->debug & L3_DEB_STATE)
	{
//		l3_debug(l3, "%s : FROMDOWN CallRef %d", l3->inst.name, qi->cr);
		printk(KERN_DEBUG "%s : FROMDOWN CallRef %d\n", l3->inst.name, qi->cr);
	}	
#endif
	
//	TRACE();
	if (qi->crlen == 0)
	{/* Dummy Callref */
		if (qi->type == MT_FACILITY)
		{
			l3ins_facility(l3->dummy, hh->prim, skb);
			spin_unlock_irqrestore(&l3->lock, flags);
			return(0);
		}
#if AS_ISDN_DEBUG_L3_DATA
		else if (l3->debug & L3_DEB_WARN)
		{
//			l3_debug(l3, "INS USER up dummy Callref (no facility msg or ie)");
			printk(KERN_WARNING "INS USER up dummy Callref (no facility msg or ie)\n");
		}	
#endif		
		dev_kfree_skb(skb);
		spin_unlock_irqrestore(&l3->lock, flags);
		return(0);
	}
	else if ((qi->cr & 0x7fff) == 0)
	{/* Global CallRef : 0x7FFF */
//	TRACE();
#if AS_ISDN_DEBUG_L3_DATA
		if (l3->debug & L3_DEB_STATE)
		{
//			l3_debug(l3, "dss1up Global CallRef");
			printk(KERN_DEBUG  "INS USER up Global CallRef\n");
		}	
#endif
		global_handler(l3, qi->type, skb);
		spin_unlock_irqrestore(&l3->lock, flags);
		return(0);
	}
	else if (!(proc = getl3proc(l3, qi->cr)))
	{/****** normal L3 message : use callRef to retrieve l3_process_t *****/
	
//	TRACE();
	/* No transaction process exist, that means no call with this callreference is active	 */
		if (qi->type == MT_SETUP)
		{
			/* SETUP creates a new l3 process as callee */
			if (qi->cr & 0x8000)
			{/* Setup with wrong CREF flag */
#if AS_ISDN_DEBUG_L3_DATA
				if (l3->debug & L3_DEB_STATE)
				{
//					l3_debug(l3, "dss1up wrong CRef flag");
					printk(KERN_WARNING "INS USER up wrong CRef flag\n");
				}	
#endif
				dev_kfree_skb(skb);
				spin_unlock_irqrestore(&l3->lock, flags);
				return(0);
			}
			if (!(proc = new_l3_process(l3, qi->cr, N303, AS_ISDN_ID_ANY)))
			{
				/* May be to answer with RELEASE_COMPLETE and
				 * CAUSE 0x2f "Resource unavailable", but this
				 * need a new_l3_process too ... arghh
				 */
				dev_kfree_skb(skb);
				spin_unlock_irqrestore(&l3->lock, flags);
				return(0);
			}
			
			/* register this ID in L4 */
			ret = AS_ISDN_l3up(proc, CC_NEW_CR | INDICATION, NULL);
			if (ret)
			{
				printk(KERN_WARNING "INS USER up: cannot register ID(%x)\n",proc->id);
				dev_kfree_skb(skb);
				release_l3_process(proc);
				spin_unlock_irqrestore(&l3->lock, flags);
				return(0);
			}
		}
		else if (qi->type == MT_STATUS)
		{
//	TRACE();
			cause = 0;
			if (qi->cause)
			{
				if (ptr[qi->cause +1] >= 2)
					cause = ptr[qi->cause + 3] & 0x7f;
				else
					cause = ptr[qi->cause + 2] & 0x7f;	
			}
			callState = 0;
			if (qi->call_state)
			{
				callState = ptr[qi->cause + 2];
			}
			/* ETS 300-104 part 2.4.1
			 * if setup has not been made and a message type
			 * MT_STATUS is received with call state == 0,
			 * we must send nothing
			 */
			if (callState != 0)
			{
				/* ETS 300-104 part 2.4.2
				 * if setup has not been made and a message type
				 * MT_STATUS is received with call state != 0,
				 * we must send MT_RELEASE_COMPLETE cause 101
				 */
				if ((proc = new_l3_process(l3, qi->cr, N303, AS_ISDN_ID_ANY)))
				{
					l3ins_msg_without_setup(proc, CAUSE_NOTCOMPAT_STATE);
				}
			}
			dev_kfree_skb(skb);
			spin_unlock_irqrestore(&l3->lock, flags);
			return(0);
		}
		else if (qi->type == MT_RELEASE_COMPLETE)
		{
			dev_kfree_skb(skb);
			spin_unlock_irqrestore(&l3->lock, flags);
			return(0);
		}
		else
		{
//	TRACE();
			/* ETS 300-104 part 2
			 * if setup has not been made and a message type
			 * (except MT_SETUP and RELEASE_COMPLETE) is received,
			 * we must send MT_RELEASE_COMPLETE cause 81 */
			if ((proc = new_l3_process(l3, qi->cr, N303, AS_ISDN_ID_ANY)))
			{
				l3ins_msg_without_setup(proc, CAUSE_INVALID_CALLREF);
			}
			dev_kfree_skb(skb);
			spin_unlock_irqrestore(&l3->lock, flags);
			return(0);
		}
	}
	
//	TRACE();
	if (l3ins_check_messagetype_validity(proc, qi->type, skb))
	{
		dev_kfree_skb(skb);
		spin_unlock_irqrestore(&l3->lock, flags);
		return(0);
	}

//	TRACE();
	l3ins_data_handler(qi, proc, skb);
//	TRACE();
	
	spin_unlock_irqrestore(&l3->lock, flags);
	return(0);
}

/* interface function used by upper layer */
int ins_fromup(AS_ISDNif_t *hif, struct sk_buff *skb)
{
	layer3_t	*l3;
	int		cr, ret = -EINVAL;
	l3_process_t	*proc;
	AS_ISDN_head_t	*hh;
	u_long		flags;

#if AS_ISDN_DEBUG_L3_DATA
	printk(KERN_ERR  "%s.%s: prim\n",__FILE__,  __FUNCTION__ );
#endif
	if (!hif || !skb)
		return(ret);

	l3 = hif->fdata;
	hh = AS_ISDN_HEAD_P(skb);
	
#if AS_ISDN_DEBUG_L3_DATA
//	if (asl3_debug)
		printk(KERN_ERR   "%s: prim(%x)\n", __FUNCTION__, hh->prim);
#endif
	if (!l3)
		return(ret);

	spin_lock_irqsave(&l3->lock, flags);
	if ((DL_STATUS | REQUEST) == hh->prim || (DL_ESTABLISH | REQUEST) == hh->prim)
	{
		l3_msg(l3, hh->prim, hh->dinfo, 0, NULL);
		dev_kfree_skb(skb);
		spin_unlock_irqrestore(&l3->lock, flags);
		return(0);
	}
	
	proc = getl3proc4id(l3, hh->dinfo);
	if ((CC_NEW_CR | REQUEST) == hh->prim)
	{
		if (proc)
		{
			printk(KERN_WARNING "%s: proc(%x) allready exist\n", __FUNCTION__, hh->dinfo);
			ret = -EBUSY;
		}
		else
		{
			cr = newcallref(l3);
			cr |= 0x8000;	/* page 60. this msg is send to side originates this CallRef???*/
			ret = -ENOMEM;
			if ((proc = new_l3_process(l3, cr, N303, hh->dinfo)))
			{
				ret = 0;
				dev_kfree_skb(skb);
			}
		}
		spin_unlock_irqrestore(&l3->lock, flags);
		return(ret);
	} 

	if (!proc)
	{
		printk(KERN_ERR "ISDN INS USER fromup without proc pr=%04x\n", hh->prim);
		spin_unlock_irqrestore(&l3->lock, flags);
		return(-EINVAL);
	}
	
	l3ins_down_handler( proc,  skb);
	
	spin_unlock_irqrestore(&l3->lock, flags);
	return(0);
}

