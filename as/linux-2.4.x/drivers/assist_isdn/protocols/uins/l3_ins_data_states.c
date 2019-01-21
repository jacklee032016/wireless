/*
* $Id: l3_ins_data_states.c,v 1.1.1.1 2006/11/29 08:55:14 lizhijie Exp $
* Common logic for L2 DATA or UNITDATA frame come from L2
* used in fromdown interface
* Li Zhijie, 2005.08.24
*/

#include "l3.h"

int ie_in_set(l3_process_t *pc, u_char ie, int *checklist);
int check_infoelements(l3_process_t *pc, struct sk_buff *skb, int *checklist);
void l3ins_std_ie_err(l3_process_t *pc, int ret);
int l3ins_get_channel_id(l3_process_t *pc, struct sk_buff *skb);
int l3ins_get_cause(l3_process_t *pc, struct sk_buff *skb);

void l3ins_release_req(l3_process_t *pc, u_char pr, void *arg);

static int l3_valid_states[] = {0,1,2,3,4,6,7,8,9,10,11,12,15,17,19,25,-1};

/* L3 message used by uins defined here */
static int ie_ALERTING[] = {IE_BEARER, IE_CHANNEL_ID | IE_MANDATORY_1,
		IE_FACILITY, IE_PROGRESS, IE_DISPLAY, IE_SIGNAL, IE_HLC,
		IE_USER_USER, -1};
static int ie_CALL_PROCEEDING[] = {IE_BEARER, IE_CHANNEL_ID | IE_MANDATORY_1,
		IE_FACILITY, IE_PROGRESS, IE_DISPLAY, IE_HLC, -1};
static int ie_CONNECT[] = {IE_BEARER, IE_CHANNEL_ID | IE_MANDATORY_1, 
		IE_FACILITY, IE_PROGRESS, IE_DISPLAY, IE_DATE, IE_SIGNAL,
		IE_CONNECT_PN, IE_CONNECT_SUB, IE_LLC, IE_HLC, IE_USER_USER, -1};
static int ie_CONNECT_ACKNOWLEDGE[] = {IE_CHANNEL_ID, IE_DISPLAY, IE_SIGNAL, -1};
static int ie_DISCONNECT[] = {IE_CAUSE | IE_MANDATORY, IE_FACILITY,
		IE_PROGRESS, IE_DISPLAY, IE_SIGNAL, IE_USER_USER, -1};
static int ie_INFORMATION[] = {IE_COMPLETE, IE_DISPLAY, IE_KEYPAD, IE_SIGNAL,
		IE_CALLED_PN, -1};
static int ie_NOTIFY[] = {IE_BEARER, IE_NOTIFY | IE_MANDATORY, IE_DISPLAY, -1};
static int ie_PROGRESS[] = {IE_BEARER, IE_CAUSE, IE_FACILITY, IE_PROGRESS |
		IE_MANDATORY, IE_DISPLAY, IE_HLC, IE_USER_USER, -1};
static int ie_RELEASE[] = {IE_CAUSE | IE_MANDATORY_1, IE_FACILITY, IE_DISPLAY,
		IE_SIGNAL, IE_USER_USER, -1};
/* a RELEASE_COMPLETE with errors don't require special actions 
static int ie_RELEASE_COMPLETE[] = {IE_CAUSE | IE_MANDATORY_1, IE_FACILITY,
		IE_DISPLAY, IE_SIGNAL, IE_USER_USER, -1};
*/
static int ie_RESUME_ACKNOWLEDGE[] = {IE_CHANNEL_ID| IE_MANDATORY, IE_FACILITY,
		IE_DISPLAY, -1};
static int ie_RESUME_REJECT[] = {IE_CAUSE | IE_MANDATORY, IE_DISPLAY, -1};
static int ie_SETUP[] = {IE_COMPLETE, IE_BEARER  | IE_MANDATORY,
		IE_CHANNEL_ID| IE_MANDATORY, IE_FACILITY, IE_PROGRESS,
		IE_NET_FAC, IE_DISPLAY, IE_KEYPAD, IE_SIGNAL, IE_CALLING_PN,
		IE_CALLING_SUB, IE_CALLED_PN, IE_CALLED_SUB, IE_REDIR_NR,
		IE_LLC, IE_HLC, IE_USER_USER, -1};
static int ie_SETUP_ACKNOWLEDGE[] = {IE_CHANNEL_ID | IE_MANDATORY, IE_FACILITY,
		IE_PROGRESS, IE_DISPLAY, IE_SIGNAL, -1};
