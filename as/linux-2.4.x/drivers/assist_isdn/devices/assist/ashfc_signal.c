/*
 * $Id: ashfc_signal.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */
 
#include <linux/delay.h>

#include "dchannel.h"
#include "bchannel.h"
#include "layer1.h"
#include "dsp.h"
#include "helper.h"
//#include <linux/isdn_compat.h>

#define SPIN_DEBUG
#define LOCK_STATISTIC
#include "hw_lock.h"
#include "ashfc.h"

static int _ashfc_l2l1_PH_CONTORL_REQ(ashfc_t *hc, bchannel_t *bch, struct sk_buff *skb)
{
	struct		dsp_features *features;
	int			slot_tx, slot_rx, bank_tx, bank_rx;
	u_long		num;
	AS_ISDN_head_t	*hh;
	int ret =0;

	hh = AS_ISDN_HEAD_P(skb);

	bch->inst.lock(hc, 0);
	switch (hh->dinfo)
	{
		/* fill features structure */
		case HW_FEATURES:
			if (skb->len != sizeof(void *))
			{
				printk(KERN_WARNING "%s : L2L1-CTRL HW_FEATURES lacks parameters\n", hc->name);
				break;
			}
#if WITH_ASHFC_DEBUG_CTRL
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L2L1-CTRL HW_FEATURE request\n", hc->name);
#endif			
			features = *((struct dsp_features **)skb->data);
			features->hfc_id = hc->id;
			if (test_bit(HFC_CHIP_DTMF, &hc->chip))
				features->hfc_dtmf = 1;
			features->hfc_loops = 0;
			features->pcm_id = hc->pcm;
			features->pcm_slots = hc->slots;
			features->pcm_banks = 2;
			ret = 0;
			break;

		/* connect interface to pcm timeslot (0..N) */
		case HW_PCM_CONN:
			if (skb->len < 4*sizeof(u_long))
			{
				printk(KERN_WARNING "%s : L2L1-CTRL HW_PCM_CONN lacks parameters\n", hc->name);
				break;
			}
			slot_tx = ((int *)skb->data)[0];
			bank_tx = ((int *)skb->data)[1];
			slot_rx = ((int *)skb->data)[2];
			bank_rx = ((int *)skb->data)[3];

#if WITH_ASHFC_DEBUG_CTRL
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L2L1-CTRL HW_PCM_CONN slot %d bank %d (TX) slot %d bank %d (RX)\n", hc->name, slot_tx, bank_tx, slot_rx, bank_rx);
#endif

			if (slot_tx<=hc->slots && bank_tx<=2 && slot_rx<=hc->slots && bank_rx<=2)
				ashfc_pcm(hc, bch->channel, slot_tx, bank_tx, slot_rx, bank_rx);
			else
				printk(KERN_WARNING "%s : L2L1-CTRL HW_PCM_CONN slot %d bank %d (TX) slot %d bank %d (RX) out of range\n", hc->name, slot_tx, bank_tx, slot_rx, bank_rx);
			ret = 0;
			break;

		/* release interface from pcm timeslot */
		case HW_PCM_DISC:

#if WITH_ASHFC_DEBUG_CTRL
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L2L1-CTRL HW_PCM_DISC\n", hc->name);
#endif

			ashfc_pcm(hc, bch->channel, -1, -1, -1, -1); 
			ret = 0;
			break;

		/* join conference (0..7) */
		case HW_CONF_JOIN:
			if (skb->len < sizeof(u_long))
			{
				printk(KERN_WARNING "%s : L2L1-CTRL HW_CONF_JOIN lacks parameters\n", hc->name);
				break;
			}
			num = ((u_long *)skb->data)[0];

#if WITH_ASHFC_DEBUG_CTRL
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L2L1-CTRL HW_CONF_JOIN conf %ld\n", hc->name, num);
#endif
			if (num <= 7)
			{
				ashfc_conf(hc, bch->channel, num); 
				ret = 0;
			}
			else
				printk(KERN_WARNING "%s : L2L1-CTRL HW_CONF_JOIN conf %ld out of range\n", __FUNCTION__, num);
			break;

		/* split conference */
		case HW_CONF_SPLIT:

#if WITH_ASHFC_DEBUG_CTRL
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L2L1-CTRL HW_CONF_SPLIT\n", hc->name);
#endif
			ashfc_conf(hc, bch->channel, -1); 
			ret = 0;
			break;

		/* set sample loop */
		case HW_SPL_LOOP_ON:

#if WITH_ASHFC_DEBUG_CTRL
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L2L1-CTRL HW_SPL_LOOP_ON (len = %d)\n", hc->name, skb->len);
#endif
			ashfc_splloop(hc, bch->channel, skb->data, skb->len);
			ret = 0;
			break;

		/* set silence */
		case HW_SPL_LOOP_OFF:

#if WITH_ASHFC_DEBUG_CTRL
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L2L1-CTRL HW_SPL_LOOP_OFF\n", hc->name);
#endif
			ashfc_splloop(hc, bch->channel, NULL, 0);
			ret = 0;
			break;

		default:
			printk(KERN_ERR  "%s : L2L1-CTRL unknown PH_CONTROL info %x\n", hc->name, hh->dinfo);
			ret = -EINVAL;
	}
	bch->inst.unlock(hc);
	
	return ret;
}

