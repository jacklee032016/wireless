/* $Id: dsp_core.c,v 1.1.1.1 2006/11/29 08:55:19 lizhijie Exp $
 *
 * Author       Andreas Eversberg (jolly@jolly.de)
 * Based on source code structure by
 *		Karsten Keil (keil@isdn4linux.de)
 *
 *		This file is (c) under GNU PUBLIC LICENSE
 *		For changes and modifications please read
 *		../../../Documentation/isdn/mISDN.cert
 *
 * Thanks to    Karsten Keil (great drivers)
 *              Cologne Chip (great chips)
 *
 * This module does:
 *		Real-time tone generation
 *		DTMF detection
 *		Real-time cross-connection and conferrence
 *		Compensate jitter due to system load and hardware fault.
 *		All features are done in kernel space and will be realized
 *		using hardware, if available and supported by chip set.
 *		Blowfish encryption/decryption
 */

/* STRUCTURE:
 *
 * The dsp module provides layer 2 for b-channels (64kbit). It provides
 * transparent audio forwarding with special digital signal processing:
 *
 * - (1) generation of tones
 * - (2) detection of dtmf tones
 * - (3) crossconnecting and conferences
 * - (4) echo generation for delay test
 * - (5) volume control
 * - (6) disable receive data
 * - (7) encryption/decryption
 *
 * Look:
 *             TX            RX
 *         ------upper layer------
 *             |             ^
 *             |             |(6)
 *             v             |
 *       +-----+-------------+-----+
 *       |(3)(4)                   |
 *       |                         |
 *       |                         |
 *       |           CMX           |
 *       |                         |
 *       |                         |
 *       |                         |
 *       |                         |
 *       |           +-------------+
 *       |           |       ^
 *       |           |       |
 *       |           |       |
 *       |+---------+|  +----+----+
 *       ||(1)      ||  |(5)      |
 *       ||         ||  |         |
 *       ||  Tones  ||  |RX Volume|
 *       ||         ||  |         |
 *       ||         ||  |         |
 *       |+----+----+|  +----+----+
 *       +-----+-----+       ^
 *             |             | 
 *             |             |
 *             v             |
 *        +----+----+   +----+----+
 *        |(5)      |   |(2)      |
 *        |         |   |         |
 *        |TX Volume|   |  DTMF   |
 *        |         |   |         |
 *        |         |   |         |
 *        +----+----+   +----+----+
 *             |             ^ 
 *             |             |
 *             v             |
 *        +----+----+   +----+----+
 *        |(7)      |   |(7)      |
 *        |         |   |         |
 *        | Encrypt |   | Decrypt |
 *        |         |   |         |
 *        |         |   |         |
 *        +----+----+   +----+----+
 *             |             ^ 
 *             |             |
 *             v             |
 *         ------card  layer------
 *             TX            RX
 *
 * Above you can see the logical data flow. If software is used to do the
 * process, it is actually the real data flow. If hardware is used, data
 * may not flow, but hardware commands to the card, to provide the data flow
 * as shown.
 *
 * NOTE: The channel must be activated in order to make dsp work, even if
 * no data flow to the upper layer is intended. Activation can be done
 * after and before controlling the setting using PH_CONTROL requests.
 *
 * DTMF: Will be detected by hardware if possible. It is done before CMX 
 * processing.
 *
 * Tones: Will be generated via software if endless looped audio fifos are
 * not supported by hardware. Tones will override all data from CMX.
 * It is not required to join a conference to use tones at any time.
 *
 * CMX: Is transparent when not used. When it is used, it will do
 * crossconnections and conferences via software if not possible through
 * hardware. If hardware capability is available, hardware is used.
 *
 * Echo: Is generated by CMX and is used to check performane of hard and
 * software CMX.
 *
 * The CMX has special functions for conferences with one, two and more
 * members. It will allow different types of data flow. Receive and transmit
 * data to/form upper layer may be swithed on/off individually without loosing
 * features of CMX, Tones and DTMF.
 *
 * If all used features can be realized in hardware, and if transmit and/or
 * receive data ist disabled, the card may not send/receive any data at all.
 * Not receiving is usefull if only announcements are played. Not sending is
 * usefull if an answering machine records audio. Not sending and receiving is
 * usefull during most states of the call. If supported by hardware, tones
 * will be played without cpu load. Small PBXs and NT-Mode applications will
 * not need expensive hardware when processing calls.
 *
 *
 * LOCKING:
 *
 * When data is received from upper or lower layer (card), the complete dsp
 * module is locked by a global lock.  When data is ready to be transmitted
 * to a different layer, the module is unlocked. It is not allowed to hold a
 * lock outside own layer.
 * Reasons: Multiple threads must not process cmx at the same time, if threads
 * serve instances, that are connected in same conference.
 * PH_CONTROL must not change any settings, join or split conference members
 * during process of data.
 * 
 *
 * TRANSMISSION:
 *

TBD

There are three things that need to receive data from card:
 - software DTMF decoder
 - software cmx (if conference exists)
 - upper layer, if rx-data not disabled

Whenever dtmf decoder is turned on or off, software cmx changes, rx-data is disabled or enabled, or card becomes activated, then rx-data is disabled or enabled using a special command to the card.

There are three things that need to transmit data to card:
 - software tone generation (part of cmx)
 - software cmx
 - upper layer, if tx-data is written to tx-buffer

Whenever cmx is changed, or data is sent from upper layer, the transmission is triggered by an silence freame (if not already tx_pending==1). When the confirm is received from card, next frame is
sent using software cmx, if tx-data is still available, or if software tone generation is used,
or if cmx is currently using software.


 
 */