static int ie_STATUS[] = {IE_CAUSE | IE_MANDATORY, IE_CALL_STATE |
		IE_MANDATORY, IE_DISPLAY, -1};
static int ie_STATUS_ENQUIRY[] = {IE_DISPLAY, -1};
static int ie_SUSPEND_ACKNOWLEDGE[] = {IE_FACILITY, IE_DISPLAY, -1};
static int ie_SUSPEND_REJECT[] = {IE_CAUSE | IE_MANDATORY, IE_DISPLAY, -1};
/* not used 
 * static int ie_CONGESTION_CONTROL[] = {IE_CONGESTION | IE_MANDATORY,
 *		IE_CAUSE | IE_MANDATORY, IE_DISPLAY, -1};
 * static int ie_USER_INFORMATION[] = {IE_MORE_DATA, IE_USER_USER | IE_MANDATORY, -1};
 * static int ie_RESTART[] = {IE_CHANNEL_ID, IE_DISPLAY, IE_RESTART_IND |
 *		IE_MANDATORY, -1};
 */
static int ie_FACILITY[] = {IE_FACILITY | IE_MANDATORY, IE_DISPLAY, -1};



static void l3ins_release_cmpl(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;

//TRACE();	
	StopAllL3Timer(pc);
	newl3state(pc, 0);
	if (AS_ISDN_l3up(pc, CC_RELEASE_COMPLETE | INDICATION, skb))
		dev_kfree_skb(skb);
	release_l3_process(pc);
}

static void l3ins_alerting(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff *skb = arg;
	int ret;

//TRACE();	
	ret = check_infoelements(pc, skb, ie_ALERTING);
	if (ERR_IE_COMPREHENSION == ret)
	{
		l3ins_std_ie_err(pc, ret);
		dev_kfree_skb(skb);
		return;
	}

	L3DelTimer(&pc->timer);	/* T304 */
	if (pc->t303skb)
	{
		dev_kfree_skb(pc->t303skb);
		pc->t303skb = NULL;
	}
	newl3state(pc, 4);
	if (ret)
		l3ins_std_ie_err(pc, ret);
	if (AS_ISDN_l3up(pc, CC_ALERTING | INDICATION, skb))
		dev_kfree_skb(skb);
}

static void l3ins_call_proc(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret;
	u_char		cause;

//TRACE();	
	if (!(ret = l3ins_get_channel_id(pc, skb)))
	{
		if ((0 == pc->bc) || (3 == pc->bc))
		{
#if AS_ISDN_DEBUG_L3_CTRL
			if (pc->l3->debug & L3_DEB_WARN)
			{
//				l3_debug(pc->l3, "setup answer with wrong chid %x", pc->bc);
				printk(KERN_WARNING "setup answer with wrong chid %x\n", pc->bc);
			}	
#endif			
			l3ins_status_send(pc, CAUSE_INVALID_CONTENTS);
			return;
		}
	}
	else if (1 == pc->state)
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "setup answer wrong chid (ret %d)", ret);
			printk(KERN_WARNING "setup answer wrong chid (ret %d)\n", ret);
		}	
#endif
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3ins_status_send(pc, cause);
		return;
	}
	/* Now we are on none mandatory IEs */
	ret = check_infoelements(pc, skb, ie_CALL_PROCEEDING);
	if (ERR_IE_COMPREHENSION == ret)
	{
		l3ins_std_ie_err(pc, ret);
		dev_kfree_skb(skb);
		return;
	}
	L3DelTimer(&pc->timer);
	if (pc->t303skb)
	{
		dev_kfree_skb(pc->t303skb);
		pc->t303skb = NULL;
	}
	newl3state(pc, 3);
	L3AddTimer(&pc->timer, T310, CC_T310);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3ins_std_ie_err(pc, ret);
	if (AS_ISDN_l3up(pc, CC_PROCEEDING | INDICATION, skb))
		dev_kfree_skb(skb);
}

static void l3ins_connect(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret;

//TRACE();	
	ret = check_infoelements(pc, skb, ie_CONNECT);
	if (ERR_IE_COMPREHENSION == ret)
	{
		l3ins_std_ie_err(pc, ret);
		dev_kfree_skb(skb);
		return;
	}
	L3DelTimer(&pc->timer);	/* T310 */
	if (pc->t303skb)
	{
		dev_kfree_skb(pc->t303skb);
		pc->t303skb = NULL;
	}
	l3ins_message(pc, MT_CONNECT_ACKNOWLEDGE);
	newl3state(pc, 10);
	if (ret)
		l3ins_std_ie_err(pc, ret);
	if (AS_ISDN_l3up(pc, CC_CONNECT | INDICATION, skb))
		dev_kfree_skb(skb);
}

