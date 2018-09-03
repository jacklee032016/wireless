/*
* $Id: phy_tx.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "phy.h"
#include "phy_hw_desc.h"

#include "if_llc.h"

/* Setup a h/w transmit queue.  */
struct ath_txq *ath_txq_setup(struct ath_softc *sc, int qtype, int subtype)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ath_hal *ah = sc->sc_ah;
	HAL_TXQ_INFO qi;
	int qnum;

	memset(&qi, 0, sizeof(qi));
	qi.tqi_subtype = subtype;
	qi.tqi_aifs = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmin = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmax = HAL_TXQ_USEDEFAULT;
	/*
	 * Enable interrupts only for EOL and DESC conditions.
	 * We mark tx descriptors to receive a DESC interrupt
	 * when a tx queue gets deep; otherwise waiting for the
	 * EOL to reap descriptors.  Note that this is done to
	 * reduce interrupt load and this only defers reaping
	 * descriptors, never transmitting frames.  Aside from
	 * reducing interrupts this also permits more concurrency.
	 * The only potential downside is if the tx queue backs
	 * up in which case the top half of the kernel may backup
	 * due to a lack of tx descriptors.
	 */
	qi.tqi_qflags = TXQ_FLAG_TXEOLINT_ENABLE | TXQ_FLAG_TXDESCINT_ENABLE;
#ifdef HAS_VMAC
	// XXX should fix this so that we only jimmy the queue properties
	// when in softmac mode?
	qi.tqi_qflags |= 
	  (TXQ_FLAG_TXOKINT_ENABLE | TXQ_FLAG_TXERRINT_ENABLE | TXQ_FLAG_BACKOFF_DISABLE);
	// Force contention window to be small
	// XXX maybe set this on individual packet queues after setup?
	qi.tqi_cwmin = 1;
	qi.tqi_cwmax = 1;
#endif
	qnum = ath_hal_setuptxqueue(ah, qtype, &qi);
	if (qnum == -1)
	{
		/*
		 * NB: don't print a message, this happens 
		 * normally on parts with too few tx queues
		 */
		return NULL;
	}

	if (qnum >= N(sc->sc_txq))
	{
		printk("%s: hal qnum %u out of range, max %u!\n",	sc->sc_dev.name, qnum, (unsigned int) N(sc->sc_txq));
		ath_hal_releasetxqueue(ah, qnum);
		return NULL;
	}
	
	if (!ATH_TXQ_SETUP(sc, qnum))
	{
		struct ath_txq *txq = &sc->sc_txq[qnum];

		txq->axq_qnum = qnum;
		txq->axq_depth = 0;
		txq->axq_intrcnt = 0;
		txq->axq_link = NULL;
		STAILQ_INIT(&txq->axq_q);
		ATH_TXQ_LOCK_INIT(sc, txq);
		sc->sc_txqsetup |= 1<<qnum;
	}
	return &sc->sc_txq[qnum];
#undef N
}

/*
 * Setup a hardware data transmit queue for the specified
 * access control.  The hal may not support all requested
 * queues in which case it will return a reference to a
 * previously setup queue.  We record the mapping from ac's
 * to h/w queues for use by ath_tx_start and also track
 * the set of h/w queues being used to optimize work in the
 * transmit interrupt handler and related routines.
 */
int ath_tx_setup(struct ath_softc *sc, int ac, int haltype)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ath_txq *txq;

	if (ac >= N(sc->sc_ac2q))
	{
		printk("%s: AC %u out of range, max %u!\n", sc->sc_dev.name, ac, (unsigned int) N(sc->sc_ac2q));
		return 0;
	}

	txq = ath_txq_setup(sc, HAL_TX_QUEUE_DATA, haltype);
	if (txq != NULL)
	{
		sc->sc_ac2q[ac] = txq;
		return 1;
	}
	else
		return 0;
#undef N
}

/* Update WME parameters for a transmit queue. */
static int ath_txq_update(struct ath_softc *sc, int ac)
{
#define	ATH_EXPONENT_TO_VALUE(v)	((1<<v)-1)
#define	ATH_TXOP_TO_US(v)		(v<<5)
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_txq *txq = sc->sc_ac2q[ac];
	struct wmeParams *wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[ac];
	struct ath_hal *ah = sc->sc_ah;
	HAL_TXQ_INFO qi;

	ath_hal_gettxqueueprops(ah, txq->axq_qnum, &qi);
	qi.tqi_aifs = wmep->wmep_aifsn;
	qi.tqi_cwmin = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmin);
	qi.tqi_cwmax = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmax);	
	qi.tqi_burstTime = ATH_TXOP_TO_US(wmep->wmep_txopLimit);

	if (!ath_hal_settxqueueprops(ah, txq->axq_qnum, &qi))
	{
		mdev_printf(&sc->sc_dev, "unable to update hardware queue parameters for %s traffic!\n", ieee80211_wme_acnames[ac]);
		return 0;
	}
	else
	{
		ath_hal_resettxqueue(ah, txq->axq_qnum); /* push to h/w */
		return 1;
	}
