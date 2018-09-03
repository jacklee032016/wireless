/*-
 * $Id: if_ath.c,v 1.1.1.1 2006/11/30 16:46:13 lizhijie Exp $
 */


/*
 * Driver for the Atheros Wireless LAN controller.
 *
 * This software is derived from work of Atsushi Onoe; his contribution
 * is greatly appreciated.
 */
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/cache.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#ifdef HAS_VMAC
#include <linux/etherdevice.h>
#include "vmac.h"
#include "vmac_ath.h"
#endif

#include <asm/uaccess.h>

#include "if_ethersubr.h"		/* for ETHER_IS_MULTICAST */
#include "if_media.h"
#include "if_llc.h"

#include <net80211/ieee80211_var.h>

#include "radar.h"

#include "if_athvar.h"
#include "ah_desc.h"
#include "ah_devid.h"			/* XXX to identify IBM cards */
#include "ah.h"

#ifdef ATH_PCI		/* PCI BUS */
#include "if_ath_pci.h"
#endif			/* PCI BUS */
#ifdef ATH_AHB		/* AHB BUS */
#include "if_ath_ahb.h"
#endif			/* AHB BUS */


#include "if_ath.h"

//#define AR_DEBUG
#ifdef AR_DEBUG
static	int ath_debug = 0;
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,52))
MODULE_PARM(countrycode, "i");
MODULE_PARM(outdoor, "i");
MODULE_PARM(xchanmode, "i");
#else
module_param(countrycode, int, 0);
module_param(outdoor, int, 0);
module_param(xchanmode, int, 0);
#endif
MODULE_PARM_DESC(countrycode, "Override default country code");
MODULE_PARM_DESC(outdoor, "Enable/disable outdoor use");
MODULE_PARM_DESC(xchanmode, "Enable/disable extended channel mode");


void ath_suspend(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: flags %x\n", __func__, dev->flags);
	if (sc->sc_softled) 
	    	ath_hal_gpioset(ah, sc->sc_ledpin, 1);

	ath_stop(dev);
}

void
ath_resume(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: flags %x\n", __func__, dev->flags);
	ath_init(dev);
	if (sc->sc_softled) {
	    	ath_hal_gpioCfgOutput(ah, sc->sc_ledpin);
	    	ath_hal_gpioset(ah, sc->sc_ledpin, 0);
	}
}

void
ath_shutdown(struct net_device *dev)
{
#ifdef AR_DEBUG
	struct ath_softc *sc = dev->priv;
#endif

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: flags %x\n", __func__, dev->flags);

	ath_stop(dev);
}


u_int ath_chan2flags(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const u_int modeflags[] = {
		0,			/* IEEE80211_MODE_AUTO */
		CHANNEL_A,		/* IEEE80211_MODE_11A */
		CHANNEL_B,		/* IEEE80211_MODE_11B */
		CHANNEL_PUREG,		/* IEEE80211_MODE_11G */
		0,			/* IEEE80211_MODE_FH */
		CHANNEL_T,		/* IEEE80211_MODE_TURBO_A */
		CHANNEL_108G		/* IEEE80211_MODE_TURBO_G */
	};
	enum ieee80211_phymode mode = ieee80211_chan2mode(ic, chan);

	KASSERT(mode < N(modeflags), ("unexpected phy mode %u", mode));
	KASSERT(modeflags[mode] != 0, ("mode %u undefined", mode));
	return modeflags[mode];
#undef N
}



/* called when attached */
int ath_media_change(struct net_device *dev)
{
#define	IS_UP(dev) \
	((dev->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
	int error;

	error = ieee80211_media_change(dev);
	if (error == ENETRESET) {
		if (IS_UP(dev))
			error = ath_init(dev);
		else
			error = 0;
	}
	return error;
#undef IS_UP
}


/*
 * Calculate the receive filter according to the
 * operating mode and state:
 *
 * o always accept unicast, broadcast, and multicast traffic
 * o maintain current state of phy error reception (the hal
 *   may enable phy error frames for noise immunity work)
 * o probe request frames are accepted only when operating in
 *   hostap, adhoc, or monitor modes
 * o enable promiscuous mode according to the interface state
 * o accept beacons:
 *   - when operating in adhoc mode so the 802.11 layer creates
 *     node table entries for peers,
 *   - when operating in station mode for collecting rssi data when
 *     the station is otherwise quiet, or
 *   - when scanning
 * o accept any additional packets specified by sc_rxfilter
 */
static u_int32_t
ath_calcrxfilter(struct ath_softc *sc, enum ieee80211_state state)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct net_device *dev = ic->ic_dev;
	u_int32_t rfilt;

	rfilt = (ath_hal_getrxfilter(ah) & HAL_RX_FILTER_PHYERR)
	      | HAL_RX_FILTER_UCAST | HAL_RX_FILTER_BCAST | HAL_RX_FILTER_MCAST | HAL_RX_FILTER_PHYRADAR;
	if (ic->ic_opmode != IEEE80211_M_STA)
		rfilt |= HAL_RX_FILTER_PROBEREQ;
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    (dev->flags & IFF_PROMISC))
		rfilt |= HAL_RX_FILTER_PROM;
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_IBSS ||
	    state == IEEE80211_S_SCAN)
		rfilt |= HAL_RX_FILTER_BEACON;

	rfilt |= sc->sc_rxfilter;
