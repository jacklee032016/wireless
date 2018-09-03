/*
* $Id: phy_init_utils.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

/* utils functions used when phy device attached, lizhijie */

#include "_phy_init.h"
#include "phy_hw_desc.h"

int ath_countrycode = CTRY_DEFAULT;	/* country code */
int ath_regdomain = 0;			/* regulatory domain */
int ath_dwelltime = 200;		/* 5 channels/second */
int ath_calinterval = 30;		/* calibrate every 30 secs */
int ath_outdoor = AH_TRUE;		/* enable outdoor use */
int ath_xchanmode = AH_TRUE;		/* enable extended channels */

int countrycode = -1;
int outdoor = -1;
int xchanmode = -1;


int _phy_rate_setup(MESH_DEVICE *mdev, u_int mode)
{
	struct ath_softc *sc = mdev->priv;
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


int _phy_desc_alloc(struct ath_softc *sc)
{
#define	DS2PHYS(_sc, _ds) \
	((_sc)->sc_desc_daddr + ((caddr_t)(_ds) - (caddr_t)(_sc)->sc_desc))
	
	int bsize;
	struct ath_desc *ds;
	struct ath_buf *bf;
	int i;

	/* allocate descriptors */
	sc->sc_desc_len = sizeof(struct ath_desc) * (ATH_TXBUF * ATH_TXDESC + ATH_RXBUF + ATH_BCBUF + 1);
	sc->sc_desc = bus_alloc_consistent(sc->sc_bdev, sc->sc_desc_len, &sc->sc_desc_daddr);
	if (sc->sc_desc == NULL)
	{
		mdev_printf(&sc->sc_dev, "%s, could not allocate descriptors\n", __func__);
		return ENOMEM;
	}
	ds = sc->sc_desc;
	DPRINTF(sc, ATH_DEBUG_ANY, "%s: DMA map: %p (%u) -> %p\n",  __func__, ds, (unsigned int) sc->sc_desc_len, (caddr_t) sc->sc_desc_daddr);

	/* allocate buffers */
	bsize = sizeof(struct ath_buf) * (ATH_TXBUF + ATH_RXBUF + ATH_BCBUF + 1);
	bf = kmalloc(bsize, GFP_KERNEL);
	if (bf == NULL)
		goto bad;
	memset(bf, 0, bsize);
	sc->sc_bufptr = bf;

	STAILQ_INIT(&sc->sc_rxbuf);
	for (i = 0; i < ATH_RXBUF; i++, bf++, ds++)
	{
		bf->bf_desc = ds;
		bf->bf_daddr = DS2PHYS(sc, ds);
		STAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	}

	STAILQ_INIT(&sc->sc_txbuf);
	for (i = 0; i < ATH_TXBUF; i++, bf++, ds += ATH_TXDESC)
	{
		bf->bf_desc = ds;
		bf->bf_daddr = DS2PHYS(sc, ds);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
	}

	STAILQ_INIT(&sc->sc_bbuf);
	for (i = 0; i < ATH_BCBUF; i++, bf++, ds++)
	{
		bf->bf_desc = ds;
		bf->bf_daddr = DS2PHYS(sc, ds);
		STAILQ_INSERT_TAIL(&sc->sc_bbuf, bf, bf_list);
	}

	return 0;
bad:
	bus_free_consistent(sc->sc_bdev, sc->sc_desc_len, sc->sc_desc, sc->sc_desc_daddr);
	sc->sc_desc = NULL;
	return ENOMEM;
#undef DS2PHYS
}


static void __ath_descdma_cleanup(struct ath_softc *sc, ath_bufhead *head)
{
	struct ath_buf *bf;
	struct ieee80211_node *ni;

	STAILQ_FOREACH(bf, head, bf_list)
	{
		if (bf->bf_skb)
		{
			bus_unmap_single(sc->sc_bdev,	bf->bf_skbaddr, sc->sc_rxbufsize, BUS_DMA_FROMDEVICE);
			dev_kfree_skb(bf->bf_skb);
			bf->bf_skb = NULL;
		}
		
		ni = bf->bf_node;
		bf->bf_node = NULL;
		if (ni != NULL)
		{
			/* Reclaim node reference. */
			ieee80211_free_node(ni);
		}
	}

	STAILQ_INIT(head);
}

void _phy_desc_free(struct ath_softc *sc)
{
        __ath_descdma_cleanup(sc, &sc->sc_bbuf);
        __ath_descdma_cleanup(sc, &sc->sc_txbuf);
        __ath_descdma_cleanup(sc, &sc->sc_rxbuf);

	/* Free memory associated with all descriptors */
	bus_free_consistent(sc->sc_bdev, sc->sc_desc_len, sc->sc_desc, sc->sc_desc_daddr);

	kfree(sc->sc_bufptr);
	sc->sc_bufptr = NULL;
}

/* Setup a h/w transmit queue for beacons. */
int _phy_beaconq_setup(struct ath_hal *ah)
{
	HAL_TXQ_INFO qi;

	memset(&qi, 0, sizeof(qi));
	qi.tqi_aifs = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmin = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmax = HAL_TXQ_USEDEFAULT;
	/* NB: don't enable any interrupts */
	return ath_hal_setuptxqueue(ah, HAL_TX_QUEUE_BEACON, &qi);
}

struct ieee80211_node *_phy_node_alloc(struct ieee80211_node_table *nt)
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
	phy_rate_node_init(sc, an);

	DPRINTF(sc, ATH_DEBUG_NODE, "%s: an %p\n", __func__, an);
	return &an->an_node;
}