const char *dsp_revision = "$Revision: 1.1.1.1 $";

#include <linux/delay.h>
#include <linux/config.h>
#include <linux/module.h>
/* added by lizhijie*/
#include <linux/init.h>

#include <linux/vmalloc.h>
#include "layer1.h"
#include "helper.h"
#include "debug.h"
#include "dsp.h"
#include "hw_lock.h"

static char DSPName[] = "DSP";
mISDNobject_t dsp_obj;
mISDN_HWlock_t dsp_lock;

static int debug = 0;
int dsp_debug;
static int options = 0;
int dsp_options;
#ifndef AUTOJITTER
int poll = 0;
#endif

#ifdef MODULE
MODULE_AUTHOR("Andreas Eversberg");
MODULE_PARM(debug, "1i");
MODULE_PARM(options, "1i");
#ifndef AUTOJITTER
MODULE_PARM(poll, "1i");
#endif
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

/*
 * sending next frame to card (triggered by PH_DATA_IND)
 */
static void
sendevent(dsp_t *dsp)
{
	struct sk_buff *nskb;

	lock_HW(&dsp_lock, 0);
	if (dsp->b_active && dsp->tx_pending) {
		/* get data from cmx */
		nskb = dsp_cmx_send(dsp, dsp->tx_pending, 0);
		dsp->tx_pending = 0;
		if (!nskb) {
			unlock_HW(&dsp_lock);
			printk(KERN_ERR "%s: failed to create tx packet\n", __FUNCTION__);
			return;
		}
		/* crypt if enabled */
		if (dsp->bf_enable)
			dsp_bf_encrypt(dsp, nskb->data, nskb->len);
		/* change volume if requested */
		if (dsp->tx_volume)
			dsp_change_volume(nskb, dsp->tx_volume);
		/* send subsequent requests to card */
		unlock_HW(&dsp_lock);
		if (dsp->inst.down.func(&dsp->inst.down, nskb)) {
			dev_kfree_skb(nskb);
			printk(KERN_ERR "%s: failed to send tx packet\n", __FUNCTION__);
		}
	} else {
		dsp->tx_pending = 0;
		unlock_HW(&dsp_lock);
	}
}


/*
 * special message process for DL_CONTROL | REQUEST
 */
