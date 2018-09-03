
#include "if_ath_vmac.h"

vmac_layer_t the_athmac; 				// atheros 802.11 mac
vmac_layer_t the_athphy; 					// atheros softmac phy
static int ath_vmac_layers_registered;

static unsigned char ath_cu_softmac_getheadertype_cb(struct ath_softc*,struct sk_buff*);
// returns CU_SOFTMAC_HEADER_* variable
static int ath_cu_softmac_getheadertype(struct ath_softc*,struct sk_buff*);

#if 0 
/*changed as following,lizhijie, 2006.03.02 */
#include <asm-i386/io.h>
#endif
#include <asm/io.h>
#include <linux/serial_reg.h>

/* @@@ARS@@@ */
void vmac_ath_set_phocus_state(u_int16_t state,int16_t settle)
{
	#define BOTH_EMPTY      (UART_LSR_TEMT | UART_LSR_THRE)

	u_int16_t val = 0x80 | state;
	volatile unsigned int status;
	// 400usec is default "settle time" for phocus
	if (0 > settle)
		settle = 400;

	outb((int)UART_MCR_RTS, 0x3f8 + UART_MCR);      /* toggle rts on */
	outb((int)val, 0x3f8 + UART_TX);                /* send the state value */

	do
	{
		status = inb(0x3f8 + UART_LSR);
	} while ( (status & BOTH_EMPTY) != BOTH_EMPTY);

	outb((int)0, 0x3f8 + UART_MCR);           /* toggle rts off */
	// need a delay for the time it takes the t/r modules to configure the DACs
	udelay(settle);
#undef BOTH_EMPTY
}

/* Immediately recalibrate the PHY to account for temperature/environment changes. */
void ath_calibrate_now(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ath_hal *ah = sc->sc_ah;

	sc->sc_stats.ast_per_cal++;

	DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s: channel %u/%x\n",	__func__, sc->sc_curchan.channel, sc->sc_curchan.channelFlags);

	if (ath_hal_getrfgain(ah) == HAL_RFGAIN_NEED_CHANGE)
	{/* Rfgain is out of bounds, reset the chip to load new gain values. */
		sc->sc_stats.ast_per_rfgain++;
		ath_reset(dev);
	}
	
	if (!ath_hal_calibrate(ah, &sc->sc_curchan))
	{
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: calibration of channel %u failed\n", __func__, sc->sc_curchan.channel);
		sc->sc_stats.ast_per_calfail++;
	}
}

// Some "internal" softmac utility functions that smooth
// implementation of the external exported functions.
struct sk_buff *ath_alloc_skb(u_int size, u_int align)
{
	struct sk_buff *skb;
	u_int off;

	skb = dev_alloc_skb(size + align-1);
	if (skb != NULL)
	{
		off = ((unsigned long) skb->data) % align;
		if (off != 0)
			skb_reserve(skb, align - off);
	}
	return skb;
}

/* fill the content in Control Buffer (Tag) */
unsigned char ath_cu_softmac_tag_cb(struct ath_softc* sc,struct ath_desc* ds,struct sk_buff* skb)
{
	struct ieee80211com *ic = &sc->sc_ic;
	unsigned char cbtype = 0;

	// RX timestamp
	u_int64_t tsf = 0;

	cbtype = ath_cu_softmac_getheadertype_cb(sc,skb);
	// tag the packet with the divined header type, bitrate, timestamp,
	// CRC error status, rssi...
	skb->cb[ATH_VMAC_CB_HEADER_TYPE] = cbtype;
	tsf = ath_hal_gettsf64(sc->sc_ah);
	if ((tsf & 0x7fff) < ds->ds_rxstat.rs_tstamp)
		tsf -= 0x8000;
	*((u_int64_t*)(skb->cb+ATH_VMAC_CB_RX_TSF0)) = (ds->ds_rxstat.rs_tstamp | (tsf &~ 0x7fff));

	// keep count of packets that contain crc errors
	//sc->sc_vmac_rx_total++;
	if (ds->ds_rxstat.rs_status & HAL_RXERR_CRC)
	{
		skb->cb[ATH_VMAC_CB_RX_CRCERR] = 1;
		//sc->sc_vmac_rx_crcerror++;
	}
	else
	{
		skb->cb[ATH_VMAC_CB_RX_CRCERR] = 0;
	}

	skb->cb[ATH_VMAC_CB_RATE] = sc->sc_hwmap[ds->ds_rxstat.rs_rate].ieeerate;
	skb->cb[ATH_VMAC_CB_RX_RSSI] = ds->ds_rxstat.rs_rssi;
	skb->cb[ATH_VMAC_CB_RX_CHANNEL] = ieee80211_mhz2ieee(ic->ic_ibss_chan->ic_freq,0);
	skb->cb[ATH_VMAC_CB_ANTENNA] = sc->sc_vmac_phocus_state;

	return cbtype;
}