void _phy_node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
        struct ath_softc *sc = ic->ic_dev->priv;
	DPRINTF(sc, ATH_DEBUG_NODE, "%s: ni %p\n", __func__, ni);
/* 
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)		// TODO: seems we need this still
		if (ATH_TXQ_SETUP(sc, i))
			ath_tx_cleanq(&sc->sc_txq[i], ni);
*/
	phy_rate_node_cleanup(sc, ATH_NODE(ni));
	sc->sc_node_free(ni);
}

u_int8_t _phy_node_getrssi(const struct ieee80211_node *ni)
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
static inline uint64_t __ath_tsf_extend(struct ath_hal *ah, uint32_t rstamp)
{
	uint64_t tsf;

	tsf = ath_hal_gettsf64(ah);

	/* Compensate for rollover. */
	if ((tsf & 0x7fff) < rstamp)
		tsf -= 0x8000;
	
	return ((tsf & ~(uint64_t)0x7fff) | rstamp);
}

/* Intercept management frames to collect beacon rssi data and to do ibss merges. */
void _phy_recv_mgmt(struct ieee80211com *ic, struct sk_buff *skb,
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
				u_int64_t tsf = __ath_tsf_extend(sc->sc_ah, rstamp);
				/*
				 * Handle ibss merge as needed; check the tsf on the
				 * frame before attempting the merge.  The 802.11 spec
				 * says the station should change it's bssid to match
				 * the oldest station with the same ssid, where oldest
				 * is determined by the tsf.  Note that hardware
				 * reconfiguration happens through callback to
				 * _phy_newstate as the state machine will go from
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
void _phy_announce(struct ath_softc *sc)
{
#define	HAL_MODE_DUALBAND	(HAL_MODE_11A|HAL_MODE_11B)

	MESH_DEVICE *dev = &sc->sc_dev;
	struct ath_hal *ah = sc->sc_ah;
	u_int modes, cc;
	int i;

	mdev_printf(dev, "mac %d.%d phy %d.%d", ah->ah_macVersion, ah->ah_macRev, ah->ah_phyRev >> 4, ah->ah_phyRev & 0xf);
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
			printk(" 5ghz radio %d.%d 2ghz radio %d.%d", ah->ah_analog5GhzRev >> 4, ah->ah_analog5GhzRev & 0xf, ah->ah_analog2GhzRev >> 4, ah->ah_analog2GhzRev & 0xf);
		else
			printk(" radio %d.%d", ah->ah_analog5GhzRev >> 4, ah->ah_analog5GhzRev & 0xf);
	}
	else
		printk(" radio %d.%d", ah->ah_analog5GhzRev >> 4, ah->ah_analog5GhzRev & 0xf);
	
	printk("\n");
	printk("%s: 802.11 address: %s\n",
		dev->name, swjtu_mesh_mac_sprintf(dev->dev_addr));
	for (i = 0; i <= WME_AC_VO; i++)
	{
		struct ath_txq *txq = sc->sc_ac2q[i];
		mdev_printf(dev, "Use hw queue %u for %s traffic\n", txq->axq_qnum, ieee80211_wme_acnames[i]);
	}

	mdev_printf(dev, "Use hw queue %u for CAB traffic\n", sc->sc_cabq->axq_qnum);
	mdev_printf(dev, "Use hw queue %u for beacons\n", sc->sc_bhalq);
	
#undef HAL_MODE_DUALBAND
}


/* Periodically recalibrate the PHY to account for temperature/environment changes. */
void _phy_calibrate(unsigned long arg)
{
	MESH_DEVICE *dev = (MESH_DEVICE *) arg;
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	struct ath_hal *ah = sc->sc_ah;

#ifdef HAS_VMAC
	if (!sc->sc_vmac_noautocalibrate)
	{
#endif
	sc->sc_stats.ast_per_cal++;

	DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s: channel %u/%x\n", __func__, sc->sc_curchan.channel, sc->sc_curchan.channelFlags);

	if (ath_hal_getrfgain(ah) == HAL_RFGAIN_NEED_CHANGE)
	{
		/*
		 * Rfgain is out of bounds, reset the chip
		 * to load new gain values.
		 */
		sc->sc_stats.ast_per_rfgain++;
		_phy_reset(dev);
	}

	if (!ath_hal_calibrate(ah, &sc->sc_curchan))
	{
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: calibration of channel %u failed\n", __func__, sc->sc_curchan.channel);
		sc->sc_stats.ast_per_calfail++;
	}
#ifdef HAS_VMAC
	}
#endif
	sc->sc_cal_ch.expires = jiffies + (ath_calinterval * HZ);
	add_timer(&sc->sc_cal_ch);
}

/*
 * Setup driver-specific state for a newly associated node.
 * Note that we're called also on a re-associate, the isnew
 * param tells us if this is the first time or not.
 */
void _phy_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{
	struct ath_softc *sc = ic->ic_dev->priv;

	phy_rate_newassoc(sc, ATH_NODE(ni), isnew);
}


/* Callback from the 802.11 layer to update the slot time based on the current setting. */
void _phy_updateslot(MESH_DEVICE *dev)
{
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	/*
	 * When not coordinating the BSS, change the hardware
	 * immediately.  For other operation we defer the change
	 * until beacon updates have propagated to the stations.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		sc->sc_updateslot = UPDATE;
	else
		_phy_setslottime(sc);
}

/*
 * Set and or change channels.  If the channel is really being changed,
 * it's done by reseting the chip.  To accomplish this we must
 * first cleanup any pending DMA, then restart stuff after a la
 * _phy_init.
 */
static int __phy_chan_set(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	HAL_CHANNEL hchan;

	/*
	 * Convert to a HAL channel description with
	 * the flags constrained to reflect the current
	 * operating mode.
	 */
	if (chan == IEEE80211_CHAN_ANYC)
	{
		return 0;
	}
	
	hchan.channel = chan->ic_freq;
	hchan.channelFlags = _phy_chan2flags(ic, chan);

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: %u (%u MHz) -> %u (%u MHz)\n", __func__,
	    ath_hal_mhz2ieee(sc->sc_curchan.channel,sc->sc_curchan.channelFlags),
	    	sc->sc_curchan.channel, ath_hal_mhz2ieee(hchan.channel, hchan.channelFlags), hchan.channel);
	
	if (hchan.channel != sc->sc_curchan.channel ||hchan.channelFlags != sc->sc_curchan.channelFlags)
	{
		HAL_STATUS status;

		/*
		 * To switch channels clear any pending DMA operations;
		 * wait long enough for the RX fifo to drain, reset the
		 * hardware at the new frequency, and then re-enable
		 * the relevant bits of the h/w.
		 */
		ath_hal_intrset(ah, 0);		/* disable interrupts */
		ath_draintxq(sc);		/* clear pending tx frames */
		_phy_stoprecv(sc);		/* turn off frame recv */
#ifdef HAS_VMAC
		if (!ath_hal_reset(ah, IEEE80211_M_MONITOR, &hchan, AH_TRUE, &status))
#else
		if (!ath_hal_reset(ah, ic->ic_opmode, &hchan, AH_TRUE, &status))
#endif
		{
			mdev_printf(ic->ic_dev, "__phy_chan_set: unable to reset channel %u (%u Mhz)\n",
				ieee80211_chan2ieee(ic, chan), chan->ic_freq);
			return EIO;
		}
		sc->sc_curchan = hchan;
		_phy_update_txpow(sc);		/* update tx power state */

		/*
		 * Re-enable rx framework.
		 */
		if (_phy_startrecv(sc) != 0) 
		{
			mdev_printf(ic->ic_dev, "__phy_chan_set: unable to restart recv logic\n");
			return EIO;
		}

		/*
		 * Change channels and update the h/w rate map
		 * if we're switching; e.g. 11a to 11b/g.
		 */
		ic->ic_ibss_chan = chan;
		_phy_chan_change(sc, chan);

		/* Re-enable interrupts. */
		ath_hal_intrset(ah, sc->sc_imask);
	}
	return 0;
}

int _phy_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_node *ni;
	int i, error;
	const u_int8_t *bssid;
	u_int32_t rfilt;
#if 0	
	static const HAL_LED_STATE leds[] =
	{
	    HAL_LED_INIT,	/* IEEE80211_S_INIT */
	    HAL_LED_SCAN,	/* IEEE80211_S_SCAN */
	    HAL_LED_AUTH,	/* IEEE80211_S_AUTH */
	    HAL_LED_ASSOC, 	/* IEEE80211_S_ASSOC */
	    HAL_LED_RUN, 	/* IEEE80211_S_RUN */
	};
#endif

	DPRINTF(sc, ATH_DEBUG_STATE, "%s: %s -> %s\n", __func__,
		ieee80211_state_name[ic->ic_state],
		ieee80211_state_name[nstate]);

	del_timer(&sc->sc_scan_ch);		/* ap/neighbor scan timer */
	del_timer(&sc->sc_cal_ch);		/* periodic calibration timer */
#if 0	
	ath_hal_setledstate(ah, leds[nstate]);	/* set LED */
#endif
	meshif_stop_queue(dev);			/* before we do anything else */
#if 0
	if (sc->sc_rawdev_enabled)
		meshif_stop_queue(&sc->sc_rawdev);
#endif

	if (nstate == IEEE80211_S_INIT)
	{
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
		/*
		 * NB: disable interrupts so we don't rx frames.
		 */
		ath_hal_intrset(ah, sc->sc_imask &~ ~HAL_INT_GLOBAL);	// TODO: compiler warns integer overflow
		/*
		 * Notify the rate control algorithm.
		 */
		phy_rate_newstate(sc, nstate);
		goto done;
	}
	ni = ic->ic_bss;
	error = __phy_chan_set(sc, ni->ni_chan);
	if (error != 0)
		goto bad;
	rfilt = _phy_calcrxfilter(sc, nstate);
	if (nstate == IEEE80211_S_SCAN)
		bssid = dev->broadcast;
	else
		bssid = ni->ni_bssid;
	
	ath_hal_setrxfilter(ah, rfilt);
	DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %s\n", __func__, rfilt, swjtu_mesh_mac_sprintf(bssid));

	if (nstate == IEEE80211_S_RUN && ic->ic_opmode == IEEE80211_M_STA)
		ath_hal_setassocid(ah, bssid, ni->ni_associd);
	else
		ath_hal_setassocid(ah, bssid, 0);
	
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
	{
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			if (ath_hal_keyisvalid(ah, i))
				ath_hal_keysetmac(ah, i, bssid);
	}

	/*
	 * Notify the rate control algorithm so rates
	 * are setup should ath_beacon_alloc be called.
	 */
	phy_rate_newstate(sc, nstate);

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
	{
		/* nothing to do */;
	}
	else if (nstate == IEEE80211_S_RUN)
	{
		DPRINTF(sc, ATH_DEBUG_STATE,
			"%s(RUN): ic_flags=0x%08x iv=%d bssid=%s "
			"capinfo=0x%04x chan=%d\n"
			 , __func__
			 , ic->ic_flags
			 , ni->ni_intval
			 , swjtu_mesh_mac_sprintf(ni->ni_bssid)
			 , ni->ni_capinfo
			 , ieee80211_chan2ieee(ic, ni->ni_chan));

		/*
		 * Allocate and setup the beacon frame for AP or adhoc mode.
		 */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS)
		{
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
	}
	else
	{
		ath_hal_intrset(ah, sc->sc_imask &~ (HAL_INT_SWBA | HAL_INT_BMISS));
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
	if (nstate == IEEE80211_S_RUN)
	{
		/* start periodic recalibration timer */
		mod_timer(&sc->sc_cal_ch, jiffies + (ath_calinterval * HZ));
	}
	else if (nstate == IEEE80211_S_SCAN)
	{
		/* start ap/neighbor scan timer */
		mod_timer(&sc->sc_scan_ch, jiffies + ((HZ * ath_dwelltime) / 1000));
	}
bad:
	
	meshif_start_queue(dev);
#if 0	
	if (sc->sc_rawdev_enabled) 
		meshif_start_queue(&sc->sc_rawdev);
#endif
	return error;
}


void _phy_next_scan(unsigned long arg)
{
	MESH_DEVICE *dev = (MESH_DEVICE *) arg;
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
}


int _phy_getchannels(MESH_DEVICE *dev, u_int cc, HAL_BOOL outdoor, HAL_BOOL xchanmode)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	HAL_CHANNEL *chans;
	int i, ix, nchan;

	chans = kmalloc(IEEE80211_CHAN_MAX * sizeof(HAL_CHANNEL), GFP_KERNEL);
	if (chans == NULL)
	{
		mdev_printf(dev, "unable to allocate channel table\n");
		return ENOMEM;
	}
	
	if (!ath_hal_init_channels(ah, chans, IEEE80211_CHAN_MAX, &nchan,
	    cc, HAL_MODE_ALL, outdoor, xchanmode))
	{
		u_int32_t rd;

		ath_hal_getregdomain(ah, &rd);
		mdev_printf(dev, "unable to collect channel list from hal; "
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
	for (i = 0; i < nchan; i++)
	{
		HAL_CHANNEL *c = &chans[i];
		ix = ath_hal_mhz2ieee(c->channel, c->channelFlags);
		if (ix > IEEE80211_CHAN_MAX) {
			mdev_printf(dev, "bad hal channel %u (%u/%x) ignored\n",
				ix, c->channel, c->channelFlags);
			continue;
		}
		/* NB: flags are known to be compatible */
		if (ic->ic_channels[ix].ic_freq == 0)
		{
			ic->ic_channels[ix].ic_freq = c->channel;
			ic->ic_channels[ix].ic_flags = c->channelFlags;
		}
		else
		{
			/* channels overlap; e.g. 11g and 11b */
			ic->ic_channels[ix].ic_flags |= c->channelFlags;
		}
	}
	kfree(chans);
	return 0;
}