#ifdef HAS_VMAC
	if (sc->sc_vmac) { //&& ic->ic_opmode == IEEE80211_M_MONITOR) {
	  // Make sure we receive ALL the packets from the hal when in "SDR"
	  // mode
	  rfilt = ATH_VMAC_HAL_RX_FILTER_ALLOWALL;
	}
#endif
	return rfilt;
}

void ath_mode_init(struct net_device *dev)
{
        struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	u_int32_t rfilt, mfilt[2], val;
	u_int8_t pos;
	struct dev_mc_list *mc;

	/* configure rx filter */
	rfilt = ath_calcrxfilter(sc, ic->ic_state);
	ath_hal_setrxfilter(ah, rfilt);

	/* configure operational mode */
	ath_hal_setopmode(ah);

	/*
	 * Handle any link-level address change.  Note that we only
	 * need to force ic_myaddr; any other addresses are handled
	 * as a byproduct of the ifnet code marking the interface
	 * down then up.
	 *
	 * XXX should get from lladdr instead of arpcom but that's more work
	 */
	IEEE80211_ADDR_COPY(ic->ic_myaddr, dev->dev_addr);
	ath_hal_setmac(ah, ic->ic_myaddr);

	/* calculate and install multicast filter */
	if ((dev->flags & IFF_ALLMULTI) == 0) {
		mfilt[0] = mfilt[1] = 0;
		for (mc = dev->mc_list; mc; mc = mc->next) {
			/* calculate XOR of eight 6bit values */
			val = LE_READ_4(mc->dmi_addr + 0);
			pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
			val = LE_READ_4(mc->dmi_addr + 3);
			pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
			pos &= 0x3f;
			mfilt[pos / 32] |= (1 << (pos % 32));
		}
	}
	else
	{
		mfilt[0] = mfilt[1] = ~0;
	}

	ath_hal_setmcastfilter(ah, mfilt[0], mfilt[1]);
	DPRINTF(sc, ATH_DEBUG_MODE, "%s: RX filter 0x%x, MC filter %08x:%08x\n",	__func__, rfilt, mfilt[0], mfilt[1]);
}

/* Set the slot time based on the current setting. */
void ath_setslottime(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;

	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		ath_hal_setslottime(ah, HAL_SLOT_TIME_9);
	else
		ath_hal_setslottime(ah, HAL_SLOT_TIME_20);
	sc->sc_updateslot = OK;
}

/* Callback from the 802.11 layer to update the slot time based on the current setting. */
void ath_updateslot(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	/*
	 * When not coordinating the BSS, change the hardware
	 * immediately.  For other operation we defer the change
	 * until beacon updates have propagated to the stations.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		sc->sc_updateslot = UPDATE;
	else
		ath_setslottime(sc);
}

int ath_rxbuf_init(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_hal *ah = sc->sc_ah;
	struct sk_buff *skb;
	struct ath_desc *ds;
	int headroom_needed = 0;
	
	if (sc->sc_ic.ic_opmode == IEEE80211_M_MONITOR) {
		headroom_needed = sizeof(wlan_ng_prism2_header);
	} else if (sc->sc_rawdev.type == ARPHRD_IEEE80211_PRISM) {
		headroom_needed = sizeof(wlan_ng_prism2_header);
	} else if (sc->sc_rawdev.type == ARPHRD_IEEE80211_RADIOTAP) {
		headroom_needed = sizeof(struct ath_rx_radiotap_header);
	}

	/* 
	 * Check if we have enough headroom. If not, just free the skb
	 * and we'll alloc another one below.
	 */
	if (bf->bf_skb && skb_headroom(bf->bf_skb) < headroom_needed) {
		bus_unmap_single(sc->sc_bdev,
				 bf->bf_skbaddr, sc->sc_rxbufsize,
				 BUS_DMA_FROMDEVICE);
		dev_kfree_skb(bf->bf_skb);
		bf->bf_skb = NULL;
	}

	skb = bf->bf_skb;

	if (skb == NULL) {
		u_int off;

		/*
		 * Allocate buffer with headroom_needed space for the
		 * fake physical layer header at the start.
		 */
		skb = dev_alloc_skb(sc->sc_rxbufsize +
				    headroom_needed + 
				    sc->sc_cachelsz - 1);
		if (skb == NULL) {
			DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: skbuff alloc of size %d failed\n",
				__func__,
				(int)(sc->sc_rxbufsize
				      + headroom_needed
				      + sc->sc_cachelsz - 1));
			sc->sc_stats.ast_rx_nobuf++;
			return ENOMEM;
		}
		/*
		 * Reserve space for the fake physical layer header.
		 */
		skb_reserve(skb, headroom_needed);
		/*
		 * Cache-line-align.  This is important (for the
		 * 5210 at least) as not doing so causes bogus data
		 * in rx'd frames.
		 */
		off = ((unsigned long) skb->data) % sc->sc_cachelsz;
		if (off != 0)
			skb_reserve(skb, sc->sc_cachelsz - off);

		skb->dev = &sc->sc_dev;
		bf->bf_skb = skb;
		bf->bf_skbaddr = bus_map_single(sc->sc_bdev,
			skb->data, sc->sc_rxbufsize, BUS_DMA_FROMDEVICE);
		if (BUS_DMA_MAP_ERROR(bf->bf_skbaddr)) {
			if_printf(&sc->sc_dev, "%s: DMA mapping failed\n", __func__);
			dev_kfree_skb(skb);
			bf->bf_skb = NULL;
			sc->sc_stats.ast_rx_busdma++;
			return ENOMEM;
		}
	}

	/*
	 * Setup descriptors.  For receive we always terminate
	 * the descriptor list with a self-linked entry so we'll
	 * not get overrun under high load (as can happen with a
	 * 5212 when ANI processing enables PHY error frames).
	 *
	 * To insure the last descriptor is self-linked we create
	 * each descriptor as self-linked and add it to the end.  As
	 * each additional descriptor is added the previous self-linked
	 * entry is ``fixed'' naturally.  This should be safe even
	 * if DMA is happening.  When processing RX interrupts we
	 * never remove/process the last, self-linked, entry on the
	 * descriptor list.  This insures the hardware always has
	 * someplace to write a new frame.
	 */
	ds = bf->bf_desc;
	ds->ds_link = bf->bf_daddr;	/* link to self */
	ds->ds_data = bf->bf_skbaddr;
	ath_hal_setuprxdesc(ah, ds
		, skb_tailroom(skb)	/* buffer size */
		, 0
	);

	if (sc->sc_rxlink != NULL)
		*sc->sc_rxlink = bf->bf_daddr;
	sc->sc_rxlink = &ds->ds_link;
	return 0;
}