int ath_cu_softmac_packetduration(struct net_device* dev,struct sk_buff* skb)
{
	int pktduration = 0;
	int rix = 0;
	struct ath_softc* sc = dev->priv;
	struct ath_hal *ah = sc->sc_ah;
	const HAL_RATE_TABLE* rt = 0;

	if (skb)
	{ // Get length, bitrate, tack on the "built in" tx latency...
		//sc->sc_vmac_tdma_txlatency
		rix = sc->sc_rixmap[skb->cb[ATH_VMAC_CB_RATE] & IEEE80211_RATE_VAL];
		if (0xFF == rix)
		{
			rix = 0;
		}
		rt = sc->sc_currates;
		pktduration = ath_hal_computetxtime(ah,rt,skb->len,rix,AH_TRUE);
	}

	return pktduration;
}

/* intop : is interrupt Operation */
int ath_cu_softmac_handle_txdone(struct net_device* dev, int intop)
{
	struct ath_buf* bf = 0;
	struct ath_softc* sc = dev->priv;
	struct ath_txq* txq = 0;
	struct ath_desc *ds = 0;
	struct sk_buff* skb = 0;
	struct ath_hal *ah = sc->sc_ah;
	void* macpriv = 0;
	int i;
	int result = 0;
	int status;
	macpriv = sc->sc_vmac_mac->mac_private;

	/* Process each active queue.   */
	/* XXX faster to read ISR_S0_S and ISR_S1_S to determine q's? */
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
	{
		if (ATH_TXQ_SETUP(sc, i))
		{
			//ath_tx_processq(sc, &sc->sc_txq[i]);
			txq = &sc->sc_txq[i];

			for (;;)
			{
				ATH_TXQ_LOCK(txq);
				txq->axq_intrcnt = 0;	/* reset periodic desc intr count */
				bf = STAILQ_FIRST(&txq->axq_q);
				if (bf == NULL)
				{
					txq->axq_link = NULL;
					ATH_TXQ_UNLOCK(txq);
					break;
				}
				ds = bf->bf_desc;		/* NB: last decriptor */
				status = ath_hal_txprocdesc(ah, ds);
#ifdef AR_DEBUG
				if (sc->sc_debug & ATH_DEBUG_XMIT_DESC)
					ath_printtxbuf(bf, status == HAL_OK);
#endif
				if (status == HAL_EINPROGRESS)
				{
					ATH_TXQ_UNLOCK(txq);
					break;
				}

				ATH_TXQ_REMOVE_HEAD(txq, bf_list);
				ATH_TXQ_UNLOCK(txq);
				bus_unmap_single(sc->sc_bdev, bf->bf_skbaddr, bf->bf_skb->len, BUS_DMA_TODEVICE);

				// Packet done, decrement the "in flight" counter
				atomic_dec(&(sc->sc_vmac_tx_packets_inflight));
				skb = bf->bf_skb;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
				read_lock(&(sc->sc_vmac_mac_lock));
				if (1)
#else
				if (read_trylock(&(sc->sc_vmac_mac_lock)))
#endif
				{
					// XXX make a dummy MAC layer to avoid this check?
					if (sc->sc_vmac_mac->mac_tx_done)
					{
						int txdoneresult = (sc->sc_vmac_mac->mac_tx_done)(macpriv,skb,intop);
						if (VMAC_MAC_NOTIFY_OK == txdoneresult)
						{
							result = 0;
						}
						else if (VMAC_MAC_NOTIFY_RUNAGAIN == txdoneresult)
						{
							result = 1;
						}
						else
						{/* Something went wrong -- free the packet and move on. */
							dev_kfree_skb(skb);
							skb = 0;
							result = 0;
						}
					}
					else
					{ /* No hook function set -- free the packet and move on. */
						dev_kfree_skb(skb);
						skb = 0;
						result = 0;
					}
					read_unlock(&(sc->sc_vmac_mac_lock));
				}
				else
				{ // unable to get lock -- free packet and move on
					dev_kfree_skb(skb);
					skb = 0;
					result = 0;
				}

				// bf->bf_skb is now owned by the softmac or dead. Pull it out of the
				// tx buffer and reallocate.
				bf->bf_skb = NULL;
				bf->bf_node = NULL;

				ATH_TXBUF_LOCK(sc);
				STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				ATH_TXBUF_UNLOCK(sc);
			}
		}

	}

	return result;
}

