/*
 * $Id: l3helper.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * L3 data struct helper functions
 */

#include <linux/module.h>
#include "asISDN.h"
#include "dss1.h"
#include "helper.h"

/* Following data array are used for Q931_info_t and L3 message maintain
* From CAUSE to USERUSER field in Q931_info_t, there are 32 field which
* keep the length of IEs, so Array is based on this condition  	
*/

/* for example, IE(0x04) is in index of 0, IE(0x08) is in index of 1 */
static signed char _AS_ISDN_l3_ie2pos[128] = 
{
			-1,-1,-1,-1, 0,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,
			 2,-1,-1,-1, 3,-1,-1,-1, 4,-1,-1,-1, 5,-1, 6,-1,
			 7,-1,-1,-1,-1,-1,-1, 8, 9,10,-1,-1,11,-1,-1,-1,
			-1,-1,-1,-1,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
			13,-1,14,15,16,17,18,19,-1,-1,-1,-1,20,21,-1,-1,
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,22,23,-1,-1,
			24,25,-1,-1,26,-1,-1,-1,27,28,-1,-1,29,30,31,-1
};

			
static unsigned char _AS_ISDN_l3_pos2ie[32] = 
{
			0x04, 0x08, 0x10, 0x14, 0x18, 0x1c, 0x1e, 0x20,
			0x27, 0x28, 0x29, 0x2c, 0x34, 0x40, 0x42, 0x43,
			0x44, 0x45, 0x46, 0x47, 0x4c, 0x4d, 0x6c, 0x6d,
			0x70, 0x71, 0x74, 0x78, 0x79, 0x7c, 0x7d, 0x7e
};

signed int AS_ISDN_l3_ie2pos(u_char c)
{
	if (c>0x7f) /* bit 8 must be 0 : refer to P.62 */
		return(-1);
	return(_AS_ISDN_l3_ie2pos[c]);
}

unsigned char AS_ISDN_l3_pos2ie(int pos)
{
	return(_AS_ISDN_l3_pos2ie[pos]);
}

void AS_ISDN_initQ931_info(Q931_info_t *qi)
{
	memset(qi, 0, sizeof(Q931_info_t));
};

/* create a skb which hold L3 msg, and allocate extra space for Q931_info_t */
struct sk_buff *
#ifdef AS_ISDN_MEMDEBUG
__mid_alloc_l3msg(int len, u_char msgType, char *fn, int line)
#else
AS_ISDN_alloc_l3msg(int len, u_char msgType)
#endif
{
	struct sk_buff	*skb;
	Q931_info_t	*qi;

#ifdef AS_ISDN_MEMDEBUG
	if (!(skb = __mid_alloc_skb(len + L3_EXTRA_SIZE +1, GFP_ATOMIC, fn, line)))
#else
	if (!(skb = alloc_skb(len + L3_EXTRA_SIZE +1, GFP_ATOMIC)))
#endif
 	{		
 		printk(KERN_WARNING "AS_ISDN: No skb for L3\n");
		return (NULL);
	}

	qi = (Q931_info_t *)skb_put(skb, L3_EXTRA_SIZE +1);
	AS_ISDN_initQ931_info(qi);
	
	qi->type = msgType;
	return (skb);
}

/*add variable IE into skb(L3 message) */
void AS_ISDN_AddvarIE(struct sk_buff *skb, u_char *ie)
{
	u_char	*p, *ps;
	u16	*ies;
	int	l;
	Q931_info_t *qi = (Q931_info_t *)skb->data;

	ies = &qi->bearer_capability;
	ps = (u_char *) qi;
	ps += L3_EXTRA_SIZE;
	if (*ie & 0x80)
	{ /* one octet IE */
		if (*ie == IE_MORE_DATA)
			ies = &qi->more_data;
		else if (*ie == IE_COMPLETE)
			ies = &qi->sending_complete;
		else if ((*ie & 0xf0) == IE_CONGESTION)
			ies = &qi->congestion_level;
		else
		{
			int_error();
			return;
		}
		l = 1;
	}
	else
	{
		if (_AS_ISDN_l3_ie2pos[*ie]<0)
		{
			int_error();
			return;
		}
		ies += _AS_ISDN_l3_ie2pos[*ie];
		l = ie[1] + 2;
	}
	
	p = skb_put(skb, l);	/* allocate extra space for keep this IE */
	*ies = (u16)(p - ps);
	memcpy(p, ie, l);
}