/** Add additional headers to a transmitted frame and netif_rx it on
 * a monitor or raw device */
void ath_tx_capture(struct net_device *dev, struct ath_desc *ds, struct sk_buff *skb)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	u_int32_t tsf;

	/* 
	 * release the owner of this skb since we're basically
	 * recycling it 
	 */
	if (atomic_read(&skb->users) != 1) {
		struct sk_buff *skb2 = skb;
		skb = skb_clone(skb, GFP_ATOMIC);
		if (skb == NULL) {
			dev_kfree_skb(skb2);
			return;
		}
		kfree_skb(skb2);
	} else {
		skb_orphan(skb);
	}

	switch (dev->type) {
	case ARPHRD_IEEE80211:
		break;
	case ARPHRD_IEEE80211_PRISM: {
		wlan_ng_prism2_header *ph;
		if (skb_headroom(skb) < sizeof(wlan_ng_prism2_header) &&
		    pskb_expand_head(skb, 
				     sizeof(wlan_ng_prism2_header), 
				     0, GFP_ATOMIC)) {
			DPRINTF(sc, ATH_DEBUG_RECV, 
				"%s: couldn't pskb_expand_head\n", __func__);
			goto bad;
		}

		ph = (wlan_ng_prism2_header *)
			skb_push(skb, sizeof(wlan_ng_prism2_header));
		memset(ph, 0, sizeof(wlan_ng_prism2_header));
		
		ph->msgcode = DIDmsg_lnxind_wlansniffrm;
		ph->msglen = sizeof(wlan_ng_prism2_header);
		strcpy(ph->devname, sc->sc_dev.name);
		
		ph->hosttime.did = DIDmsg_lnxind_wlansniffrm_hosttime;
		ph->hosttime.status = 0;
		ph->hosttime.len = 4;
		ph->hosttime.data = jiffies;
		
		/* Pass up tsf clock in mactime */
		ph->mactime.did = DIDmsg_lnxind_wlansniffrm_mactime;
		ph->mactime.status = 0;
		ph->mactime.len = 4;
		/*
		 * Rx descriptor has the low 15 bits of the tsf at
		 * the time the frame was received.  Use the current
		 * tsf to extend this to 32 bits.
		 */
		tsf = ath_hal_gettsf32(sc->sc_ah);
		if ((tsf & 0x7fff) < ds->ds_rxstat.rs_tstamp)
			tsf -= 0x8000;
		ph->mactime.data = ds->ds_rxstat.rs_tstamp | (tsf &~ 0x7fff);
		
		ph->istx.did = DIDmsg_lnxind_wlansniffrm_istx;
		ph->istx.status = 0;
		ph->istx.len = 4;
		ph->istx.data = P80211ENUM_truth_true;
		
		ph->frmlen.did = DIDmsg_lnxind_wlansniffrm_frmlen;
		ph->frmlen.status = 0;
		ph->frmlen.len = 4;
		ph->frmlen.data = ds->ds_rxstat.rs_datalen;
		
		ph->channel.did = DIDmsg_lnxind_wlansniffrm_channel;
		ph->channel.status = 0;
		ph->channel.len = 4;
		ph->channel.data = ieee80211_mhz2ieee(ic->ic_ibss_chan->ic_freq,0);
		
		ph->rssi.did = DIDmsg_lnxind_wlansniffrm_rssi;
		ph->rssi.status = 0;
		ph->rssi.len = 4;
		ph->rssi.data = ds->ds_rxstat.rs_rssi;
		
		ph->noise.did = DIDmsg_lnxind_wlansniffrm_noise;
		ph->noise.status = 0;
		ph->noise.len = 4;
		ph->noise.data = -95;
		
		ph->signal.did = DIDmsg_lnxind_wlansniffrm_signal;
		ph->signal.status = 0;
		ph->signal.len = 4;
		ph->signal.data = -95 + ds->ds_rxstat.rs_rssi;
		
		ph->rate.did = DIDmsg_lnxind_wlansniffrm_rate;
		ph->rate.status = 0;
		ph->rate.len = 4;
		ph->rate.data = sc->sc_hwmap[ds->ds_txstat.ts_rate &~ HAL_TXSTAT_ALTRATE].ieeerate;
		break;
	}
	case ARPHRD_IEEE80211_RADIOTAP: {
		struct ath_tx_radiotap_header *th;

		if (skb_headroom(skb) < sizeof(struct ath_tx_radiotap_header) &&
		    pskb_expand_head(skb, 
				     sizeof(struct ath_tx_radiotap_header), 
				     0, GFP_ATOMIC)) {
			DPRINTF(sc, ATH_DEBUG_RECV, 
				"%s: couldn't pskb_expand_head\n", __func__);
			goto bad;
		}
		
		th = (struct ath_tx_radiotap_header *) skb_push(skb, sizeof(struct ath_tx_radiotap_header));
		memset(th, 0, sizeof(struct ath_tx_radiotap_header));
		th->wt_ihdr.it_version = 0;
		th->wt_ihdr.it_len = sizeof(struct ath_tx_radiotap_header);
		th->wt_ihdr.it_present = ATH_TX_RADIOTAP_PRESENT;
		th->wt_flags = 0;
		th->wt_rate = sc->sc_hwmap[ds->ds_txstat.ts_rate &~ HAL_TXSTAT_ALTRATE].ieeerate;
		th->wt_txpower = 0;
		th->wt_antenna = ds->ds_txstat.ts_antenna;
		th->wt_tx_flags = 0;
		if (ds->ds_txstat.ts_status) 
			th->wt_tx_flags |= IEEE80211_RADIOTAP_F_TX_FAIL;
		th->wt_rts_retries = ds->ds_txstat.ts_shortretry;
		th->wt_data_retries = ds->ds_txstat.ts_longretry;
		
		break;
	}
	default:
		break;
	}

	skb->dev = dev;
	skb->mac.raw = skb->data;
	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = __constant_htons(0x0019);  /* ETH_P_80211_RAW */
	netif_rx(skb);
	return;

 bad:
	dev_kfree_skb(skb);
	return;
}


