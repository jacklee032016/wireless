/*
* $Id: phy_rawdev_sysctl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

/* raw device is not support in mesh environments */

#include "phy.h"

/** _phy_start for raw 802.11 packets. */
static int ath_start_raw(struct sk_buff *skb, struct net_device *dev)
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
	struct ath_txq *txq;
	struct ath_buf *bf;
	HAL_PKT_TYPE atype;
	int pktlen, hdrlen, try0, pri, dot11Rate, txpower; 
	u_int8_t ctsrate, ctsduration, txrate;
	u_int8_t cix = 0xff;         /* NB: silence compiler */
	u_int flags = 0;
	struct ieee80211_frame *wh; 
	struct ath_desc *ds;
	const HAL_RATE_TABLE *rt;

#ifdef HAS_VMAC
	// Keep packets out of the device if it isn't up OR it's been appropriated by the VMAC
	if ((sc->sc_dev.flags & IFF_RUNNING) == 0 || sc->sc_invalid ||sc->sc_vmac)
#else
	if ((sc->sc_dev.flags & IFF_RUNNING) == 0 || sc->sc_invalid)
#endif
	{
		/* device is not up... silently discard packet */
		dev_kfree_skb(skb);
		return 0;
	}

	/*
	 * Grab a TX buffer and associated resources.
	 */
	ATH_TXBUF_LOCK_BH(sc);
	bf = STAILQ_FIRST(&sc->sc_txbuf);
	if (bf != NULL)
		STAILQ_REMOVE_HEAD(&sc->sc_txbuf, bf_list);
	
	/* XXX use a counter and leave at least one for mgmt frames */
	if (STAILQ_EMPTY(&sc->sc_txbuf))
	{
		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: stop queue\n", __func__);
		sc->sc_stats.ast_tx_qstop++;
		meshif_stop_queue(&sc->sc_dev);
#if  0		
		meshif_stop_queue(&sc->sc_rawdev);
#endif
	}
	ATH_TXBUF_UNLOCK_BH(sc);
	
	if (bf == NULL)
	{/* NB: should not happen */
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: out of xmit buffers\n",__func__);
		sc->sc_stats.ast_tx_nobuf++;
		dev_kfree_skb(skb);
		return 0;
	}

	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
	flags = HAL_TXDESC_INTREQ | HAL_TXDESC_CLRDMASK;
	try0 = ATH_TXMAXTRY;
	dot11Rate = 0;
	ctsrate = 0;
	ctsduration = 0;
	pri = 0;
	txpower = 60;
	txq = sc->sc_ac2q[pri];
	txrate = rt->info[0].rateCode;
	atype =  HAL_PKT_TYPE_NORMAL;

	/*
	 * strip any physical layer header off the skb, if it is
	 * present, and read out any settings we can, like what txrate
	 * to send this packet at.
	 */
	switch(dev->type)
	{
		case ARPHRD_IEEE80211:
			break;
		case ARPHRD_IEEE80211_PRISM:
		{
			wlan_ng_prism2_header *ph = NULL;
			ph = (wlan_ng_prism2_header *) skb->data;
			/* does it look like there is a prism header here? */
			if (skb->len > sizeof (wlan_ng_prism2_header) &&
			    ph->msgcode == DIDmsg_lnxind_wlansniffrm &&
			    ph->rate.did == DIDmsg_lnxind_wlansniffrm_rate)
			{
				dot11Rate = ph->rate.data;
				skb_pull(skb, sizeof(wlan_ng_prism2_header));
			} 
			break;
		}
		
		case ARPHRD_IEEE80211_RADIOTAP:
		{
			struct ieee80211_radiotap_header *th = (struct ieee80211_radiotap_header *) skb->data;
			if (rt_check_header(th, skb->len))
			{
				if (rt_el_present(th, IEEE80211_RADIOTAP_RATE))
				{
					dot11Rate = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_RATE));
				}
				
				if (rt_el_present(th, IEEE80211_RADIOTAP_DATA_RETRIES))
				{
					try0 = 1 + *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_DATA_RETRIES));
				}

				if (rt_el_present(th, IEEE80211_RADIOTAP_DBM_TX_POWER))
				{
					txpower = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_DBM_TX_POWER));
					if (txpower > 60) 
						txpower = 60;
					
				}

				skb_pull(skb, th->it_len);
			}
			break;
		}
		default:
			/* nothing */
			break;
	}
	
	if (dot11Rate != 0)
	{
		int index = sc->sc_rixmap[dot11Rate & IEEE80211_RATE_VAL];
		if (index >= 0 && index < rt->rateCount)
		{
			txrate = rt->info[index].rateCode;
		}
	}

	wh = (struct ieee80211_frame *) skb->data;
	pktlen = skb->len + IEEE80211_CRC_LEN;
	hdrlen = sizeof(struct ieee80211_frame);

	if (hdrlen < pktlen) 
		hdrlen = pktlen;

	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
	{
		flags |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
		sc->sc_stats.ast_tx_noack++;
	}
	
	if (IFF_DUMPPKTS(sc, ATH_DEBUG_XMIT) )
		ieee80211_dump_pkt(skb->data, skb->len, sc->sc_hwmap[txrate].ieeerate, -1);

	/*
	 * Load the DMA map so any coalescing is done.  This
	 * also calculates the number of descriptors we need.
	 */
	bf->bf_skbaddr = bus_map_single(sc->sc_bdev, skb->data, pktlen, BUS_DMA_TODEVICE);
	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: skb %p [data %p len %u] skbaddr %lx\n", __func__, skb, skb->data, skb->len, (long unsigned int) bf->bf_skbaddr);

	if (BUS_DMA_MAP_ERROR(bf->bf_skbaddr))
	{
		mdev_printf(dev, "%s: DMA mapping failed\n", __func__);
		dev_kfree_skb(skb);
		bf->bf_skb = NULL;
		sc->sc_stats.ast_tx_busdma++;

		ATH_TXBUF_LOCK_BH(sc);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK_BH(sc);
		return -EIO;
	}
	
	bf->bf_skb = skb;
	bf->bf_node = NULL;

	/* setup descriptors */
	ds = bf->bf_desc;
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	/*
	 * Formulate first tx descriptor with tx controls.
	 */
	/* XXX check return value? */
	ath_hal_setuptxdesc(ah, ds, pktlen	/* packet length */, hdrlen		/* header length */
		, atype			/* Atheros packet type */
		, txpower     	        /* txpower */
		, txrate, try0		/* series 0 rate/tries */
		, HAL_TXKEYIX_INVALID	/* key cache index */
		, sc->sc_txantenna	/* antenna mode */
		, flags			/* flags */
		, ctsrate		/* rts/cts rate */
		, ctsduration		/* rts/cts duration */
	);

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

	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: Q%d: %08x %08x %08x %08x %08x %08x\n",  __func__, txq->axq_qnum, ds->ds_link, ds->ds_data, ds->ds_ctl0, ds->ds_ctl1, ds->ds_hw[0], ds->ds_hw[1]);
	/*
	 * Insert the frame on the outbound list and
	 * pass it on to the hardware.
	 */
	ATH_TXQ_LOCK_BH(txq);

	if (flags & (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA))
	{
		u_int32_t txopLimit = IEEE80211_TXOP_TO_US(cap->cap_wmeParams[pri].wmep_txopLimit);
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
		if (txopLimit != 0)
		{
			sc->sc_stats.ast_tx_ctsburst++;
			if (updateCTSForBursting(ah, ds, txq) == 0)
			{
				/*
				 * This frame was not covered by RTS/CTS from
				 * the previous frame in the burst; update the
				 * descriptor pointers so this frame is now
				 * treated as the last frame for extending a
				 * burst.
				 */
				txq->axq_lastdsWithCTS = ds;
				/* set gating Desc to final desc */
				txq->axq_gatingds = (struct ath_desc *)txq->axq_link;
			}
			else
				sc->sc_stats.ast_tx_ctsext++;
		}
	}
	ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);
	DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: txq depth = %d\n", __func__, txq->axq_depth);

	if (txq->axq_link == NULL)
	{
		ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
		DPRINTF(sc, ATH_DEBUG_XMIT,	"%s: TXDP[%u] = %p (%p) depth %d\n", __func__,	txq->axq_qnum, (caddr_t)bf->bf_daddr, bf->bf_desc, txq->axq_depth);
	}
	else
	{
		*txq->axq_link = bf->bf_daddr;
		DPRINTF(sc, ATH_DEBUG_XMIT,	"%s: link[%u](%p)=%p (%p) depth %d\n", __func__,
			txq->axq_qnum, txq->axq_link, (caddr_t)bf->bf_daddr, bf->bf_desc, txq->axq_depth);
	}
	txq->axq_link = &bf->bf_desc->ds_link;
	/*
	 * The CAB queue is started from the SWBA handler since
	 * frames only go out on DTIM and to avoid possible races.
	 */
	if (txq != sc->sc_cabq)
		ath_hal_txstart(ah, txq->axq_qnum);
	ATH_TXQ_UNLOCK_BH(txq);

	sc->sc_devstats.tx_packets++;
	sc->sc_devstats.tx_bytes += skb->len;
	sc->sc_dev.trans_start = jiffies;
	sc->sc_rawdev.trans_start = jiffies;

	return 0;