static void l3ins_connect_ack(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret;

// TRACE();	
	ret = check_infoelements(pc, skb, ie_CONNECT_ACKNOWLEDGE);
	if (ERR_IE_COMPREHENSION == ret)
	{
		l3ins_std_ie_err(pc, ret);
		dev_kfree_skb(skb);
		return;
	}
	newl3state(pc, 10);
	L3DelTimer(&pc->timer);
	if (pc->t303skb)
	{
		dev_kfree_skb(pc->t303skb);
		pc->t303skb = NULL;
	}
	if (ret)
		l3ins_std_ie_err(pc, ret);
	if (AS_ISDN_l3up(pc, CC_CONNECT_ACKNOWLEDGE | INDICATION, skb))
		dev_kfree_skb(skb);
}

static void l3ins_disconnect(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret;
	u_char		cause = 0;

//TRACE();	
	StopAllL3Timer(pc);
	if ((ret = l3ins_get_cause(pc, skb)))
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "DISC get_cause ret(%d)", ret);
			printk(KERN_WARNING "DISC get_cause ret(%d)\n", ret);
		}	
#endif		
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
	} 
	ret = check_infoelements(pc, skb, ie_DISCONNECT);
	if (ERR_IE_COMPREHENSION == ret)
		cause = CAUSE_MANDATORY_IE_MISS;
	else if ((!cause) && (ERR_IE_UNRECOGNIZED == ret))
		cause = CAUSE_IE_NOTIMPLEMENTED;
	ret = pc->state;

	if (cause)
		newl3state(pc, 19);
	else
		newl3state(pc, 12);

	if (11 != ret)
	{
		if (AS_ISDN_l3up(pc, CC_DISCONNECT | INDICATION, skb))
		dev_kfree_skb(skb);
	}
	else if (!cause)
	{
		l3ins_release_req(pc, pr, NULL);
		dev_kfree_skb(skb);
	}
	else
		dev_kfree_skb(skb);

	if (cause)
	{
		l3ins_message_cause(pc, MT_RELEASE, cause);
		L3AddTimer(&pc->timer, T308, CC_T308_1);
	}
}

static void l3ins_setup_ack(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret;
	u_char		cause;

//TRACE();	
	if (!(ret = l3ins_get_channel_id(pc, skb)))
	{
		if ((0 == pc->bc) || (3 == pc->bc))
		{
#if AS_ISDN_DEBUG_L3_CTRL
			if (pc->l3->debug & L3_DEB_WARN)
			{
//				l3_debug(pc->l3, "setup answer with wrong chid %x", pc->bc);
				printk(KERN_WARNING "setup answer with wrong chid %x\n", pc->bc);
			}	
#endif
			l3ins_status_send(pc, CAUSE_INVALID_CONTENTS);
			dev_kfree_skb(skb);
			return;
		}
	}
	else
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "setup answer wrong chid (ret %d)", ret);
			printk(KERN_WARNING "setup answer wrong chid (ret %d)\n", ret);
		}	
#endif
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3ins_status_send(pc, cause);
		dev_kfree_skb(skb);
		return;
	}

	/* Now we are on none mandatory IEs */
	ret = check_infoelements(pc, skb, ie_SETUP_ACKNOWLEDGE);
	if (ERR_IE_COMPREHENSION == ret)
	{
		l3ins_std_ie_err(pc, ret);
		dev_kfree_skb(skb);
		return;
	}

	L3DelTimer(&pc->timer);
	if (pc->t303skb)
	{
		dev_kfree_skb(pc->t303skb);
		pc->t303skb = NULL;
	}
	newl3state(pc, 2);
	L3AddTimer(&pc->timer, T304, CC_T304);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3ins_std_ie_err(pc, ret);
	if (AS_ISDN_l3up(pc, CC_SETUP_ACKNOWLEDGE | INDICATION, skb))
		dev_kfree_skb(skb);
}

