/* 
 * $Id: ashfc.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * hfc_multi.c  low level driver for hfc-4s/hfc-8s/hfc-e1 based cards
 */

/* module parameters:
 * type:
	Bit 8		= uLaw (instead of aLaw)
	Bit 9		= Enable DTMF detection on all B-channels
	Bit 10	= spare 
	Bit 11	= Set PCM bus into slave mode.
	Bit 12	= Ignore missing frame clock on PCM bus.
	Bit 13	= Use direct RX clock for PCM sync rather than PLL. (E1 only)
	Bit 14	= Use external ram (128K)
	Bit 15	= Use external ram (512K)
	Bit 16	= Use 64 timeslots instead of 32
	Bit 17	= Use 128 timeslots instead of anything else

 * protocol:
	NOTE: Must be given for all ports, not for the number of cards.
	HFC-4S/HFC-8S/HFC-E1 bits:
 	Bit 0-3 	= protocol
	Bit 4		= NT-Mode
	Bit 5		= PTP (instead of multipoint)

	HFC-4S/HFC-8S only bits:
	Bit 16	= Use master clock for this S/T interface (ony once per chip).
	Bit 17	= transmitter line setup (non capacitive mode) DONT CARE!
	Bit 18	= Disable E-channel. (No E-channel processing)
	Bit 19	= Register E-channel as D-stack (TE-mode only)

	HFC-E1 only bits:
	Bit 16	= interface: 0=copper, 1=optical
	Bit 17	= reserved (later for 32 B-channels transparent mode)
	Bit 18	= Report LOS
	Bit 19	= Report AIS
	Bit 20	= Report SLIP
	Bit 21-22 = elastic jitter buffer (1-3), Use 0 for default.
(all other bits are reserved and shall be 0)

 * layermask:
	NOTE: Must be given for all ports, not for the number of cards.
	mask of layers to be used for D-channel stack

 * debug:
	NOTE: only one debug value must be given for all cards
	enable debugging (see hfc_multi.h for debug options)

 * poll:
	NOTE: only one poll value must be given for all cards
	Give the number of samples for each fifo process.
	By default 128 is used. Decrease to reduce delay, increase to
	reduce cpu load. If unsure, don't mess with it!
	Valid is 8, 16, 32, 64, 128, 256.

 * pcm:
	NOTE: only one pcm value must be given for all cards
	Give the id of the PCM bus. All PCM busses with the same ID
	are expected to be connected and have equal slots.
	Only one chip of the PCM bus must be master, the others slave.
	-1 means no support of PCM bus.
 */

/* debug using register map (never use this, it will flood your system log) */
//#define HFC_REGISTER_MAP

#include <linux/pci.h>
#include <linux/delay.h>


#include "dchannel.h"
#include "bchannel.h"
#include "layer1.h"
#include "dsp.h"
#include "helper.h"

#if 0
#define SPIN_DEBUG
#define LOCK_STATISTIC
#endif

#include "hw_lock.h"
#include "ashfc.h"


u_char ashfc_silence =	0xff;	/* silence by LAW ,uLaw; a law is 0x2a */


int ashfc_debug = \
	ASHFC_DEBUG_INIT |\
	ASHFC_DEBUG_MGR |\
	ASHFC_DEBUG_STATE |\
	ASHFC_DEBUG_MODE |\
	ASHFC_DEBUG_FIFO |\
	ASHFC_DEBUG_MSG ;

/*
	ASHFC_DEBUG_SYNC |\
	ASHFC_DEBUG_CRC |\
	ASHFC_DEBUG_LOCK |\
	ASHFC_DEBUG_DTMF |\
*/

/************************** lock and unlock stuff *************************/
int lock_dev(void *data, int nowait)
{
	register AS_ISDN_HWlock_t *lock = &((ashfc_t *)data)->lock;
#if WITH_ASHFC_DEBUG_CTRL
	if (ashfc_debug & ASHFC_DEBUG_LOCK)
		printk(KERN_ERR  "%s\n", __FUNCTION__);
#endif	
	return(lock_HW(lock, nowait));
}