/* Set the default antenna. */
void ath_setdefantenna(struct ath_softc *sc, u_int antenna)
{
	struct ath_hal *ah = sc->sc_ah;

	/* XXX block beacon interrupts */
	ath_hal_setdefantenna(ah, antenna);
	if (sc->sc_defant != antenna)
		sc->sc_stats.ast_ant_defswitch++;
	sc->sc_defant = antenna;
	sc->sc_rxotherant = 0;
}


/* Disable the receive h/w in preparation for a reset. */
void ath_stoprecv(struct ath_softc *sc)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((caddr_t)(_sc)->sc_desc + \
		((_pa) - (_sc)->sc_desc_daddr)))
	struct ath_hal *ah = sc->sc_ah;

	ath_hal_stoppcurecv(ah);	/* disable PCU */
	ath_hal_setrxfilter(ah, 0);	/* clear recv filter */
	ath_hal_stopdmarecv(ah);	/* disable DMA engine */
	mdelay(3);			/* 3ms is long enough for 1 frame */
#ifdef AR_DEBUG
	if (sc->sc_debug & (ATH_DEBUG_RESET | ATH_DEBUG_FATAL)) {	// TODO: compiler warns integer overflow
		struct ath_buf *bf;

		printk("%s: rx queue %p, link %p\n", __func__,
			(caddr_t)(uintptr_t) ath_hal_getrxbuf(ah), sc->sc_rxlink);
		STAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
			struct ath_desc *ds = bf->bf_desc;
			HAL_STATUS status = ath_hal_rxprocdesc(ah, ds,
				bf->bf_daddr, PA2DESC(sc, ds->ds_link));
			if (status == HAL_OK || (sc->sc_debug & ATH_DEBUG_FATAL))
				ath_printrxbuf(bf, status == HAL_OK);
		}
	}
#endif
	sc->sc_rxlink = NULL;		/* just in case */
#undef PA2DESC
}

