/*
 * $Id: l3_utils.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include "l3.h"

#if 0
void l3_debug(layer3_t *l3, char *fmt, ...)
{
	logdata_t log;

	va_start(log.args, fmt);
	log.fmt = fmt;
	log.head = l3->inst.name;
	l3->inst.obj->ctrl(&l3->inst, MGR_DEBUGDATA | REQUEST, &log);
	va_end(log.args);
}
#endif

/* refer to Specs, vol.3. P.57, Figure4.1 
Param p: msg pinter */
u_char *findie(u_char * p, int size, u_char ie, int wanted_set/* codeset */)
{
	int l, codeset, maincodeset;
	u_char *pend = p + size;

//TRACE();	
	/* skip protocol discriminator, callref and message type */
	p++;			/* Skip Protocol Discriminator */
	l = (*p++) & 0xf;	/* length of callRef*/
	p += l;			/* Skip CallRef length */
	p++;			/* Skip MessageType */
	codeset = 0;
	maincodeset = 0;
	/* while there are bytes left... */
	while (p < pend)
	{
		if ((*p & 0xf0) == 0x90)
		{
			codeset = *p & 0x07;
			if (!(*p & 0x08))
				maincodeset = codeset;
		}

		if (codeset == wanted_set)
		{
			if (*p == ie)
			{
				/* improved length check (Werner Cornelius) */
				if (!(*p & 0x80))
				{
					if ((pend - p) < 2)
						return(NULL);
					if (*(p+1) > (pend - (p+2)))
						return(NULL);
					p++; /* points to len */
				}
				return (p);
			}
			else if ((*p > ie) && !(*p & 0x80))
				return (NULL);
		}

		if (!(*p & 0x80))
		{
			p++;
			l = *p;
			p += l;
			codeset = maincodeset;
		}
		p++;
	}
	return (NULL);
}

/* get CallRef from L3 message */
int getcallref(u_char * p)
{
	int l, cr = 0;

//TRACE();	
	p++;			/* protocol discriminator */

	if (*p & 0xfe)		/* wrong callref BRI only 1 octet*/
		return(-2);

	l = 0xf & *p++;		/* callref length */

	if (!l)			/* dummy CallRef */
		return(-1);

	cr = *p++;
	return (cr);
}

int newcallref(layer3_t *l3)
{
	int max = 127;

//TRACE();	
	if (test_bit(FLG_CRLEN2, &l3->Flag))
		max = 32767;

	if (l3->OrigCallRef >= max)
		l3->OrigCallRef = 1;
	else
		l3->OrigCallRef++;
	
	return (l3->OrigCallRef);
}

void newl3state(l3_process_t *pc, int state)
{
//TRACE();	
#if AS_ISDN_DEBUG_L3_DATA
	if (pc->l3->debug & L3_DEB_STATE && pc->l3->l3m.printdebug)
	{
//		(pc->l3->l3m.printdebug)(&pc->l3->l3m, "newstate cr %d %d --> %d", pc->callref & 0x7F, pc->state, state);
		printk(KERN_DEBUG "newstate cr %d %d --> %d", pc->callref & 0x7F, pc->state, state);
	}	
#endif
	pc->state = state;
}

static void L3ExpireTimer(L3Timer_t *t)
{
	u_long		flags;

//TRACE();	
	spin_lock_irqsave(&t->pc->l3->lock, flags);
	t->pc->l3->p_mgr(t->pc, t->event, NULL);
	spin_unlock_irqrestore(&t->pc->l3->lock, flags);
}

void L3InitTimer(l3_process_t *pc, L3Timer_t *t)
{
//TRACE();	
	t->pc = pc;
	t->tl.function = (void *) L3ExpireTimer;
	t->tl.data = (long) t;
	init_timer(&t->tl);
}

void L3DelTimer(L3Timer_t *t)
{
//TRACE();	
	del_timer(&t->tl);
}