#undef ATH_TXOP_TO_US
#undef ATH_EXPONENT_TO_VALUE
}

/* Callback from the 802.11 layer to update WME parameters. */
int ath_wme_update(struct ieee80211com *ic)
{
	struct ath_softc *sc = ic->ic_dev->priv;

	return !ath_txq_update(sc, WME_AC_BE) ||
	    !ath_txq_update(sc, WME_AC_BK) ||
	    !ath_txq_update(sc, WME_AC_VI) ||
	    !ath_txq_update(sc, WME_AC_VO) ? EIO : 0;
}

/* Reclaim resources for a setup queue. */
void ath_tx_cleanupq(struct ath_softc *sc, struct ath_txq *txq)
{

	ath_hal_releasetxqueue(sc->sc_ah, txq->axq_qnum);
	ATH_TXQ_LOCK_DESTROY(txq);
	sc->sc_txqsetup &= ~(1<<txq->axq_qnum);
}

/* Reclaim all tx queue resources. */
void ath_tx_cleanup(struct ath_softc *sc)
{
	int i;

	ATH_LOCK_DESTROY(sc);
	ATH_TXBUF_LOCK_DESTROY(sc);
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
	{
		if (ATH_TXQ_SETUP(sc, i))
			ath_tx_cleanupq(sc, &sc->sc_txq[i]);
	}
}

