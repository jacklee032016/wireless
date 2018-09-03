/*
* $Id: if_ath_init.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>

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

int ath_countrycode = CTRY_DEFAULT;	/* country code */
int ath_regdomain = 0;			/* regulatory domain */
int ath_dwelltime = 200;		/* 5 channels/second */
int ath_calinterval = 30;		/* calibrate every 30 secs */
int ath_outdoor = AH_TRUE;		/* enable outdoor use */
int ath_xchanmode = AH_TRUE;		/* enable extended channels */

int countrycode = -1;
int outdoor = -1;
int xchanmode = -1;

static const char *hal_status_desc[] =
{
	"No error",
	"No hardware present or device not yet supported",
	"Memory allocation failed",
	"Hardware didn't respond as expected",
	"EEPROM magic number invalid",
	"EEPROM version invalid",
	"EEPROM unreadable",
	"EEPROM checksum invalid",
	"EEPROM read problem",
	"EEPROM mac address invalid",
	"EEPROM size not supported",
	"Attempt to change write-locked EEPROM",
	"Invalid parameter to function",
	"Hardware revision not supported",
	"Hardware self-test failed",
	"Operation incomplete"
};



static void ath_fatal_tasklet(TQUEUE_ARG data)
{
	struct net_device *dev = (struct net_device *)data;

	if_printf(dev, "hardware error; resetting\n");
	ath_reset(dev);
}

static void ath_radar_tasklet (TQUEUE_ARG data)
{
	struct net_device *dev = (struct net_device *)data;
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c;

	c = radar_handle_interference (ic);

	if (c == NULL)
	{
 		ath_stop (dev);
		printk ("%s: FATAL ERROR - All available channels are marked as being interfered by radar. Stopping radio.\n", dev->name);
		return;
	}

	ic->ic_des_chan = c;
	ic->ic_ibss_chan = c;
	ieee80211_new_state (ic, IEEE80211_S_INIT, -1);
	ath_init (dev);
}

/* overflow */
static void ath_rxorn_tasklet(TQUEUE_ARG data)
{
	struct net_device *dev = (struct net_device *)data;

	if_printf(dev, "rx FIFO overrun; resetting\n");
	ath_reset(dev);
}

static void ath_bmiss_tasklet(TQUEUE_ARG data)
{
	struct net_device *dev = (struct net_device *)data;
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s\n", __func__);
	KASSERT(ic->ic_opmode == IEEE80211_M_STA, ("unexpect operating mode %u", ic->ic_opmode));
	if (ic->ic_state == IEEE80211_S_RUN)
	{
		/*
		 * Rather than go directly to scan state, try to
		 * reassociate first.  If that fails then the state
		 * machine will drop us into scanning after timing
		 * out waiting for a probe response.
		 */
		ieee80211_new_state(ic, IEEE80211_S_ASSOC, -1);
	}
}


static int ath_rate_setup(struct net_device *dev, u_int mode)
{
	struct ath_softc *sc = dev->priv;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	const HAL_RATE_TABLE *rt;
	struct ieee80211_rateset *rs;
	int i, maxrates;

	switch (mode)
	{
		case IEEE80211_MODE_11A:
			sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11A);
			break;
		case IEEE80211_MODE_11B:
			sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11B);
			break;
		case IEEE80211_MODE_11G:
			sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11G);
			break;
		case IEEE80211_MODE_TURBO_A:
			sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_TURBO);
			break;
		case IEEE80211_MODE_TURBO_G:
			sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_108G);
			break;
		default:
			DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid mode %u\n", __func__, mode);
			return 0;
	}
	
	rt = sc->sc_rates[mode];
	if (rt == NULL)
		return 0;
	if (rt->rateCount > IEEE80211_RATE_MAXSIZE)
	{
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: rate table too small (%u > %u)\n", __func__, rt->rateCount, IEEE80211_RATE_MAXSIZE);
		maxrates = IEEE80211_RATE_MAXSIZE;
	}
	else
		maxrates = rt->rateCount;

	rs = &ic->ic_sup_rates[mode];
	for (i = 0; i < maxrates; i++)
		rs->rs_rates[i] = rt->info[i].dot11Rate;
	rs->rs_nrates = maxrates;
	return 1;
}


