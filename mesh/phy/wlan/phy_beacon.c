/*
* $Id: phy_beacon.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#include "phy.h"

#include "phy_hw_desc.h"

/* Allocate and setup an initial beacon frame. */
int ath_beacon_alloc(struct ath_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_buf *bf;
	struct sk_buff *skb;

	bf = STAILQ_FIRST(&sc->sc_bbuf);
	if (bf == NULL)
	{
		DPRINTF(sc, ATH_DEBUG_BEACON, "%s: no dma buffers\n", __func__);
		sc->sc_stats.ast_be_nobuf++;	/* XXX */
		return ENOMEM;			/* XXX */
	}
	/*
	 * NB: the beacon data buffer must be 32-bit aligned;
	 * we assume the mbuf routines will return us something
	 * with this alignment (perhaps should assert).
	 */
	skb = ieee80211_beacon_alloc(ic, ni, &sc->sc_boff);
	if (skb == NULL)
	{
		DPRINTF(sc, ATH_DEBUG_BEACON, "%s: cannot get sk_buff\n", __func__);
		sc->sc_stats.ast_be_nobuf++;
		return ENOMEM;
	}

	if (bf->bf_skb != NULL)
	{
		bus_unmap_single(sc->sc_bdev,	bf->bf_skbaddr, bf->bf_skb->len, BUS_DMA_TODEVICE);
		dev_kfree_skb(bf->bf_skb);
		bf->bf_skb = NULL;
		bf->bf_node = NULL;
	}
	
	bf->bf_skb = skb;
	bf->bf_node = ieee80211_ref_node(ni);

	return 0; // TODO: return value
}

/* Setup the beacon frame for transmit. */
static void ath_beacon_setup(struct ath_softc *sc, struct ath_buf *bf)
{
#define	USE_SHPREAMBLE(_ic) \
	(((_ic)->ic_flags & (IEEE80211_F_SHPREAMBLE | IEEE80211_F_USEBARKER))\
		== IEEE80211_F_SHPREAMBLE)
		
	struct ieee80211_node *ni = bf->bf_node;
	struct ieee80211com *ic = ni->ni_ic;
	struct sk_buff *skb = bf->bf_skb;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_node *an = ATH_NODE(ni);
	struct ath_desc *ds;
	int antenna = sc->sc_txantenna;
	int flags;
	u_int8_t rate;

	bf->bf_skbaddr = bus_map_single(sc->sc_bdev, skb->data, skb->len, BUS_DMA_TODEVICE);
	DPRINTF(sc, ATH_DEBUG_BEACON,"%s: skb %p [data %p len %u] skbaddr %p\n",	__func__, skb, skb->data, skb->len, (caddr_t) bf->bf_skbaddr);

	if (BUS_DMA_MAP_ERROR(bf->bf_skbaddr))
	{
		mdev_printf(&sc->sc_dev, "%s: DMA mapping failed\n", __func__);
		return;
	}
	
	/* setup descriptors */
	ds = bf->bf_desc;

	flags = HAL_TXDESC_NOACK;
	if (ic->ic_opmode == IEEE80211_M_IBSS && sc->sc_hasveol)
	{
		ds->ds_link = bf->bf_daddr;	/* self-linked */
		flags |= HAL_TXDESC_VEOL;
		/*
		 * Let hardware handle antenna switching if txantenna is not set
		 */
	}
	else
	{
		ds->ds_link = 0;
		/*
		 * Switch antenna every 4 beacons if txantenna is not set
		 * XXX assumes two antenna
		 */
		if (antenna == 0)
		{
			antenna = (sc->sc_stats.ast_be_xmit & 4 ? 2 : 1);
		}
	}

	ds->ds_data = bf->bf_skbaddr;
	/*
	 * Calculate rate code.
	 * XXX everything at min xmit rate
	 */
	if (USE_SHPREAMBLE(ic))
		rate = an->an_tx_mgtratesp;
	else
		rate = an->an_tx_mgtrate;
	
	ath_hal_setuptxdesc(ah, ds
		, skb->len + IEEE80211_CRC_LEN	/* frame length */
		, sizeof(struct ieee80211_frame)/* header length */
		, HAL_PKT_TYPE_BEACON		/* Atheros packet type */
		, ni->ni_txpower		/* txpower XXX */
		, rate, 1			/* series 0 rate/tries */
		, HAL_TXKEYIX_INVALID		/* no encryption */
		, antenna			/* antenna mode */
		, flags				/* no ack, veol for beacons */
		, 0				/* rts/cts rate */
		, 0				/* rts/cts duration */
	);

	/* NB: beacon's BufLen must be a multiple of 4 bytes */
	ath_hal_filltxdesc(ah, ds
		, roundup(skb->len, 4)		/* buffer length */
		, AH_TRUE			/* first segment */
		, AH_TRUE			/* last segment */
		, ds				/* first descriptor */
	);
#undef USE_SHPREAMBLE
}