static int
dsp_control_req(dsp_t *dsp, mISDN_head_t *hh, struct sk_buff *skb)
{
	struct		sk_buff *nskb;
	int ret = 0;
	int cont;
	u8 *data;
	int len;

	if (skb->len < sizeof(int)) {
		printk(KERN_ERR "%s: PH_CONTROL message too short\n", __FUNCTION__);
	}
	cont = *((int *)skb->data);
	len = skb->len - sizeof(int);
	data = skb->data + sizeof(int);

	switch (cont) {
		case DTMF_TONE_START: /* turn on DTMF */
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: start dtmf\n", __FUNCTION__);
			dsp_dtmf_goertzel_init(dsp);
			/* checking for hardware capability */
			if (dsp->features.hfc_dtmf) {
				dsp->dtmf.hardware = 1;
				dsp->dtmf.software = 0;
			} else {
				dsp->dtmf.hardware = 0;
				dsp->dtmf.software = 1;
			}
			break;
		case DTMF_TONE_STOP: /* turn off DTMF */
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: stop dtmf\n", __FUNCTION__);
			dsp->dtmf.hardware = 0;
			dsp->dtmf.software = 0;
			break;
		case CMX_CONF_JOIN: /* join / update conference */
			if (len != sizeof(int)) {
				ret = -EINVAL;
				break;
			}
			if (*((u32 *)data) == 0)
				goto conf_split;
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: join conference %d\n", __FUNCTION__, *((u32 *)data));
			ret = dsp_cmx_conf(dsp, *((u32 *)data));
			if (dsp_debug & DEBUG_DSP_CMX)
				dsp_cmx_debug(dsp);
			break;
		case CMX_CONF_SPLIT: /* remove from conference */
			conf_split:
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: release conference\n", __FUNCTION__);
			ret = dsp_cmx_conf(dsp, 0);
			if (dsp_debug & DEBUG_DSP_CMX)
				dsp_cmx_debug(dsp);
			break;
		case TONE_PATT_ON: /* play tone */
			if (len != sizeof(int)) {
				ret = -EINVAL;
				break;
			}
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: turn tone 0x%x on\n", __FUNCTION__, *((int *)skb->data));
			ret = dsp_tone(dsp, *((int *)data));
			if (!ret)
				dsp_cmx_hardware(dsp->conf, dsp);
			if (!dsp->tone.tone)
				goto tone_off;
			break;
		case TONE_PATT_OFF: /* stop tone */
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: turn tone off\n", __FUNCTION__);
			dsp_tone(dsp, 0);
			dsp_cmx_hardware(dsp->conf, dsp);
			/* reset tx buffers (user space data) */
			tone_off:
			dsp->R_tx = dsp->W_tx = 0;
			break;
		case VOL_CHANGE_TX: /* change volume */
			if (len != sizeof(int)) {
				ret = -EINVAL;
				break;
			}
			dsp->tx_volume = *((int *)data);
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: change tx volume to %d\n", __FUNCTION__, dsp->tx_volume);
			dsp_cmx_hardware(dsp->conf, dsp);
			break;
		case VOL_CHANGE_RX: /* change volume */
			if (len != sizeof(int)) {
				ret = -EINVAL;
				break;
			}
			dsp->rx_volume = *((int *)data);
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: change rx volume to %d\n", __FUNCTION__, dsp->tx_volume);
			dsp_cmx_hardware(dsp->conf, dsp);
			break;
		case CMX_ECHO_ON: /* enable echo */
			dsp->echo = 1; /* soft echo */
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: enable cmx-echo\n", __FUNCTION__);
			dsp_cmx_hardware(dsp->conf, dsp);
			if (dsp_debug & DEBUG_DSP_CMX)
				dsp_cmx_debug(dsp);
			break;
		case CMX_ECHO_OFF: /* disable echo */
			dsp->echo = 0;
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: disable cmx-echo\n", __FUNCTION__);
			dsp_cmx_hardware(dsp->conf, dsp);
			if (dsp_debug & DEBUG_DSP_CMX)
				dsp_cmx_debug(dsp);
			break;
		case CMX_RECEIVE_ON: /* enable receive to user space */
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: enable receive to user space\n", __FUNCTION__);
			dsp->rx_disabled = 0;
			dsp_cmx_hardware(dsp->conf, dsp);
			break;
		case CMX_RECEIVE_OFF: /* disable receive to user space */
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: disable receive to user space\n", __FUNCTION__);
			dsp->rx_disabled = 1;
			dsp_cmx_hardware(dsp->conf, dsp);
			break;
		case CMX_MIX_ON: /* enable mixing of transmit data with conference members */
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: enable mixing of tx-data with conf mebers\n", __FUNCTION__);
			dsp->tx_mix = 1;
			dsp_cmx_hardware(dsp->conf, dsp);
			if (dsp_debug & DEBUG_DSP_CMX)
				dsp_cmx_debug(dsp);
			break;
		case CMX_MIX_OFF: /* disable mixing of transmit data with conference members */
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: disable mixing of tx-data with conf mebers\n", __FUNCTION__);
			dsp->tx_mix = 0;
			dsp_cmx_hardware(dsp->conf, dsp);
			if (dsp_debug & DEBUG_DSP_CMX)
				dsp_cmx_debug(dsp);
			break;
		case BF_ENABLE_KEY: /* turn blowfish on */
			if (len<4 || len>56) {
				ret = -EINVAL;
				break;
			}
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: turn blowfish on (key not shown)\n", __FUNCTION__);
			ret = dsp_bf_init(dsp, (u8*)data, len);
			/* set new cont */
			if (!ret)
				cont = BF_ACCEPT;
			else
				cont = BF_REJECT;
			/* send indication if it worked to set it */
			nskb = create_link_skb(PH_CONTROL | INDICATION, 0, sizeof(int), &cont, 0);
			unlock_HW(&dsp_lock);
			if (nskb) {
				if (dsp->inst.up.func(&dsp->inst.up, nskb))
					dev_kfree_skb(nskb);
			}
			lock_HW(&dsp_lock, 0);
			if (!ret)
				dsp_cmx_hardware(dsp->conf, dsp);
			break;
		case BF_DISABLE: /* turn blowfish off */
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: turn blowfish off\n", __FUNCTION__);
			dsp_bf_cleanup(dsp);
			dsp_cmx_hardware(dsp->conf, dsp);
			break;
		default:
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: ctrl req %x unhandled\n", __FUNCTION__, cont);
			ret = -EINVAL;
	}
	return(ret);
}


