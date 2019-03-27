/*
 * $Id: ashfc_bh.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * Bottom Half for assist HFC-4S chip drivers
 * Li Zhijie, 2005.08.12
 */
#include <linux/delay.h>

#include "dchannel.h"
#include "bchannel.h"
#include "layer1.h"

#include "helper.h"

#if 0
#define SPIN_DEBUG
#define LOCK_STATISTIC
#endif
#include "hw_lock.h"
#include "ashfc.h"

/* number of POLL_TIMER interrupts for G2 timeout (min 120ms) */
static int nt_t1_count[] = { 480, 240, 120, 60, 30, 15, 8, 4 };


/************ D-channel BottomHalf : handle state change event **********/
void ashfc_dch_bh(dchannel_t *dch)
{
	ashfc_t *hc = dch->inst.data;
	u_int prim = PH_SIGNAL | INDICATION;
	u_int para = 0;
	as_isdn_if_t *upif = &dch->inst.up;
	int ch;

	if (!dch)
	{
		printk(KERN_WARNING "%s : DCH-BH-HW ERROR given dch is NULL\n", __FUNCTION__ );
		return;
	}
	ch = dch->channel;

#if 0
	/* Loss Of Signal */
	if (test_and_clear_bit(D_LOS, &dch->event))
	{
		if (ashfc_debug & ASHFC_DEBUG_STATE)
			printk(KERN_ERR  "%s: LOS detected\n", __FUNCTION__);
		skb = create_link_skb(PH_CONTROL | INDICATION, HW_LOS, 0, NULL, 0);
		if (skb)
		{
			hh = AS_ISDN_HEAD_P(skb);
			if (if_newhead(upif, hh->prim, hh->dinfo, skb))
				dev_kfree_skb(skb);
		}
	}
	/* Loss Of Signal OFF */
	if (test_and_clear_bit(D_LOS_OFF, &dch->event))
	{
		if (ashfc_debug & ASHFC_DEBUG_STATE)
			printk(KERN_ERR  "%s: LOS gone\n", __FUNCTION__);
		skb = create_link_skb(PH_CONTROL | INDICATION, HW_LOS_OFF, 0, NULL, 0);
		if (skb)
		{
			hh = AS_ISDN_HEAD_P(skb);
			if (if_newhead(upif, hh->prim, hh->dinfo, skb))
				dev_kfree_skb(skb);
		}
	}
	
	/* Alarm Indication Signal */
	if (test_and_clear_bit(D_AIS, &dch->event))
	{
		if (ashfc_debug & ASHFC_DEBUG_STATE)
			printk(KERN_ERR  "%s: AIS detected\n", __FUNCTION__);
		skb = create_link_skb(PH_CONTROL | INDICATION, HW_AIS, 0, NULL, 0);
		if (skb)
		{
			hh = AS_ISDN_HEAD_P(skb);
			if (if_newhead(upif, hh->prim, hh->dinfo, skb))
				dev_kfree_skb(skb);
		}
	}
	/* Alarm Indication Signal OFF */
	if (test_and_clear_bit(D_AIS_OFF, &dch->event))
	{
		if (ashfc_debug & ASHFC_DEBUG_STATE)
			printk(KERN_ERR  "%s: AIS gone\n", __FUNCTION__);
		skb = create_link_skb(PH_CONTROL | INDICATION, HW_AIS_OFF, 0, NULL, 0);
		if (skb)
		{
			hh = AS_ISDN_HEAD_P(skb);
			if (if_newhead(upif, hh->prim, hh->dinfo, skb))
				dev_kfree_skb(skb);
		}
	}
	
	/* SLIP detected */
	if (test_and_clear_bit(D_SLIP_TX, &dch->event))
	{
		if (ashfc_debug & ASHFC_DEBUG_STATE)
			printk(KERN_ERR  "%s: bit SLIP detected TX\n", __FUNCTION__);
		skb = create_link_skb(PH_CONTROL | INDICATION, HW_SLIP_TX, 0, NULL, 0);
		if (skb)
		{
			hh = AS_ISDN_HEAD_P(skb);
			if (if_newhead(upif, hh->prim, hh->dinfo, skb))
				dev_kfree_skb(skb);
		}
	}
	if (test_and_clear_bit(D_SLIP_RX, &dch->event))
	{
		if (ashfc_debug & ASHFC_DEBUG_STATE)
			printk(KERN_ERR  "%s: bit SLIP detected RX\n", __FUNCTION__);
		skb = create_link_skb(PH_CONTROL | INDICATION, HW_SLIP_RX, 0, NULL, 0);
		if (skb)
		{
			hh = AS_ISDN_HEAD_P(skb);
			if (if_newhead(upif, hh->prim, hh->dinfo, skb))
				dev_kfree_skb(skb);
		}
	}
#endif

	if (test_and_clear_bit(D_L1STATECHANGE, &dch->event))
	{/* HFC-4S/8S */
		if (!test_bit(HFC_CFG_NTMODE, &hc->chan[ch].cfg))
		{/* TE mode */
#if WITH_ASHFC_DEBUG_STATE
//			if (ashfc_debug & ASHFC_DEBUG_STATE)
				printk(KERN_ERR  "%s : DCH-BH-HW S/T TE newstate %x\n", hc->name, dch->ph_state);
#endif
			switch (dch->ph_state)
			{
				case ( 0):
					prim = PH_CONTROL | INDICATION;
					para = HW_RESET;
					break;

				case (TE_STATE_F3_DEACTIVATED):
					prim = PH_CONTROL | INDICATION;
					para = HW_DEACTIVATE;
					break;

				case (TE_STATE_F5_IDENTIFY_INPUT):
				/* first Rx signal from DSU, cease to transmit INFO1 and waiting Idengtify INFO2 or INFO 4
				not yet recoginize whether it is INFO2 or INFO4
				*/	
				case (TE_STATE_F8_LOST_FRAMEING):
				/* lost frame sync and waiting re-sync by Rx INFO2/INFO4 or deactivation by Rx INFO0*/	
					para = ANYSIGNAL;
					break;

				case (TE_STATE_F6_SYNCHRONIZED):
				/* Rx INFO2 and response with INFO3,waiting INFO4 */
					para = INFO2;
					break;

				case (TE_STATE_F7_ACTIVATED ):
				/* protocol activated in both directions */	
					para = INFO4_P8;
					break;

				default:
					return;
			}
		}/* end of TE mode */
		else
		{ /* NT mode */
#if WITH_ASHFC_DEBUG_STATE
//			if (ashfc_debug & ASHFC_DEBUG_STATE)
				printk(KERN_ERR  "%s : DCH-BH-HW S/T NT newstate %x\n", hc->name, dch->ph_state);
#endif
			dch->inst.lock(hc, 0);
			switch (dch->ph_state)
			{
				case (NT_STATE_G2_PEND_ACTIVATION):
				/* partially active state, DSU sending INFO2 while waiting INFO3 
				enter by : 1, upper layer with PH_ACTIVATE_REQ
				2:Rx INFO0 ; lost framing when in G3(activated)
				*/
					if (hc->chan[ch].nt_timer == 0)
					{
						hc->chan[ch].nt_timer = -1;
						HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
						HFC_outb(hc, A_ST_WR_STATE, 4 | V_ST_LD_STA); /* G4 */
						udelay(6); /* wait at least 5,21us */
						HFC_outb(hc, A_ST_WR_STATE, 4);
						dch->ph_state = 4;
					}
					else
					{
						/* one extra count for the next event */
						hc->chan[ch].nt_timer = nt_t1_count[ashfc_poll_timer] + 1;
						HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
						HFC_outb(hc, A_ST_WR_STATE, 2 | V_SET_G2_G3); /* allow G2 -> G3 transition */
					}
					upif = NULL;
					break;

				case (NT_STATE_G1_DEACTIVATED):
					/*DSU is not transmitting */
					prim = PH_DEACTIVATE | INDICATION;
					para = 0;
					hc->chan[ch].nt_timer = -1;
					break;

				case (NT_STATE_G4_PEND_DEACTIVATION):
					/* waite timer expired before return to deactive state */
					hc->chan[ch].nt_timer = -1;
					upif = NULL;
					break;

				case (NT_STATE_G3_ACTIVATED):
					/* NT activated with INFO3 and TE with INFO 4 */
					prim = PH_ACTIVATE | INDICATION;
					para = 0;
					hc->chan[ch].nt_timer = -1;
					break;

				default:
					break;/* end of NT mode */
			}
			dch->inst.unlock(hc);
		}
	}/* end of L1STATECHANGE */
	/*added 2005.12.21,lizhijie, remove some odd msg in upper layer */
	else
		return;

	/* transmit new state to upper layer if available */
	if (hc->created[hc->chan[ch].port])
	{
		while(upif)
		{
			if_link(upif, prim, para, 0, NULL, 0);
			upif = upif->clone;
		}
	}
}