static int _ashfc_l2l1_DOWN_REQ(ashfc_t *hc, bchannel_t *bch, struct sk_buff *skb)
{
	AS_ISDN_head_t	*hh;
	
	hh = AS_ISDN_HEAD_P(skb);


#if WITH_ASHFC_DEBUG_CTRL
	if (ashfc_debug & ASHFC_DEBUG_MSG)
		printk(KERN_ERR  "%s : L2L1-DOWN PH_DEACTIVATE ch %d (0..32)\n", hc->name, bch->channel);
#endif

	/* deactivate B-channel if not already deactivated */
	bch->inst.lock(hc, 0);
	if (bch->next_skb)
	{
		test_and_clear_bit(BC_FLG_TX_NEXT, &bch->Flag); 
		dev_kfree_skb(bch->next_skb);
		bch->next_skb = NULL;
	}

	bch->tx_idx = bch->tx_len = bch->rx_idx = 0;
	test_and_clear_bit(BC_FLG_TX_BUSY, &bch->Flag);
	hc->chan[bch->channel].slot_tx = -1;
	hc->chan[bch->channel].slot_rx = -1;
	hc->chan[bch->channel].conf = -1;
	ashfc_chan_mode(hc, bch->channel, ISDN_PID_NONE, hc->chan[bch->channel].slot_tx, hc->chan[bch->channel].bank_tx, hc->chan[bch->channel].slot_rx, hc->chan[bch->channel].bank_rx);
	bch->protocol = ISDN_PID_NONE;
	test_and_clear_bit(BC_FLG_ACTIV, &bch->Flag);
	bch->inst.unlock(hc);
	skb_trim(skb, 0);
//printk("5\n");
	if (hh->prim != (MGR_DISCONNECT | REQUEST))
	{
		if (bch->inst.pid.protocol[2] == ISDN_PID_L2_B_RAWDEV)
			if (bch->dev)
				if_link(&bch->dev->rport.pif, hh->prim | CONFIRM, 0, 0, NULL, 0);
			
		if (!if_newhead(&bch->inst.up, hh->prim | CONFIRM, 0, skb))
			return(0);
//printk("6\n");
	}
//printk("7\n");
	return  0;
}

static int _ashfc_l2l1_LINK_REQ(ashfc_t *hc, bchannel_t *bch, struct sk_buff *skb)
{
	AS_ISDN_head_t	*hh;
	int ret =0;

	hh = AS_ISDN_HEAD_P(skb);

//TRACE();
	/* activate B-channel if not already activated */

#if WITH_ASHFC_DEBUG_CTRL
	if (ashfc_debug & ASHFC_DEBUG_MSG)
		printk(KERN_ERR  "%s : L2L1-LINK PH_ACTIVATE ch %d (0..32)\n", hc->name, bch->channel);
#endif

	if (test_and_set_bit(BC_FLG_ACTIV, &bch->Flag))
		ret = 0;
	else
	{
		bch->inst.lock(hc, 0);
		ret = ashfc_chan_mode(hc, bch->channel, bch->inst.pid.protocol[1], hc->chan[bch->channel].slot_tx, hc->chan[bch->channel].bank_tx, hc->chan[bch->channel].slot_rx, hc->chan[bch->channel].bank_rx);
		if (!ret)
		{
			bch->protocol = bch->inst.pid.protocol[1];
			if (bch->protocol)/* start TX */
				bch_sched_event(bch, B_XMTBUFREADY);
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
		}
		bch->inst.unlock(hc);
	}

//TRACE();
	
	if( (bch->inst.pid.protocol[2] == ISDN_PID_L2_B_RAWDEV) && (bch->dev) )
	{
		if_link(&bch->dev->rport.pif, hh->prim | CONFIRM, 0, 0, NULL, 0);
//		TRACE();
	}

	skb_trim(skb, 0);
	
	return(if_newhead(&bch->inst.up, hh->prim | CONFIRM, ret, skb));
}