#undef updateCTSForBursting
#undef CTS_DURATION
}

static int ath_rawdev_attach(struct ath_softc *sc) 
{
	struct net_device *rawdev;
	unsigned t;
	rawdev = &sc->sc_rawdev;
	strcpy(rawdev->name, sc->sc_dev.name);
	strcat(rawdev->name, "raw");
	rawdev->priv = sc;

	/* ether_setup clobbers type, so save it */
	t = rawdev->type;
	ether_setup(rawdev);
	rawdev->type = t;

	rawdev->stop = NULL;
	rawdev->hard_start_xmit = ath_start_raw;
	rawdev->set_multicast_list = NULL;
	rawdev->get_stats = _phy_getstats;
	rawdev->tx_queue_len = ATH_TXBUF;
	rawdev->flags |= IFF_NOARP;
	rawdev->flags &= ~IFF_MULTICAST;
	rawdev->mtu = IEEE80211_MAX_LEN;

	if (register_netdev(rawdev)) {
		goto bad;
	}

	if ((sc->sc_dev.flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
		meshif_start_queue(&sc->sc_rawdev);

	sc->sc_rawdev_enabled = 1;

	return 0;
 bad:
	return -1;
}

void ath_rawdev_detach(struct ath_softc *sc) 
{
	if (sc->sc_rawdev_enabled)
	{
		sc->sc_rawdev_enabled = 0;
		meshif_stop_queue(&sc->sc_rawdev);
		unregister_netdev(&sc->sc_rawdev);
	}
}