static int ath_desc_alloc(struct ath_softc *sc)
{
#define	DS2PHYS(_sc, _ds) \
	((_sc)->sc_desc_daddr + ((caddr_t)(_ds) - (caddr_t)(_sc)->sc_desc))
	int bsize;
	struct ath_desc *ds;
	struct ath_buf *bf;
	int i;

	/* allocate descriptors */
	sc->sc_desc_len = sizeof(struct ath_desc) *
				(ATH_TXBUF * ATH_TXDESC + ATH_RXBUF + ATH_BCBUF + 1);
	sc->sc_desc = bus_alloc_consistent(sc->sc_bdev, sc->sc_desc_len, &sc->sc_desc_daddr);
	if (sc->sc_desc == NULL)
	{
		if_printf(&sc->sc_dev, "%s, could not allocate descriptors\n", __func__);
		return ENOMEM;
	}
	ds = sc->sc_desc;
	DPRINTF(sc, ATH_DEBUG_ANY, "%s: DMA map: %p (%u) -> %p\n",
	    __func__, ds, (unsigned int) sc->sc_desc_len, (caddr_t) sc->sc_desc_daddr);

	/* allocate buffers */
	bsize = sizeof(struct ath_buf) * (ATH_TXBUF + ATH_RXBUF + ATH_BCBUF + 1);
	bf = kmalloc(bsize, GFP_KERNEL);
	if (bf == NULL)
		goto bad;
	memset(bf, 0, bsize);
	sc->sc_bufptr = bf;

	STAILQ_INIT(&sc->sc_rxbuf);
	for (i = 0; i < ATH_RXBUF; i++, bf++, ds++) {
		bf->bf_desc = ds;
		bf->bf_daddr = DS2PHYS(sc, ds);
		STAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	}

	STAILQ_INIT(&sc->sc_txbuf);
	for (i = 0; i < ATH_TXBUF; i++, bf++, ds += ATH_TXDESC) {
		bf->bf_desc = ds;
		bf->bf_daddr = DS2PHYS(sc, ds);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
	}

	STAILQ_INIT(&sc->sc_bbuf);
	for (i = 0; i < ATH_BCBUF; i++, bf++, ds++) {
		bf->bf_desc = ds;
		bf->bf_daddr = DS2PHYS(sc, ds);
		STAILQ_INSERT_TAIL(&sc->sc_bbuf, bf, bf_list);
	}

	return 0;
bad:
	bus_free_consistent(sc->sc_bdev, sc->sc_desc_len,
		sc->sc_desc, sc->sc_desc_daddr);
	sc->sc_desc = NULL;
	return ENOMEM;
#undef DS2PHYS
}


static void ath_descdma_cleanup(struct ath_softc *sc, ath_bufhead *head)
{
	struct ath_buf *bf;
	struct ieee80211_node *ni;

	STAILQ_FOREACH(bf, head, bf_list){
		if (bf->bf_skb) {
			bus_unmap_single(sc->sc_bdev,
				bf->bf_skbaddr, sc->sc_rxbufsize,
				BUS_DMA_FROMDEVICE);
			dev_kfree_skb(bf->bf_skb);
			bf->bf_skb = NULL;
		}
		ni = bf->bf_node;
		bf->bf_node = NULL;
		if (ni != NULL) {
			/*
			 * Reclaim node reference.
			 */
			ieee80211_free_node(ni);
		}
	}

	STAILQ_INIT(head);
}

static void ath_desc_free(struct ath_softc *sc)
{
        ath_descdma_cleanup(sc, &sc->sc_bbuf);
        ath_descdma_cleanup(sc, &sc->sc_txbuf);
        ath_descdma_cleanup(sc, &sc->sc_rxbuf);

	/* Free memory associated with all descriptors */
	bus_free_consistent(sc->sc_bdev, sc->sc_desc_len, sc->sc_desc, sc->sc_desc_daddr);

	kfree(sc->sc_bufptr);
	sc->sc_bufptr = NULL;
}


/* Setup a h/w transmit queue for beacons. */
static int ath_beaconq_setup(struct ath_hal *ah)
{
	HAL_TXQ_INFO qi;

	memset(&qi, 0, sizeof(qi));
	qi.tqi_aifs = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmin = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmax = HAL_TXQ_USEDEFAULT;
	/* NB: don't enable any interrupts */
	return ath_hal_setuptxqueue(ah, HAL_TX_QUEUE_BEACON, &qi);
}

