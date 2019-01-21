/*
 * $Id: ashfc_hw_utils.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */
 
#include <linux/pci.h>
#include <linux/delay.h>

#include "dchannel.h"
#include "bchannel.h"
#include "layer1.h"
#include "helper.h"


#define SPIN_DEBUG
#define LOCK_STATISTIC
#include "hw_lock.h"
#include "ashfc.h"

/***************************************************************/
/* activate/deactivate hardware for selected channels and mode */
/***************************************************************/

/* configure B/D-channel with the given protocol
 * ch eqals to the HFC-channel (0-31)
 * ch is the number of channel (0-4,4-7,8-11,12-15,16-19,20-23,24-27,28-31 for S/T )
 * the hdlc interrupts will be set/unset */
int ashfc_chan_mode(ashfc_t *hc, int ch, int protocol, int slot_tx, int bank_tx, int slot_rx, int bank_rx)
{
	int flow_tx = 0, flow_rx = 0, routing = 0;
	int oslot_tx = hc->chan[ch].slot_tx;
	int oslot_rx = hc->chan[ch].slot_rx;
	int conf = hc->chan[ch].conf;

#if WITH_ASHFC_DEBUG_CTRL
	if (ashfc_debug & ASHFC_DEBUG_MODE)
		printk(KERN_ERR  "%s : channel %d protocol %x slot %d bank %d (TX) slot %d bank %d (RX) on port %d\n", hc->name, ch, protocol, slot_tx, bank_tx, slot_rx, bank_rx, hc->chan[ch].port );
#endif

/************** remove old slot for this channel ******************/
	if (oslot_tx>=0 && slot_tx!=oslot_tx)
	{/* TX : old slot has been configed and is not equal old slot : remove from slot */
#if WITH_ASHFC_DEBUG_CTRL
		if (ashfc_debug & ASHFC_DEBUG_MODE)
			printk(KERN_ERR  "%s : remove from slot %d (TX)\n", hc->name, oslot_tx);
#endif
		if (hc->slot_owner[oslot_tx<<1] == ch)
		{
			HFC_outb(hc, R_SLOT, oslot_tx<<1);
			HFC_outb(hc, A_SL_CFG, 0);
			HFC_outb(hc, A_CONF, 0);
			hc->slot_owner[oslot_tx<<1] = -1;
		}
#if WITH_ASHFC_DEBUG_CTRL
		else
		{
			if (ashfc_debug & ASHFC_DEBUG_MODE)
				printk(KERN_ERR  "%s : we are not owner of this slot anymore, channel %d is.\n", hc->name, hc->slot_owner[oslot_tx<<1]);
		}
#endif
	}
	if (oslot_rx>=0 && slot_rx!=oslot_rx)
	{/* RX :remove from slot */
#if WITH_ASHFC_DEBUG_CTRL
		if (ashfc_debug & ASHFC_DEBUG_MODE)
			printk(KERN_ERR  "%s : remove from slot %d (RX)\n", hc->name, oslot_rx);
#endif
		if (hc->slot_owner[(oslot_rx<<1)|1] == ch)
		{
			HFC_outb(hc, R_SLOT, (oslot_rx<<1) | V_SL_DIR);
			HFC_outb(hc, A_SL_CFG, 0);
			hc->slot_owner[(oslot_rx<<1)|1] = -1;
		}
#if WITH_ASHFC_DEBUG_CTRL
		else
		{
			if (ashfc_debug & ASHFC_DEBUG_MODE)
				printk(KERN_ERR  "%s : we are not owner of this slot anymore, channel %d is.\n", hc->name, hc->slot_owner[(oslot_rx<<1)|1]);
		} 
#endif
	}

	/* new config now */
	if (slot_tx < 0)
	{/* Tx  */
		flow_tx = 0x80; /* FIFO->ST */
		/* disable pcm slot */
		hc->chan[ch].slot_tx = -1;
		hc->chan[ch].bank_tx = 0;
	}
	else
	{
		/* set pcm slot */
		if (hc->chan[ch].txpending)
			flow_tx = 0x80; /* FIFO->ST */
		else
			flow_tx = 0xc0; /* PCM->ST */
		/* put on slot */
		routing = bank_tx?0xc0:0x80;	/*output buffer for 0xc0:0x80==STIO2:STIO1 enable*/
		if (conf>=0 || bank_tx>1)
			routing = 0x40; /* loop PCM data internally */
#if WITH_ASHFC_DEBUG_CTRL
		if (ashfc_debug & ASHFC_DEBUG_MODE)
			printk(KERN_ERR  "%s : put to slot %d bank %d flow %02x routing %02x conf %d (TX)\n", hc->name, slot_tx, bank_tx, flow_tx, routing, conf);
#endif
		HFC_outb(hc, R_SLOT, slot_tx<<1);
		HFC_outb(hc, A_SL_CFG, (ch<<1) | routing);			/* assocaited this HFC Channel(ch) with STIO1/2 */
		HFC_outb(hc, A_CONF, (conf<0)?0:(conf|V_CONF_SL));	/* added this PCM slot into Conf(erence) */
		hc->slot_owner[slot_tx<<1] = ch;
		hc->chan[ch].slot_tx = slot_tx;
		hc->chan[ch].bank_tx = bank_tx;
	}
	
	if (slot_rx < 0)
	{/* Tx newer config*/
		/* disable pcm slot */
		flow_rx = 0x80; /* ST->FIFO */
		hc->chan[ch].slot_rx = -1;
		hc->chan[ch].bank_rx = 0;
	}
	else
	{
		/* set pcm slot */
		if (hc->chan[ch].txpending)
			flow_rx = 0x80; /* ST->FIFO */
		else
			flow_rx = 0xc0; /* ST->(FIFO,PCM) */
		/* put on slot */
		routing = bank_rx?0x80:0xc0; /* reversed : 0x80:0xc0==STIO2:STIO1 */
		if (conf>=0 || bank_rx>1)
			routing = 0x40; /* loop */
#if WITH_ASHFC_DEBUG_CTRL
		if (ashfc_debug & ASHFC_DEBUG_MODE)
			printk(KERN_ERR  "%s: put to slot %d bank %d flow %02x routing %02x conf %d (RX)\n", hc->name, slot_rx, bank_rx, flow_rx, routing, conf);
#endif
		HFC_outb(hc, R_SLOT, (slot_rx<<1) | V_SL_DIR);
		HFC_outb(hc, A_SL_CFG, (ch<<1) | V_CH_DIR | routing);
		hc->slot_owner[(slot_rx<<1)|1] = ch;
		hc->chan[ch].slot_rx = slot_rx;
		hc->chan[ch].bank_rx = bank_rx;
	}
	
	switch (protocol)
	{
		case (ISDN_PID_NONE):
			/* disable TX fifo */
			HFC_outb(hc, R_FIFO, ch<<1);
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, flow_tx | 0x00 | V_IFF);	/* Inter frame fill is 1s */
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			/* disable RX fifo */
			HFC_outb(hc, R_FIFO, (ch<<1)|1);
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, flow_rx | 0x00);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);

			if ((ch&0x3)<2)
			{
				hc->hw.a_st_ctrl0[hc->chan[ch].port] &= ((ch&0x3)==0)?~V_B1_EN:~V_B2_EN;
				HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
				HFC_outb(hc, A_ST_CTRL0,  hc->hw.a_st_ctrl0[hc->chan[ch].port]);
			}
			break;

		case (ISDN_PID_L1_B_64TRANS): /* 64 Kbit/s : B-channel : transparent mode */
			/* enable TX fifo */
