/*
* $Id: ashfc_ioctl.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
*/

#include <linux/delay.h>

#include  "as_isdn_ctrl.h"
#include "dchannel.h"
#include "bchannel.h"
#include "layer1.h"
#include "helper.h"

#include "hw_lock.h"
#include "ashfc.h"

static int _ashfc_bch_activate( bchannel_t *bch )
{
	int ret =0;
	ashfc_t *hc = bch->private;
	/* activate B-channel if not already activated */

#if WITH_ASHFC_DEBUG_DATA
	if (ashfc_debug & ASHFC_DEBUG_MSG)
		printk(KERN_ERR  "%s : RAWDEV ACTIVATE ch %d (minor=%d)\n", hc->name, bch->channel, bch->dev->minor );
#endif
//	TRACE();

		test_and_set_bit(FLG_AS_ISDNPORT_ENABLED, &bch->dev->wport.Flag);
	if (test_and_set_bit(BC_FLG_ACTIV, &bch->Flag))
		ret = 0;
	else
	{
//		bch->inst.lock(hc, 0);
		ret = ashfc_chan_mode(hc, bch->channel, ISDN_PID_L1_B_64TRANS, hc->chan[bch->channel].slot_tx, hc->chan[bch->channel].bank_tx, hc->chan[bch->channel].slot_rx, hc->chan[bch->channel].bank_rx);
		if (!ret)
		{/* success, then startup TX */
			bch->protocol = ISDN_PID_L1_B_64TRANS;
			if (bch->protocol)/* start TX */
				bch_sched_event(bch, B_XMTBUFREADY);
#if 0			
			if (bch->protocol==ISDN_PID_L1_B_64TRANS && !hc->dtmf)
			{
				/* start decoder */
				hc->dtmf = 1;

#if WITH_ASHFC_DEBUG_CTRL
				if (ashfc_debug & ASHFC_DEBUG_DTMF)
					printk(KERN_ERR  "%s : L2L1-LINK start dtmf decoder\n", hc->name);
#endif
				HFC_outb(hc, R_DTMF, hc->hw.r_dtmf | V_RST_DTMF);
			}
#endif			
		}
//		bch->inst.unlock(hc);
	}

	return 0;
}

static int _ashfc_bch_deactivate( bchannel_t *bch )
{
#if WITH_ASHFC_DEBUG_DATA
	ashfc_t *hc = bch->private;
	if (ashfc_debug & ASHFC_DEBUG_MSG)
		printk(KERN_ERR  "%s : RAWDEV DEACTIVATE ch %d (minor %d)\n", hc->name, bch->channel, bch->dev->minor );
#endif
	test_and_set_bit(BC_FLG_WAIT_DEACTIVATE, &bch->Flag);
	test_and_clear_bit(FLG_AS_ISDNPORT_ENABLED, &bch->dev->wport.Flag);
	return 0;
}

int  ashfc_bch_ioctl(bchannel_t *bch, unsigned int cmd, unsigned long data)
{
//	TRACE();
	switch(cmd) 
	{
		case AS_ISDN_CTL_ACTIVATE:
			return _ashfc_bch_activate( bch);
			break;
#if 0
			get_user(j, (int *)data);
			if (j) 
			{
				spin_lock_irqsave(&chan->lock, flags);
				chan->flags |= AS_CHAN_FLAG_AUDIO;
				spin_unlock_irqrestore(&chan->lock, flags);
			} 
			else 
			{
			/* Coming out of audio mode, also clear all 
			   conferencing and gain related info as well
			   as echo canceller */
				spin_lock_irqsave(&chan->lock, flags);
				chan->flags &= ~AS_CHAN_FLAG_AUDIO;
						
				if (chan->gainalloc && chan->rxgain)
					rxgain = chan->rxgain;
				else
					rxgain = NULL;

				as_channel_set_default_gain( chan);
				chan->gainalloc = 0;
				spin_unlock_irqrestore(&chan->lock, flags);

				if (rxgain)
					kfree(rxgain);
			}
#endif			
		case AS_ISDN_CTL_DEACTIVATE:
			return _ashfc_bch_deactivate( bch);
			break;
		
		default:
//			return as_chanandpseudo_ioctl(inode, file, cmd, data, unit);
			printk(KERN_ERR "Not support IOCTL command\n");
	}

	return 0;
}

