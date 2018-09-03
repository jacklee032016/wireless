/*
* $Id: wlan_output_crypto.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_output.h"

#include "if_llc.h"
#include "if_ethersubr.h"

#if 0
/*
 * Insure there is sufficient headroom and tailroom to
 * encapsulate the 802.11 data frame.  If room isn't
 * already there, reallocate so there is enough space.
 * Drivers and cipher modules assume we have done the
 * necessary work and fail rudely if they don't find
 * the space they need.
 */
static struct sk_buff *__ieee80211_skbhdr_adjust(struct ieee80211com *ic, int hdrsize, struct ieee80211_key *key, struct sk_buff *skb)
{
	int need_headroom = hdrsize;
	int need_tailroom = 0;

	if (key != NULL)
	{
		const struct ieee80211_cipher *cip = key->wk_cipher;
		/*
		 * Adjust for crypto needs.  When hardware crypto is
		 * being used we assume the hardware/driver will deal
		 * with any padding (on the fly, without needing to
		 * expand the frame contents).  When software crypto
		 * is used we need to insure room is available at the
		 * front and back and also for any per-MSDU additions.
		 */
		/* XXX belongs in crypto code? */
		need_headroom += cip->ic_header;
		/* XXX pre-calculate per key */
		if (key->wk_flags & IEEE80211_KEY_SWCRYPT)
			need_tailroom += cip->ic_trailer;
		/* XXX frags */
		if (key->wk_flags & IEEE80211_KEY_SWMIC)
			need_tailroom += cip->ic_miclen;
	}
	/*
	 * We know we are called just after stripping an Ethernet
	 * header and before prepending an LLC header.  This means we 
	 * need to assure the LLC header fits in.
	 */
	need_headroom += sizeof(struct llc);
	skb = skb_unshare(skb, GFP_ATOMIC);
	if (skb == NULL)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT, "%s: cannot unshare for encapsulation\n", __func__);
		ic->ic_stats.is_tx_nobuf++;
	}
	else if (skb_tailroom(skb) < need_tailroom)
	{
		int headroom = skb_headroom(skb) < need_headroom ?need_headroom - skb_headroom(skb) : 0;

		if (pskb_expand_head(skb, headroom,
			need_tailroom - skb_tailroom(skb), GFP_ATOMIC))
		{
			dev_kfree_skb(skb);
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT,  "%s: cannot expand storage (tail)\n", __func__);
			ic->ic_stats.is_tx_nobuf++;
			skb = NULL;
		}
	}
	else if (skb_headroom(skb) < need_headroom)
	{
		struct sk_buff *tmp = skb;
		skb = skb_realloc_headroom(skb, need_headroom);
		dev_kfree_skb(tmp);
		if (skb == NULL)
		{
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT,  "%s: cannot expand storage (head)\n", __func__);
			ic->ic_stats.is_tx_nobuf++;
		}
	}
	return skb;
}
#endif

#define	KEY_UNDEFINED(k)	((k).wk_cipher == &ieee80211_cipher_none)
/*
 * Return the transmit key to use in sending a unicast frame.
 * If a unicast key is set we use that.  When no unicast key is set
 * we fall back to the default transmit key.
 */ 
static inline struct ieee80211_key *__ieee80211_crypto_getucastkey(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (KEY_UNDEFINED(ni->ni_ucastkey))
	{
		if (ic->ic_def_txkey == IEEE80211_KEYIX_NONE || KEY_UNDEFINED(ic->ic_nw_keys[ic->ic_def_txkey]))
			return NULL;
		return &ic->ic_nw_keys[ic->ic_def_txkey];
	}
	else
	{
		return &ni->ni_ucastkey;
	}
}

/*
 * Return the transmit key to use in sending a multicast frame.
 * Multicast traffic always uses the group key which is installed as
 * the default tx key.
 */ 
static inline struct ieee80211_key *__ieee80211_crypto_getmcastkey(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ic->ic_def_txkey == IEEE80211_KEYIX_NONE ||KEY_UNDEFINED(ic->ic_nw_keys[ic->ic_def_txkey]))
		return NULL;
	
	return &ic->ic_nw_keys[ic->ic_def_txkey];
}

#if 0
/*
 * Encapsulate an outbound data frame.  The sk_buff chain is updated.
 * If an error is encountered NULL is returned.  The caller is required
 * to provide a node reference and pullup the ethernet header in the
 * first sk_buff.
 */
struct sk_buff *ieee80211_encap(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni)
{
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct ieee80211_key *key;
	struct llc *llc;
	int hdrsize, datalen, addqos;

	KASSERT(skb->len >= sizeof(eh), ("no ethernet header!"));
	memcpy(&eh, skb->data, sizeof(struct ether_header));
	skb_pull(skb, sizeof(struct ether_header));