/* called in TX and reschedule worktq again */
void ath_cu_softmac_txdone_tasklet(TQUEUE_ARG data)
{
	struct net_device *dev = (struct net_device *)data;
	struct ath_softc* sc = dev->priv;
	int goagain = 0;

	//printk(KERN_DEBUG "if_ath: in txdone_tasklet\n");
	goagain = ath_cu_softmac_handle_txdone(dev, 0);
	if (goagain)
	{
		int needmark = 0;
		ATH_SCHEDULE_TQUEUE(&sc->sc_vmac_worktq, &needmark);
		if (needmark)
		{
			mark_bh(IMMEDIATE_BH);
		}
	}
}

/* scheduled when init, and reschedule worktq */
void ath_cu_softmac_work_tasklet(TQUEUE_ARG data)
{
	struct net_device *dev = (struct net_device *)data;
	struct ath_softc *sc = dev->priv;
	void* macpriv = 0;
	
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
	read_lock(&(sc->sc_vmac_mac_lock));
#else
	if (!read_trylock(&(sc->sc_vmac_mac_lock)))
	{
		return;
	}
#endif

	// See if we've got a "hook" function set -- run it if we do
	//printk(KERN_DEBUG "In ath_cu_softmac_work_tasklet\n");
	macpriv = sc->sc_vmac_mac->mac_private;
	if (sc->sc_vmac_mac->mac_work)
	{
		int workresult = VMAC_MAC_NOTIFY_OK;
		int needmark = 0;
		//printk(KERN_DEBUG "In ath_cu_softmac_work_tasklet: have a workfunc!\n");
		workresult = (sc->sc_vmac_mac->mac_work)(macpriv, 0);
		if (VMAC_MAC_NOTIFY_RUNAGAIN == workresult)
		{
			//printk(KERN_DEBUG "In ath_cu_softmac_work_tasklet: rescheduling!\n");
			ATH_SCHEDULE_TQUEUE(&sc->sc_vmac_worktq,&needmark);
			if (needmark)
			{
				mark_bh(IMMEDIATE_BH);
			}
		}
	}

	read_unlock(&(sc->sc_vmac_mac_lock));
}

static int ath_cu_softmac_getheadertype(struct ath_softc* sc,struct sk_buff* skb)
{
	int result = CU_SOFTMAC_HEADER_UNKNOWN;

	if ((sc->sc_vmac_wifictl0 == skb->data[0]) && (sc->sc_vmac_wifictl1 == skb->data[1]))
	{
		result = CU_SOFTMAC_HEADER_REGULAR;
	}
	else
	{
		result = CU_SOFTMAC_HEADER_UNKNOWN;
	}

	return result;
}