/*
 * messages from upper layers
 */
static int
dsp_from_up(mISDNif_t *hif, struct sk_buff *skb)
{
	dsp_t			*dsp;
	mISDN_head_t		*hh;
	int			ret = 0;

	if (!hif || !hif->fdata || !skb)
		return(-EINVAL);
	dsp = hif->fdata;
	if (!dsp->inst.down.func)
		return(-ENXIO);

	hh = mISDN_HEAD_P(skb);
	switch(hh->prim) {
		case DL_DATA | RESPONSE:
		case PH_DATA | RESPONSE:
			/* ignore response */
			break;
		case DL_DATA | REQUEST:
		case PH_DATA | REQUEST:
			lock_HW(&dsp_lock, 0);
			/* send data to tx-buffer (if no tone is played) */
			if (!dsp->tone.tone)
				dsp_cmx_transmit(dsp, skb);
			unlock_HW(&dsp_lock);
			break;
		case PH_CONTROL | REQUEST:
			lock_HW(&dsp_lock, 0);
			ret = dsp_control_req(dsp, hh, skb);
			unlock_HW(&dsp_lock);
			break;
		case DL_ESTABLISH | REQUEST:
		case PH_ACTIVATE | REQUEST:
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: activating b_channel %s\n", __FUNCTION__, dsp->inst.name);
			lock_HW(&dsp_lock, 0);
			dsp->tx_pending = 0;
			if (dsp->dtmf.hardware || dsp->dtmf.software)
				dsp_dtmf_goertzel_init(dsp);
			unlock_HW(&dsp_lock);
			hh->prim = PH_ACTIVATE | REQUEST;
			return(dsp->inst.down.func(&dsp->inst.down, skb));
		case DL_RELEASE | REQUEST:
		case PH_DEACTIVATE | REQUEST:
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: releasing b_channel %s\n", __FUNCTION__, dsp->inst.name);
			lock_HW(&dsp_lock, 0);
			dsp->tx_pending = 0;
			dsp->tone.tone = dsp->tone.hardware = dsp->tone.software = 0;
			if (timer_pending(&dsp->tone.tl))
				del_timer(&dsp->tone.tl);
			unlock_HW(&dsp_lock);
			hh->prim = PH_DEACTIVATE | REQUEST;
			return(dsp->inst.down.func(&dsp->inst.down, skb));
		default:
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: msg %x unhandled %s\n", __FUNCTION__, hh->prim, dsp->inst.name);
			ret = -EINVAL;
			break;
	}
	if (!ret)
		dev_kfree_skb(skb);
	return(ret);
}


/*
 * messages from lower layers
 */