int ath_tx_start(MESH_DEVICE *dev, struct ieee80211_node *ni, struct ath_buf *bf, struct sk_buff *skb)
{
#define	CTS_DURATION \
	ath_hal_computetxtime(ah, rt, IEEE80211_ACK_LEN, cix, AH_TRUE)
#define	updateCTSForBursting(_ah, _ds, _txq) \
	ath_hal_updateCTSForBursting(_ah, _ds, \
	    _txq->axq_linkbuf != NULL ? _txq->axq_linkbuf->bf_desc : NULL, \
	    _txq->axq_lastdsWithCTS, _txq->axq_gatingds, \
	    txopLimit, CTS_DURATION)
	    
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	const struct chanAccParams *cap = &ic->ic_wme.wme_chanParams;
	int iswep, ismcast, keyix, hdrlen, pktlen, try0;
	u_int8_t rix, txrate, ctsrate;
	u_int8_t cix = 0xff;		/* NB: silence compiler */
	struct ath_desc *ds;
	struct ath_txq *txq;
	struct ieee80211_frame *wh;
	u_int subtype, flags, ctsduration;
	HAL_PKT_TYPE atype;
	const HAL_RATE_TABLE *rt;
	HAL_BOOL shortPreamble;
	struct ath_node *an;
	struct llc *llc;
	int eapol;
	u_int pri;

	wh = (struct ieee80211_frame *) skb->data;
	iswep = wh->i_fc[1] & IEEE80211_FC1_WEP;
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	hdrlen = ieee80211_anyhdrsize(wh);
	// TODO: not so correct (WDS)
	llc = (struct llc *) (skb->data + sizeof(struct ieee80211_frame));
	
	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: ether_type: 0x%x\n", __func__, __constant_htons(llc->llc_snap.ether_type));

#define	ETHERTYPE_PAE	0x888e		/* EAPOL PAE/802.1x, eg EAP (Extensible Authentication Protocol) */
/* copy from if_ethersubr.h */	
	if (__constant_htons(llc->llc_snap.ether_type) == ETHERTYPE_PAE)
		eapol = 1;
	else
		eapol = 0;
	llc = NULL;

	/*
	 * Packet length must not include any
	 * pad bytes; deduct them here.
	 */
	//TODO: ??? pktlen = m0->m_pkthdr.len - (hdrlen & 3);
	pktlen = skb->len - (hdrlen & 3);

	if (iswep) {
		const struct ieee80211_cipher *cip;
		struct ieee80211_key *k;

		/*
		 * Construct the 802.11 header+trailer for an encrypted
		 * frame. The only reason this can fail is because of an
		 * unknown or unsupported cipher/key type.
		 */
		k = ieee80211_crypto_encap(ic, ni, skb);
		if (k == NULL) {
			/*
			 * This can happen when the key is yanked after the
			 * frame was queued.  Just discard the frame; the
			 * 802.11 layer counts failures and provides
			 * debugging/diagnostics.
			 */
			dev_kfree_skb(skb);
			return -EIO;
		}
		/*
		 * Adjust the packet + header lengths for the crypto
		 * additions and calculate the h/w key index.  When
		 * a s/w mic is done the frame will have had any mic
		 * added to it prior to entry so skb->len above will
		 * account for it. Otherwise we need to add it to the
		 * packet length.
		 */
		cip = k->wk_cipher;
		hdrlen += cip->ic_header;
		pktlen += cip->ic_header + cip->ic_trailer;
		if ((k->wk_flags & IEEE80211_KEY_SWMIC) == 0)
			pktlen += cip->ic_miclen;
		keyix = k->wk_keyix;

		/* packet header may have moved, reset our local pointer */
		wh = (struct ieee80211_frame *) skb->data;
	} else
		keyix = HAL_TXKEYIX_INVALID;

	pktlen += IEEE80211_CRC_LEN;

	/*
	 * Load the DMA map so any coalescing is done.  This
	 * also calculates the number of descriptors we need.
	 */
	bf->bf_skbaddr = bus_map_single(sc->sc_bdev, skb->data, pktlen, BUS_DMA_TODEVICE);
	
	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: skb %p [data %p len %u] skbaddr %lx\n",
		__func__, skb, skb->data, skb->len, (long unsigned int) bf->bf_skbaddr);

	if (BUS_DMA_MAP_ERROR(bf->bf_skbaddr)) {
		mdev_printf(dev, "%s: DMA mapping failed\n", __func__);
		printk("%s: DMA mapping failed\n", __func__);
		dev_kfree_skb(skb);
		bf->bf_skb = NULL;
		sc->sc_stats.ast_tx_busdma++;
		return -EIO;
	}
	bf->bf_skb = skb;
	bf->bf_node = ni;

	/* setup descriptors */
	ds = bf->bf_desc;
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	/*
	 * NB: the 802.11 layer marks whether or not we should
	 * use short preamble based on the current mode and
	 * negotiated parameters.
	 */
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE))
	{
		shortPreamble = AH_TRUE;
		sc->sc_stats.ast_tx_shortpre++;
	}
	else
	{
		shortPreamble = AH_FALSE;
	}
	an = ATH_NODE(ni);
	flags = HAL_TXDESC_CLRDMASK;		/* XXX needed for crypto errs */
	/*
	 * Calculate Atheros packet type from IEEE80211 packet header,
	 * setup for rate calculations, and select h/w transmit queue.
	 */
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_MGT:
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
			atype = HAL_PKT_TYPE_BEACON;
		else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			atype = HAL_PKT_TYPE_PROBE_RESP;
		else if (subtype == IEEE80211_FC0_SUBTYPE_ATIM)
			atype = HAL_PKT_TYPE_ATIM;
		else
			atype = HAL_PKT_TYPE_NORMAL;	/* XXX */
		rix = 0;			/* XXX lowest rate */
		try0 = ATH_TXMAXTRY;
		if (shortPreamble)
			txrate = an->an_tx_mgtratesp;
		else
			txrate = an->an_tx_mgtrate;
		/* NB: force all management frames to highest queue */
		if (ni->ni_flags & IEEE80211_NODE_QOS) {
			pri = WME_AC_VO;
		} else
			pri = WME_AC_BE;
		flags |= HAL_TXDESC_INTREQ;	/* force interrupt */
		break;
	case IEEE80211_FC0_TYPE_CTL:
		atype = HAL_PKT_TYPE_PSPOLL;	/* stop setting of duration */
		rix = 0;			/* XXX lowest rate */
		try0 = ATH_TXMAXTRY;
		if (shortPreamble)
			txrate = an->an_tx_mgtratesp;
		else
			txrate = an->an_tx_mgtrate;
		/* NB: force all ctl frames to highest queue */
		if (ni->ni_flags & IEEE80211_NODE_QOS) {
			pri = WME_AC_VO;
		} else
			pri = WME_AC_BE;
		flags |= HAL_TXDESC_INTREQ;	/* force interrupt */
		break;
	case IEEE80211_FC0_TYPE_DATA:
		PHY_DEBUG_INFO("Data Frame are send out in WIFI\n");
		atype = HAL_PKT_TYPE_NORMAL;		/* default */
		if (ismcast) {
			rix = 0;			/* XXX lowest rate */
			try0 = 0;
			if (shortPreamble)
				txrate = an->an_tx_mgtratesp;
			else
				txrate = an->an_tx_mgtrate;
		}
		else if (eapol) {
			rix = 0;			/* XXX lowest rate */
			try0 = 0;			// TODO: userspace or hardware retry?
			if (shortPreamble)
				txrate = an->an_tx_mgtratesp;
			else
				txrate = an->an_tx_mgtrate;
			flags |= HAL_TXDESC_INTREQ;	/* force interrupt */
		} else {
			/*
			 * Data frames; consult the rate control module.
			 */
			phy_rate_findrate(sc, an, shortPreamble, pktlen,
					  &rix, &try0, &txrate);
		}
		sc->sc_txrate = txrate;			/* for LED blinking */
		/*
		 * Default all non-QoS traffic to the background queue.
		 */
		if (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS) {
			pri = M_WME_GETAC(skb);
			if (cap->cap_wmeParams[pri].wmep_noackPolicy) {
				flags |= HAL_TXDESC_NOACK;
				sc->sc_stats.ast_tx_noack++;
			}
		} else
			pri = WME_AC_BE;
		break;
	default:
		mdev_printf(dev, "bogus frame type 0x%x (%s)\n",
			wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK, __func__);
		printk("bogus frame type 0x%x (%s)\n",
		       wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK, __func__);
		/* XXX statistic */
		DPRINTF(sc, ATH_DEBUG_FATAL, "%s: kfree_skb: skb %p [data %p len %u] skbaddr %lx\n",
			__func__, skb, skb->data, skb->len, (long unsigned int) bf->bf_skbaddr);
		dev_kfree_skb(skb);
		bf->bf_skb = NULL;
		return -EIO;
	}
	txq = sc->sc_ac2q[pri];

	/*
	 * When servicing one or more stations in power-save mode
	 * multicast frames must be buffered until after the beacon.
	 * We use the CAB queue for that.
	 */
	if (ismcast && ic->ic_ps_sta) {
		txq = sc->sc_cabq;
		/* XXX? more bit in 802.11 frame header */
	}

	/*
	 * Calculate miscellaneous flags.
	 */
	if (ismcast) {
		flags |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
		sc->sc_stats.ast_tx_noack++;
	} else if (pktlen > ic->ic_rtsthreshold) {
		flags |= HAL_TXDESC_RTSENA;	/* RTS based on frame length */
		cix = rt->info[rix].controlRate;
		sc->sc_stats.ast_tx_rts++;
	}

	/*
	 * If 802.11g protection is enabled, determine whether
	 * to use RTS/CTS or just CTS.  Note that this is only
	 * done for OFDM unicast frames.
	 */
	if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
	    rt->info[rix].phy == IEEE80211_T_OFDM &&
	    (flags & HAL_TXDESC_NOACK) == 0) {
		/* XXX fragments must use CCK rates w/ protection */
		if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
			flags |= HAL_TXDESC_RTSENA;
		else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
			flags |= HAL_TXDESC_CTSENA;
		cix = rt->info[sc->sc_protrix].controlRate;
		sc->sc_stats.ast_tx_protect++;
	}

	/*
	 * Calculate duration.  This logically belongs in the 802.11
	 * layer but it lacks sufficient information to calculate it.
	 */
	if ((flags & HAL_TXDESC_NOACK) == 0 &&
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL) {
		u_int16_t dur;
		/*
		 * XXX not right with fragmentation.
		 */
		if (shortPreamble)
			dur = rt->info[rix].spAckDuration;
		else
			dur = rt->info[rix].lpAckDuration;
		*(u_int16_t *)wh->i_dur = htole16(dur);
	}

	/*
	 * Calculate RTS/CTS rate and duration if needed.
	 */
	ctsduration = 0;
	if (flags & (HAL_TXDESC_RTSENA|HAL_TXDESC_CTSENA)) {
		/*
		 * CTS transmit rate is derived from the transmit rate
		 * by looking in the h/w rate table.  We must also factor
		 * in whether or not a short preamble is to be used.
		 */
		/* NB: cix is set above where RTS/CTS is enabled */
		KASSERT(cix != 0xff, ("cix not setup"));
		ctsrate = rt->info[cix].rateCode;
		/*
		 * Compute the transmit duration based on the frame
		 * size and the size of an ACK frame.  We call into the
		 * HAL to do the computation since it depends on the
		 * characteristics of the actual PHY being used.
		 *
		 * NB: CTS is assumed the same size as an ACK so we can
		 *     use the precalculated ACK durations.
		 */
		if (shortPreamble) {
			ctsrate |= rt->info[cix].shortPreamble;
			if (flags & HAL_TXDESC_RTSENA)		/* SIFS + CTS */
				ctsduration += rt->info[cix].spAckDuration;
			ctsduration += ath_hal_computetxtime(ah,
				rt, pktlen, rix, AH_TRUE);
			if ((flags & HAL_TXDESC_NOACK) == 0)	/* SIFS + ACK */
				ctsduration += rt->info[cix].spAckDuration;
		} else {
			if (flags & HAL_TXDESC_RTSENA)		/* SIFS + CTS */
				ctsduration += rt->info[cix].lpAckDuration;
			ctsduration += ath_hal_computetxtime(ah,
				rt, pktlen, rix, AH_FALSE);
			if ((flags & HAL_TXDESC_NOACK) == 0)	/* SIFS + ACK */
				ctsduration += rt->info[cix].lpAckDuration;
		}
		/*
		 * Must disable multi-rate retry when using RTS/CTS.
		 */
		try0 = ATH_TXMAXTRY;
	} else
		ctsrate = 0;

	if (IFF_DUMPPKTS(sc, ATH_DEBUG_XMIT))
		ieee80211_dump_pkt(skb->data, skb->len,
			sc->sc_hwmap[txrate].ieeerate, -1);

	/* 
	 * Determine if a tx interrupt should be generated for
	 * this descriptor.  We take a tx interrupt to reap
	 * descriptors when the h/w hits an EOL condition or
	 * when the descriptor is specifically marked to generate
	 * an interrupt.  We periodically mark descriptors in this
	 * way to insure timely replenishing of the supply needed
	 * for sending frames.  Defering interrupts reduces system
	 * load and potentially allows more concurrent work to be
	 * done but if done to aggressively can cause senders to
	 * backup.
	 *
	 * NB: use >= to deal with sc_txintrperiod changing
	 *     dynamically through sysctl.
	 */
	if (flags & HAL_TXDESC_INTREQ) {
		txq->axq_intrcnt = 0;
	} else if (++txq->axq_intrcnt >= sc->sc_txintrperiod) {
		flags |= HAL_TXDESC_INTREQ;
		txq->axq_intrcnt = 0;
	}

	/*
	 * Formulate first tx descriptor with tx controls.
	 */
	/* XXX check return value? */
	ath_hal_setuptxdesc(ah, ds
		, pktlen		/* packet length */
		, hdrlen		/* header length */
		, atype			/* Atheros packet type */
		, ni->ni_txpower	/* txpower */
		, txrate, try0		/* series 0 rate/tries */
		, keyix			/* key cache index */
		, sc->sc_txantenna	/* antenna mode */
		, flags			/* flags */
		, ctsrate		/* rts/cts rate */
		, ctsduration		/* rts/cts duration */
	);
	/*
	 * Setup the multi-rate retry state only when we're
	 * going to use it.  This assumes ath_hal_setuptxdesc
	 * initializes the descriptors (so we don't have to)
	 * when the hardware supports multi-rate retry and
	 * we don't use it.
	 */
	if (try0 != ATH_TXMAXTRY)
		phy_rate_setupxtxdesc(sc, an, ds, shortPreamble, rix);

	/*
	 * Fillin the remainder of the descriptor info.
	 */
	ds->ds_link = 0;
	ds->ds_data = bf->bf_skbaddr;
	ath_hal_filltxdesc(ah, ds
		, skb->len		/* segment length */
		, AH_TRUE		/* first segment */
		, AH_TRUE		/* last segment */
		, ds			/* first descriptor */
	);
	
	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: Q%d: %08x %08x %08x %08x %08x %08x\n",
	    __func__, txq->axq_qnum, ds->ds_link, ds->ds_data,
	    ds->ds_ctl0, ds->ds_ctl1, ds->ds_hw[0], ds->ds_hw[1]);
	/*
	 * Insert the frame on the outbound list and
	 * pass it on to the hardware.
	 */
	ATH_TXQ_LOCK_BH(txq);
	if (flags & (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA)) {
		u_int32_t txopLimit = IEEE80211_TXOP_TO_US(
			cap->cap_wmeParams[pri].wmep_txopLimit);
		/*
		 * When bursting, potentially extend the CTS duration
		 * of a previously queued frame to cover this frame
		 * and not exceed the txopLimit.  If that can be done
		 * then disable RTS/CTS on this frame since it's now
		 * covered (burst extension).  Otherwise we must terminate
		 * the burst before this frame goes out so as not to
		 * violate the WME parameters.  All this is complicated
		 * as we need to update the state of packets on the
		 * (live) hardware queue.  The logic is buried in the hal
		 * because it's highly chip-specific.
		 */
		if (txopLimit != 0) {
			sc->sc_stats.ast_tx_ctsburst++;
			if (updateCTSForBursting(ah, ds, txq) == 0) {
				/*
				 * This frame was not covered by RTS/CTS from
				 * the previous frame in the burst; update the
				 * descriptor pointers so this frame is now
				 * treated as the last frame for extending a
				 * burst.
				 */
				txq->axq_lastdsWithCTS = ds;
				/* set gating Desc to final desc */
				txq->axq_gatingds =
					(struct ath_desc *)txq->axq_link;
			} else
				sc->sc_stats.ast_tx_ctsext++;
		}
	}
	ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);
	DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: txq depth = %d\n",
		__func__, txq->axq_depth);
	if (txq->axq_link == NULL) {
		ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
		DPRINTF(sc, ATH_DEBUG_XMIT,
			"%s: TXDP[%u] = %p (%p) depth %d\n", __func__,
			txq->axq_qnum, (caddr_t)bf->bf_daddr, bf->bf_desc,
			txq->axq_depth);
	} else {
		*txq->axq_link = bf->bf_daddr;
		DPRINTF(sc, ATH_DEBUG_XMIT,
			"%s: link[%u](%p)=%p (%p) depth %d\n", __func__,
			txq->axq_qnum, txq->axq_link,
			(caddr_t)bf->bf_daddr, bf->bf_desc, txq->axq_depth);
	}
	txq->axq_link = &bf->bf_desc->ds_link;
	/*
	 * The CAB queue is started from the SWBA handler since
	 * frames only go out on DTIM and to avoid possible races.
	 */
	if (txq != sc->sc_cabq)
		ath_hal_txstart(ah, txq->axq_qnum);
	ATH_TXQ_UNLOCK_BH(txq);

	dev->trans_start = jiffies;