static struct ieee80211_node *ath_node_alloc(struct ieee80211_node_table *nt)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ath_softc *sc = ic->ic_dev->priv;
	const size_t space = sizeof(struct ath_node) + sc->sc_rc->arc_space;
	struct ath_node *an;

	an = kmalloc(space, GFP_ATOMIC);
	if (an == NULL)
	{/* XXX stat+msg */
		return NULL;
	}
	memset(an, 0, space);
	an->an_avgrssi = ATH_RSSI_DUMMY_MARKER;
	an->an_halstats.ns_avgbrssi = ATH_RSSI_DUMMY_MARKER;
	an->an_halstats.ns_avgrssi = ATH_RSSI_DUMMY_MARKER;
	an->an_halstats.ns_avgtxrssi = ATH_RSSI_DUMMY_MARKER;
	ath_rate_node_init(sc, an);

	DPRINTF(sc, ATH_DEBUG_NODE, "%s: an %p\n", __func__, an);
	return &an->an_node;
}

static void ath_node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
        struct ath_softc *sc = ic->ic_dev->priv;
	DPRINTF(sc, ATH_DEBUG_NODE, "%s: ni %p\n", __func__, ni);
/* 
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)		// TODO: seems we need this still
		if (ATH_TXQ_SETUP(sc, i))
			ath_tx_cleanq(&sc->sc_txq[i], ni);
*/
	ath_rate_node_cleanup(sc, ATH_NODE(ni));
	sc->sc_node_free(ni);
}

static u_int8_t ath_node_getrssi(const struct ieee80211_node *ni)
{
#define	HAL_EP_RND(x, mul) \
	((((x)%(mul)) >= ((mul)/2)) ? ((x) + ((mul) - 1)) / (mul) : (x)/(mul))
	u_int32_t avgrssi = ATH_NODE_CONST(ni)->an_avgrssi;
	int32_t rssi;

	/*
	 * When only one frame is received there will be no state in
	 * avgrssi so fallback on the value recorded by the 802.11 layer.
	 */
	if (avgrssi != ATH_RSSI_DUMMY_MARKER)
		rssi = HAL_EP_RND(avgrssi, HAL_RSSI_EP_MULTIPLIER);
	else
		rssi = ni->ni_rssi;
	/* NB: theoretically we shouldn't need this, but be paranoid */
	return rssi < 0 ? 0 : rssi > 127 ? 127 : rssi;
#undef HAL_EP_RND
}


/* Extend 15-bit time stamp from rx descriptor to
 * a full 64-bit TSF using the current h/w TSF.  */
static inline uint64_t ath_tsf_extend(struct ath_hal *ah, uint32_t rstamp)
{
	uint64_t tsf;

	tsf = ath_hal_gettsf64(ah);

	/* Compensate for rollover. */
	if ((tsf & 0x7fff) < rstamp)
		tsf -= 0x8000;
	
	return ((tsf & ~(uint64_t)0x7fff) | rstamp);
}

/* Intercept management frames to collect beacon rssi data and to do ibss merges. */
static void ath_recv_mgmt(struct ieee80211com *ic, struct sk_buff *skb,
	struct ieee80211_node *ni,	int subtype, int rssi, u_int32_t rstamp)
{
	struct ath_softc *sc = ic->ic_dev->priv;

	/*
	 * Call up first so subsequent work can use information
	 * potentially stored in the node (e.g. for ibss merge).
	 */
	sc->sc_recv_mgmt(ic, skb, ni, subtype, rssi, rstamp);
	switch (subtype)
	{
		case IEEE80211_FC0_SUBTYPE_BEACON:
			/* update rssi statistics for use by the hal */
			ATH_RSSI_LPF((ATH_NODE(ni))->an_halstats.ns_avgbrssi, rssi);
			/* fall thru... */
		case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
			if (ic->ic_opmode == IEEE80211_M_IBSS && ic->ic_state == IEEE80211_S_RUN)
			{
				/* Extend rstamp with the current tsf to 64 bit */
				u_int64_t tsf = ath_tsf_extend(sc->sc_ah, rstamp);
				/*
				 * Handle ibss merge as needed; check the tsf on the
				 * frame before attempting the merge.  The 802.11 spec
				 * says the station should change it's bssid to match
				 * the oldest station with the same ssid, where oldest
				 * is determined by the tsf.  Note that hardware
				 * reconfiguration happens through callback to
				 * ath_newstate as the state machine will go from
				 * RUN -> RUN when this happens.
				 */
				if (le64toh(ni->ni_tstamp.tsf) >= tsf)
				{
					DPRINTF(sc, ATH_DEBUG_STATE, "ibss merge, rstamp %u tsf %llx tstamp %llx\n", rstamp, tsf, ni->ni_tstamp.tsf);
					ieee80211_ibss_merge(ic, ni);
				}
			}
			break;
	}
}