/* for both PH_DATA and DL_DATA, call by write thread and BH */
static int _ashfc_l2l1_DATA_REQ(ashfc_t *hc, bchannel_t *bch, struct sk_buff *skb)
{
	AS_ISDN_head_t	*hh;
	AS_ISDNif_t *hif;
	int prim = 0;

	hh = AS_ISDN_HEAD_P(skb);
	if (skb->len == 0)
	{
		printk(KERN_WARNING "%s : L2L1-DATA skb too small\n", hc->name);
		return(-EINVAL);
	}
	if (skb->len > MAX_DATA_MEM)
	{
		printk(KERN_WARNING "%s : L2L1-DATA skb too large\n", hc->name);
		return(-EINVAL);
	}

	/* check for pending next_skb */
	if (bch->next_skb)
	{
//		printk(KERN_WARNING "%s : L2L1-DATA next_skb exist ERROR (skb->len=%d next_skb->len=%d)\n", hc->name, skb->len, bch->next_skb->len);
		return(-EBUSY);
	}
	/* if we have currently a pending tx skb */
	bch->inst.lock(hc, 0);
	if (test_and_set_bit(BC_FLG_TX_BUSY, &bch->Flag))
	{
		test_and_set_bit(BC_FLG_TX_NEXT, &bch->Flag);
		bch->next_skb = skb;
		bch->inst.unlock(hc);
		return(0);
	}

	/* write to fifo */
	bch->tx_len = skb->len;
	memcpy(bch->tx_buf, skb->data, bch->tx_len);
	bch->tx_idx = 0;

	ashfc_tx(hc, bch->channel, NULL, bch);
	/* start fifo */
	HFC_outb_(hc, R_FIFO, 0|ASHFC_MSB_FIRST );
	HFC_wait_(hc);

	bch->inst.unlock(hc);
#if 0	
	if ((bch->inst.pid.protocol[2] == ISDN_PID_L2_B_RAWDEV) && bch->dev)
		hif = &bch->dev->rport.pif;		/* carefull for this hif */
	else
		hif = &bch->inst.up;
	skb_trim(skb, 0);
	/* feedback confirm primitive */
	return(if_newhead(hif, hh->prim | CONFIRM,	hh->dinfo, skb));
#else
	/* modify to simplify the Data access path, lizhijie,2005.12.21 */
	hif = &bch->dev->rport.pif;	
	prim = hh->prim;
	dev_kfree_skb(skb);
	as_isdn_rdata_raw_data( hif, prim|CONFIRM, NULL);
	return 0;
#endif
}


/************** Layer2 -> Layer 1 Transfer : B channel *************/
/* messages from layer 2 to layer 1 are processed here. */
int ashfc_l2l1(AS_ISDNif_t *hif, struct sk_buff *skb)
{
	bchannel_t	*bch;
	int			ret = -EINVAL;
	ashfc_t		*hc;
	AS_ISDN_head_t	*hh;


#if WITH_ASHFC_DEBUG_DATA
	printk(KERN_ERR "%s : L2L1-LINK is called\n", __FUNCTION__ );
#endif

	if (!hif || !skb)
		return(ret);
	hh = AS_ISDN_HEAD_P(skb);
	bch = hif->fdata;
	hc = bch->inst.data;

	if ((hh->prim == PH_DATA_REQ) || (hh->prim == (DL_DATA | REQUEST)))
	{
		return _ashfc_l2l1_DATA_REQ( hc, bch,  skb);
	}
	else if ((hh->prim == (PH_ACTIVATE | REQUEST)) || (hh->prim == (DL_ESTABLISH  | REQUEST)))
	{
		return _ashfc_l2l1_LINK_REQ(hc, bch,  skb);
	}
	else if ((hh->prim == (PH_DEACTIVATE | REQUEST))
	 || (hh->prim == (DL_RELEASE | REQUEST))
	 || (hh->prim == (MGR_DISCONNECT | REQUEST)))
	{
		return _ashfc_l2l1_DOWN_REQ(hc, bch, skb);
	}
	else	 if (hh->prim == (PH_CONTROL | REQUEST))
	{
		return _ashfc_l2l1_PH_CONTORL_REQ( hc,  bch,  skb);
	}
	else
	{
		printk(KERN_WARNING "%s : L2L1 unknown prim(%x)\n", hc->name, hh->prim);
		ret = -EINVAL;
	}

//TRACE();
	if (!ret)
	{
//		printk("3\n");
		dev_kfree_skb(skb);
//		printk("4\n");
	}
	return(ret);
}

