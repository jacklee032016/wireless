/*****************************************************************************
 *  Copyright 2005, Univerity of Colorado at Boulder.                        *
 *                                                                           *
 *                        All Rights Reserved                                *
 *                                                                           *
 *  Permission to use, copy, modify, and distribute this software and its    *
 *  documentation for any purpose other than its incorporation into a        *
 *  commercial product is hereby granted without fee, provided that the      *
 *  above copyright notice appear in all copies and that both that           *
 *  copyright notice and this permission notice appear in supporting         *
 *  documentation, and that the name of the University not be used in        *
 *  advertising or publicity pertaining to distribution of the software      *
 *  without specific, written prior permission.                              *
 *                                                                           *
 *  UNIVERSITY OF COLORADO DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS      *
 *  SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND        *
 *  FITNESS FOR ANY PARTICULAR PURPOSE.  IN NO EVENT SHALL THE UNIVERSITY    *
 *  OF COLORADO BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL         *
 *  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA       *
 *  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER        *
 *  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR         *
 *  PERFORMANCE OF THIS SOFTWARE.                                            *
 *                                                                           * 
 ****************************************************************************/

/*
 * This is RemoteMAC
 *  adds atheros phy control header to received packets
 *  executes atheros phy control header operations on outgoing packets
 */

#include <linux/module.h>
#if __ARM__
#else
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include "softmac_remotemac.h"
#include "cu_softmac_api.h"
#include "cu_softmac_ath_api.h"
#include "ktund/ktund.h"

MODULE_LICENSE("GPL");

/* private instance data */
struct remotemac_instance
{
	vmac_mac_t *macinfo;
	vmac_phy_t *phyinfo;
	vmac_phy_t *dummyphy;
	mac_rx_func_t netif_rx;
	void *netif_rx_priv;

	struct tasklet_struct rxq_tasklet;
	struct sk_buff_head   rxq;

	int id;
	rwlock_t lock;
};

struct remotemac_rx_header
{
	u_int8_t crcerr_id;
	u_int8_t crcerr;
	u_int8_t channel_id;
	u_int8_t channel;
	u_int8_t rssi_id;
	u_int8_t rssi;
	u_int8_t rate_id;
	u_int8_t rate;
	u_int8_t mactime_id;
	u_int64_t mactime;
};

static int remotemac_next_id;

static const char *remotemac_name = "remotemac";

/**
 * Every VMAC MAC or PHY layer provides a vmac_layer_t interface
 */
static vmac_layer_t the_remotemac;

/* detach from phy layer */
static int remotemac_mac_detach(void *me)
{
	//printk("%s\n", __func__);
	struct remotemac_instance *inst = me;
	unsigned long flags;

	write_lock_irqsave(&(inst->lock), flags);
	if (inst->phyinfo != inst->dummyphy)
	{
		vmac_free_phy(inst->phyinfo);
		inst->phyinfo = inst->dummyphy;
	}
	write_unlock_irqrestore(&(inst->lock), flags);

	return 0;
}

/* attach to a phy layer */
static int remotemac_mac_attach(void *me, vmac_phy_t *phyinfo)
{
	//printk("%s\n", __func__);
	struct remotemac_instance *inst = me;
	unsigned long flags;

	remotemac_mac_detach(me);
	phyinfo = vmac_get_phy(phyinfo);
	if (phyinfo)
	{
		write_lock_irqsave(&(inst->lock), flags);
		inst->phyinfo = phyinfo;
		write_unlock_irqrestore(&(inst->lock), flags);
	}
	return 0;
}

static int remotemac_mac_set_netif_rx_func(void *me, mac_rx_func_t rxfunc, void *rxpriv) 
{
	//printk("%s\n", __func__);
	struct remotemac_instance *inst = me;
	unsigned long flags;

	write_lock_irqsave(&(inst->lock), flags);
	// rxpriv == 0 is true when ktund is the caller
	// this is a hack to prevent others (multimac) from
	// setting netif_rx to something else
	if (rxpriv == 0)
	{
		inst->netif_rx = rxfunc;
		inst->netif_rx_priv = rxpriv;
		//printk("%s set %p\n", __func__, rxfunc);
	}
	write_unlock_irqrestore(&(inst->lock), flags);

	return 0;
}

