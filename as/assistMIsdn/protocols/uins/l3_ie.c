/*
 * $Id: l3_ie.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include "l3.h"

static int comp_required[] = {1,2,3,5,6,7,9,10,11,14,15,-1};


struct ie_len 
{
	int ie;
	int len;
};

/* IE and its maxlen */
static struct ie_len max_ie_len[] = 
{
	{IE_SEGMENT, 		4},
	{IE_BEARER, 			12},
	{IE_CAUSE, 			32},
	{IE_CALL_ID, 		10},
	{IE_CALL_STATE, 		3},
	{IE_CHANNEL_ID,		34},
	{IE_FACILITY, 		255},
	{IE_PROGRESS, 		4},
	{IE_NET_FAC, 		255},
	{IE_NOTIFY, 			255}, /* 3-* Q.932 Section 9 */
	{IE_DISPLAY, 		82},
	{IE_DATE, 			8},
	{IE_KEYPAD, 			34},
	{IE_SIGNAL, 			3},
	{IE_INFORATE, 		6},
	{IE_E2E_TDELAY, 		11},
	{IE_TDELAY_SEL, 		5},
	{IE_PACK_BINPARA, 	3},
	{IE_PACK_WINSIZE, 	4},
	{IE_PACK_SIZE, 		4},
	{IE_CUG, 			7},
	{IE_REV_CHARGE, 	3},
	{IE_CALLING_PN, 		24},		/* 36 in INS */
	{IE_CALLING_SUB, 	23},
	{IE_CALLED_PN, 		24},
	{IE_CALLED_SUB, 		23},		/* 36 in INS */
	{IE_REDIR_NR, 		255},
	{IE_TRANS_SEL, 		255},
	{IE_RESTART_IND, 	3},
	{IE_LLC, 				18},
	{IE_HLC, 			5},
	{IE_USER_USER, 		131},
	{-1,0},
};

/* get max length of IE */
static int getmax_ie_len(u_char ie)
{
	int i = 0;
	while (max_ie_len[i].ie != -1)
	{
		if (max_ie_len[i].ie == ie)
			return(max_ie_len[i].len);
		i++;
	}
	return(255);
}

/* check whether this IE is in IE checkList : >0 : Yes; =0, No, <0 : Error */
int ie_in_set(l3_process_t *pc, u_char ie, int *checklist)
{
	int ret = 1;

	while (*checklist != -1)
	{
		if ((*checklist & 0xff) == ie)
		{/* bit 8 is 1 is not validate for all IEs */
			if (ie & 0x80)
				return(-ret);
			else
				return(ret);
		}
		ret++;
		checklist++;
	}
	return(0);
}


/* general check routine for all IEs in L3 message */
int check_infoelements(l3_process_t *pc, struct sk_buff *skb, int *checklist)
{
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	int		*cl = checklist;
	u_char		*p, ie;
	u16		*iep;
	int		i, l, newpos, oldpos;
	int		err_seq = 0, err_len = 0, err_compr = 0, err_ureg = 0;
	
	p = skb->data;
	p += L3_EXTRA_SIZE;
	iep = &qi->bearer_capability;
	oldpos = -1;

	for (i=0; i<32; i++)
	{
		if (iep[i])
		{
			ie = AS_ISDN_l3_pos2ie(i);
			if ((newpos = ie_in_set(pc, ie, cl)))
			{
				if (newpos > 0)
				{
					if (newpos < oldpos)
						err_seq++;
					else
						oldpos = newpos;
				}
			}
			else
			{
				if (ie_in_set(pc, ie, comp_required))
					err_compr++;
				else
					err_ureg++;
			}
			l = p[iep[i] +1];
			if (l > getmax_ie_len(ie))
				err_len++;
		}
	}
	
	if (err_compr | err_ureg | err_len | err_seq)
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (pc->l3->debug & L3_DEB_CHECK)
		{
//			l3_debug(pc->l3, "check IE MT(%x) %d/%d/%d/%d",qi->type, err_compr, err_ureg, err_len, err_seq);
			printk(KERN_DEBUG "check IE MT(%x) %d/%d/%d/%d\n",qi->type, err_compr, err_ureg, err_len, err_seq);
		}	
#endif		
		if (err_compr)
			return(ERR_IE_COMPREHENSION);
		if (err_ureg)
			return(ERR_IE_UNRECOGNIZED);
		if (err_len)
			return(ERR_IE_LENGTH);
		if (err_seq)
			return(ERR_IE_SEQUENCE);
	} 
	return(0);
}