/* deactivate operation function for l1hw without lock */
static void _ashfc_l1hw_deactivate_op(ashfc_t *hc, dchannel_t *dch)
{
	dch->ph_state = 0;
	/* start deactivation */

	HFC_outb(hc, R_ST_SEL, hc->chan[dch->channel].port);
	HFC_outb(hc, A_ST_WR_STATE, V_ST_ACT*2); /* deactivate */

	discard_queue(&dch->rqueue);
	if (dch->next_skb)
	{
		dev_kfree_skb(dch->next_skb);
		dch->next_skb = NULL;
	}

	dch->tx_idx = dch->tx_len = hc->chan[dch->channel].rx_idx = 0;
	test_and_clear_bit(FLG_TX_NEXT, &dch->DFlags);
	test_and_clear_bit(FLG_TX_BUSY, &dch->DFlags);

	if (test_and_clear_bit(FLG_DBUSY_TIMER, &dch->DFlags))
		del_timer(&dch->dbusytimer);
	
	if (test_and_clear_bit(FLG_L1_DBUSY, &dch->DFlags))
		dchannel_sched_event(dch, D_CLEARBUSY);
}

static int _ashfc_l1hw_PH_ACTIVATE_REQ(ashfc_t *hc, dchannel_t *dch)
{
	int ret = 0;
	
	if (test_bit(HFC_CFG_NTMODE, &hc->chan[dch->channel].cfg))
	{

#if WITH_ASHFC_DEBUG_CTRL
		if (ashfc_debug & ASHFC_DEBUG_MSG)
			printk(KERN_ERR  "%s : L1HW-ACTIVE PH_ACTIVATE port %d (0..%d)\n", hc->name, hc->chan[dch->channel].port, hc->type-1);
#endif

		dch->inst.lock(hc, 0);

		/* start activation */
		HFC_outb(hc, R_ST_SEL, hc->chan[dch->channel].port);
		HFC_outb(hc, A_ST_WR_STATE, V_ST_LD_STA | 1); /* G1 */
		udelay(6); /* wait at least 5,21us */
		HFC_outb(hc, A_ST_WR_STATE, 1);
		HFC_outb(hc, A_ST_WR_STATE, 1 | (V_ST_ACT*3)); /* activate */
		dch->ph_state = 1;

		dch->inst.unlock(hc);
	}
	else
	{

#if WITH_ASHFC_DEBUG_CTRL
		if (ashfc_debug & ASHFC_DEBUG_MSG)
			printk(KERN_ERR  "%s : L1HW-ACTIVE PH_ACTIVATE no NT-mode port %d (0..%d)\n", hc->name, hc->chan[dch->channel].port, hc->type-1);
#endif
		ret = -EINVAL;
	}
	return ret;
}