#if WITH_MISC_MODE
	sc->sc_rawdev.trans_start = jiffies;
#endif
	return 0;
#undef updateCTSForBursting
#undef CTS_DURATION
}

static void ath_tx_draintxq(struct ath_softc *sc, struct ath_txq *txq)
{
#ifdef AR_DEBUG
	struct ath_hal *ah = sc->sc_ah;
#endif
	struct ieee80211_node *ni;
	struct ath_buf *bf;

	/*
	 * NB: this assumes output has been stopped and
	 *     we do not need to block _phy_tx_tasklet
	 */
	for (;;)
	{
		ATH_TXQ_LOCK_BH(txq);
		bf = STAILQ_FIRST(&txq->axq_q);
		if (bf == NULL)
		{
			txq->axq_link = NULL;
			ATH_TXQ_UNLOCK_BH(txq);
			break;
		}
		ATH_TXQ_REMOVE_HEAD(txq, bf_list);
		ATH_TXQ_UNLOCK_BH(txq);
#ifdef AR_DEBUG
		if (sc->sc_debug & ATH_DEBUG_RESET)
			ath_printtxbuf(bf,
				ath_hal_txprocdesc(ah, bf->bf_desc) == HAL_OK);
#endif /* AR_DEBUG */
		bus_unmap_single(sc->sc_bdev,
			bf->bf_skbaddr, bf->bf_skb->len, BUS_DMA_TODEVICE);
		dev_kfree_skb(bf->bf_skb);
		bf->bf_skb = NULL;
		ni = bf->bf_node;
		bf->bf_node = NULL;
		if (ni != NULL) {
			/*
			 * Reclaim node reference.
			 */
			ieee80211_free_node(ni);
		}
		ATH_TXBUF_LOCK_BH(sc);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK_BH(sc);
	}
}