static int
dsp_from_down(mISDNif_t *hif,  struct sk_buff *skb)
{
	dsp_t		*dsp;
	mISDN_head_t	*hh;
	int		ret = 0;
	u8		*digits;
	int 		cont;
	struct		sk_buff *nskb;

	if (!hif || !hif->fdata || !skb)
		return(-EINVAL);
	dsp = hif->fdata;
	if (!dsp->inst.up.func)
		return(-ENXIO);

	hh = mISDN_HEAD_P(skb);
	switch(hh->prim)
	{
		case PH_DATA | CONFIRM:
		case DL_DATA | CONFIRM:
			break;
		case PH_DATA | INDICATION:
		case DL_DATA | INDICATION:
			if (skb->len < 1)
				break;
			lock_HW(&dsp_lock, 0);
			/* decrypt if enabled */
			if (dsp->bf_enable)
				dsp_bf_decrypt(dsp, skb->data, skb->len);
			/* check if dtmf soft decoding is turned on */
			if (dsp->dtmf.software) {
				digits = dsp_dtmf_goertzel_decode(dsp, skb->data, skb->len, (dsp_options&DSP_OPT_ULAW)?1:0);
				if (digits) while(*digits) {
					if (dsp_debug & DEBUG_DSP_DTMF)
						printk(KERN_DEBUG "%s: sending software decoded digit(%c) to upper layer %s\n", __FUNCTION__, *digits, dsp->inst.name);
					cont = DTMF_TONE_VAL | *digits;
					nskb = create_link_skb(PH_CONTROL | INDICATION, 0, sizeof(int), &cont, 0);
					unlock_HW(&dsp_lock);
					if (!nskb) {
						lock_HW(&dsp_lock, 0);
						break;
					}
					if (dsp->inst.up.func(&dsp->inst.up, nskb))
						dev_kfree_skb(nskb);
					lock_HW(&dsp_lock, 0);
					digits++;
				}
			}
			/* change volume if requested */
			if (dsp->rx_volume)
				dsp_change_volume(skb, dsp->rx_volume);
			/* we need to process receive data if software */
			if (dsp->pcm_slot_tx<0 && dsp->pcm_slot_rx<0) {
				/* process data from card at cmx */
				dsp_cmx_receive(dsp, skb);
			}
			/* we send data only if software or if we have some
			 * or if we cannot do tones with hardware
			 */
			if ((dsp->pcm_slot_tx<0 && !dsp->features.hfc_loops) /* software crossconnects OR software loops */
			 || dsp->R_tx != dsp->W_tx /* data in buffer */
			 || (dsp->echo==1 && dsp->pcm_slot_tx<0) /* software echo */
			 || (dsp->tone.tone && dsp->tone.software)) { /* software loops */
				/* schedule sending skb->len bytes */
				dsp->tx_pending = skb->len;
				schedule_work(&dsp->sendwork);
			}
			if (dsp->rx_disabled) {
				/* if receive is not allowed */
				dev_kfree_skb(skb);
				unlock_HW(&dsp_lock);
				return(0);
			}
			unlock_HW(&dsp_lock);
			hh->prim = DL_DATA | INDICATION;
			return(dsp->inst.up.func(&dsp->inst.up, skb));
		case PH_CONTROL | INDICATION:
			lock_HW(&dsp_lock, 0);
			if (dsp_debug & DEBUG_DSP_DTMFCOEFF)
				printk(KERN_DEBUG "%s: PH_CONTROL received: %x (len %d) %s\n", __FUNCTION__, hh->dinfo, skb->len, dsp->inst.name);
			switch (hh->dinfo) {
				case HW_HFC_COEFF: /* getting coefficients */
				if (!dsp->dtmf.hardware) {
					if (dsp_debug & DEBUG_DSP_DTMFCOEFF)
						printk(KERN_DEBUG "%s: ignoring DTMF coefficients from HFC\n", __FUNCTION__);
					break;
				}
				if (dsp->inst.up.func) {
					digits = dsp_dtmf_goertzel_decode(dsp, skb->data, skb->len, 2);
					if (digits) while(*digits) {
						int k;
						struct sk_buff *nskb;
						if (dsp_debug & DEBUG_DSP_DTMF)
							printk(KERN_DEBUG "%s: now sending software decoded digit(%c) to upper layer %s\n", __FUNCTION__, *digits, dsp->inst.name);
						k = *digits | DTMF_TONE_VAL;
						nskb = create_link_skb(PH_CONTROL | INDICATION, 0, sizeof(int), &k, 0);
						unlock_HW(&dsp_lock);
						if (!nskb) {
							lock_HW(&dsp_lock, 0);
							break;
						}
						if (dsp->inst.up.func(&dsp->inst.up, nskb))
							dev_kfree_skb(nskb);
						lock_HW(&dsp_lock, 0);
						digits++;
					}
				}
				break;

				default:
				if (dsp_debug & DEBUG_DSP_CORE)
					printk(KERN_DEBUG "%s: ctrl ind %x unhandled %s\n", __FUNCTION__, hh->dinfo, dsp->inst.name);
				ret = -EINVAL;
			}
			unlock_HW(&dsp_lock);
			break;
		case PH_ACTIVATE | CONFIRM:
			lock_HW(&dsp_lock, 0);
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: b_channel is now active %s\n", __FUNCTION__, dsp->inst.name);
			/* bchannel now active */
			dsp->b_active = 1;
			dsp->W_tx = dsp->R_tx = 0; /* clear TX buffer */
			dsp->W_rx = dsp->R_rx = 0; /* clear RX buffer */
			if (dsp->conf)
				dsp->W_rx = dsp->R_rx = dsp->conf->W_max;
			memset(dsp->rx_buff, 0, sizeof(dsp->rx_buff));
			dsp_cmx_hardware(dsp->conf, dsp);
//			/* now trigger transmission */
//#ifndef AUTOJITTER
//			dsp->tx_pending = 1;
//			schedule_work(&dsp->sendwork);
//#endif
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: done with activation, sending confirm to user space. %s\n", __FUNCTION__, dsp->inst.name);
			/* send activation to upper layer */
			hh->prim = DL_ESTABLISH | CONFIRM;
			unlock_HW(&dsp_lock);
			return(dsp->inst.up.func(&dsp->inst.up, skb));
		case PH_DEACTIVATE | CONFIRM:
			lock_HW(&dsp_lock, 0);
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: b_channel is now inactive %s\n", __FUNCTION__, dsp->inst.name);
			/* bchannel now inactive */
			dsp->b_active = 0;
			dsp_cmx_hardware(dsp->conf, dsp);
			hh->prim = DL_RELEASE | CONFIRM;
			unlock_HW(&dsp_lock);
			return(dsp->inst.up.func(&dsp->inst.up, skb));
		default:
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: msg %x unhandled %s\n", __FUNCTION__, hh->prim, dsp->inst.name);
			ret = -EINVAL;
	}
	if (!ret)
		dev_kfree_skb(skb);
	return(ret);
}