int _ashfc_l1hw_PH_CONTROL_REQ(ashfc_t *hc, dchannel_t *dch, struct sk_buff *skb)
{
	AS_ISDN_head_t	*hh;
	int		slot_tx, slot_rx, bank_tx, bank_rx;
	int		ret = 0;

	hh = AS_ISDN_HEAD_P(skb);

//	TRACE();
	dch->inst.lock(hc, 0);
	switch (hh->dinfo)
	{
		case HW_RESET:
		/* start activation */
//	TRACE();
			HFC_outb(hc, R_ST_SEL, hc->chan[dch->channel].port);
			HFC_outb(hc, A_ST_WR_STATE, V_ST_LD_STA | 3); /* G1 */
			udelay(6); /* wait at least 5,21us */
			HFC_outb(hc, A_ST_WR_STATE, 3);
			HFC_outb(hc, A_ST_WR_STATE, 3 | (V_ST_ACT*3)); /* activate */
			break;

		case HW_DEACTIVATE:

#if WITH_ASHFC_DEBUG_STATE
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L1HW-CTRL HW_DEACTIVATE on port %d\n", hc->name, hc->chan[dch->channel].port);
#endif
			//goto hw_deactivate; /* after lock */
			_ashfc_l1hw_deactivate_op( hc, dch);
			break;
			
			/* connect interface to pcm timeslot (0..N) */
		case HW_PCM_CONN:

			if (skb->len < 4*sizeof(u_long))
			{
				printk(KERN_WARNING "%s : L1HW-CTRL HW_PCM_CONN lacks parameters on port %d\n", hc->name, hc->chan[dch->channel].port);
				break;
			}
			slot_tx = ((int *)skb->data)[0];
			bank_tx = ((int *)skb->data)[1];
			slot_rx = ((int *)skb->data)[2];
			bank_rx = ((int *)skb->data)[3];
				

#if WITH_ASHFC_DEBUG_STATE
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L1HW-CTRL HW_PCM_CONN slot %d bank %d (TX) slot %d bank %d (RX) on port %d\n", hc->name, slot_tx, bank_tx, slot_rx, bank_rx, hc->chan[dch->channel].port);
#endif
			if (slot_tx<=hc->slots && bank_tx<=2 && slot_rx<=hc->slots && bank_rx<=2)
				ashfc_pcm(hc, dch->channel, slot_tx, bank_tx, slot_rx, bank_rx);
			else
				printk(KERN_WARNING "%s : L1HW-CTRL HW_PCM_CONN slot %d bank %d (TX) slot %d bank %d (RX) out of range on port %d\n", hc->name, slot_tx, bank_tx, slot_rx, bank_rx, hc->chan[dch->channel].port);
			break;

			/* release interface from pcm timeslot */
		case HW_PCM_DISC:

#if WITH_ASHFC_DEBUG_STATE
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L1HW-CTRL HW_PCM_DISC on port %d\n", hc->name, hc->chan[dch->channel].port);
#endif
			ashfc_pcm(hc, dch->channel, -1, -1, -1, -1); 
			break;

		default:
			printk(KERN_ERR  "%s : L1HW-CTRL unknown PH_CONTROL info %x on port %d\n", hc->name, hh->dinfo, hc->chan[dch->channel].port);
			ret = -EINVAL;
	}
//	TRACE();
	dch->inst.unlock(hc);

	dev_kfree_skb(skb);

#if WITH_ASHFC_DEBUG_CTRL
	TRACE();
#endif
	return ret;
}

int _ashfc_l1hw_PH_DATA_REQ(ashfc_t *hc, dchannel_t *dch, struct sk_buff *skb)
{
	AS_ISDN_head_t	*hh;

	hh = AS_ISDN_HEAD_P(skb);
//	printk(KERN_ERR  "%s: prim(%x)\n", __FUNCTION__, hh->prim);
	/* check oversize */
	if (skb->len == 0)
	{
		printk(KERN_WARNING "%s : L1HW-DATA skb too small on port %d\n", hc->name, hc->chan[dch->channel].port);
		return(-EINVAL);
	}

	if (skb->len > MAX_DFRAME_LEN_L1)
	{
		printk(KERN_WARNING "%s : L1HW-DATA skb too large on port %d\n", hc->name, hc->chan[dch->channel].port);
		return(-EINVAL);
	}
	/* check for pending next_skb */

	if (dch->next_skb)
	{
		printk(KERN_WARNING "%s : L1HW-DATA next_skb exist ERROR (skb->len=%d next_skb->len=%d) on port %d\n", hc->name, skb->len, dch->next_skb->len, hc->chan[dch->channel].port);
		return(-EBUSY);
	}


	/* if we have currently a pending tx skb */
	dch->inst.lock(hc, 0);
	if (test_and_set_bit(FLG_TX_BUSY, &dch->DFlags))
	{
		test_and_set_bit(FLG_TX_NEXT, &dch->DFlags);
		dch->next_skb = skb;
		dch->inst.unlock(hc);
		return(0);
	}


	/* write to fifo */
	dch->tx_len = skb->len;
	memcpy(dch->tx_buf, skb->data, dch->tx_len);
//		if (ashfc_debug & ASHFC_DEBUG_FIFO) {
//			printk(KERN_ERR  "%s:from user space:\n", __FUNCTION__);
//			i = 0;
//			while(i < dch->tx_len)
//				printk(" %02x", dch->tx_buf[i++]);
//			printk("\n");
//		}
	dch->tx_idx = 0;

	ashfc_tx(hc, dch->channel, dch, NULL);

	/* start fifo */
	HFC_outb(hc, R_FIFO, 0);
	HFC_wait(hc);
	dch->inst.unlock(hc);
	skb_trim(skb, 0);
	
	/* feedback confirm primitive to user process */
	return(if_newhead(&dch->inst.up, PH_DATA_CNF,	hh->dinfo, skb));
}