/* Announce various information on device/driver attach.  */
static void ath_announce(struct ath_softc *sc)
{
#define	HAL_MODE_DUALBAND	(HAL_MODE_11A|HAL_MODE_11B)
	struct net_device *dev = &sc->sc_dev;
	struct ath_hal *ah = sc->sc_ah;
	u_int modes, cc;
	int i;

	if_printf(dev, "mac %d.%d phy %d.%d", ah->ah_macVersion, ah->ah_macRev, ah->ah_phyRev >> 4, ah->ah_phyRev & 0xf);
	/*
	 * Print radio revision(s).  We check the wireless modes
	 * to avoid falsely printing revs for inoperable parts.
	 * Dual-band radio revs are returned in the 5Ghz rev number.
	 */
	ath_hal_getcountrycode(ah, &cc);
	modes = ath_hal_getwirelessmodes(ah, cc);
	if ((modes & HAL_MODE_DUALBAND) == HAL_MODE_DUALBAND)
	{
		if (ah->ah_analog5GhzRev && ah->ah_analog2GhzRev)
			printk(" 5ghz radio %d.%d 2ghz radio %d.%d",
				ah->ah_analog5GhzRev >> 4,
				ah->ah_analog5GhzRev & 0xf,
				ah->ah_analog2GhzRev >> 4,
				ah->ah_analog2GhzRev & 0xf);
		else
			printk(" radio %d.%d", ah->ah_analog5GhzRev >> 4, ah->ah_analog5GhzRev & 0xf);
	} else
		printk(" radio %d.%d", ah->ah_analog5GhzRev >> 4, ah->ah_analog5GhzRev & 0xf);
	
	printk("\n");
	for (i = 0; i <= WME_AC_VO; i++)
	{
		struct ath_txq *txq = sc->sc_ac2q[i];
                    if_printf(dev, "Use hw queue %u for %s traffic\n", txq->axq_qnum, ieee80211_wme_acnames[i]);
	}
        if_printf(dev, "Use hw queue %u for CAB traffic\n", sc->sc_cabq->axq_qnum);
        if_printf(dev, "Use hw queue %u for beacons\n", sc->sc_bhalq);
#undef HAL_MODE_DUALBAND
}