static void l3ins_setup(l3_process_t *pc, u_char pr, void *arg)
{
	u_char		*p, cause, bc2 = 0;
	int		bcfound = 0;
	struct sk_buff	*skb = arg;
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	int		err = 0;

//TRACE();
	/*
	 * Bearer Capabilities
	 */
	/* only the first occurence 'll be detected ! */
	p = skb->data;
	if (qi->bearer_capability)
	{
		p += L3_EXTRA_SIZE + qi->bearer_capability;
		p++;
		if ((p[0] < 2) || (p[0] > 11))
			err = 1;
		else
		{
			bc2 = p[2] & 0x7f;
			switch (p[1] & 0x7f) {
				case 0x00: /* Speech */
				case 0x10: /* 3.1 Khz audio */
				case 0x08: /* Unrestricted digital information */
				case 0x09: /* Restricted digital information */
				case 0x11:
					/* Unrestr. digital information  with 
					 * tones/announcements ( or 7 kHz audio
					 */
				case 0x18: /* Video */
					break;
				default:
					err = 2;
					break;
			}
			switch (p[2] & 0x7f) {
				case 0x40: /* packed mode */
				case 0x10: /* 64 kbit */
				case 0x11: /* 2*64 kbit */
				case 0x13: /* 384 kbit */
				case 0x15: /* 1536 kbit */
				case 0x17: /* 1920 kbit */
					break;
				default:
					err = 3;
					break;
			}
		}
		
		if (err)
		{
#if AS_ISDN_DEBUG_L3_CTRL
			if (pc->l3->debug & L3_DEB_WARN)
			{
//				l3_debug(pc->l3, "setup with wrong bearer(l=%d:%x,%x)", p[0], p[1], p[2]);
				printk(KERN_WARNING "setup with wrong bearer(l=%d:%x,%x)\n", p[0], p[1], p[2]);
			}	
#endif
			l3ins_msg_without_setup(pc, CAUSE_INVALID_CONTENTS);
			dev_kfree_skb(skb);
			return;
		} 
	}
	else
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "setup without bearer capabilities");
			printk(KERN_WARNING "setup without bearer capabilities\n");
		}	
#endif
		/* ETS 300-104 1.3.3 */
		l3ins_msg_without_setup(pc, CAUSE_MANDATORY_IE_MISS);
		dev_kfree_skb(skb);
		return;
	}
	
	/* Channel Identification */
	if (!(err = l3ins_get_channel_id(pc, skb)))
	{/* Channel ID is right */
		if (pc->bc)
		{
			if ((3 == pc->bc) && (0x10 == bc2))
			{
#if AS_ISDN_DEBUG_L3_CTRL
				if (pc->l3->debug & L3_DEB_WARN)
				{
//					l3_debug(pc->l3, "setup with wrong chid %x",	pc->bc);
					printk(KERN_WARNING "setup with wrong chid %x\n",	pc->bc);
				}	
#endif
				l3ins_msg_without_setup(pc, CAUSE_INVALID_CONTENTS);
				dev_kfree_skb(skb);
				return;
			}
			bcfound++;
		}
		else
		{/* no channel, refer page 94 */
#if AS_ISDN_DEBUG_L3_CTRL
			if (pc->l3->debug & L3_DEB_WARN)
			{
//				l3_debug(pc->l3, "setup without bchannel, call waiting");
				printk(KERN_DEBUG "setup without bchannel, call waiting\n");
			}	
#endif
			bcfound++;
		} 
	}
	else
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "setup with wrong chid ret %d", err);
			printk(KERN_WARNING "setup with wrong chid ret %d\n", err);
		}	
#endif
		if (err == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3ins_msg_without_setup(pc, cause);
		dev_kfree_skb(skb);
		return;
	}
	
	/* Now we are on none mandatory IEs */
	err = check_infoelements(pc, skb, ie_SETUP);
	if (ERR_IE_COMPREHENSION == err)
	{
		l3ins_msg_without_setup(pc, CAUSE_MANDATORY_IE_MISS);
		dev_kfree_skb(skb);
		return;
	}
	newl3state(pc, 6);
	L3DelTimer(&pc->timer);
	L3AddTimer(&pc->timer, T_CTRL, CC_TCTRL);
	if (err) /* STATUS for none mandatory IE errors after actions are taken */
		l3ins_std_ie_err(pc, err);
// already done
//	err = AS_ISDN_l3up(pc, CC_NEW_CR | INDICATION, NULL);
	if (AS_ISDN_l3up(pc, CC_SETUP | INDICATION, skb))
		dev_kfree_skb(skb);
}

static void l3ins_release(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret, cause=0;

//TRACE();	
	StopAllL3Timer(pc);
	if ((ret = l3ins_get_cause(pc, skb)))
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "REL get_cause ret(%d)", ret);
			printk(KERN_WARNING "REL get_cause ret(%d)\n", ret);
		}	