int L3AddTimer(L3Timer_t *t,  int millisec, int event)
{
//TRACE();	
	if (timer_pending(&t->tl))
	{
		printk(KERN_WARNING "L3AddTimer: timer already active!\n");
		return -1;
	}
	init_timer(&t->tl);
	t->event = event;
	t->tl.expires = jiffies + (millisec * HZ) / 1000;
	add_timer(&t->tl);
	return 0;
}

void StopAllL3Timer(l3_process_t *pc)
{
//TRACE();	
	L3DelTimer(&pc->timer);
	if (pc->t303skb)
	{
		dev_kfree_skb(pc->t303skb);
		pc->t303skb = NULL;
	}
}

/*
static void no_l3_proto(struct PStack *st, int pr, void *arg)
{
	struct sk_buff *skb = arg;

	AS_ISDN_putstatus(st->l1.hardware, "L3", "no D protocol");
	if (skb) {
		dev_kfree_skb(skb);
	}
}

static int no_l3_proto_spec(struct PStack *st, isdn_ctrl *ic)
{
	printk(KERN_WARNING "AS_ISDN: no specific protocol handler for proto %lu\n",ic->arg & 0xFF);
	return(-1);
}
*/

/* get l3_process_t with callref */
l3_process_t *getl3proc(layer3_t *l3, int cr)
{
	l3_process_t *p;
	
//TRACE();	
	list_for_each_entry(p, &l3->plist, list)
		if (p->callref == cr)
			return (p);
	return (NULL);
}

/* get l3_process_t with Id(address) from l3 management entity  */
l3_process_t *getl3proc4id(layer3_t *l3, u_int id)
{
	l3_process_t *p;

//TRACE();	
	list_for_each_entry(p, &l3->plist, list)
		if (p->id == id)
			return (p);
	return (NULL);
}

l3_process_t *new_l3_process(layer3_t *l3, int callref, int n303, u_int id)
{
	l3_process_t	*p = NULL;

//TRACE();	
	if (id == AS_ISDN_ID_ANY)
	{
		if (l3->entity == AS_ISDN_ENTITY_NONE)
		{
			printk(KERN_WARNING "%s: no entity allocated for l3(%x)\n",	__FUNCTION__, l3->id);
			return (NULL);
		}
		
		if (l3->pid_cnt == 0x7FFF)
			l3->pid_cnt = 0;

		while(l3->pid_cnt <= 0x7FFF)
		{
			l3->pid_cnt++;
			id = l3->pid_cnt | (l3->entity << 16);
			p = getl3proc4id(l3, id);
			if (!p)
				break;
		}

		if (p)
		{
			printk(KERN_WARNING "%s: no free process_id for l3(%x) entity(%x)\n", __FUNCTION__, l3->id, l3->entity);
			return (NULL);
		}
	}
	else
	{
		/* l3_process_t has been exist for this ID: id from other entity */
		p = getl3proc4id(l3, id);
		if (p)
		{
			printk(KERN_WARNING "%s: process_id(%x) allready in use in l3(%x)\n", __FUNCTION__, id, l3->id);
			return (NULL);
		}
	}
	
	if (!(p = kmalloc(sizeof(l3_process_t), GFP_ATOMIC)))
	{
		printk(KERN_ERR "AS_ISDN can't get memory for cr %d\n", callref);
		return (NULL);
	}
	memset(p, 0, sizeof(l3_process_t));
	p->l3 = l3;
	p->id = id;
	p->callref = callref;
	p->n303 = n303;
	L3InitTimer(p, &p->timer);
	list_add_tail(&p->list, &l3->plist);
	return (p);
};

/* up interface function */
int AS_ISDN_l3up(l3_process_t *l3p, u_int prim, struct sk_buff *skb)
{
	layer3_t *l3;
	int err = -EINVAL;

	if (!l3p)
		return(-EINVAL);

//TRACE();	
	l3 = l3p->l3;
	if (!skb)
	{
//		TRACE();	
		err = if_link(&l3->inst.up, prim, l3p->id, 0, NULL, 0);
	}
	else
	{
//TRACE();	
		err = if_newhead(&l3->inst.up, prim, l3p->id, skb);
	}
	return(err);
}