/* Enable the receive h/w following a reset. */
int ath_startrecv(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct net_device *dev = ic->ic_dev;
	struct ath_buf *bf;

	/*
	 * Cisco's VPN software requires that drivers be able to
	 * receive encapsulated frames that are larger than the MTU.
	 * Since we can't be sure how large a frame we'll get, setup
	 * to handle the larges on possible.  If you're not using the
	 * VPN driver and want to save memory, define ATH_ENFORCE_MTU
	 * and you'll allocate only what you really need.
	 */
#ifdef ATH_ENFORCE_MTU
	if (sc->sc_ic.ic_opmode == IEEE80211_M_MONITOR) {
		sc->sc_rxbufsize = roundup(IEEE80211_MAX_LEN, sc->sc_cachelsz);
	} else {
		sc->sc_rxbufsize = roundup(sizeof(struct ieee80211_frame) +
			dev->mtu + IEEE80211_CRC_LEN +
			(IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN +
			 IEEE80211_WEP_CRCLEN), sc->sc_cachelsz);
	}
#else
	sc->sc_rxbufsize = roundup(IEEE80211_MAX_LEN, sc->sc_cachelsz);
#endif
	DPRINTF(sc, ATH_DEBUG_RESET, "%s: mtu %u cachelsz %u rxbufsize %u\n",
		__func__, dev->mtu, sc->sc_cachelsz, sc->sc_rxbufsize);

	sc->sc_rxlink = NULL;
	STAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
		int error = ath_rxbuf_init(sc, bf);
		if (error != 0) {
			DPRINTF(sc, ATH_DEBUG_RECV,
				"%s: ath_rxbuf_init failed %d\n",
				__func__, error);
			return error;
		}
	}

	bf = STAILQ_FIRST(&sc->sc_rxbuf);
	ath_hal_putrxbuf(ah, bf->bf_daddr);
	ath_hal_rxena(ah);		/* enable recv descriptors */
	ath_mode_init(dev);		/* set filters, etc. */
	ath_hal_startpcurecv(ah);	/* re-enable PCU/DMA engine */
	return 0;
}

/* Update internal state after a channel change. */
void ath_chan_change(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_phymode mode;
	u_int16_t flags;

	/*
	 * Change channels and update the h/w rate map
	 * if we're switching; e.g. 11a to 11b/g.
	 */
	mode = ieee80211_chan2mode(ic, chan);
	if (mode != sc->sc_curmode)
		ath_setcurmode(sc, mode);
	/*
	 * Update BPF state.  NB: ethereal et. al. don't handle
	 * merged flags well so pick a unique mode for their use.
	 */
	if (IEEE80211_IS_CHAN_A(chan))
		flags = IEEE80211_CHAN_A;
	/* XXX 11g schizophrenia */
	else if (IEEE80211_IS_CHAN_G(chan) ||
	    IEEE80211_IS_CHAN_PUREG(chan))
		flags = IEEE80211_CHAN_G;
	else
		flags = IEEE80211_CHAN_B;
	if (IEEE80211_IS_CHAN_T(chan))
		flags |= IEEE80211_CHAN_TURBO;
}

/*
 * Set/change channels.  If the channel is really being changed,
 * it's done by reseting the chip.  To accomplish this we must
 * first cleanup any pending DMA, then restart stuff after a la
 * ath_init.
 */
static int ath_chan_set(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	HAL_CHANNEL hchan;

	/*
	 * Convert to a HAL channel description with
	 * the flags constrained to reflect the current
	 * operating mode.
	 */
	if (chan == IEEE80211_CHAN_ANYC) {
		return 0;
	}
	hchan.channel = chan->ic_freq;
	hchan.channelFlags = ath_chan2flags(ic, chan);

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: %u (%u MHz) -> %u (%u MHz)\n",
	    __func__,
	    ath_hal_mhz2ieee(sc->sc_curchan.channel,
		sc->sc_curchan.channelFlags),
	    	sc->sc_curchan.channel,
	    ath_hal_mhz2ieee(hchan.channel, hchan.channelFlags), hchan.channel);
	if (hchan.channel != sc->sc_curchan.channel ||
	    hchan.channelFlags != sc->sc_curchan.channelFlags) {
		HAL_STATUS status;

		/*
		 * To switch channels clear any pending DMA operations;
		 * wait long enough for the RX fifo to drain, reset the
		 * hardware at the new frequency, and then re-enable
		 * the relevant bits of the h/w.
		 */
		ath_hal_intrset(ah, 0);		/* disable interrupts */
		ath_draintxq(sc);		/* clear pending tx frames */
		ath_stoprecv(sc);		/* turn off frame recv */
#ifdef HAS_VMAC
		if (!ath_hal_reset(ah, IEEE80211_M_MONITOR, &hchan, AH_TRUE, &status)) {
#else
		if (!ath_hal_reset(ah, ic->ic_opmode, &hchan, AH_TRUE, &status)) {
#endif
			if_printf(ic->ic_dev, "ath_chan_set: unable to reset "
				"channel %u (%u Mhz)\n",
				ieee80211_chan2ieee(ic, chan), chan->ic_freq);
			return EIO;
		}
		sc->sc_curchan = hchan;
		ath_update_txpow(sc);		/* update tx power state */

		/*
		 * Re-enable rx framework.
		 */
		if (ath_startrecv(sc) != 0) {
			if_printf(ic->ic_dev,
				"ath_chan_set: unable to restart recv logic\n");
			return EIO;
		}

		/*
		 * Change channels and update the h/w rate map
		 * if we're switching; e.g. 11a to 11b/g.
		 */
		ic->ic_ibss_chan = chan;
		ath_chan_change(sc, chan);

		/*
		 * Re-enable interrupts.
		 */
		ath_hal_intrset(ah, sc->sc_imask);
	}
	return 0;
}

void ath_next_scan(unsigned long arg)
{
	struct net_device *dev = (struct net_device *) arg;
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
}