void unlock_dev(void *data)
{
	register AS_ISDN_HWlock_t *lock = &((ashfc_t *)data)->lock;
#if WITH_ASHFC_DEBUG_CTRL
	if (ashfc_debug & ASHFC_DEBUG_LOCK)
		printk(KERN_ERR  "%s\n", __FUNCTION__);
#endif	
	unlock_HW(lock);
}


/********************************************************************/
/* timer callback for D-chan busy resolution. Currently no function */
/********************************************************************/
void ashfc_dbusy_timer(ashfc_t *hc)
{
}


/*************************** connect/disconnect PCM **************************/
void ashfc_pcm(ashfc_t *hc, int ch, int slot_tx, int bank_tx, int slot_rx, int bank_rx)
{
	if (slot_rx<0 || slot_tx<0 || bank_tx<0 || bank_rx<0)
	{
		/* disable PCM */
		ashfc_chan_mode(hc, ch, hc->chan[ch].protocol, -1, 0, -1, 0);
		return;
	}

	/* enable pcm */
	ashfc_chan_mode(hc, ch, hc->chan[ch].protocol, slot_tx, bank_tx, slot_rx, bank_rx);
}


/************************** set/disable conference **************************/
void ashfc_conf(ashfc_t *hc, int ch, int num)
{
	if (num>=0 && num<=7)
		hc->chan[ch].conf = num;
	else
		hc->chan[ch].conf = -1;
	ashfc_chan_mode(hc, ch, hc->chan[ch].protocol, hc->chan[ch].slot_tx, hc->chan[ch].bank_tx, hc->chan[ch].slot_rx, hc->chan[ch].bank_rx);
}


/*************************** set/disable sample loop ***************************/
// NOTE: this function is experimental and therefore disabled
void ashfc_splloop(ashfc_t *hc, int ch, u_char *data, int len)
{
	u_char *d, *dd;
	bchannel_t *bch = hc->chan[ch].bch;

#if 0
	/* flush pending TX data */
	if (bch->next_skb)
	{
		test_and_clear_bit(BC_FLG_TX_NEXT, &bch->Flag); 
		dev_kfree_skb(bch->next_skb);
		bch->next_skb = NULL;
	}
#endif	
	bch->tx_idx = bch->tx_len = bch->rx_idx = 0;

	/* prevent overflow */
	if (len > hc->Zlen-1)
		len = hc->Zlen-1;

	/* select fifo */
	HFC_outb_(hc, R_FIFO, (ch<<1)|0x80 );
	HFC_wait_(hc);

	/* reset fifo */
	HFC_outb(hc, A_SUBCH_CFG, 0);
	udelay(500);
	HFC_outb_(hc, R_INC_RES_FIFO, V_RES_F);
	HFC_wait_(hc);
	udelay(500);

	/* if off */
	if (len <= 0)
	{
		HFC_outb_(hc, A_FIFO_DATA0_NOINC, ashfc_silence);
		if (hc->chan[ch].slot_tx>=0)
		{
#if WITH_ASHFC_DEBUG_CTRL
//			if (ashfc_debug & ASHFC_DEBUG_MODE)
				printk(KERN_ERR  "%s: connecting PCM due to no more TONE: channel %d slot_tx %d\n", __FUNCTION__, ch, hc->chan[ch].slot_tx);
#endif			
			/* connect slot */
			HFC_outb(hc, A_CON_HDLC, 0xc0 | 0x00 | V_HDLC_TRP | V_IFF);
			HFC_outb(hc, R_FIFO, (ch<<1 | 1)|0x80 );
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, 0xc0 | 0x00 | V_HDLC_TRP | V_IFF);
		}
		hc->chan[ch].txpending = ASHFC_TXPENDING_NO;
		return;
	}

	/* loop fifo */

	/* set mode */
	hc->chan[ch].txpending = ASHFC_TXPENDING_SPLLOOP;

//printk("len=%d %02x %02x %02x\n", len, data[0], data[1], data[2]);
	/* write loop data */
	d = data;

#ifndef CONFIG_HFCMULTI_PCIMEM
	HFC_set(hc, A_FIFO_DATA0);
#endif
	dd = d + (len & 0xfffc);
	while(d != dd)
	{
#if CONFIG_HFCMULTI_PCIMEM
		HFC_outl_(hc, A_FIFO_DATA0, *((unsigned long *)d));
#else
		HFC_putl(hc, *((unsigned long *)d));
#endif
		d+=4;
		
	}
	dd = d + (len & 0x0003);