static void ath_tx_stopdma(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;

	(void) ath_hal_stoptxdma(ah, txq->axq_qnum);
	DPRINTF(sc, ATH_DEBUG_RESET, "%s: tx queue [%u] %p, link %p\n",
	    __func__, txq->axq_qnum,
	    (caddr_t)(uintptr_t) ath_hal_gettxbuf(ah, txq->axq_qnum),
	    txq->axq_link);
}

/* Drain the transmit queues and reclaim resources. */
void ath_draintxq(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	int i;

	/* XXX return value */
	if (!sc->sc_invalid) {
		/* don't touch the hardware if marked invalid */
		(void) ath_hal_stoptxdma(ah, sc->sc_bhalq);
		DPRINTF(sc, ATH_DEBUG_RESET,
		    "%s: beacon queue %p\n", __func__,
		    (caddr_t)(uintptr_t) ath_hal_gettxbuf(ah, sc->sc_bhalq));
		for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
			if (ATH_TXQ_SETUP(sc, i))
				ath_tx_stopdma(sc, &sc->sc_txq[i]);
		}
	}
	sc->sc_dev.trans_start = jiffies;
#if WITH_MISC_MODE
	sc->sc_rawdev.trans_start = jiffies;
#endif

	meshif_start_queue(&sc->sc_dev);		// TODO: needed here?