/*
 * Transmit a beacon frame at SWBA.  Dynamic updates to the
 * frame contents are done as needed and the slot time is
 * also adjusted based on current state.
 */
/* tx beacon frame in ISR in order to real-time process */ 
void ath_beacon_tasklet(MESH_DEVICE *mdev)
{
	struct ath_softc *sc = (struct ath_softc *)mdev->priv;
	struct ath_buf *bf = STAILQ_FIRST(&sc->sc_bbuf);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct sk_buff *skb;
	int ncabq, otherant;

	DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s\n", __func__);

#if 0	
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_MONITOR ||
#endif	    
	if( bf == NULL || bf->bf_skb == NULL) 
	{
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: ic_flags=%x bf=%p bf_skb=%p\n", __func__, ic->ic_flags, bf, bf ? bf->bf_skb : NULL);
		return;
	}
	/*
	 * Check if the previous beacon has gone out.  If
	 * not don't don't try to post another, skip this
	 * period and wait for the next.  Missed beacons
	 * indicate a problem and should not occur.  If we
	 * miss too many consecutive beacons reset the device.
	 */
	if (ath_hal_numtxpending(ah, sc->sc_bhalq) != 0)
	{
		sc->sc_bmisscount++;
		DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: missed %u consecutive beacons\n", __func__, sc->sc_bmisscount);
		if (sc->sc_bmisscount > 3)
		{/* NB: 3 is a guess */
			DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: stuck beacon time (%u missed)\n", __func__, sc->sc_bmisscount);
			ATH_SCHEDULE_TQUEUE(&sc->sc_bstuckq, &needmark);
		}
		return;
	}
	
	if (sc->sc_bmisscount != 0)
	{
		DPRINTF(sc, ATH_DEBUG_BEACON, "%s: resume beacon xmit after %u misses\n", __func__, sc->sc_bmisscount);
		sc->sc_bmisscount = 0;
	}

	/*
	 * Update dynamic beacon contents.  If this returns
	 * non-zero then we need to remap the memory because
	 * the beacon frame changed size (probably because
	 * of the TIM bitmap).
	 */
	skb = bf->bf_skb;
	ncabq = ath_hal_numtxpending(ah, sc->sc_cabq->axq_qnum);
	if (ieee80211_beacon_update(ic, bf->bf_node, &sc->sc_boff, skb, ncabq))
	{
		DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: update, beacon len changed %d to %d\n",	__func__, bf->bf_skb->len, skb->len);
		
		/* XXX too conservative? */
		bus_unmap_single(sc->sc_bdev, bf->bf_skbaddr, bf->bf_skb->len, BUS_DMA_TODEVICE);

		bf->bf_skbaddr = bus_map_single(sc->sc_bdev, skb->data, skb->len, BUS_DMA_TODEVICE);
		if (BUS_DMA_MAP_ERROR(bf->bf_skbaddr))
		{
			mdev_printf(mdev, "%s: DMA mapping failed\n", __func__);
			return;
		}
	}

	/*
	 * Handle slot time change when a non-ERP station joins/leaves
	 * an 11g network.  The 802.11 layer notifies us via callback,
	 * we mark updateslot, then wait one beacon before effecting
	 * the change.  This gives associated stations at least one
	 * beacon interval to note the state change.
	 */
	/* XXX locking */
	if (sc->sc_updateslot == UPDATE)
		sc->sc_updateslot = COMMIT;	/* commit next beacon */
	else if (sc->sc_updateslot == COMMIT)
		_phy_setslottime(sc);		/* commit change to h/w */

	/*
	 * Check recent per-antenna transmit statistics and flip
	 * the default antenna if noticeably more frames went out
	 * on the non-default antenna.
	 * XXX assumes 2 anntenae
	 */
	otherant = sc->sc_defant & 1 ? 2 : 1;
	if (sc->sc_ant_tx[otherant] > sc->sc_ant_tx[sc->sc_defant] + 2)
		ath_setdefantenna(sc, otherant);
	sc->sc_ant_tx[1] = sc->sc_ant_tx[2] = 0;

	/* Construct tx descriptor. */
	ath_beacon_setup(sc, bf);

	/*
	 * Stop any current dma and put the new frame on the queue.
	 * This should never fail since we check above that no frames
	 * are still pending on the queue.
	 */
	if (!ath_hal_stoptxdma(ah, sc->sc_bhalq))
	{
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: beacon queue %u did not stop?\n",__func__, sc->sc_bhalq);
		/* NB: the HAL still stops DMA, so proceed */
	}
	bus_dma_sync_single(sc->sc_bdev, bf->bf_skbaddr, bf->bf_skb->len, BUS_DMA_TODEVICE);

	/*
	 * Enable the CAB queue before the beacon queue to
	 * insure CAB frames are triggered by this beacon.
	 * The CAB queue holds multicast traffic for stations in 
	 * power-save mode.
	 *
	 * NB: only at DTIM
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP && ncabq > 0 && sc->sc_boff.bo_tim[4] & 1)
		ath_hal_txstart(ah, sc->sc_cabq->axq_qnum);
	
	ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
	ath_hal_txstart(ah, sc->sc_bhalq);
	DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: TXDP[%u] = %p (%p)\n", __func__, sc->sc_bhalq, (caddr_t)bf->bf_daddr, bf->bf_desc);

	sc->sc_stats.ast_be_xmit++;
}


/* Reclaim beacon resources. */
void ath_beacon_free(struct ath_softc *sc)
{
	struct ath_buf *bf;

	STAILQ_FOREACH(bf, &sc->sc_bbuf, bf_list)
	{
		if (bf->bf_skb != NULL)
		{
			bus_unmap_single(sc->sc_bdev, bf->bf_skbaddr, bf->bf_skb->len, BUS_DMA_TODEVICE);
			dev_kfree_skb(bf->bf_skb);
			bf->bf_skb = NULL;
		}

		if (bf->bf_node != NULL)
		{
			ieee80211_free_node(bf->bf_node);
			bf->bf_node = NULL;
		}
	}
}

/*
 * Configure the beacon and sleep timers.
 *
 * When operating as an AP this resets the TSF and sets
 * up the hardware to notify us when we need to issue beacons.
 *
 * When operating in station mode this sets up the beacon
 * timers according to the timestamp of the last received
 * beacon and the current TSF, configures PCF and DTIM
 * handling, programs the sleep registers so the hardware
 * will wakeup in time to receive beacons, and configures
 * the beacon miss handling so we'll receive a BMISS
 * interrupt when we stop seeing beacons from the AP
 * we've associated with.
 */
void ath_beacon_config(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	u_int32_t nexttbtt, intval;

	nexttbtt = (LE_READ_4(ni->ni_tstamp.data + 4) << 22) |
	    (LE_READ_4(ni->ni_tstamp.data) >> 10);
	intval = ni->ni_intval & HAL_BEACON_PERIOD;
	if (nexttbtt == 0)		/* e.g. for ap mode */
		nexttbtt = intval;
	else if (intval)		/* NB: can be 0 for monitor mode */
		nexttbtt = roundup(nexttbtt, intval); 
	
	DPRINTF(sc, ATH_DEBUG_BEACON, "%s: nexttbtt %u intval %u (%u)\n",__func__, nexttbtt, intval, ni->ni_intval);
	if (ic->ic_opmode == IEEE80211_M_STA)
	{
		HAL_BEACON_STATE bs;

		/* NB: no PCF support right now */
		memset(&bs, 0, sizeof(bs));
		bs.bs_intval = intval;
		bs.bs_nexttbtt = nexttbtt;
		bs.bs_dtimperiod = bs.bs_intval;
		bs.bs_nextdtim = nexttbtt;
		/*
		 * The 802.11 layer records the offset to the DTIM
		 * bitmap while receiving beacons; use it here to
		 * enable h/w detection of our AID being marked in
		 * the bitmap vector (to indicate frames for us are
		 * pending at the AP).
		 */
		bs.bs_timoffset = ni->ni_timoff;
		/*
		 * Calculate the number of consecutive beacons to miss
		 * before taking a BMISS interrupt.  The configuration
		 * is specified in ms, so we need to convert that to
		 * TU's and then calculate based on the beacon interval.
		 * Note that we clamp the result to at most 10 beacons.
		 */
		bs.bs_bmissthreshold = howmany(ic->ic_bmisstimeout, intval);
		if (bs.bs_bmissthreshold > 10)
			bs.bs_bmissthreshold = 10;
		else if (bs.bs_bmissthreshold <= 0)
			bs.bs_bmissthreshold = 1;

		/*
		 * Calculate sleep duration.  The configuration is
		 * given in ms.  We insure a multiple of the beacon
		 * period is used.  Also, if the sleep duration is
		 * greater than the DTIM period then it makes senses
		 * to make it a multiple of that.
		 *
		 * XXX fixed at 100ms
		 */
		bs.bs_sleepduration = roundup(IEEE80211_MS_TO_TU(100), bs.bs_intval);
		if (bs.bs_sleepduration > bs.bs_dtimperiod)
			bs.bs_sleepduration = roundup(bs.bs_sleepduration, bs.bs_dtimperiod);

		DPRINTF(sc, ATH_DEBUG_BEACON, 
			"%s: intval %u nexttbtt %u dtim %u nextdtim %u bmiss %u sleep %u cfp:period %u maxdur %u next %u timoffset %u\n"
			, __func__
			, bs.bs_intval
			, bs.bs_nexttbtt
			, bs.bs_dtimperiod
			, bs.bs_nextdtim
			, bs.bs_bmissthreshold
			, bs.bs_sleepduration
			, bs.bs_cfpperiod
			, bs.bs_cfpmaxduration
			, bs.bs_cfpnext
			, bs.bs_timoffset
		);
		ath_hal_intrset(ah, 0);
		ath_hal_beacontimers(ah, &bs);
		sc->sc_imask |= HAL_INT_BMISS;
		ath_hal_intrset(ah, sc->sc_imask);
	}
	else
	{
		ath_hal_intrset(ah, 0);
		if (nexttbtt == intval)
			intval |= HAL_BEACON_RESET_TSF;
		if (ic->ic_opmode == IEEE80211_M_IBSS)
		{
			/*
			 * In IBSS mode enable the beacon timers but only
			 * enable SWBA interrupts if we need to manually
			 * prepare beacon frames.  Otherwise we use a
			 * self-linked tx descriptor and let the hardware
			 * deal with things.
			 */
			intval |= HAL_BEACON_ENA;
			if (!sc->sc_hasveol)
				sc->sc_imask |= HAL_INT_SWBA;
		}
		else if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		{
			/*
			 * In AP mode we enable the beacon timers and
			 * SWBA interrupts to prepare beacon frames.
			 */
			intval |= HAL_BEACON_ENA;
			sc->sc_imask |= HAL_INT_SWBA;	/* beacon prepare */
		}
		ath_hal_beaconinit(ah, nexttbtt, intval);
		sc->sc_bmisscount = 0;
		ath_hal_intrset(ah, sc->sc_imask);
		/*
		 * When using a self-linked beacon descriptor in
		 * ibss mode load it once here.
		 */
		if (ic->ic_opmode == IEEE80211_M_IBSS && sc->sc_hasveol)
			ath_beacon_tasklet(&sc->sc_dev);
	}
}