/*
 * desroy DSP instances
 */
static void
release_dsp(dsp_t *dsp)
{
	mISDNinstance_t	*inst = &dsp->inst;
	conference_t *conf;

	lock_HW(&dsp_lock, 0);
	if (timer_pending(&dsp->tone.tl))
		del_timer(&dsp->tone.tl);
#ifdef HAS_WORKQUEUE
	if (dsp->sendwork.pending)
		printk(KERN_ERR "%s: pending sendwork: %lx %s\n", __FUNCTION__, dsp->sendwork.pending, dsp->inst.name);
#else
	if (dsp->sendwork.sync)
		printk(KERN_ERR "%s: pending sendwork: %lx %s\n", __FUNCTION__, dsp->sendwork.sync, dsp->inst.name);
#endif
	if (dsp_debug & DEBUG_DSP_MGR)
		printk(KERN_DEBUG "%s: removing conferences %s\n", __FUNCTION__, dsp->inst.name);
	conf = dsp->conf;
	if (conf) {
		dsp_cmx_del_conf_member(dsp);
		if (!list_empty(&conf->mlist)) {
			dsp_cmx_del_conf(conf);
		}
	}

	if (dsp_debug & DEBUG_DSP_MGR)
		printk(KERN_DEBUG "%s: disconnecting instances %s\n", __FUNCTION__, dsp->inst.name);
	if (inst->up.peer) {
		inst->up.peer->obj->ctrl(inst->up.peer,
			MGR_DISCONNECT | REQUEST, &inst->up);
	}
	if (inst->down.peer) {
		inst->down.peer->obj->ctrl(inst->down.peer,
			MGR_DISCONNECT | REQUEST, &inst->down);
	}

	if (dsp_debug & DEBUG_DSP_MGR)
		printk(KERN_DEBUG "%s: remove & destroy object %s\n", __FUNCTION__, dsp->inst.name);
	list_del(&dsp->list);
	dsp_obj.ctrl(inst, MGR_UNREGLAYER | REQUEST, NULL);
	unlock_HW(&dsp_lock);
	vfree(dsp);

	if (dsp_debug & DEBUG_DSP_MGR)
		printk(KERN_DEBUG "%s: dsp instance released\n", __FUNCTION__);
}


/*
 * create new DSP instances
 */
