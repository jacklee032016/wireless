/*
 * $Id: bchannel.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include <linux/module.h>
#include "asISDN.h"
#include "layer1.h"
#include "bchannel.h"
#include "helper.h"


/* this function is used as bch->dev->rport->pif which is called in BCH->bh handler 
* This function is added for accelebrate the data read and write loop execute both
* in BH context and write thread context, lizhijie, 2005.12.21
*/
int as_rawdev_bh_do_read_data(bchannel_t *bch,  int prim, struct sk_buff *skb)
{
	as_isdn_rawdev_t		*dev;
	u_long				flags;
	int					retval = 0;
	struct sk_buff 		*header;

	if (!bch || !bch->dev )//||  !skb)
		return(-EINVAL);
	dev = bch->dev;

//	printk(KERN_WARNING "%s : device with minor=%d\n", __FUNCTION__, dev->minor );
	if ( prim == (PH_DATA | INDICATION))
	{/* case 1: Rx Data from low layer
	  put skb into dev->rport.queue, wakeup process wait on dev->read */
		if (test_bit(FLG_AS_ISDNPORT_OPEN, &dev->rport.Flag))
		{
			spin_lock_irqsave(&dev->rport.lock, flags);
			if (skb_queue_len(&dev->rport.queue) >= dev->rport.maxqlen)
			{
				printk(KERN_WARNING "%s : queue in rport of device %d is fulled\n", __FUNCTION__ , dev->minor);
				header = skb_dequeue(&dev->rport.queue);
				dev_kfree_skb( header);/*added 09.16 */
//				retval = -ENOSPC;
			}
//			else
			skb_queue_tail(&dev->rport.queue, skb);
			
//	TRACE();
			spin_unlock_irqrestore(&dev->rport.lock, flags);
			wake_up_interruptible(&dev->rport.procq);
		}
		else
		{
			printk(KERN_WARNING "%s: PH_DATA_IND device(%d) not open for read\n", __FUNCTION__, dev->minor);
			dev_kfree_skb(skb);/*added 09.16 */
			retval = -ENOENT;
		}
	}
	else //if ( prim == (PH_DATA | CONFIRM))
	{/* */
//		test_and_clear_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag);
//	printk(KERN_WARNING "%s : device with minor=%d\n", __FUNCTION__, dev->minor );

		if (test_bit(FLG_AS_ISDNPORT_OPEN, &dev->wport.Flag))
		{
//			TRACE();
			spin_lock_irqsave(&dev->wport.lock, flags);
			
			if((skb = skb_dequeue(&dev->wport.queue)))
			{
//				TRACE();
#if AS_ISDN_DEBUG_CORE_DATA
				if (device_debug & DEBUG_DEV_OP)
					printk(KERN_ERR  "%s: write port %d flag(%lx)\n", __FUNCTION__,dev->minor, dev->wport.Flag);
#endif
				spin_unlock_irqrestore(&dev->wport.lock, flags);
//			TRACE();
				retval = if_newhead(&dev->wport.pif, PH_DATA | REQUEST, (int)skb, skb);
//				spin_lock_irqsave(&dev->wport.lock, flags);
				if (retval)
				{
					printk(KERN_WARNING "%s: dev(%d) down err(%d)\n", __FUNCTION__, dev->minor, retval);
					dev_kfree_skb(skb);
				}

	//			wake_up(&dev->wport.procq);
				wake_up_interruptible(&dev->wport.procq);
	//	TRACE();
			}
		
		}
		else
		{
			printk(KERN_WARNING "%s: PH_DATA_CNF device(%d) not open for Write\n", __FUNCTION__, dev->minor);
			retval = -ENOENT;
		}
#if 0

		spin_lock_irqsave(&dev->wport.lock, flags);
#if 0
		if (test_and_set_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag))
		{
			spin_unlock_irqrestore(&dev->wport.lock, flags);
			return(0);
		}
#endif		
		while ((skb = skb_dequeue(&dev->wport.queue)))
		{
#if AS_ISDN_DEBUG_CORE_DATA
			if (device_debug & DEBUG_DEV_OP)
				printk(KERN_ERR  "%s: writ port flag(%lx)\n", __FUNCTION__, dev->wport.Flag);
#endif


			if (test_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag))
			{
				skb_queue_head(&dev->wport.queue, skb); 
				wake_up_interruptible(&dev->wport.procq);	/*added, lizhijie,2005.12.21 */
				break;
			}

			if (test_bit(FLG_AS_ISDNPORT_ENABLED, &dev->wport.Flag))
			{
				spin_unlock_irqrestore(&dev->wport.lock, flags);
				retval = if_newhead(&dev->wport.pif, PH_DATA | REQUEST, (int)skb, skb);
				spin_lock_irqsave(&dev->wport.lock, flags);
				if (retval)
				{
					printk(KERN_WARNING "%s: dev(%d) down err(%d)\n", __FUNCTION__, dev->minor, retval);
					dev_kfree_skb(skb);
				}
				else
				{
					test_and_set_bit(FLG_AS_ISDNPORT_BLOCK, &dev->wport.Flag);
//					wake_up(&dev->wport.procq);
					wake_up_interruptible(&dev->wport.procq);
					break;
				}
			}
			else
			{
				printk(KERN_WARNING "%s: dev(%d) wport not enabled\n", __FUNCTION__, dev->minor);
				dev_kfree_skb(skb);
			}
//			wake_up(&dev->wport.procq);
			wake_up_interruptible(&dev->wport.procq);
//	TRACE();
		}
		
		test_and_clear_bit(FLG_AS_ISDNPORT_BUSY, &dev->wport.Flag);
		spin_unlock_irqrestore(&dev->wport.lock, flags);