int ashfc_bch_stop_tranceive(bchannel_t *bch)
{
//	int ret =0;
	ashfc_t *hc = bch->private;

#if WITH_ASHFC_DEBUG_CTRL
	if (ashfc_debug & ASHFC_DEBUG_MSG)
		printk(KERN_ERR  "%s : DEACTIVATE B Ch %d (device %d)\n", hc->name, bch->channel, bch->dev->minor );
#endif

	/* deactivate B-channel if not already deactivated */
//	bch->inst.lock(hc, 0);

	bch->tx_idx = bch->tx_len = bch->rx_idx = 0;
	test_and_clear_bit(BC_FLG_TX_BUSY, &bch->Flag);
	hc->chan[bch->channel].slot_tx = -1;
	hc->chan[bch->channel].slot_rx = -1;
	hc->chan[bch->channel].conf = -1;
	ashfc_chan_mode(hc, bch->channel, ISDN_PID_NONE, hc->chan[bch->channel].slot_tx, hc->chan[bch->channel].bank_tx, hc->chan[bch->channel].slot_rx, hc->chan[bch->channel].bank_rx);
	bch->protocol = ISDN_PID_NONE;
	test_and_clear_bit(BC_FLG_ACTIV, &bch->Flag);
//	bch->inst.unlock(hc);

	test_and_clear_bit(BC_FLG_WAIT_DEACTIVATE, &bch->Flag);

	return 0;
}