/* Periodically recalibrate the PHY to account for temperature/environment changes. */
void ath_calibrate(unsigned long arg)
{
	struct net_device *dev = (struct net_device *) arg;
	struct ath_softc *sc = dev->priv;
	struct ath_hal *ah = sc->sc_ah;

#ifdef HAS_VMAC
	if (!sc->sc_vmac_noautocalibrate) {
#endif
	sc->sc_stats.ast_per_cal++;

	DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s: channel %u/%x\n",
		__func__, sc->sc_curchan.channel, sc->sc_curchan.channelFlags);

	if (ath_hal_getrfgain(ah) == HAL_RFGAIN_NEED_CHANGE) {
		/*
		 * Rfgain is out of bounds, reset the chip
		 * to load new gain values.
		 */
		sc->sc_stats.ast_per_rfgain++;
		ath_reset(dev);
	}
	if (!ath_hal_calibrate(ah, &sc->sc_curchan)) {
		DPRINTF(sc, ATH_DEBUG_ANY,
			"%s: calibration of channel %u failed\n",
			__func__, sc->sc_curchan.channel);
		sc->sc_stats.ast_per_calfail++;
	}
#ifdef HAS_VMAC
	}
#endif
	sc->sc_cal_ch.expires = jiffies + (ath_calinterval * HZ);
	add_timer(&sc->sc_cal_ch);
}


int ath_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct net_device *dev = ic->ic_dev;
	struct ath_softc *sc = dev->priv;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_node *ni;
	int i, error;
	const u_int8_t *bssid;
	u_int32_t rfilt;
	static const HAL_LED_STATE leds[] = {
	    HAL_LED_INIT,	/* IEEE80211_S_INIT */
	    HAL_LED_SCAN,	/* IEEE80211_S_SCAN */
	    HAL_LED_AUTH,	/* IEEE80211_S_AUTH */
	    HAL_LED_ASSOC, 	/* IEEE80211_S_ASSOC */
	    HAL_LED_RUN, 	/* IEEE80211_S_RUN */
	};

	DPRINTF(sc, ATH_DEBUG_STATE, "%s: %s -> %s\n", __func__,
		ieee80211_state_name[ic->ic_state],
		ieee80211_state_name[nstate]);

	del_timer(&sc->sc_scan_ch);		/* ap/neighbor scan timer */
	del_timer(&sc->sc_cal_ch);		/* periodic calibration timer */
	ath_hal_setledstate(ah, leds[nstate]);	/* set LED */
	netif_stop_queue(dev);			/* before we do anything else */
	if (sc->sc_rawdev_enabled)
		netif_stop_queue(&sc->sc_rawdev);

	if (nstate == IEEE80211_S_INIT) {
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
		/*
		 * NB: disable interrupts so we don't rx frames.
		 */
		ath_hal_intrset(ah, sc->sc_imask &~ ~HAL_INT_GLOBAL);	// TODO: compiler warns integer overflow
		/*
		 * Notify the rate control algorithm.
		 */
		ath_rate_newstate(sc, nstate);
		goto done;
	}
	ni = ic->ic_bss;
	error = ath_chan_set(sc, ni->ni_chan);
	if (error != 0)
		goto bad;
	rfilt = ath_calcrxfilter(sc, nstate);
	if (nstate == IEEE80211_S_SCAN)
		bssid = dev->broadcast;
	else
		bssid = ni->ni_bssid;
	ath_hal_setrxfilter(ah, rfilt);
	DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %s\n",
		 __func__, rfilt, ether_sprintf(bssid));

	if (nstate == IEEE80211_S_RUN && ic->ic_opmode == IEEE80211_M_STA)
		ath_hal_setassocid(ah, bssid, ni->ni_associd);
	else
		ath_hal_setassocid(ah, bssid, 0);
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			if (ath_hal_keyisvalid(ah, i))
				ath_hal_keysetmac(ah, i, bssid);
	}

	/*
	 * Notify the rate control algorithm so rates
	 * are setup should ath_beacon_alloc be called.
	 */
	ath_rate_newstate(sc, nstate);

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		/* nothing to do */;
	} else if (nstate == IEEE80211_S_RUN) {
		DPRINTF(sc, ATH_DEBUG_STATE,
			"%s(RUN): ic_flags=0x%08x iv=%d bssid=%s "
			"capinfo=0x%04x chan=%d\n"
			 , __func__
			 , ic->ic_flags
			 , ni->ni_intval
			 , ether_sprintf(ni->ni_bssid)
			 , ni->ni_capinfo
			 , ieee80211_chan2ieee(ic, ni->ni_chan));

		/*
		 * Allocate and setup the beacon frame for AP or adhoc mode.
		 */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS) {
			/*
			 * Stop any previous beacon DMA.  This may be
			 * necessary, for example, when an ibss merge
			 * causes reconfiguration; there will be a state
			 * transition from RUN->RUN that means we may
			 * be called with beacon transmission active.
			 */
			ath_hal_stoptxdma(ah, sc->sc_bhalq);
			ath_beacon_free(sc);
			error = ath_beacon_alloc(sc, ni);
			if (error != 0)
				goto bad;
		}

		/*
		 * Configure the beacon and sleep timers.
		 */
		ath_beacon_config(sc);
	} else {
		ath_hal_intrset(ah,
			sc->sc_imask &~ (HAL_INT_SWBA | HAL_INT_BMISS));
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
	}