#endif

		
	}
#if 0
	else
	{
		printk(KERN_WARNING "%s: prim(%x) not supported in BChannel BH device %d\n", __FUNCTION__, prim, dev->minor);
		retval = -EINVAL;
	}
#endif

//	TRACE();
	return(retval);
}


/* responsible for the data flow from ISR to upper protocol, layer1 or layer3(L2_TRANS) 
* BH is scheduled in ISR explicitely 
*/
static void bchannel_bh(bchannel_t *bch)
{
	struct sk_buff	*skb;

//	TRACE();
	if (!bch)
		return;
	if ((!bch->dev) )
	{
#if AS_ISDN_DEBUG_CORE_BCHAN
		printk(KERN_WARNING "%s: without up.func\n", __FUNCTION__);
#endif
		return;
	}
#if 0
//	printk(KERN_WARNING "bchannel %d (device %d) BH\n",bch->channel, bch->dev->minor);
	if (bch->dev)
		printk(KERN_ERR  "%s: rpflg(%x) wpflg(%x)\n", __FUNCTION__,
			bch->dev->rport.Flag, bch->dev->wport.Flag);
#endif

	if (test_and_clear_bit(B_XMTBUFREADY, &bch->event))
	{/*  */
//	TRACE();
		/* skb has been freed when TX */
		as_rawdev_bh_do_read_data( bch, PH_DATA|CONFIRM, NULL);
	}
	
	if (test_and_clear_bit(B_RCVBUFREADY, &bch->event))
	{/* rx ready : received a request */
//	TRACE();
		while ((skb = skb_dequeue(&bch->rqueue)))
		{
			as_rawdev_bh_do_read_data(bch, PH_DATA | INDICATION, skb );
		}
	}

	/* handle DTMF skb, when DTMF skb is passed, PH_CONTROL|IND has been added 
	* but in this function, a new header of PH_DATA|IND is added, so we get DTMF coeffiecents as audio data
	*/
	if (bch->hw_bh)
		bch->hw_bh(bch);

}

int as_isdn_init_bch(bchannel_t *bch)
{
//	int	devtyp = AS_ISDN_RAW_DEVICE;

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
//	bch->next_skb = NULL;
	bch->Flag = 0;
	bch->event = 0;
	bch->rx_idx = 0;
	bch->tx_len = 0;
	bch->tx_idx = 0;
	
	/* init bottom-half */
//	INIT_WORK(&bch->work, (void *)(void *)bchannel_bh, bch);
	INIT_TQUEUE(&bch->work, (void *)(void *)bchannel_bh, bch);
	
	bch->hw_bh = NULL;
	
#if 0
	if (!bch->dev)
	{
		if (bch->inst.obj->ctrl(&bch->dev, MGR_GETDEVICE | REQUEST,	&devtyp))
		{
			printk(KERN_WARNING "AS_ISDN: no raw device for bchannel\n");
		}
	}
#endif

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
#if 0	
	if (bch->next_skb)
	{
		dev_kfree_skb(bch->next_skb);
		bch->next_skb = NULL;
	}
#endif

#if 0
	if (bch->inst.obj->ctrl(bch->dev, MGR_DELDEVICE | REQUEST, NULL))
	{
		printk(KERN_WARNING "AS_ISDN: del raw device error\n");
	} 
	else
#endif

//	bch->dev = NULL;
	return(0);
}

EXPORT_SYMBOL(as_isdn_init_bch);
EXPORT_SYMBOL(as_isdn_free_bch);