int ath_attach(u_int16_t devid, struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah;
	HAL_STATUS status;
	int error = 0, i;
	u_int8_t csz;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: devid 0x%x\n", __func__, devid);

	/*
	 * Cache line size is used to size and align various
	 * structures used to communicate with the hardware.
	 */
	bus_read_cachesize(sc, &csz);
	/* XXX assert csz is non-zero */
	sc->sc_cachelsz = csz << 2;		/* convert to bytes */

	ATH_LOCK_INIT(sc);
	ATH_TXBUF_LOCK_INIT(sc);

	ATH_INIT_TQUEUE(&sc->sc_fataltq,		ath_fatal_tasklet,	dev);
	ATH_INIT_TQUEUE(&sc->sc_radartq,	ath_radar_tasklet,	dev);
	ATH_INIT_TQUEUE(&sc->sc_rxorntq,	ath_rxorn_tasklet,	dev);
	ATH_INIT_TQUEUE(&sc->sc_bmisstq,	ath_bmiss_tasklet,	dev);

	ATH_INIT_TQUEUE(&sc->sc_rxtq,		ath_rx_tasklet,		dev);
	ATH_INIT_TQUEUE(&sc->sc_bstuckq,	ath_bstuck_tasklet,	dev);

#ifdef HAS_VMAC
	/*
	 * softmac specific initialization
	 */
	ATH_INIT_TQUEUE(&sc->sc_vmac_worktq,ath_cu_softmac_work_tasklet,dev);

	atomic_set(&(sc->sc_vmac_tx_packets_inflight),0);
	sc->sc_vmac_mac_lock = RW_LOCK_UNLOCKED;
	sc->sc_vmac_defaultmac = 0;
	sc->sc_vmac_mac = 0;
	sc->sc_vmac_phy = 0;
	sc->sc_vmac_txlatency = ATH_VMAC_DEFAULT_TXLATENCY;
	sc->sc_vmac_phocus_settletime = ATH_VMAC_DEFAULT_PHOCUS_SETTLETIME;

	sc->sc_vmac_wifictl0 = CU_SOFTMAC_WIFICTL0;
	sc->sc_vmac_wifictl1 = CU_SOFTMAC_WIFICTL1;

	sc->sc_vmac_inst = kmalloc(sizeof(struct ath_vmac_instance),GFP_ATOMIC);
	memset(sc->sc_vmac_inst, 0, sizeof(struct ath_vmac_instance));
	sc->sc_vmac_inst->lock = RW_LOCK_UNLOCKED;
	sc->sc_vmac_inst->defaultphy = 0;
	sc->sc_vmac_inst->phyinfo = 0;

	ath_vmac_register(sc);
#endif

	/*
	 * Attach the hal and verify ABI compatibility by checking
	 * the hal's ABI signature against the one the driver was
	 * compiled with.  A mismatch indicates the driver was
	 * built with an ah.h that does not correspond to the hal
	 * module loaded in the kernel.
	 */
	ah = _ath_hal_attach(devid, sc, 0, (void *) dev->mem_start, &status);
	if (ah == NULL)
	{
		printk(KERN_ERR "%s: unable to attach hardware: '%s' (HAL status %u)\n",__func__, hal_status_desc[status], status);
		error = ENXIO;
		goto bad;
	}
	
	if (ah->ah_abi != HAL_ABI_VERSION)
	{
		printk(KERN_ERR "%s: HAL ABI mismatch; driver expects 0x%x, HAL reports 0x%x\n", __func__, HAL_ABI_VERSION, ah->ah_abi);
		error = ENXIO;          /* XXX */
		goto bad;
	}
	sc->sc_ah = ah;

	/*
	 * Check if the MAC has multi-rate retry support.
	 * We do this by trying to setup a fake extended
	 * descriptor.  MAC's that don't have support will
	 * return false w/o doing anything.  MAC's that do
	 * support it will return true w/o doing anything.
	 */
	sc->sc_mrretry = ath_hal_setupxtxdesc(ah, NULL, 0,0, 0,0, 0,0);

	/*
	 * Check if the device has hardware counters for PHY
	 * errors.  If so we need to enable the MIB interrupt
	 * so we can act on stat triggers.
	 */
	if (ath_hal_hwphycounters(ah))
		sc->sc_needmib = 1;

	/*
	 * Get the hardware key cache size.
	 */
	sc->sc_keymax = ath_hal_keycachesize(ah);
	if (sc->sc_keymax > sizeof(sc->sc_keymap) * NBBY)
	{
		if_printf(dev,"Warning, using only %zu of %u key cache slots\n", sizeof(sc->sc_keymap) * NBBY, sc->sc_keymax);
		sc->sc_keymax = sizeof(sc->sc_keymap) * NBBY;
	}
	/*
	 * Reset the key cache since some parts do not
	 * reset the contents on initial power up.
	 */
	for (i = 0; i < sc->sc_keymax; i++)
		ath_hal_keyreset(ah, i);
	/*
	 * Mark key cache slots associated with global keys
	 * as in use.  If we knew TKIP was not to be used we
	 * could leave the +32, +64, and +32+64 slots free.
	 * XXX only for splitmic.
	 */
	for (i = 0; i < IEEE80211_WEP_NKID; i++)
	{
		setbit(sc->sc_keymap, i);
		setbit(sc->sc_keymap, i+32);
		setbit(sc->sc_keymap, i+64);
		setbit(sc->sc_keymap, i+32+64);
	}

	/*
	 * Collect the channel list using the default country
	 * code and including outdoor channels.  The 802.11 layer
	 * is resposible for filtering this list based on settings
	 * like the phy mode.
	 */
	if (countrycode != -1)
		ath_countrycode = countrycode;
	if (outdoor != -1)
		ath_outdoor = outdoor;
	if (xchanmode != -1)
		ath_xchanmode = xchanmode;
	error = ath_getchannels(dev, ath_countrycode, ath_outdoor, ath_xchanmode);
	if (error != 0)
		goto bad;

	/*
	 * Setup rate tables for all potential media types.
	 */
	ath_rate_setup(dev, IEEE80211_MODE_11A);
	ath_rate_setup(dev, IEEE80211_MODE_11B);
	ath_rate_setup(dev, IEEE80211_MODE_11G);
	ath_rate_setup(dev, IEEE80211_MODE_TURBO_A);
	ath_rate_setup(dev, IEEE80211_MODE_TURBO_G);
	/* NB: setup here so ath_rate_update is happy */
	ath_setcurmode(sc, IEEE80211_MODE_11A);

	/*
	 * Allocate tx+rx descriptors and populate the lists.
	 */
	error = ath_desc_alloc(sc);
	if (error != 0)
	{
		if_printf(dev, "failed to allocate descriptors: %d\n", error);
		goto bad;
	}

	/*
	 * Allocate hardware transmit queues: one queue for
	 * beacon frames and one data queue for each QoS
	 * priority.  Note that the hal handles reseting
	 * these queues at the needed time.
	 *
	 * XXX PS-Poll
	 */
	sc->sc_bhalq = ath_beaconq_setup(ah);
	if (sc->sc_bhalq == (u_int) -1)
	{
		if_printf(dev, "unable to setup a beacon xmit queue!\n");
		goto bad2;
	}
	sc->sc_cabq = ath_txq_setup(sc, HAL_TX_QUEUE_CAB, 0);
	if (sc->sc_cabq == NULL) {
		if_printf(dev, "unable to setup CAB xmit queue!\n");
		error = EIO;
		goto bad2;
	}
	/* NB: insure BK queue is the lowest priority h/w queue */
	if (!ath_tx_setup(sc, WME_AC_BK, HAL_WME_AC_BK))
	{
		if_printf(dev, "unable to setup xmit queue for %s traffic!\n",	ieee80211_wme_acnames[WME_AC_BK]);
		error = EIO;
		goto bad2;
	}
	if (!ath_tx_setup(sc, WME_AC_BE, HAL_WME_AC_BE) ||
	    !ath_tx_setup(sc, WME_AC_VI, HAL_WME_AC_VI) ||
	    !ath_tx_setup(sc, WME_AC_VO, HAL_WME_AC_VO))
	{
		/* 
		 * Not enough hardware tx queues to properly do WME;
		 * just punt and assign them all to the same h/w queue.
		 * We could do a better job of this if, for example,
		 * we allocate queues when we switch from station to AP mode.
		 */
		if (sc->sc_ac2q[WME_AC_VI] != NULL)
			ath_tx_cleanupq(sc, sc->sc_ac2q[WME_AC_VI]);
		if (sc->sc_ac2q[WME_AC_BE] != NULL)
			ath_tx_cleanupq(sc, sc->sc_ac2q[WME_AC_BE]);
		sc->sc_ac2q[WME_AC_BE] = sc->sc_ac2q[WME_AC_BK];
		sc->sc_ac2q[WME_AC_VI] = sc->sc_ac2q[WME_AC_BK];
		sc->sc_ac2q[WME_AC_VO] = sc->sc_ac2q[WME_AC_BK];
	}

	/* 
	 * Special case certain configurations.  Note the
	 * CAB queue is handled by these specially so don't
	 * include them when checking the txq setup mask.
	 */
	switch (sc->sc_txqsetup &~ (1<<sc->sc_cabq->axq_qnum))
	{
		case 0x01:
			ATH_INIT_TQUEUE(&sc->sc_txtq, ath_tx_tasklet_q0, dev);
			break;
		case 0x0f:
			ATH_INIT_TQUEUE(&sc->sc_txtq, ath_tx_tasklet_q0123, dev);
			break;
		default:
			ATH_INIT_TQUEUE(&sc->sc_txtq, ath_tx_tasklet, dev);
			break;
	}

	/*
	 * Setup rate control.  Some rate control modules
	 * call back to change the anntena state so expose
	 * the necessary entry points.
	 * XXX maybe belongs in struct ath_ratectrl?
	 */
	sc->sc_setdefantenna = ath_setdefantenna;
	sc->sc_rc = ath_rate_attach(sc);
	if (sc->sc_rc == NULL)
	{
		error = EIO;
		goto bad2;
	}

	init_timer(&sc->sc_scan_ch);
	sc->sc_scan_ch.function = ath_next_scan;
	sc->sc_scan_ch.data = (unsigned long) dev;

	init_timer(&sc->sc_cal_ch);
	sc->sc_cal_ch.function = ath_calibrate;
	sc->sc_cal_ch.data = (unsigned long) dev;

	sc->sc_blinking = 0;
	sc->sc_ledstate = 1;
	sc->sc_ledon = 0;			/* low true */
	sc->sc_ledidle = (2700*HZ)/1000;	/* 2.7sec */

	init_timer(&sc->sc_ledtimer);
	sc->sc_ledtimer.function = ath_led_off;
	sc->sc_ledtimer.data = (unsigned long) sc;
	/*
	 * Auto-enable soft led processing for IBM cards and for
	 * 5211 minipci cards.  Users can also manually enable/disable
	 * support with a sysctl.
	 */
	sc->sc_softled = (devid == AR5212_DEVID_IBM || devid == AR5211_DEVID);
	if (sc->sc_softled)
	{
		ath_hal_gpioCfgOutput(ah, sc->sc_ledpin);
		ath_hal_gpioset(ah, sc->sc_ledpin, !sc->sc_ledon);
	}

	/* lizhijie*/
	ath_net_init(dev);

	
	ic->ic_dev = dev;
	ic->ic_devstats = &sc->sc_devstats;
	ic->ic_init = ath_init;
	ic->ic_reset = ath_reset;
	ic->ic_newassoc = ath_newassoc;
	ic->ic_updateslot = ath_updateslot;
	ic->ic_wme.wme_update = ath_wme_update;
	/* XXX not right but it's not used anywhere important */
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps =
		  IEEE80211_C_IBSS		/* ibss, nee adhoc, mode */
		| IEEE80211_C_HOSTAP		/* hostap mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_SHSLOT		/* short slot time supported */
		| IEEE80211_C_WPA		/* capable of WPA1+WPA2 */
		;

	/* initialize management queue	 */
	skb_queue_head_init(&ic->ic_mgtq);

	/* Query the hal to figure out h/w crypto support. */
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_WEP))
		ic->ic_caps |= IEEE80211_C_WEP;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_AES_OCB))
		ic->ic_caps |= IEEE80211_C_AES;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_AES_CCM))
		ic->ic_caps |= IEEE80211_C_AES_CCM;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_CKIP))
		ic->ic_caps |= IEEE80211_C_CKIP;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_TKIP))
	{
		ic->ic_caps |= IEEE80211_C_TKIP;
		/*
		 * Check if h/w does the MIC and/or whether the
		 * separate key cache entries are required to
		 * handle both tx+rx MIC keys.
		 */
		if (ath_hal_ciphersupported(ah, HAL_CIPHER_MIC))
			ic->ic_caps |= IEEE80211_C_TKIPMIC;
		if (ath_hal_tkipsplit(ah))
			sc->sc_splitmic = 1;
	}
	/* Transmit Power Control
	 * TPC support can be done either with a global cap or
	 * per-packet support.  The latter is not available on
	 * all parts.  We're a bit pedantic here as all parts
	 * support a global cap.
	 */
	sc->sc_hastpc = ath_hal_hastpc(ah);
	if (sc->sc_hastpc || ath_hal_hastxpowlimit(ah))
		ic->ic_caps |= IEEE80211_C_TXPMGT;

	/*
	 * Mark WME capability only if we have sufficient
	 * hardware queues to do proper priority scheduling.
	 */
	if (sc->sc_ac2q[WME_AC_BE] != sc->sc_ac2q[WME_AC_BK])
		ic->ic_caps |= IEEE80211_C_WME;

	/** Check for frame bursting capability. */
	if (ath_hal_hasbursting(ah))
		ic->ic_caps |= IEEE80211_C_BURST;

	/*
	 * Indicate we need the 802.11 header padded to a
	 * 32-bit boundary for 4-address and QoS frames.
	 */
	ic->ic_flags |= IEEE80211_F_DATAPAD;

	/* Query the hal about antenna support. */
	if (ath_hal_hasdiversity(ah))
	{
		sc->sc_hasdiversity = 1;
		sc->sc_diversity = ath_hal_getdiversity(ah);
	}
	sc->sc_defant = ath_hal_getdefantenna(ah);

	/*
	 * Not all chips have the VEOL(Virtual EOL) support we want to
	 * use with IBSS beacons; check here for it.
	 */
	sc->sc_hasveol = ath_hal_hasveol(ah);

	sc->sc_rxfilter = 0;
	sc->sc_rawdev_enabled = 0;
	sc->sc_rawdev.type = ARPHRD_IEEE80211;

	/* get mac address from hardware */
	ath_hal_getmac(ah, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(dev->dev_addr, ic->ic_myaddr);

	/* call MI attach routine. */
	ieee80211_ifattach(ic);
	/* override default methods */
	ic->ic_node_alloc = ath_node_alloc;
	sc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = ath_node_free;
	ic->ic_node_getrssi = ath_node_getrssi;
	sc->sc_recv_mgmt = ic->ic_recv_mgmt;
	ic->ic_recv_mgmt = ath_recv_mgmt;
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = ath_newstate;
	
	ic->ic_crypto.cs_key_alloc = ath_key_alloc;
	ic->ic_crypto.cs_key_delete = ath_key_delete;
	ic->ic_crypto.cs_key_set = ath_key_set;
	ic->ic_crypto.cs_key_update_begin = ath_key_update_begin;
	ic->ic_crypto.cs_key_update_end = ath_key_update_end;

	radar_init(ic);

	/* complete initialization */
	ieee80211_media_init(ic, ath_media_change, ieee80211_media_status);

	if (register_netdev(dev))
	{
		printk(KERN_ERR "%s: unable to register device\n", dev->name);
		goto bad3;
	}

	/*
	 * Attach dynamic MIB vars and announce support
	 * now that we have a device name with unit number.
	 */
#ifdef CONFIG_SYSCTL
	ath_dynamic_sysctl_register(sc);
	ath_rate_dynamic_sysctl_register(sc);
	ieee80211_sysctl_register(ic);
#endif

	ieee80211_announce(ic);
	ath_announce(sc);
	return 0;
	
bad3:
	ieee80211_ifdetach(ic);
	ath_rate_detach(sc->sc_rc);
bad2:
	if (sc->sc_txq[WME_AC_BK].axq_qnum != (u_int) -1)
	{
		ATH_TXQ_LOCK_DESTROY(&sc->sc_txq[WME_AC_BK]);
	}
	if (sc->sc_txq[WME_AC_BE].axq_qnum != (u_int) -1)
	{
		ATH_TXQ_LOCK_DESTROY(&sc->sc_txq[WME_AC_BE]);
	}
	if (sc->sc_txq[WME_AC_VI].axq_qnum != (u_int) -1)
	{
		ATH_TXQ_LOCK_DESTROY(&sc->sc_txq[WME_AC_VI]);
	}
	if (sc->sc_txq[WME_AC_VO].axq_qnum != (u_int) -1) {
		ATH_TXQ_LOCK_DESTROY(&sc->sc_txq[WME_AC_VO]);
	}
	ath_tx_cleanup(sc);
	ath_desc_free(sc);
bad:
	if (ah)
	{
		ath_hal_detach(ah);
	}
	sc->sc_invalid = 1;
	return error;
}

