/*
 * $Id: bchannel.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include <linux/module.h>
#include <linux/asISDNif.h>
#include "layer1.h"
#include "bchannel.h"
#include "helper.h"


/* responsible for the data flow from ISR to upper protocol, layer1 or layer3(L2_TRANS) 
* BH is scheduled in ISR explicitely 
*/
static void bchannel_bh(bchannel_t *bch)
{
	struct sk_buff	*skb;
	u_int 		pr;
	int		ret;
	AS_ISDN_head_t	*hh;
	AS_ISDNif_t	*hif;

//	TRACE();
	if (!bch)
		return;
//	if (!bch->inst.up.func)		/* comments 2005.12.21, lizhijie*/
	if ((!bch->dev) )
	{
#if AS_ISDN_DEBUG_CORE_BCHAN
		printk(KERN_WARNING "%s: without up.func\n", __FUNCTION__);
#endif
		return;
	}
//	TRACE();
#if 0
	printk(KERN_ERR  "%s: event %x\n", __FUNCTION__, bch->event);
	if (bch->dev)
		printk(KERN_ERR  "%s: rpflg(%x) wpflg(%x)\n", __FUNCTION__,
			bch->dev->rport.Flag, bch->dev->wport.Flag);
#endif
	/* only for the skb transfer in bch->next_skb */
	if (test_and_clear_bit(B_XMTBUFREADY, &bch->event))
	{/* tx ready : confirm to our request(which is TX), eg. this is confirm to our request just send out */
	/* no skb is provided in PH_DATA_CONFIRM */
//	TRACE();
#if 0
		/* commmeted, lizhijie,2005.12.21 */
		skb = bch->next_skb;
		if (skb)
		{
			hh = AS_ISDN_HEAD_P(skb);
			bch->next_skb = NULL;
			
//	TRACE();
			if (bch->inst.pid.protocol[2] == ISDN_PID_L2_B_TRANS)
				/* data come as DL_DATA provide to L3 */
				pr = DL_DATA | CONFIRM;/* only in l2 when L2_B_TRANS */
			else
				pr = PH_DATA | CONFIRM;
			
			if ((bch->inst.pid.protocol[2] == ISDN_PID_L2_B_RAWDEV) && bch->dev)
			{
				/* provide to user process directly ??? */
				hif = &bch->dev->rport.pif;
			}
			else
			{
				hif = &bch->inst.up;
			}

			if (if_newhead(hif, pr, hh->dinfo, skb))
				dev_kfree_skb(skb);
		}
#endif
		hif = &bch->dev->rport.pif;
		/* skb has been freed when TX */
		as_isdn_rdata_raw_data( hif, PH_DATA|CONFIRM, NULL);
	}
//	TRACE();
	
	if (test_and_clear_bit(B_RCVBUFREADY, &bch->event))
	{/* rx ready : received a request */
//	TRACE();
#if 0
		if ((bch->inst.pid.protocol[2] == ISDN_PID_L2_B_RAWDEV) && bch->dev)
		{
			/* data come as DL_DATA provide to L3, no L1 logic is need */
			hif = &bch->dev->rport.pif;/* only in l2 when L2_B_TRANS */
		}	
		else
		{
			hif = &bch->inst.up;
		}
#endif
		hif = &bch->dev->rport.pif;/* only in l2 when L2_B_TRANS */

//	TRACE();
		while ((skb = skb_dequeue(&bch->rqueue)))
		{
			if (bch->inst.pid.protocol[2] == ISDN_PID_L2_B_TRANS)
				/* provide to user process directly ??? */
				pr = DL_DATA | INDICATION;
			else
				pr = PH_DATA | INDICATION;

//			printk(KERN_ERR "bch-rqueue length is %d\n", skb_queue_len(&bch->rqueue) );

			/*raw device : function pointer is assigned in udevice->get_free_rawdevice(void) */
//			ret = if_newhead(hif, pr, AS_ISDN_ID_ANY, skb);
			as_isdn_rdata_raw_data(hif, pr, skb );
#if 0
//			if (ret < 0)
			{
#if AS_ISDN_DEBUG_CORE_BCHAN
				printk(KERN_WARNING "%s: deliver err %d\n",	__FUNCTION__, ret);
#endif
				dev_kfree_skb(skb);	
			}
#endif

		}
	}

//	TRACE();
#if 0
	/* handle DTMF skb, when DTMF skb is passed, PH_CONTROL|IND has been added 
	* but in this function, a new header of PH_DATA|IND is added, so we get DTMF coeffiecents as audio data
	*/
	if (bch->hw_bh)
		bch->hw_bh(bch);
#endif
//	TRACE();
}