//			HFC_outb(hc, R_FIFO, (ch<<1) );		/* PCM should be in MSB ??, refer to P.168 */
			HFC_outb(hc, R_FIFO,( (ch<<1)|ASHFC_MSB_FIRST) );		/* lizhijie, 2005.09.07 */
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, flow_tx | 0x00 | V_HDLC_TRP | V_IFF);		/* transparent mode selected, not HDLC mode */
			HFC_outb(hc, A_SUBCH_CFG, 0);			/* This is B channel, so sub-channel process is not needed */
			HFC_outb(hc, A_IRQ_MSK, 0);				/* No IRQ is needed in transparent mode */
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);	/* FIFO reset. After reset, it is cleared automatically */
			HFC_wait(hc);
			HFC_outb_(hc, A_FIFO_DATA0_NOINC, ashfc_silence); /* tx silence: write 16 bit without increment Z  */

			/* enable RX fifo */
//			HFC_outb(hc, R_FIFO, (ch<<1)|ASHFC_DATA_RX);

			HFC_outb(hc, R_FIFO, ((ch<<1)|ASHFC_DATA_RX|ASHFC_MSB_FIRST) );
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, flow_rx | 0x00 | V_HDLC_TRP);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);

			hc->hw.a_st_ctrl0[hc->chan[ch].port] |= ((ch&0x3)==0)?V_B1_EN:V_B2_EN;	/*check B1 or B2 */
			HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);		/* select the S/T interface this channel belong to */
			HFC_outb(hc, A_ST_CTRL0,  hc->hw.a_st_ctrl0[hc->chan[ch].port]);	/* enable this B1/B2 channel */

			break;

		case (ISDN_PID_L1_B_64HDLC): /* B-channel */
			/* enable TX fifo */
			HFC_outb(hc, R_FIFO, ((ch<<1)|ASHFC_MSB_FIRST));
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, flow_tx | 0x04);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, V_IRQ);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			/* enable RX fifo */
			HFC_outb(hc, R_FIFO, ((ch<<1)|1|0x80) );
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, flow_rx | 0x04);	/* in transparent mode for this FIFO, 64 bytes a IRQ is generated */
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, V_IRQ);			/* enable IRQ for this selected FIFO */
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);

			hc->hw.a_st_ctrl0[hc->chan[ch].port] |= ((ch&0x3)==0)?V_B1_EN:V_B2_EN;
			HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
			HFC_outb(hc, A_ST_CTRL0,  hc->hw.a_st_ctrl0[hc->chan[ch].port]);

			break;

		case (ISDN_PID_L1_TE_S0): /* D-channel S0 */
		case (ISDN_PID_L1_NT_S0):
			/* enable TX fifo */
			HFC_outb(hc, R_FIFO, ch<<1);
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, flow_tx | 0x04 | V_IFF);
			HFC_outb(hc, A_SUBCH_CFG, 2);			/* this is D channel, only 16kbit/s */
			HFC_outb(hc, A_IRQ_MSK, V_IRQ);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			/* enable RX fifo */
			HFC_outb(hc, R_FIFO, (ch<<1)|1);
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, flow_rx | 0x04);
			HFC_outb(hc, A_SUBCH_CFG, 2);
			HFC_outb(hc, A_IRQ_MSK, V_IRQ);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			break;

		default:
			printk(KERN_ERR  "%s : protocol not known %x for channel mode\n", hc->name, protocol);
			hc->chan[ch].protocol = ISDN_PID_NONE;
			return(-ENOPROTOOPT);
	}
	hc->chan[ch].protocol = protocol;
	return(0);
}