void AS_ISDN_AddIE(struct sk_buff *skb, u_char ie, u_char *iep)
{
	u_char	*p, *ps;
	u16	*ies;
	int	l;
	Q931_info_t *qi = (Q931_info_t *)skb->data;

	if (ie & 0x80)
	{ /* one octett IE */
		if (ie == IE_MORE_DATA)
			ies = &qi->more_data;
		else if (ie == IE_COMPLETE)
			ies = &qi->sending_complete;
		else if ((ie & 0xf0) == IE_CONGESTION)
			ies = &qi->congestion_level;
		else
		{
			int_error();
			return;
		}
		l = 0;
	}
	else
	{
		if (!iep || !iep[0])
			return;
		
		ies = &qi->bearer_capability;
		if (_AS_ISDN_l3_ie2pos[ie]<0)
		{
			int_error();
			return;
		}
		ies += _AS_ISDN_l3_ie2pos[ie];
		l = iep[0] + 1;
	}
	
	ps = (u_char *) qi;
	ps += L3_EXTRA_SIZE;
	p = skb_put(skb, l+1);
	*ies = (u16)(p - ps);
	*p++ = ie;
	if (l)
		memcpy(p, iep, l);
}

#if 0
void AS_ISDN_LogL3Msg(struct sk_buff *skb)
{
	u_char		*p,*ps, *t, tmp[32];
	u16		*ies;
	int		i,j;
	Q931_info_t	*qi = (Q931_info_t *)skb->data;
	as_isdn_head_t	*hh;

	hh = AS_ISDN_HEAD_P(skb);

#if AS_ISDN_DEBUG_L3_DATA
	printk(KERN_ERR  "L3Msg prim(%x) id(%x) len(%d)\n", hh->prim, hh->dinfo, skb->len);
#endif
	if (skb->len < sizeof(Q931_info_t))
		return;

	ies = &qi->bearer_capability;
	ps = (u_char *) qi;
	ps += L3_EXTRA_SIZE;
#if AS_ISDN_DEBUG_L3_DATA
	printk(KERN_ERR  "L3Msg type(%02x) qi(%p) ies(%p) ps(%p)\n", qi->type, qi, ies, ps);
#endif

	for (i=0;i<32;i++)
	{
		if (ies[i])
		{
			p = ps + ies[i];
			t = tmp;
			*t = 0;
			for (j=0; j<p[1]; j++)
			{
				if (j>9)
				{
					sprintf(t, " ...");
				}
				t += sprintf(t, " %02x", p[j+2]);
			}
			printk(KERN_ERR  "L3Msg ies[%d] off(%d) ie(%02x/%02x) len(%d) %s\n", i, ies[i], _AS_ISDN_l3_pos2ie[i], *p, p[1], tmp);
		}
	}
}
#endif

EXPORT_SYMBOL(AS_ISDN_l3_pos2ie);
EXPORT_SYMBOL(AS_ISDN_l3_ie2pos);
EXPORT_SYMBOL(AS_ISDN_initQ931_info);
#ifdef AS_ISDN_MEMDEBUG
EXPORT_SYMBOL(__mid_alloc_l3msg);
#else
EXPORT_SYMBOL(AS_ISDN_alloc_l3msg);
#endif
EXPORT_SYMBOL(AS_ISDN_AddvarIE);
EXPORT_SYMBOL(AS_ISDN_AddIE);
#if 0
EXPORT_SYMBOL(AS_ISDN_LogL3Msg);
#endif