static int
new_dsp(mISDNstack_t *st, mISDN_pid_t *pid) 
{
	int err = 0;
	dsp_t *ndsp;

	if (dsp_debug & DEBUG_DSP_MGR)
		printk(KERN_DEBUG "%s: creating new dsp instance\n", __FUNCTION__);

	if (!st || !pid)
		return(-EINVAL);
	if (!(ndsp = vmalloc(sizeof(dsp_t)))) {
		printk(KERN_ERR "%s: vmalloc dsp_t failed\n", __FUNCTION__);
		return(-ENOMEM);
	}
	memset(ndsp, 0, sizeof(dsp_t));
	memcpy(&ndsp->inst.pid, pid, sizeof(mISDN_pid_t));
	ndsp->inst.obj = &dsp_obj;
	ndsp->inst.data = ndsp;
	if (!mISDN_SetHandledPID(&dsp_obj, &ndsp->inst.pid)) {
		int_error();
		err = -ENOPROTOOPT;
		free_mem:
		vfree(ndsp);
		return(err);
	}
	sprintf(ndsp->inst.name, "DSP_S%x/C%x",
		(st->id&0xff), (st->id&0xff00)>>8);
	ndsp->inst.up.owner = &ndsp->inst;
	ndsp->inst.down.owner = &ndsp->inst;
//#ifndef AUTOJITTER
	/* set frame size to start */
	ndsp->largest = 64 << 1;
//#endif
	ndsp->features.hfc_id = -1; /* current PCM id */
	ndsp->features.pcm_id = -1; /* current PCM id */
	ndsp->pcm_slot_rx = -1; /* current CPM slot */
	ndsp->pcm_slot_tx = -1;
	ndsp->pcm_bank_rx = -1;
	ndsp->pcm_bank_tx = -1;
	ndsp->hfc_conf = -1; /* current conference number */
	/* set timer */
	ndsp->tone.tl.function = (void *)dsp_tone_timeout;
	ndsp->tone.tl.data = (long) ndsp;
	init_timer(&ndsp->tone.tl);
	/* init send que */
	INIT_WORK(&ndsp->sendwork, (void *)(void *)sendevent, ndsp);
	lock_HW(&dsp_lock, 0);
	/* append and register */
	list_add_tail(&ndsp->list, &dsp_obj.ilist);
	err = dsp_obj.ctrl(st, MGR_REGLAYER | INDICATION, &ndsp->inst);
	if (err) {
		printk(KERN_ERR "%s: failed to register layer %s\n", __FUNCTION__, ndsp->inst.name);
		list_del(&ndsp->list);
		unlock_HW(&dsp_lock);
		goto free_mem;
	}
	if (dsp_debug & DEBUG_DSP_MGR)
		printk(KERN_DEBUG "%s: dsp instance created %s\n", __FUNCTION__, ndsp->inst.name);
	unlock_HW(&dsp_lock);
	return(err);
}


/*
 * ask for hardware features
 */
static void
dsp_feat(dsp_t *dsp)
{
	struct sk_buff *nskb;
	void *feat;

	if (!(dsp_options & DSP_OPT_NOHARDWARE)) {
		feat = &dsp->features;
		nskb = create_link_skb(PH_CONTROL | REQUEST, HW_FEATURES, sizeof(feat), &feat, 0);
		if (nskb) {
			if (dsp->inst.down.func(&dsp->inst.down, nskb)) {
				dev_kfree_skb(nskb);
				if (dsp_debug & DEBUG_DSP_MGR)
					printk(KERN_DEBUG "%s: no features supported by %s\n", __FUNCTION__, dsp->inst.name);
			} else {
				if (dsp_debug & DEBUG_DSP_MGR)
					printk(KERN_DEBUG "%s: features of %s: hfc_id=%d hfc_dtmf=%d hfc_loops=%d pcm_id=%d pcm_slots=%d pcm_banks=%d\n",
					 __FUNCTION__, dsp->inst.name,
					 dsp->features.hfc_id,
					 dsp->features.hfc_dtmf,
					 dsp->features.hfc_loops,
					 dsp->features.pcm_id,
					 dsp->features.pcm_slots,
					 dsp->features.pcm_banks);
			}
		}
	}
}


/*
 * manager for DSP instances
 */