#endif

		if ((ret == -1) && (pc->state != 11))
			cause = CAUSE_MANDATORY_IE_MISS;
		else if (ret != -1)
			cause = CAUSE_INVALID_CONTENTS;
	}
	ret = check_infoelements(pc, skb, ie_RELEASE);
	if (ERR_IE_COMPREHENSION == ret)
		cause = CAUSE_MANDATORY_IE_MISS;
	else if ((ERR_IE_UNRECOGNIZED == ret) && (!cause))
		cause = CAUSE_IE_NOTIMPLEMENTED;
	if (cause)
		l3ins_message_cause(pc, MT_RELEASE_COMPLETE, cause);
	else
		l3ins_message(pc, MT_RELEASE_COMPLETE);
	if (AS_ISDN_l3up(pc, CC_RELEASE | INDICATION, skb))
		dev_kfree_skb(skb);
	newl3state(pc, 0);
	release_l3_process(pc);
}

static void l3ins_progress(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	int		err = 0;
	u_char		*p, cause = CAUSE_INVALID_CONTENTS;

//TRACE();	
	if (qi->progress)
	{
		p = skb->data;
		p += L3_EXTRA_SIZE + qi->progress;
		p++;
		if (p[0] != 2)
		{
			err = 1;
		}
		else if (!(p[1] & 0x70))
		{
			switch (p[1])
			{
				case 0x80:
				case 0x81:
				case 0x82:
				case 0x84:
				case 0x85:
				case 0x87:
				case 0x8a:
					switch (p[2])
					{
						case 0x81:
						case 0x82:
						case 0x83:
						case 0x84:
						case 0x88:
							break;
						default:
							err = 2;
							break;
					}
					break;
				default:
					err = 3;
					break;
			}
		}
	}
	else
	{
		cause = CAUSE_MANDATORY_IE_MISS;
		err = 4;
	}

	if (err)
	{	
#if AS_ISDN_DEBUG_L3_CTRL
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "progress error %d", err);
			printk(KERN_WARNING "progress error %d\n", err);
		}	
#endif
		l3ins_status_send(pc, cause);
		dev_kfree_skb(skb);
		return;
	}

	/* Now we are on none mandatory IEs */
	err = check_infoelements(pc, skb, ie_PROGRESS);
	if (err)
		l3ins_std_ie_err(pc, err);
	if (ERR_IE_COMPREHENSION != err)
	{
		if (AS_ISDN_l3up(pc, CC_PROGRESS | INDICATION, skb))
			dev_kfree_skb(skb);
	}
	else
		dev_kfree_skb(skb);
}

static void l3ins_notify(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	int		err = 0;
	u_char		*p, cause = CAUSE_INVALID_CONTENTS;
                        
//TRACE();	
	if (qi->notify)
	{
		p = skb->data;
		p += L3_EXTRA_SIZE + qi->notify;
		p++;
		if (p[0] != 1)
		{
			err = 1;
#if 0
		} else {
			
			switch (p[1]) {
				case 0x80:
				case 0x81:
				case 0x82:
					break;
				default:
					err = 2;
					break;
			}
#endif
		}
	}
	else
	{
		cause = CAUSE_MANDATORY_IE_MISS;
		err = 3;
	}

	if (err)
	{
#if AS_ISDN_DEBUG_L3_CTRL
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "notify error %d", err);
			printk(KERN_WARNING "notify error %d\n", err);
		}	
#endif
		l3ins_status_send(pc, cause);
		dev_kfree_skb(skb);
		return;
	}
	
	/* Now we are on none mandatory IEs */
	err = check_infoelements(pc, skb, ie_NOTIFY);
	if (err)
		l3ins_std_ie_err(pc, err);

	if (ERR_IE_COMPREHENSION != err)
	{
		if (AS_ISDN_l3up(pc, CC_NOTIFY | INDICATION, skb))
			dev_kfree_skb(skb);
	}
	else
		dev_kfree_skb(skb);
}

static void l3ins_status_enq(l3_process_t *pc, u_char pr, void *arg)
{
	int		ret;
	struct sk_buff	*skb = arg;

	ret = check_infoelements(pc, skb, ie_STATUS_ENQUIRY);
	l3ins_std_ie_err(pc, ret);
	l3ins_status_send(pc, CAUSE_STATUS_RESPONSE);
	if (AS_ISDN_l3up(pc, CC_STATUS_ENQUIRY | INDICATION, skb))
		dev_kfree_skb(skb);
}