/* skb's Control Buffer ??? */
static unsigned char ath_cu_softmac_getheadertype_cb(struct ath_softc* sc,struct sk_buff* skb)
{
	int result = ATH_VMAC_CB_HEADER_UNKNOWN;

	if ((sc->sc_vmac_wifictl0 == skb->data[0]) && (sc->sc_vmac_wifictl1 == skb->data[1]))
	{
		result = ATH_VMAC_CB_HEADER_REGULAR;
	}
	else
	{
		result = ATH_VMAC_CB_HEADER_UNKNOWN;
	}

	return result;
}

// returns length of ath_vmac_header or -1 if unknown packet type
int ath_cu_softmac_getheaderlen(struct ath_softc* sc,struct sk_buff* skb)
{
	int result = -1;
	int htype = ath_cu_softmac_getheadertype(sc,skb);

	switch (htype)
	{
		case CU_SOFTMAC_HEADER_REGULAR:
			result = sizeof(struct ath_vmac_header);
			break;
		default:
			result = -1;
			break;
	}

	return result;
}

/* CCA Noise Floor */
void vmac_ath_set_cca_nf(vmac_phy_handle nfh,u_int32_t ccanf)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	OS_REG_WRITE(sc->sc_ah,39012,ccanf);
}

void vmac_ath_set_cw(vmac_phy_handle nfh,int cwmin,int cwmax)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	struct ath_hal *ah = sc->sc_ah;
	HAL_TXQ_INFO qi;

	memset(&qi, 0, sizeof(qi));
	//ath_tx_setup(sc, WME_AC_VO, HAL_WME_AC_VO)
	qi.tqi_subtype = HAL_WME_AC_VO;
	qi.tqi_qflags = TXQ_FLAG_TXEOLINT_ENABLE | TXQ_FLAG_TXDESCINT_ENABLE;
	qi.tqi_qflags |= (TXQ_FLAG_TXOKINT_ENABLE | TXQ_FLAG_TXERRINT_ENABLE | TXQ_FLAG_BACKOFF_DISABLE);
	qi.tqi_cwmin = cwmin;
	qi.tqi_cwmax = cwmax;

	// XXX assuming we always use the WME_AC_VO queue for right now...
	ath_hal_settxqueueprops(ah,WME_AC_VO,&qi);
}

u_int32_t vmac_ath_get_slottime(vmac_phy_handle nfh)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	struct ath_hal *ah = sc->sc_ah;

	return ath_hal_getslottime(ah);
}

void vmac_ath_set_slottime(vmac_phy_handle nfh,u_int32_t slottime)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	struct ath_hal *ah = sc->sc_ah;

	ath_hal_setslottime(ah,slottime);
}

void vmac_ath_set_options(vmac_phy_handle nfh,u_int32_t options)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;

	sc->sc_vmac_options = options;
	// XXX update based on new options
}

void vmac_ath_set_default_phy_props(vmac_phy_handle nfh, struct sk_buff* packet)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;

	// default to 1 Mb/s
	packet->cb[ATH_VMAC_CB_RATE] = 2;
	packet->cb[ATH_VMAC_CB_TXINTR] = 1;
	packet->cb[ATH_VMAC_CB_HAL_PKT_TYPE] = HAL_PKT_TYPE_NORMAL;
	packet->cb[ATH_VMAC_CB_HEADER_TYPE] = ATH_VMAC_CB_HEADER_REGULAR;
	packet->cb[ATH_VMAC_CB_RX_CRCERR] = 0;
	packet->cb[ATH_VMAC_CB_RX_RSSI] = 0;
	packet->cb[ATH_VMAC_CB_RX_CHANNEL] = 0;
	packet->cb[ATH_VMAC_CB_ANTENNA] = 0; // zero is often "omni"

	packet->cb[ATH_VMAC_CB_RX_TSF0] = 0;
	packet->cb[ATH_VMAC_CB_RX_TSF1] = 0;
	packet->cb[ATH_VMAC_CB_RX_TSF2] = 0;
	packet->cb[ATH_VMAC_CB_RX_TSF3] = 0;
	packet->cb[ATH_VMAC_CB_RX_TSF4] = 0;
	packet->cb[ATH_VMAC_CB_RX_TSF5] = 0;
	packet->cb[ATH_VMAC_CB_RX_TSF6] = 0;
	packet->cb[ATH_VMAC_CB_RX_TSF7] = 0;

	packet->cb[ATH_VMAC_CB_RX_BF0] = 0;
	packet->cb[ATH_VMAC_CB_RX_BF1] = 0;
	packet->cb[ATH_VMAC_CB_RX_BF2] = 0;
	packet->cb[ATH_VMAC_CB_RX_BF3] = 0;
}