/*************** Layer 1 D-channel hardware access *************/
/* message transfer from layer 1 to hardware. */
int ashfc_l1hw(AS_ISDNif_t *hif, struct sk_buff *skb)
{
	dchannel_t	*dch;
	ashfc_t	*hc;
	int		ret = -EINVAL;
	AS_ISDN_head_t	*hh;


#if WITH_ASHFC_DEBUG_DATA
	TRACE();
#endif

	if (!hif || !skb)
		return(ret);
	
	hh = AS_ISDN_HEAD_P(skb);

#if WITH_ASHFC_DEBUG_CTRL
	printk(KERN_ERR  "%s: prim(%x)\n", __FUNCTION__, hh->prim);
#endif

	dch = hif->fdata;
	hc = dch->inst.data;
	ret = 0;

	if (hh->prim == PH_DATA_REQ)
	{
//		TRACE();
		return _ashfc_l1hw_PH_DATA_REQ( hc, dch, skb);
	}
	else if (hh->prim == (PH_SIGNAL | REQUEST))
	{
		dch->inst.lock(hc, 0);
		switch (hh->dinfo)
		{
			case INFO3_P8:
			case INFO3_P10: 

#if WITH_ASHFC_DEBUG_CTRL
				if (ashfc_debug & ASHFC_DEBUG_MSG)
					printk(KERN_ERR  "%s : L1HW INFO3_P%d\n", hc->name, (hh->dinfo==INFO3_P8)?8:10);
#endif

#if 0
				if (test_bit(HFC_CHIP_MASTER, &hc->chip))
					hc->hw.mst_m |= HFCPCI_MASTER;
				Write_hfc(hc, HFCPCI_MST_MODE, hc->hw.mst_m);
#endif
				break;
			default:
				printk(KERN_ERR  "%s : L1HW unknown PH_SIGNAL info %x on port %d\n", hc->name, hh->dinfo, hc->chan[dch->channel].port);
				ret = -EINVAL;
		}
		dch->inst.unlock(hc);
//		TRACE();

//		dev_kfree_skb(skb);
	}
	else if (hh->prim == (PH_CONTROL | REQUEST))
	{
//		TRACE();
		return _ashfc_l1hw_PH_CONTROL_REQ(hc, dch,  skb);
	}
	else if (hh->prim == (PH_ACTIVATE | REQUEST))
	{
//		TRACE();
		dev_kfree_skb(skb);
		return _ashfc_l1hw_PH_ACTIVATE_REQ(hc, dch);
	}
	else if (hh->prim == (PH_DEACTIVATE | REQUEST))
	{
//		TRACE();
		if (test_bit(HFC_CFG_NTMODE, &hc->chan[dch->channel].cfg))
		{

#if WITH_ASHFC_DEBUG_STATE
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L1L2 PH_DEACTIVATE port %d (0..%d)\n", hc->name, hc->chan[dch->channel].port, hc->type-1);
#endif
			dch->inst.lock(hc, 0);
			/* after lock */
			_ashfc_l1hw_deactivate_op( hc, dch);
			dch->inst.unlock(hc);
		}
		else
		{
#if WITH_ASHFC_DEBUG_STATE
			if (ashfc_debug & ASHFC_DEBUG_MSG)
				printk(KERN_ERR  "%s : L1L2 PH_DEACTIVATE no NT-mode port %d (0..%d)\n", hc->name, hc->chan[dch->channel].port, hc->type-1);
#endif
			ret = -EINVAL;
		}

		dev_kfree_skb(skb);
	}
	else
	{
#if WITH_ASHFC_DEBUG_STATE
		TRACE();

		if (ashfc_debug & ASHFC_DEBUG_MSG)
			printk(KERN_ERR  "%s : L1HW unknown prim %x on port %d\n", hc->name, hh->prim, hc->chan[dch->channel].port);
		TRACE();
#endif
		if(skb)
			dev_kfree_skb(skb);
	}

	return(ret);
}