int ath_detach(struct net_device *dev)
{
        struct ath_softc *sc = dev->priv;
        struct ieee80211com *ic = &sc->sc_ic;
    
	DPRINTF(sc, ATH_DEBUG_ANY, "%s: flags %x\n", __func__, dev->flags);
	ath_stop(dev);
	sc->sc_invalid = 1;
	/* 
	 * NB: the order of these is important:
	 * o call the 802.11 layer before detaching the hal to
	 *   insure callbacks into the driver to delete global
	 *   key cache entries can be handled
	 * o reclaim the tx queue data structures after calling
	 *   the 802.11 layer as we'll get called back to reclaim
	 *   node state and potentially want to use them
	 * o to cleanup the tx queues the hal is called, so detach
	 *   it last
	 * Other than that, it's straightforward...
	 */
	ieee80211_ifdetach(ic);
	ath_rate_detach(sc->sc_rc);
	ath_desc_free(sc);
	ath_tx_cleanup(sc);
	ath_hal_detach(sc->sc_ah);

#ifdef HAS_VMAC
	ath_vmac_unregister(sc);

	vmac_free_mac(sc->sc_vmac_defaultmac);
	sc->sc_vmac_defaultmac = 0;

	vmac_free_phy(sc->sc_vmac_inst->defaultphy);
	sc->sc_vmac_inst->defaultphy = 0;
	kfree(sc->sc_vmac_inst);
	sc->sc_vmac_inst = 0;
#endif

	/*
	 * NB: can't reclaim these until after ieee80211_ifdetach
	 * returns because we'll get called back to reclaim node
	 * state and potentially want to use them.
	 */
#ifdef CONFIG_SYSCTL
	ath_dynamic_sysctl_unregister(sc);
#endif /* CONFIG_SYSCTL */
	ath_rawdev_detach(sc);
	unregister_netdev(dev);
	return 0;
}