void vmac_ath_set_tx_bitrate(vmac_phy_handle nfh, struct sk_buff* packet,unsigned char rate)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	packet->cb[ATH_VMAC_CB_RATE] = rate;
}

u_int8_t vmac_ath_get_rx_bitrate(vmac_phy_handle nfh, struct sk_buff* packet)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;

	return packet->cb[ATH_VMAC_CB_RATE];
}

void vmac_ath_require_txdone_interrupt(vmac_phy_handle nfh,	struct sk_buff* packet,	int require_interrupt)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	packet->cb[ATH_VMAC_CB_TXINTR] = require_interrupt ? 1 : 0;
}

u_int64_t vmac_ath_get_rx_time(vmac_phy_handle nfh,struct sk_buff* packet)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	return *((u_int64_t*)(packet->cb+ATH_VMAC_CB_RX_TSF0));
}

u_int8_t vmac_ath_get_rx_rssi(vmac_phy_handle nfh,struct sk_buff* packet)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	return packet->cb[ATH_VMAC_CB_RX_RSSI];
}

u_int8_t vmac_ath_get_rx_channel(vmac_phy_handle nfh,struct sk_buff* packet)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	return packet->cb[ATH_VMAC_CB_RX_RSSI];
}

u_int8_t vmac_ath_has_rx_crc_error(vmac_phy_handle nfh, struct sk_buff* packet)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	return packet->cb[ATH_VMAC_CB_RX_CRCERR];
}

void vmac_ath_set_tx_antenna(vmac_phy_handle nfh, struct sk_buff* packet, u_int32_t state)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	packet->cb[ATH_VMAC_CB_ANTENNA] = (u_int8_t)state;
}

u_int32_t vmac_ath_get_rx_antenna(vmac_phy_handle nfh, struct sk_buff* packet)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	return (u_int32_t)(packet->cb[ATH_VMAC_CB_ANTENNA]);
}


/* check whether a packet is softmac packet */
int vmac_ath_issoftmac(void *data,struct sk_buff* skb)
{
	struct ath_softc *sc = (struct ath_softc *)data;
	int htype = ath_cu_softmac_getheadertype(sc,skb);

	return (CU_SOFTMAC_HEADER_UNKNOWN != htype);
}

struct sk_buff* vmac_ath_encapsulate(void *data,struct sk_buff* skb)
{
	struct ath_softc *sc = data;
	unsigned char* newheader = 0;
	struct ath_vmac_header* cush = 0;

	skb_cow(skb,sizeof(struct ath_vmac_header));
	newheader = skb_push(skb,sizeof(struct ath_vmac_header));
	memset(newheader,0,sizeof(struct ath_vmac_header));
	cush = (struct ath_vmac_header*) newheader;
	cush->wifictl0 = sc->sc_vmac_wifictl0;
	cush->wifictl1 = sc->sc_vmac_wifictl1;
	cush->wifiduration = 0x0000;
	cush->wifiaddr1_0 = 0xFF;
	skb->cb[ATH_VMAC_CB_HEADER_TYPE] = ATH_VMAC_CB_HEADER_REGULAR;
	skb->cb[ATH_VMAC_CB_HAL_PKT_TYPE] = HAL_PKT_TYPE_NORMAL;

	// Set some transmit attributes...
	skb->priority = WME_AC_VO;
	return skb;
}

