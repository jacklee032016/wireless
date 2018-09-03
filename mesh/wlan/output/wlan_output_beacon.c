/*
* $Id: wlan_output_beacon.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_output.h"

/* Allocate a beacon frame and fillin the appropriate bits. */
struct sk_buff *ieee80211_beacon_alloc(struct ieee80211com *ic, struct ieee80211_node *ni,
	struct ieee80211_beacon_offsets *bo)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ieee80211_frame *wh;
	struct sk_buff *skb;
	struct ieee80211_cb *cb;
	int pktlen;
	u_int8_t *frm, *efrm;
	u_int16_t capinfo;
	struct ieee80211_rateset *rs;

	/*
	 * beacon frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] cabability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[3] parameter set (DS)
	 *	[tlv] parameter set (IBSS/TIM)
	 *	[tlv] extended rate phy (ERP)
	 *	[tlv] extended supported rates
	 *	[tlv] WME parameters
	 *	[tlv] WPA/RSN parameters
	 * XXX Vendor-specific OIDs (e.g. Atheros)
	 * NB: we allocate the max space required for the TIM bitmap.
	 */
	rs = &ni->ni_rates;
	pktlen =   8					/* time stamp */
		 + sizeof(u_int16_t)			/* beacon interval */
		 + sizeof(u_int16_t)			/* capabilities */
		 + 2 + ni->ni_esslen			/* ssid */
	         + 2 + IEEE80211_RATE_SIZE		/* supported rates */
	         + 2 + 1				/* DS parameters */
		 + 2 + 4 + ic->ic_tim_len		/* DTIM/IBSSPARMS */
		 + 2 + 1				/* ERP */
	         + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
		 + (ic->ic_caps & IEEE80211_C_WME ?	/* WME */
			sizeof(struct ieee80211_wme_param) : 0)
		 + (ic->ic_caps & IEEE80211_C_WPA ?	/* WPA 1+2 */
			2*sizeof(struct ieee80211_ie_wpa) : 0)
		 ;
	skb = _ieee80211_getmgtframe(&frm, pktlen);
	cb = (struct ieee80211_cb *) skb->cb;

	if (skb == NULL)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"%s: cannot get buf; size %u\n", __func__, pktlen);
		ic->ic_stats.is_tx_nobuf++;
		return NULL;
	}

	memset(frm, 0, 8);	/* XXX timestamp is set by hardware/driver */
	frm += 8;
	*(u_int16_t *)frm = htole16(ni->ni_intval);
	frm += 2;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	bo->bo_caps = (u_int16_t *)frm;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;
	*frm++ = IEEE80211_ELEMID_SSID;
	
	if ((ic->ic_flags & IEEE80211_F_HIDESSID) == 0)
	{
		*frm++ = ni->ni_esslen;
		memcpy(frm, ni->ni_essid, ni->ni_esslen);
		frm += ni->ni_esslen;
	}
	else
		*frm++ = 0;
	
	frm = _ieee80211_add_rates(frm, rs);
	if (ic->ic_curmode != IEEE80211_MODE_FH)
	{
		*frm++ = IEEE80211_ELEMID_DSPARMS;
		*frm++ = 1;
		*frm++ = ieee80211_chan2ieee(ic, ni->ni_chan);
	}
	
	bo->bo_tim = frm;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
	{
		*frm++ = IEEE80211_ELEMID_IBSSPARMS;
		*frm++ = 2;
		*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
		bo->bo_tim_len = 0;
	}
	else
	{
		struct ieee80211_tim_ie *tie = (struct ieee80211_tim_ie *) frm;

		tie->tim_ie = IEEE80211_ELEMID_TIM;
		tie->tim_len = 4;	/* length */
		tie->tim_count = 0;	/* DTIM count */ 
		tie->tim_period = ic->ic_dtim_period;	/* DTIM period */
		tie->tim_bitctl = 0;	/* bitmap control */
		tie->tim_bitmap[0] = 0;	/* Partial Virtual Bitmap */
		frm += sizeof(struct ieee80211_tim_ie);
		bo->bo_tim_len = 1;
	}
	
	bo->bo_trailer = frm;
	if (ic->ic_flags & IEEE80211_F_WME)
	{
		bo->bo_wme = frm;
		frm = _ieee80211_add_wme_param(frm, &ic->ic_wme);
		ic->ic_flags &= ~IEEE80211_F_WMEUPDATE;
	}
	
	if (ic->ic_flags & IEEE80211_F_WPA)
		frm = _ieee80211_add_wpa(frm, ic);
	if (ic->ic_curmode == IEEE80211_MODE_11G ||
	    ic->ic_curmode == IEEE80211_MODE_TURBO_G)
		frm = _ieee80211_add_erp(frm, ic);
	
	efrm = _ieee80211_add_xrates(frm, rs);
	cb->ni = ni;
	bo->bo_trailer_len = efrm - bo->bo_trailer;
	skb_trim(skb, efrm - skb->data);

	wh = (struct ieee80211_frame *) skb_push(skb, sizeof(struct ieee80211_frame));
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_BEACON;
	
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)wh->i_dur = 0;
	IEEE80211_ADDR_COPY(wh->i_addr1, dev->broadcast);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
	*(u_int16_t *)wh->i_seq = 0;

	return skb;
}
EXPORT_SYMBOL(ieee80211_beacon_alloc);

