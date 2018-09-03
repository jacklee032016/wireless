/*
* $Id: if_ath_vmac.h,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#ifndef  __IF_ATH_VMAC_H__
#define __IF_ATH_VMAC_H__

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/delay.h>

#include <asm/uaccess.h>

#include "if_ethersubr.h"		/* for ETHER_IS_MULTICAST */
#include "if_media.h"
#include "if_llc.h"

#include <ieee80211_var.h>

#ifdef CONFIG_NET_WIRELESS
#include <net/iw_handler.h>
#endif

#include "radar.h"

#include "if_athvar.h"
#include "ah_desc.h"
#include "ah_devid.h"			/* XXX to identify IBM cards */
#include "ah.h"

#include "if_ath_pci.h"

#include "if_ath.h"

/* command send to /proc filesystem */
enum
{
	ATH_VMAC 						= 1,
	ATH_VMAC_RAW80211 			= 2,
	ATH_VMAC_PHOCUS_SETTLETIME 	= 3,
	ATH_VMAC_PHOCUS_ENABLE 		= 4,
	ATH_VMAC_PHOCUS_STATE 		= 5,
	ATH_VMAC_WIFIVERSION 		= 6,
	ATH_VMAC_WIFITYPE 			= 7,
	ATH_VMAC_WIFISUBTYPE 		= 8,
	ATH_VMAC_WIFIFLAGS 			= 9,
	ATH_VMAC_NOAUTOCALIBRATE 	= 10,
	ATH_VMAC_CALIBRATE_NOW 		= 11,
	ATH_VMAC_PHY_NF_RAW 		= 12,
	ATH_VMAC_MAC 					= 13,
	ATH_VMAC_ALLOW_CRCERR 		= 14,
};

enum
{
	ATH_VMAC_TX_ON_TXINTR_NO 			= 0,
	ATH_VMAC_TX_ON_TXINTR_DEFER 		= 1,
	ATH_VMAC_TX_ON_TXINTR_IMMEDIATE 	= 2,
};


void 	ath_vmac_free_phy(void *layer_private, void *phy);
void* 	ath_vmac_new_phy(void *layer_private);


void* 	ath_vmac_new_mac(void *layer_private);
void 	ath_vmac_free_mac(void *layer_private, void *mac);


struct sk_buff* 	ath_alloc_skb(u_int size, u_int align);
void 			ath_calibrate_now(struct net_device *dev);
int 				ath_cu_softmac_packetduration(struct net_device* dev,struct sk_buff* skb);
unsigned char 	ath_cu_softmac_tag_cb(struct ath_softc* sc,struct ath_desc* ds,struct sk_buff* skb);


extern vmac_layer_t the_athmac;
extern vmac_layer_t the_athphy;

#endif