static void l3ins_information(l3_process_t *pc, u_char pr, void *arg)
{
	int		ret;
	struct sk_buff	*skb = arg;

//TRACE();	
	ret = check_infoelements(pc, skb, ie_INFORMATION);
	if (ret)
		l3ins_std_ie_err(pc, ret);
	if (pc->state == 25)
	{ /* overlap receiving */
		L3DelTimer(&pc->timer);
		L3AddTimer(&pc->timer, T302, CC_T302);
	}

	if (AS_ISDN_l3up(pc, CC_INFORMATION | INDICATION, skb))
		dev_kfree_skb(skb);
}

static void l3ins_release_ind(l3_process_t *pc, u_char pr, void *arg)
{
	u_char		*p;
	struct sk_buff	*skb = arg;
	int		err, callState = -1;
	Q931_info_t	*qi = (Q931_info_t *)skb->data;

//TRACE();	
	if (qi->call_state)
	{
		p = skb->data;
		p += L3_EXTRA_SIZE + qi->call_state;
		p++;
		if (1 == *p++)
			callState = *p;
	}

	if (callState == 0)
	{
		/* ETS 300-104 7.6.1, 8.6.1, 10.6.1... and 16.1
		 * set down layer 3 without sending any message
		 */
		newl3state(pc, 0);
		err = AS_ISDN_l3up(pc, CC_RELEASE | INDICATION, skb);
		release_l3_process(pc);
	}
	else
	{
		err = AS_ISDN_l3up(pc, CC_RELEASE | INDICATION, skb);
	}

	if (err)
		dev_kfree_skb(skb);
}

static void l3ins_suspend_ack(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret;

//TRACE();	
	L3DelTimer(&pc->timer);
	newl3state(pc, 0);
	/* We don't handle suspend_ack for IE errors now */
	if ((ret = check_infoelements(pc, skb, ie_SUSPEND_ACKNOWLEDGE)))
	{
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "SUSPACK check ie(%d)",ret);
			printk(KERN_WARNING "SUSPACK check ie(%d)\n",ret);
		}	
	}	
	if (AS_ISDN_l3up(pc, CC_SUSPEND_ACKNOWLEDGE | INDICATION, skb))
		dev_kfree_skb(skb);
	release_l3_process(pc);
}

static void l3ins_suspend_rej(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret;
	u_char		cause;

//TRACE();	
	if ((ret = l3ins_get_cause(pc, skb)))
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "SUSP_REJ get_cause err(%d)", ret);
			printk(KERN_WARNING "SUSP_REJ get_cause err(%d)\n", ret);
		}	
#endif		
		if (ret == -1) 
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3ins_status_send(pc, cause);
		dev_kfree_skb(skb);
		return;
	}

	ret = check_infoelements(pc, skb, ie_SUSPEND_REJECT);
	if (ERR_IE_COMPREHENSION == ret)
	{
		l3ins_std_ie_err(pc, ret);
		dev_kfree_skb(skb);
		return;
	}

	L3DelTimer(&pc->timer);
	if (AS_ISDN_l3up(pc, CC_SUSPEND_REJECT | INDICATION, skb))
		dev_kfree_skb(skb);
	newl3state(pc, 10);

	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3ins_std_ie_err(pc, ret);
}

static void l3ins_resume_ack(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret;

//TRACE();	
	if (!(ret = l3ins_get_channel_id(pc, skb)))
	{
		if ((0 == pc->bc) || (3 == pc->bc))
		{
#if AS_ISDN_DEBUG_L3_DATA
			if (pc->l3->debug & L3_DEB_WARN)
			{
//				l3_debug(pc->l3, "resume ack with wrong chid %x", pc->bc);
				printk(KERN_WARNING "resume ack with wrong chid %x\n", pc->bc);
			}	
#endif			
			l3ins_status_send(pc, CAUSE_INVALID_CONTENTS);
			dev_kfree_skb(skb);
			return;
		}
	}
	else if (1 == pc->state)
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "resume ack without chid err(%d)",ret);
			printk(KERN_WARNING "resume ack without chid err(%d)\n",ret);
		}
#endif
		l3ins_status_send(pc, CAUSE_MANDATORY_IE_MISS);
		dev_kfree_skb(skb);
		return;
	}
	
	ret = check_infoelements(pc, skb, ie_RESUME_ACKNOWLEDGE);
	if (ERR_IE_COMPREHENSION == ret)
	{
		l3ins_std_ie_err(pc, ret);
		dev_kfree_skb(skb);
		return;
	}

	L3DelTimer(&pc->timer);
	if (AS_ISDN_l3up(pc, CC_RESUME_ACKNOWLEDGE | INDICATION, skb))
		dev_kfree_skb(skb);
	newl3state(pc, 10);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3ins_std_ie_err(pc, ret);
}