struct sk_buff* vmac_ath_decapsulate(void *data, struct sk_buff* skb)
{
	//struct ath_softc* sc = (struct ath_softc *)data;
	// strip the header off of the beginning, rip the CRC off of the end,
	// do the right thing w.r.t. packets that have CRC errors
	int haserrors = 0;

	// XXX use the getheadersize function?
	skb_pull(skb,sizeof(struct ath_vmac_header));
	skb_trim(skb, skb->len - IEEE80211_CRC_LEN);
	skb->mac.raw = skb->data;

	if (skb->cb[ATH_VMAC_CB_RX_CRCERR])
	{
		haserrors = 1;
	}

	return skb;
}


/* called by ath_attach */
void ath_vmac_register(struct ath_softc* sc)
{
	struct list_head *list;

	if (ath_vmac_layers_registered == 0)
	{
		/* register the athphy layer with softmac */
		strncpy(the_athphy.name, VMAC_MODULE_ATH_PHY, VMAC_NAME_SIZE);
		the_athphy.new_instance = ath_vmac_new_phy;
		the_athphy.destroy_instance = ath_vmac_free_phy;
		list = (struct list_head *)kmalloc(sizeof(struct list_head), GFP_ATOMIC);
		INIT_LIST_HEAD(list);
		the_athphy.layer_private = list;
		vmac_layer_register(&the_athphy);

		/* register the athmac layer with softmac */
		strncpy(the_athmac.name, VMAC_MODULE_ATH_MAC, VMAC_NAME_SIZE);
		the_athmac.new_instance = ath_vmac_new_mac;
		the_athmac.destroy_instance = ath_vmac_free_mac;
		list = (struct list_head *)kmalloc(sizeof(struct list_head), GFP_ATOMIC);
		INIT_LIST_HEAD(list);
		the_athmac.layer_private = list;
		vmac_layer_register(&the_athmac);
	}

	/* setup the instance and put it on the list of athmac instances */
	sc->sc_vmac_inst->id = ath_vmac_layers_registered;
	INIT_LIST_HEAD(&sc->sc_vmac_mac_list);
	list_add(&sc->sc_vmac_mac_list, (struct list_head *)the_athmac.layer_private);

	/* setup the instance and put it on the list of athphy instances */
	sc->sc_vmac_phy_id = ath_vmac_layers_registered;
	INIT_LIST_HEAD(&sc->sc_vmac_phy_list);
	list_add(&sc->sc_vmac_phy_list, (struct list_head *)the_athphy.layer_private);

	ath_vmac_layers_registered++;
}
    
void ath_vmac_unregister(struct ath_softc* sc)
{
	vmac_layer_unregister(&the_athphy);
	vmac_layer_unregister(&the_athmac);
	ath_vmac_layers_registered = 0;
}


EXPORT_SYMBOL(vmac_ath_set_cca_nf);
EXPORT_SYMBOL(vmac_ath_set_cw);
EXPORT_SYMBOL(vmac_ath_get_slottime);
EXPORT_SYMBOL(vmac_ath_set_slottime);
EXPORT_SYMBOL(vmac_ath_set_options);

EXPORT_SYMBOL(vmac_ath_decapsulate);
EXPORT_SYMBOL(vmac_ath_encapsulate);
EXPORT_SYMBOL(vmac_ath_issoftmac);

EXPORT_SYMBOL(vmac_ath_set_default_phy_props);
EXPORT_SYMBOL(vmac_ath_set_tx_bitrate);
EXPORT_SYMBOL(vmac_ath_get_rx_bitrate);
EXPORT_SYMBOL(vmac_ath_require_txdone_interrupt);
EXPORT_SYMBOL(vmac_ath_get_rx_time);
EXPORT_SYMBOL(vmac_ath_has_rx_crc_error);
EXPORT_SYMBOL(vmac_ath_get_rx_rssi);
EXPORT_SYMBOL(vmac_ath_get_rx_channel);
EXPORT_SYMBOL(vmac_ath_set_tx_antenna);
EXPORT_SYMBOL(vmac_ath_get_rx_antenna);
EXPORT_SYMBOL(vmac_ath_set_phocus_state);