//	dd = d + len;

	while(d != dd)
	{
#if CONFIG_HFCMULTI_PCIMEM
		HFC_outb_(hc, A_FIFO_DATA0, *d);
#else
		HFC_putb(hc, *d);
#endif
		d++;
	}

	udelay(500);
	HFC_outb(hc, A_SUBCH_CFG, V_LOOP_FIFO);
	udelay(500);

	/* disconnect slot */
	if (hc->chan[ch].slot_tx>=0)
	{
#if WITH_ASHFC_DEBUG_CTRL
		if (ashfc_debug & ASHFC_DEBUG_MODE)
			printk(KERN_ERR  "%s: disconnecting PCM due to TONE: channel %d slot_tx %d\n", __FUNCTION__, ch, hc->chan[ch].slot_tx);
#endif		
		HFC_outb(hc, A_CON_HDLC, 0x80 | 0x00 | V_HDLC_TRP | V_IFF);
		HFC_outb(hc, R_FIFO, ((ch<<1) | 1|0x80));
		HFC_wait(hc);
		HFC_outb(hc, A_CON_HDLC, 0x80 | 0x00 | V_HDLC_TRP | V_IFF);
		HFC_outb(hc, R_FIFO, ((ch<<1)|0x80) );
		HFC_wait(hc);
	}
	else
	{
		/* change fifo */
		HFC_outb(hc, R_FIFO, ((ch<<1)|0x80));
		HFC_wait(hc);
	}

//udelay(300);
}


/*********************************************************/
/* select free channel and return OK(0), -EBUSY, -EINVAL */
/*********************************************************/
int ashfc_sel_free_B_chan(ashfc_t *hc, int ch, channel_info_t *ci)
{
	bchannel_t		*bch;
	ashfc_t		*hfc;
	as_isdn_stack_t		*bst;
	struct list_head	*head;
	int				cnr;
	int				port = hc->chan[ch].port;

	if (port < 0 || port>=hc->type)
	{
		printk(KERN_WARNING "%s: port(%d) out of range", __FUNCTION__, port);
		return(-EINVAL);
	}
	
	if (!ci)
		return(-EINVAL);
	ci->st.p = NULL;
	cnr=0;
	
	bst = hc->chan[ch].dch->inst.st;
	if (list_empty(&bst->childlist))
	{
		if ((bst->id & FLG_CLONE_STACK) &&	(bst->childlist.prev != &bst->childlist)) 
		{
			head = bst->childlist.prev;
		}
		else
		{
			printk(KERN_ERR "%s: invalid empty childlist (no clone) stid(%x) childlist(%p<-%p->%p)\n",__FUNCTION__, bst->id, bst->childlist.prev, &bst->childlist, bst->childlist.next);
			return(-EINVAL);
		}
	}
	else
		head = &bst->childlist;

	list_for_each_entry(bst, head, list)
	{
		if (cnr == ((hc->type==1)?30:2)) /* 30 or 2 B-channels */
		{
			printk(KERN_WARNING "%s: fatal error: more b-stacks than ports", __FUNCTION__);
			return(-EINVAL);
		}
		if(!bst->mgr)
		{
			int_errtxt("no mgr st(%p)", bst);
			return(-EINVAL);
		}
		hfc = bst->mgr->data;
		if (!hfc)
		{
			int_errtxt("no mgr->data st(%p)", bst);
			return(-EINVAL);
		}

		if (hc->type==1)
			bch = hc->chan[cnr + 1 + (cnr>=15)].bch;
		else
			bch = hc->chan[(port<<2) + cnr].bch;

		if (!(ci->channel & (~CHANNEL_NUMBER)))
		{
			/* only number is set */
			if ((ci->channel & 0x3) == (cnr + 1))
			{
				if (bch->protocol != ISDN_PID_NONE)
					return(-EBUSY);
				ci->st.p = bst;
				return(0);
			}
		}
		if ((ci->channel & (~CHANNEL_NUMBER)) == 0x00a18300)
		{
			if (bch->protocol == ISDN_PID_NONE)
			{
				ci->st.p = bst;
				return(0);
			}
		}
		cnr++;
	}
	return(-EBUSY);
}