int as_isdn_init_bch(bchannel_t *bch)
{
	int	devtyp = AS_ISDN_RAW_DEVICE;

	if (!(bch->blog = kmalloc(MAX_BLOG_SPACE, GFP_ATOMIC)))
	{
		printk(KERN_WARNING	"AS_ISDN: No memory for blog\n");
		return(-ENOMEM);
	}
	if (!(bch->rx_buf = kmalloc(MAX_DATA_MEM, GFP_ATOMIC)))
	{
		printk(KERN_WARNING	"AS_ISDN: No memory for bchannel rx_buf\n");
		kfree(bch->blog);
		bch->blog = NULL;
		return (-ENOMEM);
	}
	if (!(bch->tx_buf = kmalloc(MAX_DATA_MEM, GFP_ATOMIC)))
	{
		printk(KERN_WARNING "AS_ISDN: No memory for bchannel tx_buf\n");
		kfree(bch->blog);
		bch->blog = NULL;
		kfree(bch->rx_buf);
		bch->rx_buf = NULL;
		return (-ENOMEM);
	}
	skb_queue_head_init(&bch->rqueue);
	bch->next_skb = NULL;
	bch->Flag = 0;
	bch->event = 0;
	bch->rx_idx = 0;
	bch->tx_len = 0;
	bch->tx_idx = 0;
	
	/* init bottom-half */
//	INIT_WORK(&bch->work, (void *)(void *)bchannel_bh, bch);
	INIT_TQUEUE(&bch->work, (void *)(void *)bchannel_bh, bch);
	bch->hw_bh = NULL;
	if (!bch->dev)
	{
		if (bch->inst.obj->ctrl(&bch->dev, MGR_GETDEVICE | REQUEST,	&devtyp))
		{
			printk(KERN_WARNING "AS_ISDN: no raw device for bchannel\n");
		}
	}
	return(0);
}

int as_isdn_free_bch(bchannel_t *bch)
{
#ifdef HAS_WORKQUEUE
	if (bch->work.pending)
		printk(KERN_ERR "as_isdn_free_bch work:(%lx)\n", bch->work.pending);
#else
	if (bch->work.sync)
		printk(KERN_ERR "as_isdn_free_bch work:(%lx)\n", bch->work.sync);
#endif
	discard_queue(&bch->rqueue);
	if (bch->blog)
	{
		kfree(bch->blog);
		bch->blog = NULL;
	}
	if (bch->rx_buf)
	{
		kfree(bch->rx_buf);
		bch->rx_buf = NULL;
	}

	if (bch->tx_buf)
	{
		kfree(bch->tx_buf);
		bch->tx_buf = NULL;
	}
	if (bch->next_skb)
	{
		dev_kfree_skb(bch->next_skb);
		bch->next_skb = NULL;
	}

	if (bch->inst.obj->ctrl(bch->dev, MGR_DELDEVICE | REQUEST, NULL))
	{
		printk(KERN_WARNING "AS_ISDN: del raw device error\n");
	} 
	else
		bch->dev = NULL;
	return(0);
}

EXPORT_SYMBOL(as_isdn_init_bch);
EXPORT_SYMBOL(as_isdn_free_bch);