	/*
	 * Insure space for additional headers.  First identify
	 * transmit key to use in calculating any buffer adjustments
	 * required.  This is also used below to do privacy
	 * encapsulation work.  Then calculate the 802.11 header
	 * size and any padding required by the driver.
	 *
	 * Note key may be NULL if we fall back to the default
	 * transmit key and that is not set.  In that case the
	 * buffer may not be expanded as needed by the cipher
	 * routines, but they will/should discard it.
	 */
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
	{
		if (ic->ic_opmode == IEEE80211_M_STA || !IEEE80211_IS_MULTICAST(eh.ether_dhost))
			key = __ieee80211_crypto_getucastkey(ic, ni);
		else
			key = __ieee80211_crypto_getmcastkey(ic, ni);
		
		if (key == NULL && eh.ether_type != __constant_htons(ETHERTYPE_PAE))
		{
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_CRYPTO,"[%s] no default transmit key (%s) deftxkey %u\n",swjtu_mesh_mac_sprintf(eh.ether_dhost), __func__,ic->ic_def_txkey);
			ic->ic_stats.is_tx_nodefkey++;
		}
	}
	else
		key = NULL;

	/* XXX 4-address format */
	/*
	 * XXX Some ap's don't handle QoS-encapsulated EAPOL
	 * frames so suppress use.  This may be an issue if other
	 * ap's require all data frames to be QoS-encapsulated
	 * once negotiated in which case we'll need to make this
	 * configurable.
	 */
	addqos = (ni->ni_flags & IEEE80211_NODE_QOS) && eh.ether_type != __constant_htons(ETHERTYPE_PAE);

	if (addqos)
		hdrsize = sizeof(struct ieee80211_qosframe);
	else
		hdrsize = sizeof(struct ieee80211_frame);
	
	if (ic->ic_flags & IEEE80211_F_DATAPAD)
		hdrsize = roundup(hdrsize, sizeof(u_int32_t));

	skb = __ieee80211_skbhdr_adjust(ic, hdrsize, key, skb);
	if (skb == NULL)
	{
		/* NB: ieee80211_skbhdr_adjust handles msgs+statistics */
		goto bad;
	}

	llc = (struct llc *) skb_push(skb, sizeof(struct llc));
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh.ether_type;
	datalen = skb->len;		/* NB: w/o 802.11 header */

	wh = (struct ieee80211_frame *)skb_push(skb, hdrsize);

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *)wh->i_dur = 0;
	
	switch (ic->ic_opmode)
	{
		case IEEE80211_M_STA:
			wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_bssid);
			IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
			IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
			IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
			/*
			 * NB: always use the bssid from ic_bss as the
			 *     neighbor's may be stale after an ibss merge
			 */
			IEEE80211_ADDR_COPY(wh->i_addr3, ic->ic_bss->ni_bssid);
			break;
		case IEEE80211_M_HOSTAP:
			wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
			IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
			IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
			IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);
			break;
		case IEEE80211_M_MONITOR:
			goto bad;
	}

	if (addqos)
	{
		struct ieee80211_qosframe *qwh = (struct ieee80211_qosframe *) wh;
		int ac, tid;

		ac = M_WME_GETAC(skb);
		/* map from access class/queue to 11e header priorty value */
		tid = WME_AC_TO_TID(ac);
		qwh->i_qos[0] = tid & IEEE80211_QOS_TID;
		if (ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmep_noackPolicy)
			qwh->i_qos[0] |= 1 << IEEE80211_QOS_ACKPOLICY_S;
		qwh->i_qos[1] = 0;
		qwh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;

		*(u_int16_t *)wh->i_seq = htole16(ni->ni_txseqs[tid] << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_txseqs[tid]++;
	}
	else
	{
		*(u_int16_t *)wh->i_seq = htole16(ni->ni_txseqs[0] << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_txseqs[0]++;
	}
	
	if (key != NULL)
	{
		/*
		 * IEEE 802.1X: send EAPOL frames always in the clear.
		 * WPA/WPA2: encrypt EAPOL keys when pairwise keys are set.
		 */
		if (eh.ether_type != __constant_htons(ETHERTYPE_PAE) ||
		    ((ic->ic_flags & IEEE80211_F_WPA) &&
		     (ic->ic_opmode == IEEE80211_M_STA ?
		      !KEY_UNDEFINED(*key) : !KEY_UNDEFINED(ni->ni_ucastkey))))
		{
			wh->i_fc[1] |= IEEE80211_FC1_WEP;
			/* XXX do fragmentation */
			if (!ieee80211_crypto_enmic(ic, key, skb))
			{
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT, "[%s] enmic failed, discard frame\n", swjtu_mesh_mac_sprintf(eh.ether_dhost));
				ic->ic_stats.is_crypto_enmicfail++;
				goto bad;
			}
		}
	}

	//TODO: hope remove of ni->ni_inact = ic->ic_inact_run; is ok

	IEEE80211_NODE_STAT(ni, tx_data);
	IEEE80211_NODE_STAT_ADD(ni, tx_bytes, datalen);

	return skb;
bad:
	if (skb != NULL)
		dev_kfree_skb(skb);
	return NULL;
}

EXPORT_SYMBOL(ieee80211_encap);
#endif