/*
 * Update the dynamic parts of a beacon frame based on the current state.
 */
int ieee80211_beacon_update(struct ieee80211com *ic, struct ieee80211_node *ni,
	struct ieee80211_beacon_offsets *bo, struct sk_buff *skb0, int mcast)
{
	int len_changed = 0;
	u_int16_t capinfo;

	IEEE80211_BEACON_LOCK(ic);
	/* XXX faster to recalculate entirely or just changes? */
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	*bo->bo_caps = htole16(capinfo);

	if (ic->ic_flags & IEEE80211_F_WME) {
		struct ieee80211_wme_state *wme = &ic->ic_wme;

		/*
		 * Check for agressive mode change.  When there is
		 * significant high priority traffic in the BSS
		 * throttle back BE traffic by using conservative
		 * parameters.  Otherwise BE uses agressive params
		 * to optimize performance of legacy/non-QoS traffic.
		 */
		if (wme->wme_flags & WME_F_AGGRMODE) {
			if (wme->wme_hipri_traffic >
			    wme->wme_hipri_switch_thresh) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
				    "%s: traffic %u, disable aggressive mode\n",
				    __func__, wme->wme_hipri_traffic);
				wme->wme_flags &= ~WME_F_AGGRMODE;
				ieee80211_wme_updateparams_locked(ic);
				wme->wme_hipri_traffic =
					wme->wme_hipri_switch_hysteresis;
			} else
				wme->wme_hipri_traffic = 0;
		} else {
			if (wme->wme_hipri_traffic <=
			    wme->wme_hipri_switch_thresh) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
				    "%s: traffic %u, enable aggressive mode\n",
				    __func__, wme->wme_hipri_traffic);
				wme->wme_flags |= WME_F_AGGRMODE;
				ieee80211_wme_updateparams_locked(ic);
				wme->wme_hipri_traffic = 0;
			} else
				wme->wme_hipri_traffic =
					wme->wme_hipri_switch_hysteresis;
		}
		if (ic->ic_flags & IEEE80211_F_WMEUPDATE) {
			(void) _ieee80211_add_wme_param(bo->bo_wme, wme);
			ic->ic_flags &= ~IEEE80211_F_WMEUPDATE;
		}
	}

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {	/* NB: no IBSS support*/
		struct ieee80211_tim_ie *tie =
			(struct ieee80211_tim_ie *) bo->bo_tim;
		if (ic->ic_flags & IEEE80211_F_TIMUPDATE) {
			u_int timlen, timoff, i;
			/* 
			 * ATIM/DTIM needs updating.  If it fits in the
			 * current space allocated then just copy in the
			 * new bits.  Otherwise we need to move any trailing
			 * data to make room.  Note that we know there is
			 * contiguous space because ieee80211_beacon_allocate
			 * insures there is space in the mbuf to write a
			 * maximal-size virtual bitmap (based on ic_max_aid).
			 */
			/*
			 * Calculate the bitmap size and offset, copy any
			 * trailer out of the way, and then copy in the
			 * new bitmap and update the information element.
			 * Note that the tim bitmap must contain at least
			 * one byte and any offset must be even.
			 */
			if (ic->ic_ps_pending != 0) {
				timoff = 128;		/* impossibly large */
				for (i = 0; i < ic->ic_tim_len; i++)
					if (ic->ic_tim_bitmap[i]) {
						timoff = i &~ 1;
						break;
					}
				KASSERT(timoff != 128, ("tim bitmap empty!"));
				for (i = ic->ic_tim_len-1; i >= timoff; i--)
					if (ic->ic_tim_bitmap[i])
						break;
				timlen = 1 + (i - timoff);
			} else {
				timoff = 0;
				timlen = 1;
			}
			if (timlen != bo->bo_tim_len) {
				/* copy up/down trailer */
				memmove(tie->tim_bitmap+timlen, bo->bo_trailer,
					bo->bo_trailer_len);
				bo->bo_trailer = tie->tim_bitmap+timlen;
				bo->bo_wme = bo->bo_trailer;
				bo->bo_tim_len = timlen;

				/* update information element */
				tie->tim_len = 3 + timlen;
				tie->tim_bitctl = timoff;
				len_changed = 1;
			}
			memcpy(tie->tim_bitmap, ic->ic_tim_bitmap + timoff,
				bo->bo_tim_len);

			ic->ic_flags &= ~IEEE80211_F_TIMUPDATE;

			IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
				"%s: TIM updated, pending %u, off %u, len %u\n",
				__func__, ic->ic_ps_pending, timoff, timlen);
		}
		/* count down DTIM period */
		if (tie->tim_count == 0)
			tie->tim_count = tie->tim_period - 1;
		else
			tie->tim_count--;
		/* update state for buffered multicast frames on DTIM */
		if (mcast && (tie->tim_count == 0 || tie->tim_period == 1))
			tie->tim_bitctl |= 1;
		else
			tie->tim_bitctl &= ~1;
	}
	IEEE80211_BEACON_UNLOCK(ic);

	return len_changed;
}
EXPORT_SYMBOL(ieee80211_beacon_update);