static void l3ins_resume_rej(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	int		ret;
	u_char		cause;

//TRACE();	
	if ((ret = l3ins_get_cause(pc, skb)))
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "RES_REJ get_cause err(%d)", ret);
			printk(KERN_WARNING "RES_REJ get_cause err(%d)\n", ret);
		}	
#endif
		if (ret == -1) 
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3ins_status_send(pc, cause);
		dev_kfree_skb(skb);
		return;
	}

	ret = check_infoelements(pc, skb, ie_RESUME_REJECT);
	if (ERR_IE_COMPREHENSION == ret)
	{
		l3ins_std_ie_err(pc, ret);
		dev_kfree_skb(skb);
		return;
	}

	L3DelTimer(&pc->timer);
	if (AS_ISDN_l3up(pc, CC_RESUME_REJECT | INDICATION, skb))
		dev_kfree_skb(skb);
	newl3state(pc, 0);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3ins_std_ie_err(pc, ret);
	release_l3_process(pc);
}

void l3ins_dummy(l3_process_t *pc, u_char pr, void *arg)
{
}


void l3ins_restart(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	L3DelTimer(&pc->timer);
	AS_ISDN_l3up(pc, CC_RELEASE | INDICATION, NULL);
	release_l3_process(pc);
	if (skb)
		dev_kfree_skb(skb);
}

void l3ins_status(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	int		ret = 0; 
	u_char		*p, cause = 0, callState = 0xff;
	
//TRACE();	
	if ((ret = l3ins_get_cause(pc, skb)))
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "STATUS get_cause ret(%d)", ret);
			printk(KERN_DEBUG "STATUS get_cause ret(%d)\n", ret);
		}	
#endif
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
	}

	if (qi->call_state)
	{
		p = skb->data;
		p += L3_EXTRA_SIZE + qi->call_state;
		p++;
		if (1 == *p++)
		{
			callState = *p;
			if (!ie_in_set(pc, callState, l3_valid_states))
				cause = CAUSE_INVALID_CONTENTS;
		}
		else
			cause = CAUSE_INVALID_CONTENTS;
	}
	else
		cause = CAUSE_MANDATORY_IE_MISS;

	if (!cause)
	{ /*  no error before */
		ret = check_infoelements(pc, skb, ie_STATUS);
		if (ERR_IE_COMPREHENSION == ret)
			cause = CAUSE_MANDATORY_IE_MISS;
		else if (ERR_IE_UNRECOGNIZED == ret)
			cause = CAUSE_IE_NOTIMPLEMENTED;
	}

	if (cause)
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "STATUS error(%d/%d)", ret, cause);
			printk(KERN_WARNING "STATUS error(%d/%d)\n", ret, cause);
		}	
#endif
		l3ins_status_send(pc, cause);
		if (cause != CAUSE_IE_NOTIMPLEMENTED)
		{
			dev_kfree_skb(skb);
			return;
		}
	}
	
	if (qi->cause)
		cause = pc->err & 0x7f;
	if ((cause == CAUSE_PROTOCOL_ERROR) && (callState == 0))
	{
		/* ETS 300-104 7.6.1, 8.6.1, 10.6.1...
		 * if received MT_STATUS with cause == 111 and call
		 * state == 0, then we must set down layer 3
		 */
		newl3state(pc, 0);
		ret = AS_ISDN_l3up(pc, CC_STATUS| INDICATION, skb);
		release_l3_process(pc);
	}
	else
		ret = AS_ISDN_l3up(pc, CC_STATUS | INDICATION, skb);

	if (ret)
		dev_kfree_skb(skb);
}