#if WITH_MISC_MODE
	if (sc->sc_rawdev_enabled)
		meshif_start_queue(&sc->sc_rawdev);
#endif

	for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
		if (ATH_TXQ_SETUP(sc, i))
			ath_tx_draintxq(sc, &sc->sc_txq[i]);
	}
	sc->sc_tx_timer = 0;
}

#ifdef HAS_VMAC
int ath_tx_raw_start(struct net_device *dev, struct ieee80211_node *ni, struct ath_buf *bf, struct sk_buff *skb)
{
#define	MIN(a,b)	((a) < (b) ? (a) : (b))
	struct ath_softc *sc = dev->priv;
	//struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	//int iswep, ismcast;
	int keyix, hdrlen, pktlen, try0;
	u_int8_t rix, txrate, ctsrate;
	//u_int8_t cix = 0xff;		/* NB: silence compiler */
	struct ath_desc *ds;
	struct ath_txq *txq;
	//struct ieee80211_frame *wh;
	//u_int subtype, flags, ctsduration;
	u_int flags, ctsduration;
	HAL_PKT_TYPE atype;
	const HAL_RATE_TABLE *rt;
	HAL_BOOL shortPreamble;
	struct ath_node *an;

	//wh = (struct ieee80211_frame *) skb->data;
	//iswep = wh->i_fc[1] & IEEE80211_FC1_WEP;
	//ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	//hdrlen = ieee80211_anyhdrsize(wh);
	hdrlen = 0;
	pktlen = skb->len;

	keyix = HAL_TXKEYIX_INVALID;
	// XXX need this?
	pktlen += IEEE80211_CRC_LEN;

	/*
	 * Load the DMA map so any coalescing is done.  This
	 * also calculates the number of descriptors we need.
	 */
	bf->bf_skbaddr = bus_map_single(sc->sc_bdev,skb->data, pktlen, BUS_DMA_TODEVICE);
	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: skb %p [data %p len %u] skbaddr %x\n",	__func__, skb, skb->data, skb->len, bf->bf_skbaddr);
	bf->bf_skb = skb;
	bf->bf_node = ni;

	/* setup descriptors */
	ds = bf->bf_desc;
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	/*
	 * NB: the 802.11 layer marks whether or not we should
	 * use short preamble based on the current mode and
	 * negotiated parameters.
	 */
	// Always use short preamble
	shortPreamble = AH_TRUE;

	an = ATH_NODE(ni);
	/*
	 * Calculate Atheros packet type from IEEE80211 packet header,
	 * setup for rate calculations, and select h/w transmit queue.
	 */
	// XXX not sure which of these packet types is appropriate
	//atype = HAL_PKT_TYPE_ATIM;
	//atype = HAL_PKT_TYPE_NORMAL;	/* XXX */
	//atype = HAL_PKT_TYPE_PSPOLL;	/* stop setting of duration */
	//atype = HAL_PKT_TYPE_BEACON;
	//atype = HAL_PKT_TYPE_PROBE_RESP;
	//atype = HAL_PKT_TYPE_PIFS;
	//atype = 16;
	atype = skb->cb[ATH_VMAC_CB_HAL_PKT_TYPE];
	try0 = ATH_TXMAXTRY;

	// XXX figure out txrate to set...
	//rix = 0;			/* XXX lowest rate */
	//txrate = an->an_tx_mgtrate;
	//if (shortPreamble)
	//txrate = an->an_tx_mgtratesp;
	//else
	//txrate = an->an_tx_mgtrate;

	/*
	 * Data frames; consult the rate control module.
	 */
	// XXX HAS_CU_SDR for now just call the findrate function...
	// As long as you set the rate with iwconfig this should result
	// in a constant bitrate.
	//phy_rate_findrate(sc, an, shortPreamble, skb->len,
	//&rix, &try0, &txrate);
	// Set bitrate based on skbuff scratch space. This uses the
	// 802.11-style unit of "512Kb/s," i.e. set it to twice
	// the desired bitrate in megabits. Setting the value
        // to 0 or something undefined in the current mode should
	// get you the lowest bitrate in the available set.
	rix = sc->sc_rixmap[skb->cb[ATH_VMAC_CB_RATE] & IEEE80211_RATE_VAL];
	if (0xFF == rix) {
	    rix = 0;
	}
        txrate = sc->sc_currates->info[rix].rateCode;

	// XXX figure out txqueue to use -- I think probably
	// the highest rate queue for now, but should let this
	// get set on a per-packet basis.
	/*
	 * Default all non-QoS traffic to the background queue.
	 */
	//if (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS) {
	  /* XXX validate skb->priority */
	  //txq = sc->sc_ac2q[skb->priority];
	//} else
	// txq = sc->sc_ac2q[WME_AC_BK];
	/* NB: force all management frames to highest queue */
	txq = sc->sc_ac2q[skb->priority];

	/*
	 * Calculate miscellaneous flags.
	 */
	flags = HAL_TXDESC_CLRDMASK | HAL_TXDESC_NOACK;
	ctsrate = 0;
	ctsduration = 0;

	/* 
	 * Determine if a tx interrupt should be generated for
	 * this descriptor.  We take a tx interrupt to reap
	 * descriptors when the h/w hits an EOL condition or
	 * when the descriptor is specifically marked to generate
	 * an interrupt.  We periodically mark descriptors in this
	 * way to insure timely replenishing of the supply needed
	 * for sending frames.  Defering interrupts reduces system
	 * load and potentially allows more concurrent work to be
	 * done but if done to aggressively can cause senders to
	 * backup.
	 *
	 * NB: use >= to deal with sc_txintrperiod changing
	 *     dynamically through sysctl.
	 */
	if ((++txq->axq_intrcnt >= sc->sc_txintrperiod) ||
	    skb->cb[ATH_VMAC_CB_TXINTR]) {
		flags |= HAL_TXDESC_INTREQ;
		txq->axq_intrcnt = 0;
	}

	/*
	 * Formulate first tx descriptor with tx controls.
	 */
	/* XXX check return value? */
	ath_hal_setuptxdesc(ah, ds
		, pktlen		/* packet length */
		, hdrlen		/* header length */
		, atype			/* Atheros packet type */
		, MIN(ni->ni_txpower,60)/* txpower */
		, txrate, try0		/* series 0 rate/tries */
		, keyix			/* key cache index */
		, sc->sc_txantenna	/* antenna mode */
		, flags			/* flags */
		, ctsrate		/* rts/cts rate */
		, ctsduration		/* rts/cts duration */
	);

	ds->ds_link = 0;
	ds->ds_data = bf->bf_skbaddr;
	ath_hal_filltxdesc(ah, ds
		, skb->len		/* segment length */
		, AH_TRUE		/* first segment */
		, AH_TRUE		/* last segment */
		, ds			/* first descriptor */
	);
	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: Q%d: %08x %08x %08x %08x %08x %08x\n",
	    __func__, txq->axq_qnum, ds->ds_link, ds->ds_data,
	    ds->ds_ctl0, ds->ds_ctl1, ds->ds_hw[0], ds->ds_hw[1]);

	/*
	 * Insert the frame on the outbound list and
	 * pass it on to the hardware.
	 */
	ATH_TXQ_LOCK_BH(txq);
	ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);
	DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: txq depth = %d\n",
		__func__, txq->axq_depth);
	if (txq->axq_link == NULL) {
		ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: TXDP[%u] = %p (%p)\n",
			__func__,
		    txq->axq_qnum, (caddr_t)bf->bf_daddr, bf->bf_desc);
	} else {
		*txq->axq_link = bf->bf_daddr;
		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: link[%u](%p)=%p (%p)\n",
			__func__,
		    txq->axq_qnum, txq->axq_link,
		    (caddr_t)bf->bf_daddr, bf->bf_desc);
	}
	txq->axq_link = &ds->ds_link;
	ATH_TXQ_UNLOCK_BH(txq);
	ath_hal_txstart(ah, txq->axq_qnum);
	dev->trans_start = jiffies;
	return 0;
#undef MIN
}
#endif