done:
	/*
	 * Invoke the parent method to complete the work.
	 */
	error = sc->sc_newstate(ic, nstate, arg);
	/*
	 * Finally, start any timers.
	 */
	if (nstate == IEEE80211_S_RUN) {
		/* start periodic recalibration timer */
		mod_timer(&sc->sc_cal_ch, jiffies + (ath_calinterval * HZ));
	} else if (nstate == IEEE80211_S_SCAN) {
		/* start ap/neighbor scan timer */
		mod_timer(&sc->sc_scan_ch,
			jiffies + ((HZ * ath_dwelltime) / 1000));
	}
bad:
	netif_start_queue(dev);
	if (sc->sc_rawdev_enabled) 
		netif_start_queue(&sc->sc_rawdev);

	return error;
}

/*
 * Setup driver-specific state for a newly associated node.
 * Note that we're called also on a re-associate, the isnew
 * param tells us if this is the first time or not.
 */
void ath_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{
	struct ath_softc *sc = ic->ic_dev->priv;

	ath_rate_newassoc(sc, ATH_NODE(ni), isnew);
}

int ath_getchannels(struct net_device *dev, u_int cc, HAL_BOOL outdoor, HAL_BOOL xchanmode)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	HAL_CHANNEL *chans;
	int i, ix, nchan;

	chans = kmalloc(IEEE80211_CHAN_MAX * sizeof(HAL_CHANNEL), GFP_KERNEL);
	if (chans == NULL) {
		if_printf(dev, "unable to allocate channel table\n");
		return ENOMEM;
	}
	if (!ath_hal_init_channels(ah, chans, IEEE80211_CHAN_MAX, &nchan,
	    cc, HAL_MODE_ALL, outdoor, xchanmode)) {
		u_int32_t rd;

		ath_hal_getregdomain(ah, &rd);
		if_printf(dev, "unable to collect channel list from hal; "
			"regdomain likely %u country code %u\n", rd, cc);
		kfree(chans);
		// XXX why is this commented out?
//		return EINVAL;
		return 0;
	}

	/*
	 * Convert HAL channels to ieee80211 ones and insert
	 * them in the table according to their channel number.
	 */
	for (i = 0; i < nchan; i++) {
		HAL_CHANNEL *c = &chans[i];
		ix = ath_hal_mhz2ieee(c->channel, c->channelFlags);
		if (ix > IEEE80211_CHAN_MAX) {
			if_printf(dev, "bad hal channel %u (%u/%x) ignored\n",
				ix, c->channel, c->channelFlags);
			continue;
		}
		/* NB: flags are known to be compatible */
		if (ic->ic_channels[ix].ic_freq == 0) {
			ic->ic_channels[ix].ic_freq = c->channel;
			ic->ic_channels[ix].ic_flags = c->channelFlags;
		} else {
			/* channels overlap; e.g. 11g and 11b */
			ic->ic_channels[ix].ic_flags |= c->channelFlags;
		}
	}
	kfree(chans);
	return 0;
}

/*
 * Turn the LED off: flip the pin and then set a timer so no
 * update will happen for the specified duration.
 */
void ath_led_off(unsigned long arg)
{
	struct ath_softc *sc = (struct ath_softc *) arg;
        
        if(sc->sc_endblink == 1){
            /* part of ath_led_done() */
            sc->sc_blinking = 0;
        } else{
            ath_hal_gpioset(sc->sc_ah, sc->sc_ledpin, !sc->sc_ledon);
            sc->sc_endblink = 1;
            mod_timer(&sc->sc_ledtimer, jiffies + sc->sc_ledoff);
        }
}

/*
 * Blink the LED according to the specified on/off times.
 */
static void
ath_led_blink(struct ath_softc *sc, int on, int off)
{
	DPRINTF(sc, ATH_DEBUG_LED, "%s: on %u off %u\n", __func__, on, off);
	ath_hal_gpioset(sc->sc_ah, sc->sc_ledpin, sc->sc_ledon);
	sc->sc_blinking = 1;
	sc->sc_endblink = 0;
	sc->sc_ledoff = off;
	mod_timer(&sc->sc_ledtimer, jiffies + on);
}

/* called in TX */
void ath_led_event(struct ath_softc *sc, int event)
{

	sc->sc_ledevent = jiffies;	/* time of last event */
	if (sc->sc_blinking)		/* don't interrupt active blink */
		return;
	switch (event) {
	case ATH_LED_POLL:
		ath_led_blink(sc, sc->sc_hwmap[0].ledon,
			sc->sc_hwmap[0].ledoff);
		break;
	case ATH_LED_TX:
		ath_led_blink(sc, sc->sc_hwmap[sc->sc_txrate].ledon,
			sc->sc_hwmap[sc->sc_txrate].ledoff);
		break;
	case ATH_LED_RX:
		ath_led_blink(sc, sc->sc_hwmap[sc->sc_rxrate].ledon,
			sc->sc_hwmap[sc->sc_rxrate].ledoff);
		break;
	}
}