void l3ins_std_ie_err(l3_process_t *pc, int ret)
{
#if AS_ISDN_DEBUG_L3_DATA
	if (pc->l3->debug & L3_DEB_CHECK)
	{
//		l3_debug(pc->l3, "check_infoelements ret %d", ret);
		printk(KERN_DEBUG "check_infoelements ret %d\n", ret);
	}
#endif

	switch(ret)
	{
		case 0: 
			break;
		case ERR_IE_COMPREHENSION:
			l3ins_status_send(pc, CAUSE_MANDATORY_IE_MISS);
			break;
		case ERR_IE_UNRECOGNIZED:
			l3ins_status_send(pc, CAUSE_IE_NOTIMPLEMENTED);
			break;
		case ERR_IE_LENGTH:
			l3ins_status_send(pc, CAUSE_INVALID_CONTENTS);
			break;
		case ERR_IE_SEQUENCE:
		default:
			break;
	}
}

/* check Channel Id in L3 msg : 0 : correct; <0 : error */
int l3ins_get_channel_id(l3_process_t *pc, struct sk_buff *skb)
{
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	u_char		*p;

	if (qi->channel_id)
	{
		p = skb->data;
		p += L3_EXTRA_SIZE + qi->channel_id;
		p++;
		if (test_bit(FLG_EXTCID, &pc->l3->Flag))
		{
			if (*p != 1)
			{
				pc->bc = 1;
				return (0);
			}
		}
		
		if (*p != 1)
		{ /* len for BRI = 1 */
#if AS_ISDN_DEBUG_L3_DATA
			if (pc->l3->debug & L3_DEB_WARN)
			{
//				l3_debug(pc->l3, "wrong chid len %d", *p);
				printk(KERN_WARNING "wrong chid len %d\n", *p);
			}	
#endif
			return (-2);
		}
		
		p++;
		if (*p & 0x60) /* InterfaceType bit = 1 */
		{ /* only Primary rate interface */
#if AS_ISDN_DEBUG_L3_DATA
			if (pc->l3->debug & L3_DEB_WARN)
			{
//				l3_debug(pc->l3, "wrong chid %x", *p);
				printk(KERN_WARNING "wrong chid %x\n", *p);
			}	
#endif
			return (-3);
		}
		pc->bc = *p & 3;	/* keep InfoChannel selection in octet 3 */
	}
	else
		return(-1);

	return(0);
}

/*get CAUSE from L3 message in skb and put it into pc->err */
int l3ins_get_cause(l3_process_t *pc, struct sk_buff *skb)
{
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	u_char		l;
	u_char		*p;

	if (qi->cause)
	{
		p = skb->data;
		p += L3_EXTRA_SIZE + qi->cause;
		p++;
		l = *p++;
		if (l>30)
		{
			return(-30);
		}

		if (l)
			l--;
		else
		{
			return(-2);
		}

		if (l && !(*p & 0x80))
		{
			l--;
			p++; /* skip recommendation */
		}
		p++;

		if (l)
		{/* bit 8 is 0 */
			if (!(*p & 0x80))
			{
				return(-3);
			}
			pc->err = *p & 0x7F;	/* put cause info(integer) into pc->err */
		}
		else
		{
			return(-4);
		}
	}
	else
		return(-1);
	
	return(0);
}