static int
dsp_manager(void *data, u_int prim, void *arg) {
	mISDNinstance_t *inst = data;
	dsp_t *dspl;
	int ret = -EINVAL;

	if (dsp_debug & DEBUG_DSP_MGR)
		printk(KERN_DEBUG "%s: data:%p prim:%x arg:%p\n", __FUNCTION__, data, prim, arg);
	if (!data)
		return(ret);
	list_for_each_entry(dspl, &dsp_obj.ilist, list) {
		if (&dspl->inst == inst) {
			ret = 0;
			break;
		}
	}
	if (ret && (prim != (MGR_NEWLAYER | REQUEST))) {
		printk(KERN_WARNING "%s: given instance(%p) not in ilist.\n", __FUNCTION__, data);
		return(ret);
	}

	switch(prim) {
	    case MGR_NEWLAYER | REQUEST:
		ret = new_dsp(data, arg);
		break;
	    case MGR_CONNECT | REQUEST:
		ret = mISDN_ConnectIF(inst, arg);
		dsp_feat(dspl);
		break;
	    case MGR_SETIF | REQUEST:
	    case MGR_SETIF | INDICATION:
		ret = mISDN_SetIF(inst, arg, prim, dsp_from_up, dsp_from_down, dspl);
		break;
	    case MGR_DISCONNECT | REQUEST:
	    case MGR_DISCONNECT | INDICATION:
		ret = mISDN_DisConnectIF(inst, arg);
		break;
	    case MGR_UNREGLAYER | REQUEST:
	    case MGR_RELEASE | INDICATION:
		if (dsp_debug & DEBUG_DSP_MGR)
			printk(KERN_DEBUG "%s: release_dsp id %x\n", __FUNCTION__, dspl->inst.st->id);

	    	release_dsp(dspl);
	    	break;
	    default:
		printk(KERN_WARNING "%s: prim %x not handled\n", __FUNCTION__, prim);
		ret = -EINVAL;
		break;
	}
	return(ret);
}


/*
 * initialize DSP object
 */
static int dsp_init(void)
{
	int err;

	/* copy variables */
	dsp_options = options;
	dsp_debug = debug;

	/* display revision */
	printk(KERN_INFO "mISDN_dsp: Audio DSP  Rev. %s (debug=0x%x)\n", mISDN_getrev(dsp_revision), debug);

#ifndef AUTOJITTER
	/* set packet size */
	switch(poll) {
		case 8:
		break;
		case 16:
		break;
		case 32:
		break;
		case 64: case 0:
		poll = 64;
		break;
		case 128:
		break;
		case 256:
		break;
		default:
		printk(KERN_ERR "%s: Wrong poll value (%d).\n", __FUNCTION__, poll);
		err = -EINVAL;
		return(err);
		
	}
#endif

	/* fill mISDN object (dsp_obj) */
	memset(&dsp_obj, 0, sizeof(dsp_obj));
#ifdef MODULE
	SET_MODULE_OWNER(&dsp_obj);
#endif
	dsp_obj.name = DSPName;
	dsp_obj.BPROTO.protocol[3] = ISDN_PID_L3_B_DSP;
	dsp_obj.own_ctrl = dsp_manager;
	INIT_LIST_HEAD(&dsp_obj.ilist);

	/* initialize audio tables */
	silence = (dsp_options&DSP_OPT_ULAW)?0xff:0x2a;
	dsp_audio_law_to_s32 = (dsp_options&DSP_OPT_ULAW)?dsp_audio_ulaw_to_s32:dsp_audio_alaw_to_s32;
	dsp_audio_generate_s2law_table();
	dsp_audio_generate_seven();
	dsp_audio_generate_mix_table();
	if (dsp_options & DSP_OPT_ULAW)
		dsp_audio_generate_ulaw_samples();
	dsp_audio_generate_volume_changes();

	/* init global lock */
	lock_HW_init(&dsp_lock);

	/* register object */
	if ((err = mISDN_register(&dsp_obj))) {
		printk(KERN_ERR "mISDN_dsp: Can't register %s error(%d)\n", DSPName, err);
		return(err);
	}

	return(0);
}


/*
 * cleanup DSP object during module removal
 */
static void dsp_cleanup(void)
{
	dsp_t	*dspl, *nd;	
	int	err;

	if (dsp_debug & DEBUG_DSP_MGR)
		printk(KERN_DEBUG "%s: removing module\n", __FUNCTION__);

	if ((err = mISDN_unregister(&dsp_obj))) {
		printk(KERN_ERR "mISDN_dsp: Can't unregister Audio DSP error(%d)\n", 
			err);
	}
	if (!list_empty(&dsp_obj.ilist)) {
		printk(KERN_WARNING "mISDN_dsp: Audio DSP object inst list not empty.\n");
		list_for_each_entry_safe(dspl, nd, &dsp_obj.ilist, list)
			release_dsp(dspl);
	}
	if (!list_empty(&Conf_list)) {
		printk(KERN_ERR "mISDN_dsp: Conference list not empty. Not all memory freed.\n");
	}
}

#ifdef MODULE
module_init(dsp_init);
module_exit(dsp_cleanup);
#endif