/*********** B-channel BottomHalf : handle DTMF event *************/
void ashfc_bch_bh(bchannel_t *bch)
{
#if 0
	ashfc_t			*hc = bch->private;//inst.data;
	struct sk_buff  	*skb;
        as_isdn_head_t    	*hh;
        as_isdn_if_t       	*hif = 0; /* make gcc happy */
#endif
		
#if WITH_ASHFC_DEBUG_DATA
	u_long			*coeff;
#endif

	if (!bch)
	{
		printk(KERN_WARNING "%s : DCH-BH-HW ERROR given bch is NULL\n", __FUNCTION__);
		return;
	}

#if 0
	/* DTMF event */
	if (test_and_clear_bit(B_DTMFREADY, &bch->event))
	{	/* DTMF coefficients in skb of dtmfqueue */
		while ((skb = skb_dequeue(&hc->chan[bch->channel].dtmfque)))
		{
#if WITH_ASHFC_DEBUG_DATA
			if (ashfc_debug & ASHFC_DEBUG_DTMF)
			{
				coeff = (u_long *)skb->data;
				printk("%s : DCH-BH-HW DTMF ready %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx len=%d\n", hc->name,
				coeff[0], coeff[1], coeff[2], coeff[3], coeff[4], coeff[5], coeff[6], coeff[7], skb->len);
			}
#endif			
			hh = AS_ISDN_HEAD_P(skb);
			
			/* when B channel is used as RAWDEV, skb is transmit to dev->rport which can be read by user process
			How is the coefficients used by user process */
#if 0			
			if ((bch->inst.pid.protocol[2] == ISDN_PID_L2_B_RAWDEV) && bch->dev)
				hif = &bch->dev->rport.pif;
			else/* else, transmit to up layer protocol in this stack */
				hif = &bch->inst.up;
#endif			
			hif = &bch->dev->rport.pif;
			if (if_newhead(hif, hh->prim, hh->dinfo, skb))
				dev_kfree_skb(skb);
		}
	}
#endif

	if( test_bit(BC_FLG_WAIT_DEACTIVATE, &bch->Flag))
	{
		ashfc_bch_stop_tranceive( bch);
	}
	
}