void l3ins_facility(l3_process_t *pc, u_char pr, void *arg)
{
	struct sk_buff	*skb = arg;
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	int		ret;
	
//TRACE();	
	ret = check_infoelements(pc, skb, ie_FACILITY);
	l3ins_std_ie_err(pc, ret);
	if (!qi->facility)
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (pc->l3->debug & L3_DEB_WARN)
		{
//			l3_debug(pc->l3, "FACILITY without IE_FACILITY");
			printk(KERN_WARNING "FACILITY without IE_FACILITY\n");
		}	
#endif
		dev_kfree_skb(skb);
		return;
	}		

	if (AS_ISDN_l3up(pc, CC_FACILITY | INDICATION, skb))
		dev_kfree_skb(skb);
}
/* pass data/primitive up to upper layer */
static struct stateentry datastatelist[] =
{
	{ALL_STATES,			MT_STATUS_ENQUIRY, 				l3ins_status_enq},
	{ALL_STATES,			MT_FACILITY, 					l3ins_facility},
	{SBIT(19),			MT_STATUS, 						l3ins_release_ind},
	{ALL_STATES,			MT_STATUS, 						l3ins_status},
	{SBIT(0),			MT_SETUP, 						l3ins_setup},
	{SBIT(6) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(11) | SBIT(12) |
	 SBIT(15) | SBIT(17) | SBIT(19) | SBIT(25),	 MT_SETUP, 	l3ins_dummy},
	{SBIT(1) |SBIT(2), 	MT_CALL_PROCEEDING, 			l3ins_call_proc},
	{SBIT(1),			MT_SETUP_ACKNOWLEDGE, 			l3ins_setup_ack},
	{SBIT(2) | SBIT(3),	MT_ALERTING, 					l3ins_alerting},
	{SBIT(2) | SBIT(3),	MT_PROGRESS, 					l3ins_progress},
	{SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) |
	 SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(19) | SBIT(25),
						MT_INFORMATION, 				l3ins_information},
	{ALL_STATES,			MT_NOTIFY, 						l3ins_notify},
	{SBIT(0) | SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(10) |
	 SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(19) | SBIT(25),
						MT_RELEASE_COMPLETE, 			l3ins_release_cmpl},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(25),
	 					MT_RELEASE, 						l3ins_release},
	{SBIT(19),  			MT_RELEASE, 						l3ins_release_ind},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(11) | SBIT(15) | SBIT(17) | SBIT(25),
	 					MT_DISCONNECT, 					l3ins_disconnect},
	{SBIT(19),			MT_DISCONNECT, 					l3ins_dummy},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4),	 MT_CONNECT, 	l3ins_connect},
	{SBIT(8),			MT_CONNECT_ACKNOWLEDGE, 		l3ins_connect_ack},
	{SBIT(15),			MT_SUSPEND_ACKNOWLEDGE, 		l3ins_suspend_ack},
	{SBIT(15),			MT_SUSPEND_REJECT, 				l3ins_suspend_rej},
	{SBIT(17),			MT_RESUME_ACKNOWLEDGE, 		l3ins_resume_ack},
	{SBIT(17),			MT_RESUME_REJECT, 				l3ins_resume_rej},
};

#define DATASLLEN \
	(sizeof(datastatelist) / sizeof(struct stateentry))

/* used in fromdown interface */
void l3ins_data_handler(Q931_info_t *qi, l3_process_t *proc, struct sk_buff *skb)
{
	AS_ISDN_head_t	*hh;
	int i;
	
	hh = AS_ISDN_HEAD_P(skb);

	/* Base in Call State, Prmitive, call up handler defined in datastatelist to pass data up */
	for (i = 0; i < DATASLLEN; i++)
	{
		if ((qi->type == datastatelist[i].primitive) && ((1 << proc->state) & datastatelist[i].state))
		{
			break;
		}
	}
		
	if (i == DATASLLEN)
	{
#if AS_ISDN_DEBUG_L3_DATA
		if (proc->l3->debug & L3_DEB_STATE)
		{
//			l3_debug(proc->l3, "uins_up%sstate %d mt %#x unhandled", (hh->prim == (DL_DATA | INDICATION)) ? " " : "(broadcast) ", proc->state, qi->type);
			printk(KERN_WARNING "uins_up%sstate %d mt %#x unhandled", (hh->prim == (DL_DATA | INDICATION)) ? " " : "(broadcast) ", proc->state, qi->type);
		}
#endif

		if ((MT_RELEASE_COMPLETE != qi->type) && (MT_RELEASE != qi->type))
		{
			l3ins_status_send(proc, CAUSE_NOTCOMPAT_STATE);
		}
		dev_kfree_skb(skb);
	}
	else
	{
#if AS_ISDN_DEBUG_L3_DATA
		TRACE();
		if (proc->l3->debug & L3_DEB_STATE)
		{
//			l3_debug(proc->l3, "uins_up% state %d mt %x", (hh->prim == (DL_DATA | INDICATION)) ? " " : "(broadcast) ", proc->state, qi->type);
			printk(KERN_DEBUG "uins_up% state %d mt %x", (hh->prim == (DL_DATA | INDICATION)) ? " " : "(broadcast) ", proc->state, qi->type);
		}
		TRACE();
#endif		
		datastatelist[i].rout(proc, hh->prim, skb);
	}
}