static int remotemac_mac_packet_tx(void *me, struct sk_buff *skb, int intop)
{
	//printk("%s\n", __func__);
	struct remotemac_instance *inst = me; 
	struct ktund_packet_hdr *khdr = (struct ktund_packet_hdr *)skb->data;

	// assume intop == 0 for linux
	BUG_ON(intop);
	if (khdr->sync != KTUND_SYNC)
	{
		/* packet not remote mac format */
		//printk("%s not remotemac format\n", __func__);
		kfree_skb(skb);
		return 0;
	}

	skb_pull(skb, sizeof(struct ktund_packet_hdr));

	read_lock(&(inst->lock));
	vmac_ath_set_default_phy_props(inst->phyinfo->phy_private, skb);

	if (khdr->ctl_len > 0)
	{
		u_int8_t *ctl = skb->data;     /* start of control data */
		u_int16_t len = khdr->ctl_len; /* length of control data */

		/* remove control data from the packet */
		skb_pull(skb, len);

		while (len > 0)
		{
			u_int8_t opc = *ctl;
			u_int8_t arg8;
			//u_int16_t arg16;
			//u_int32_t arg32;

			ctl += 1;
			len -= 1;

			//printk("opc %d\n", (int)opc);
			switch (opc)
			{
				case CU_SOFTMAC_REMOTE_TX_NOOP:
					//printk("remote tx noop\n");
					break;
				case CU_SOFTMAC_REMOTE_TX_RATE:
					/* set the rate for this packet */
					arg8 = *ctl;
					ctl += 1;
					len -= 1;
					vmac_ath_set_tx_bitrate(inst->phyinfo->phy_private, skb, arg8);
					break;

				default:
					printk("%s  flag %d undefined\n", __func__, opc);
					break;
			}
		}
	}

	if (khdr->data_len > 0)
	{
		inst->phyinfo->phy_sendpacket(inst->phyinfo->phy_private, 1, skb);
	}
	else
	{
		kfree_skb(skb);
	}

	read_unlock(&(inst->lock));
	return 0;
}

static void remotemac_rx_tasklet(unsigned long data)
{
	//printk("%s\n", __func__);
	struct remotemac_instance *inst = (struct remotemac_instance *)data;
	struct sk_buff *skb;

	while ( (skb = skb_dequeue(&inst->rxq)) )
	{
		struct remotemac_rx_header *hdr;
		struct ktund_packet_hdr *khdr;

		int len = skb->len;
		skb_cow(skb, sizeof(struct remotemac_rx_header) + sizeof(struct ktund_packet_hdr));
		hdr = (struct remotemac_rx_header *) skb_push(skb, sizeof(struct remotemac_rx_header));
		khdr = (struct ktund_packet_hdr *) skb_push(skb, sizeof(struct ktund_packet_hdr));

		khdr->sync = KTUND_SYNC;
		khdr->ctl_len = sizeof(struct remotemac_rx_header);
		khdr->data_len = len;

		read_lock(&(inst->lock));

		hdr->crcerr_id = CU_SOFTMAC_REMOTE_RX_CRCERR;
		hdr->crcerr = vmac_ath_has_rx_crc_error(inst->phyinfo->phy_private, skb);

		hdr->channel_id = CU_SOFTMAC_REMOTE_RX_CHANNEL;
		hdr->channel = vmac_ath_get_rx_channel(inst->phyinfo->phy_private, skb);

		hdr->rssi_id = CU_SOFTMAC_REMOTE_RX_RSSI;
		hdr->rssi = vmac_ath_get_rx_rssi(inst->phyinfo->phy_private, skb);

		hdr->rate_id = CU_SOFTMAC_REMOTE_RX_RATE;
		hdr->rate = vmac_ath_get_rx_bitrate(inst->phyinfo->phy_private, skb);

		hdr->mactime_id = CU_SOFTMAC_REMOTE_RX_TIME;
		hdr->mactime = vmac_ath_get_rx_time(inst->phyinfo->phy_private, skb);

		/* remote crc? */
		//skb_trim(skb, skb->len - 4/*IEEE80211_CRC_LEN*/);

		if (inst->netif_rx)
			inst->netif_rx(inst->netif_rx_priv, skb);
		else
			inst->phyinfo->phy_free_skb(inst->phyinfo->phy_private, skb);

		read_unlock(&(inst->lock));
	}
}