void ath_update_txpow(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	u_int32_t txpow;

	if (sc->sc_curtxpow != ic->ic_txpowlimit)
	{
		ath_hal_settxpowlimit(ah, ic->ic_txpowlimit);
		/* read back in case value is clamped */
		ath_hal_gettxpowlimit(ah, &txpow);
		ic->ic_txpowlimit = sc->sc_curtxpow = txpow;
	}
	/* 
	 * Fetch max tx power level for status requests.
	 */
	ath_hal_getmaxtxpow(sc->sc_ah, &txpow);
	ic->ic_bss->ni_txpower = txpow;
}

void ath_setcurmode(struct ath_softc *sc, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	/* NB: on/off times from the Atheros NDIS driver, w/ permission */
	static const struct {
		u_int		rate;		/* tx/rx 802.11 rate */
		u_int16_t	timeOn;		/* LED on time (ms) */
		u_int16_t	timeOff;	/* LED off time (ms) */
	} blinkrates[] = {
		{ 108,  40,  10 },
		{  96,  44,  11 },
		{  72,  50,  13 },
		{  48,  57,  14 },
		{  36,  67,  16 },
		{  24,  80,  20 },
		{  22, 100,  25 },
		{  18, 133,  34 },
		{  12, 160,  40 },
		{  10, 200,  50 },
		{   6, 240,  58 },
		{   4, 267,  66 },
		{   2, 400, 100 },
		{   0, 500, 130 },
	};
	const HAL_RATE_TABLE *rt;
	int i, j;

	memset(sc->sc_rixmap, 0xff, sizeof(sc->sc_rixmap));
	rt = sc->sc_rates[mode];
	KASSERT(rt != NULL, ("no h/w rate set for phy mode %u", mode));
	for (i = 0; i < rt->rateCount; i++)
		sc->sc_rixmap[rt->info[i].dot11Rate & IEEE80211_RATE_VAL] = i;
	memset(sc->sc_hwmap, 0, sizeof(sc->sc_hwmap));
	for (i = 0; i < 32; i++) {
		u_int8_t ix = rt->rateCodeToIndex[i];
		if (ix == 0xff) {
			sc->sc_hwmap[i].ledon = (500 * HZ) / 1000;
			sc->sc_hwmap[i].ledoff = (130 * HZ) / 1000;
			continue;
		}
		sc->sc_hwmap[i].ieeerate =
			rt->info[ix].dot11Rate & IEEE80211_RATE_VAL;
		sc->sc_hwmap[i].txflags = IEEE80211_RADIOTAP_F_DATAPAD;
		if (rt->info[ix].shortPreamble ||
		    rt->info[ix].phy == IEEE80211_T_OFDM)
			sc->sc_hwmap[i].txflags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		/* NB: receive frames include FCS */
		sc->sc_hwmap[i].rxflags = sc->sc_hwmap[i].txflags |
			IEEE80211_RADIOTAP_F_FCS;
		/* setup blink rate table to avoid per-packet lookup */
		for (j = 0; j < N(blinkrates)-1; j++)
			if (blinkrates[j].rate == sc->sc_hwmap[i].ieeerate)
				break;
		/* NB: this uses the last entry if the rate isn't found */
		/* XXX beware of overlow */
		sc->sc_hwmap[i].ledon = (blinkrates[j].timeOn * HZ) / 1000;
		sc->sc_hwmap[i].ledoff = (blinkrates[j].timeOff * HZ) / 1000;
	}
	sc->sc_currates = rt;
	sc->sc_curmode = mode;
	/*
	 * All protection frames are transmited at 2Mb/s for
	 * 11g, otherwise at 1Mb/s.
	 * XXX select protection rate index from rate table.
	 */
	sc->sc_protrix = ((mode == IEEE80211_MODE_11G || mode == IEEE80211_MODE_TURBO_G) ? 1 : 0);
	/* NB: caller is responsible for reseting rate control state */
#undef N
}


#ifdef AR_DEBUG
static void ath_printrxbuf(struct ath_buf *bf, int done)
{
	struct ath_desc *ds = bf->bf_desc;

	printk("R (%p %p) %08x %08x %08x %08x %08x %08x %c\n",
	    ds, (struct ath_desc *)bf->bf_daddr,
	    ds->ds_link, ds->ds_data,
	    ds->ds_ctl0, ds->ds_ctl1,
	    ds->ds_hw[0], ds->ds_hw[1],
	    !done ? ' ' : (ds->ds_rxstat.rs_status == 0) ? '*' : '!');
}

static void
ath_printtxbuf(struct ath_buf *bf, int done)
{
	struct ath_desc *ds = bf->bf_desc;

	printk("T (%p %p) %08x %08x %08x %08x %08x %08x %08x %08x %c\n",
	    ds, (struct ath_desc *)bf->bf_daddr,
	    ds->ds_link, ds->ds_data,
	    ds->ds_ctl0, ds->ds_ctl1,
	    ds->ds_hw[0], ds->ds_hw[1], ds->ds_hw[2], ds->ds_hw[3],
	    !done ? ' ' : (ds->ds_txstat.ts_status == 0) ? '*' : '!');
}
#endif /* AR_DEBUG */