static int remotemac_mac_packet_rx(void *me, struct sk_buff *skb, int intop)
{
	//printk("%s\n", __func__);
	struct remotemac_instance *inst = me;

	skb_queue_tail(&(inst->rxq), skb);
	if (intop)
		tasklet_schedule(&(inst->rxq_tasklet));
	else
		remotemac_rx_tasklet((unsigned long)me);

	return 0;
}

struct sk_buff *remotemac_alloc_skb(void *me, u_int len)
{
	//printk("%s\n", __func__);
	struct remotemac_instance *inst = me;
	struct sk_buff *skb;

	read_lock(&(inst->lock));
	skb = inst->phyinfo->phy_alloc_skb(inst->phyinfo->phy_private, len);
	read_unlock(&(inst->lock));

	return skb;
}
				
/* create and return a new remotemac instance */
static void *remotemac_new_instance (void *layer_priv)
{
	//printk("%s\n", __func__);
	struct remotemac_instance *inst;
	void *ret = 0;

	inst = kmalloc(sizeof(struct remotemac_instance), GFP_ATOMIC);
	if (inst)
	{
		memset(inst, 0, sizeof(struct remotemac_instance));

		inst->lock = RW_LOCK_UNLOCKED;
		inst->id = remotemac_next_id++;

		/* setup the macinfo structure */
		inst->macinfo = vmac_alloc_mac();
		inst->macinfo->mac_private = inst;
		inst->macinfo->layer = &the_remotemac;
		snprintf(inst->macinfo->name, VMAC_NAME_SIZE, "%s%d", the_remotemac.name, inst->id);

		/* override some macinfo functions */
		inst->macinfo->mac_tx = remotemac_mac_packet_tx;
		inst->macinfo->rx_packet = remotemac_mac_packet_rx;
		inst->macinfo->attach_phy = remotemac_mac_attach;
		inst->macinfo->detach_phy = remotemac_mac_detach;
		inst->macinfo->set_rx_func = remotemac_mac_set_netif_rx_func;

		/* setup phyinfo */
		inst->phyinfo = vmac_alloc_phy();
		inst->dummyphy = inst->phyinfo;

		/* setup rx tasklet */
		tasklet_init(&(inst->rxq_tasklet), remotemac_rx_tasklet, (unsigned long)inst);
		skb_queue_head_init(&(inst->rxq));

		/* register with softmac */
		vmac_register_mac(inst->macinfo);
		vmac_free_mac(inst->macinfo);

		ret = inst->macinfo;
	}

	return ret;
}

/* called by softmac_core when a remotemac vmac_mac_t
 * instance is deallocated 
 */
static void remotemac_free_instance (void *layer_priv, void *info)
{
	//printk("%s\n", __func__);
	vmac_mac_t *macinfo = info;
	struct remotemac_instance *inst = macinfo->mac_private;
	remotemac_mac_detach(inst);
	tasklet_disable(&(inst->rxq_tasklet));
	vmac_free_phy(inst->dummyphy);
	kfree(inst);
}

static int __init softmac_remotemac_init(void)
{
	//printk("%s\n", __func__);
	/* register the remotemac layer with softmac */
	strncpy(the_remotemac.name, remotemac_name, VMAC_NAME_SIZE);
	the_remotemac.new_instance = remotemac_new_instance;
	the_remotemac.destroy_instance = remotemac_free_instance;
	vmac_layer_register(&the_remotemac);

	return 0;
}

static void __exit softmac_remotemac_exit(void)
{
	//printk("%s\n", __func__);
	/* tell softmac we're leaving */
	vmac_layer_unregister((vmac_layer_t *)&the_remotemac);
}

module_init(softmac_remotemac_init);
module_exit(softmac_remotemac_exit);

EXPORT_SYMBOL(remotemac_alloc_skb);
