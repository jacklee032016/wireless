/*
* $Id: wlan_output.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */


#include "_output.h"

/*
 * Allocate and setup a management frame of the specified
 * size.  We return the sk_buff and a pointer to the start
 * of the contiguous data area that's been reserved based
 * on the packet length.  The data area is forced to 32-bit
 * alignment and the buffer length to a multiple of 4 bytes.
 * This is done mainly so beacon frames (that require this)
 * can use this interface too.
 */
struct sk_buff *_ieee80211_getmgtframe(u_int8_t **frm, u_int pktlen)
{
	const u_int align = sizeof(u_int32_t);
	struct ieee80211_cb *cb;
	struct sk_buff *skb;
	u_int len;

	len = roundup(sizeof(struct ieee80211_frame) + pktlen, 4);
	skb = dev_alloc_skb(len + align-1);
	if (skb != NULL)
	{
		u_int off = ((unsigned long) skb->data) % align;
		if (off != 0)
			skb_reserve(skb, align - off);

		cb = (struct ieee80211_cb *)skb->cb;
		cb->ni = NULL;
		cb->flags = 0;

		skb_reserve(skb, sizeof(struct ieee80211_frame));
		*frm = skb_put(skb, pktlen);
	}
	return skb;
}


/*
 * Send a management frame to the specified node.  The node pointer
 * must have a reference as the pointer will be passed to the driver
 * and potentially held for a long time.  If the frame is successfully
 * dispatched to the driver, then it is responsible for freeing the
 * reference (and potentially free'ing up any associated storage).
 */
static int __ieee80211_mgmt_output(struct ieee80211com *ic, struct ieee80211_node *ni, struct sk_buff *skb, int type)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ieee80211_frame *wh;
	struct ieee80211_cb *cb = (struct ieee80211_cb *)skb->cb;

	KASSERT(ni != NULL, ("null node"));

	/*
	 * We want to pass the node down to the
	 * driver's start routine.  If we don't do so then the start
	 * routine must immediately look it up again and that can
	 * cause a lock order reversal if, for example, this frame
	 * is being sent because the station is being timedout and
	 * the frame being sent is a DEAUTH message. We stuff it in 
	 * the cb structure.
	 */
	cb->ni = ni;

	wh = (struct ieee80211_frame *)skb_push(skb, sizeof(struct ieee80211_frame));
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT | type;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)wh->i_dur = 0;
	*(u_int16_t *)wh->i_seq = htole16(ni->ni_txseqs[0] << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseqs[0]++;
	/*
	 * Hack.  When sending PROBE_REQ frames while scanning we
	 * explicitly force a broadcast rather than (as before) clobber
	 * ni_macaddr and ni_bssid.  This is stopgap, we need a way
	 * to communicate this directly rather than do something
	 * implicit based on surrounding state.
	 */
	if (type == IEEE80211_FC0_SUBTYPE_PROBE_REQ && (ic->ic_flags & IEEE80211_F_SCAN))
	{
		IEEE80211_ADDR_COPY(wh->i_addr1, dev->broadcast);
		IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
		IEEE80211_ADDR_COPY(wh->i_addr3, dev->broadcast);
	}
	else
	{
		IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
		IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
		IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
	}

	if ((cb->flags & M_LINK0) != 0 && ni->ni_challenge != NULL)
	{
		cb->flags &= ~M_LINK0;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
			"[%s] encrypting frame (%s)\n",
			swjtu_mesh_mac_sprintf(wh->i_addr1), __func__);
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
	}
#ifdef IEEE80211_DEBUG
	/* avoid printing too many frames */
	if ((ieee80211_msg_debug(ic) && output_doprint(ic, type)) || ieee80211_msg_dumppkts(ic))
	{
		printf("[%s] send %s on channel %u\n",
		    swjtu_mesh_mac_sprintf(wh->i_addr1),
		    ieee80211_mgt_subtype_name[
			(type & IEEE80211_FC0_SUBTYPE_MASK) >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
		    ieee80211_chan2ieee(ic, ni->ni_chan));
	}
#endif
	IEEE80211_NODE_STAT(ni, tx_mgmt);
	IF_ENQUEUE(&ic->ic_mgtq, skb);

	(*dev->hard_start_xmit)(NULL, dev);
	return 0;
}


/*
 * Send a management frame.  The node is for the destination (or ic_bss
 * when in station mode).  Nodes other than ic_bss have their reference
 * count bumped to reflect our use for an indeterminant time.
 */
int ieee80211_send_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni, int type, int arg)
{
#define	senderr(_x, _v)	do { ic->ic_stats._v++; ret = _x; goto bad; } while (0)
	struct sk_buff *skb;
	u_int8_t *frm;
	enum ieee80211_phymode mode;
	u_int16_t capinfo;
	int has_challenge, is_shared_key, ret, timer, status;

	KASSERT(ni != NULL, ("null node"));

	/*
	 * Hold a reference on the node so it doesn't go away until after
	 * the xmit is complete all the way in the driver.  On error we
	 * will remove our reference.
	 */
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE, "ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n", __func__, __LINE__, ni, swjtu_mesh_mac_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);

	timer = 0;
	switch (type)
	{
	
		case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
			/*
			 * prreq frame format
			 *	[tlv] ssid
			 *	[tlv] supported rates
			 *	[tlv] extended supported rates
			 *	[tlv] user-specified ie's
			 */
			skb = _ieee80211_getmgtframe(&frm,
				 2 + IEEE80211_NWID_LEN
			       + 2 + IEEE80211_RATE_SIZE
			       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
			       + (ic->ic_opt_ie != NULL ? ic->ic_opt_ie_len : 0)
			);
			if (skb == NULL)
				senderr(ENOMEM, is_tx_nobuf);

			frm = _ieee80211_add_ssid(frm, ic->ic_des_essid, ic->ic_des_esslen);
			mode = ieee80211_chan2mode(ic, ni->ni_chan);
			frm = _ieee80211_add_rates(frm, &ic->ic_sup_rates[mode]);
			frm = _ieee80211_add_xrates(frm, &ic->ic_sup_rates[mode]);
			if (ic->ic_opt_ie != NULL) {
				memcpy(frm, ic->ic_opt_ie, ic->ic_opt_ie_len);
				frm += ic->ic_opt_ie_len;
			}
			skb_trim(skb, frm - skb->data);

			IEEE80211_NODE_STAT(ni, tx_probereq);
			if (ic->ic_opmode == IEEE80211_M_STA)
				timer = IEEE80211_TRANS_WAIT;
			break;

		case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
			/*
			 * probe response frame format
			 *	[8] time stamp
			 *	[2] beacon interval
			 *	[2] cabability information
			 *	[tlv] ssid
			 *	[tlv] supported rates
			 *	[tlv] parameter set (FH/DS)
			 *	[tlv] parameter set (IBSS)
			 *	[tlv] extended rate phy (ERP)
			 *	[tlv] extended supported rates
			 *	[tlv] WPA
			 *	[tlv] WME (optional)
			 */
			skb = _ieee80211_getmgtframe(&frm,
				 8
			       + sizeof(u_int16_t)
			       + sizeof(u_int16_t)
			       + 2 + IEEE80211_NWID_LEN
			       + 2 + IEEE80211_RATE_SIZE
			       + 7	/* max(7,3) */
			       + 6
			       + 3
			       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
			       /* XXX !WPA1+WPA2 fits w/o a cluster */
			       + (ic->ic_flags & IEEE80211_F_WPA ?
					2*sizeof(struct ieee80211_ie_wpa) : 0)
			       + sizeof(struct ieee80211_wme_param)
			);
			if (skb == NULL)
				senderr(ENOMEM, is_tx_nobuf);

			memset(frm, 0, 8);	/* timestamp should be filled later */
			frm += 8;
			*(u_int16_t *)frm = htole16(ic->ic_bss->ni_intval);
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
			*(u_int16_t *)frm = htole16(capinfo);
			frm += 2;

			frm = _ieee80211_add_ssid(frm, ic->ic_bss->ni_essid,
					ic->ic_bss->ni_esslen);
			frm = _ieee80211_add_rates(frm, &ni->ni_rates);

			if (ic->ic_phytype == IEEE80211_T_FH) {
	                        *frm++ = IEEE80211_ELEMID_FHPARMS;
	                        *frm++ = 5;
	                        *frm++ = ni->ni_fhdwell & 0x00ff;
	                        *frm++ = (ni->ni_fhdwell >> 8) & 0x00ff;
	                        *frm++ = IEEE80211_FH_CHANSET(
				    ieee80211_chan2ieee(ic, ni->ni_chan));
	                        *frm++ = IEEE80211_FH_CHANPAT(
				    ieee80211_chan2ieee(ic, ni->ni_chan));
	                        *frm++ = ni->ni_fhindex;
			}
			else
			{
				*frm++ = IEEE80211_ELEMID_DSPARMS;
				*frm++ = 1;
				*frm++ = ieee80211_chan2ieee(ic, ni->ni_chan);
			}

			if (ic->ic_opmode == IEEE80211_M_IBSS) {
				*frm++ = IEEE80211_ELEMID_IBSSPARMS;
				*frm++ = 2;
				*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
			}
			
			frm = _ieee80211_add_xrates(frm, &ni->ni_rates);
			if (ic->ic_curmode == IEEE80211_MODE_11G ||
			    ic->ic_curmode == IEEE80211_MODE_TURBO_G)
				frm = _ieee80211_add_erp(frm, ic);
			if (ic->ic_flags & IEEE80211_F_WPA)
				frm = _ieee80211_add_wpa(frm, ic);
			if (ic->ic_flags & IEEE80211_F_WME)
				frm = _ieee80211_add_wme_param(frm, &ic->ic_wme);		
			skb_trim(skb, frm - skb->data);
			break;

		case IEEE80211_FC0_SUBTYPE_AUTH:
			status = arg >> 16;
			arg &= 0xffff;
			has_challenge = ((arg == IEEE80211_AUTH_SHARED_CHALLENGE ||
			    arg == IEEE80211_AUTH_SHARED_RESPONSE) &&
			    ni->ni_challenge != NULL);

			/*
			 * Deduce whether we're doing open authentication or
			 * shared key authentication.  We do the latter if
			 * we're in the middle of a shared key authentication
			 * handshake or if we're initiating an authentication
			 * request and configured to use shared key.
			 */
			is_shared_key = has_challenge ||
			     arg >= IEEE80211_AUTH_SHARED_RESPONSE ||
			     (arg == IEEE80211_AUTH_SHARED_REQUEST &&
			      ic->ic_bss->ni_authmode == IEEE80211_AUTH_SHARED);

			skb = _ieee80211_getmgtframe(&frm,
				  3 * sizeof(u_int16_t)
				+ (has_challenge && status == IEEE80211_STATUS_SUCCESS ?
					sizeof(u_int16_t)+IEEE80211_CHALLENGE_LEN : 0)
			);
			
			if (skb == NULL)
				senderr(ENOMEM, is_tx_nobuf);

			((u_int16_t *)frm)[0] =
			    (is_shared_key) ? htole16(IEEE80211_AUTH_ALG_SHARED)
			                    : htole16(IEEE80211_AUTH_ALG_OPEN);
			((u_int16_t *)frm)[1] = htole16(arg);	/* sequence number */
			((u_int16_t *)frm)[2] = htole16(status);/* status */

			if (has_challenge && status == IEEE80211_STATUS_SUCCESS) {
				((u_int16_t *)frm)[3] =
				    htole16((IEEE80211_CHALLENGE_LEN << 8) |
				    IEEE80211_ELEMID_CHALLENGE);
				memcpy(&((u_int16_t *)frm)[4], ni->ni_challenge,
				    IEEE80211_CHALLENGE_LEN);
				skb_trim(skb, 4 * sizeof(u_int16_t) + IEEE80211_CHALLENGE_LEN);
				if (arg == IEEE80211_AUTH_SHARED_RESPONSE) {
					struct ieee80211_cb *cb =
						(struct ieee80211_cb *)skb->cb;
					IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
					    "[%s] request encrypt frame (%s)\n",
					    swjtu_mesh_mac_sprintf(ni->ni_macaddr), __func__);
					cb->flags |= M_LINK0; /* WEP-encrypt, please */
				}
			} else
				skb_trim(skb, 3 * sizeof(u_int16_t));

			/* XXX not right for shared key */
			if (status == IEEE80211_STATUS_SUCCESS)
				IEEE80211_NODE_STAT(ni, tx_auth);
			else
				IEEE80211_NODE_STAT(ni, tx_auth_fail);

			/*
			 * When 802.1x is not in use mark the port
			 * authorized at this point so traffic can flow.
			 */
			if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
			    status == IEEE80211_STATUS_SUCCESS &&
			    ni->ni_authmode != IEEE80211_AUTH_8021X)
				ieee80211_node_authorize(ic, ni);
			if (ic->ic_opmode == IEEE80211_M_STA)
				timer = IEEE80211_TRANS_WAIT;
			break;

		case IEEE80211_FC0_SUBTYPE_DEAUTH:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				"[%s] send station deauthenticate (reason %d)\n",
				swjtu_mesh_mac_sprintf(ni->ni_macaddr), arg);
			skb = _ieee80211_getmgtframe(&frm, sizeof(u_int16_t));
			if (skb == NULL)
				senderr(ENOMEM, is_tx_nobuf);
			*(u_int16_t *)frm = htole16(arg);	/* reason */
			skb_trim(skb, sizeof(u_int16_t));

			IEEE80211_NODE_STAT(ni, tx_deauth);
			IEEE80211_NODE_STAT_SET(ni, tx_deauth_code, arg);

			ieee80211_node_unauthorize(ic, ni);	/* port closed */
			break;

		case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
		case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
			/*
			 * asreq frame format
			 *	[2] capability information
			 *	[2] listen interval
			 *	[6*] current AP address (reassoc only)
			 *	[tlv] ssid
			 *	[tlv] supported rates
			 *	[tlv] extended supported rates
			 *	[tlv] WME
			 *	[tlv] user-specified ie's
			 */
			skb = _ieee80211_getmgtframe(&frm,
				 sizeof(u_int16_t)
			       + sizeof(u_int16_t)
			       + IEEE80211_ADDR_LEN
			       + 2 + IEEE80211_NWID_LEN
			       + 2 + IEEE80211_RATE_SIZE
			       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
			       + sizeof(struct ieee80211_wme_info)
			       + (ic->ic_opt_ie != NULL ? ic->ic_opt_ie_len : 0)
			);
			if (skb == NULL)
				senderr(ENOMEM, is_tx_nobuf);

			capinfo = 0;
			if (ic->ic_opmode == IEEE80211_M_IBSS)
				capinfo |= IEEE80211_CAPINFO_IBSS;
			else		/* IEEE80211_M_STA */
				capinfo |= IEEE80211_CAPINFO_ESS;
			if (ic->ic_flags & IEEE80211_F_PRIVACY)
				capinfo |= IEEE80211_CAPINFO_PRIVACY;
			/*
			 * NB: Some 11a AP's reject the request when
			 *     short premable is set.
			 */
			if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
			    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
				capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
			if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) &&
			    (ic->ic_caps & IEEE80211_C_SHSLOT))
				capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
			*(u_int16_t *)frm = htole16(capinfo);
			frm += 2;

			*(u_int16_t *)frm = htole16(ic->ic_lintval);
			frm += 2;

			if (type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
				IEEE80211_ADDR_COPY(frm, ic->ic_bss->ni_bssid);
				frm += IEEE80211_ADDR_LEN;
			}

			frm = _ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
			frm = _ieee80211_add_rates(frm, &ni->ni_rates);
			frm = _ieee80211_add_xrates(frm, &ni->ni_rates);
			if ((ic->ic_flags & IEEE80211_F_WME) && ni->ni_wme_ie != NULL)
				frm = _ieee80211_add_wme_info(frm, &ic->ic_wme);
			if (ic->ic_opt_ie != NULL) {
				memcpy(frm, ic->ic_opt_ie, ic->ic_opt_ie_len);
				frm += ic->ic_opt_ie_len;
			}
			skb_trim(skb, frm - skb->data);

			timer = IEEE80211_TRANS_WAIT;
			break;

		case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
		case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
			/*
			 * asreq frame format
			 *	[2] capability information
			 *	[2] status
			 *	[2] association ID
			 *	[tlv] supported rates
			 *	[tlv] extended supported rates
			 *	[tlv] WME (if enabled and STA enabled)
			 */
			skb = _ieee80211_getmgtframe(&frm,
				 sizeof(u_int16_t)
			       + sizeof(u_int16_t)
			       + sizeof(u_int16_t)
			       + 2 + IEEE80211_RATE_SIZE
			       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
			       + sizeof(struct ieee80211_wme_param)
			);
			if (skb == NULL)
				senderr(ENOMEM, is_tx_nobuf);

			capinfo = IEEE80211_CAPINFO_ESS;
			if (ic->ic_flags & IEEE80211_F_PRIVACY)
				capinfo |= IEEE80211_CAPINFO_PRIVACY;
			if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
			    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
				capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
			if (ic->ic_flags & IEEE80211_F_SHSLOT)
				capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
			*(u_int16_t *)frm = htole16(capinfo);
			frm += 2;

			*(u_int16_t *)frm = htole16(arg);	/* status */
			frm += 2;

			if (arg == IEEE80211_STATUS_SUCCESS) {
				*(u_int16_t *)frm = htole16(ni->ni_associd);
				IEEE80211_NODE_STAT(ni, tx_assoc);
			} else
				IEEE80211_NODE_STAT(ni, tx_assoc_fail);
			frm += 2;

			frm = _ieee80211_add_rates(frm, &ni->ni_rates);
			frm = _ieee80211_add_xrates(frm, &ni->ni_rates);
			if ((ic->ic_flags & IEEE80211_F_WME) && ni->ni_wme_ie != NULL)
				frm = _ieee80211_add_wme_param(frm, &ic->ic_wme);
			skb_trim(skb, frm - skb->data);
			break;

		case IEEE80211_FC0_SUBTYPE_DISASSOC:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				"[%s] send station disassociate (reason %d)\n",
				swjtu_mesh_mac_sprintf(ni->ni_macaddr), arg);
			skb = _ieee80211_getmgtframe(&frm, sizeof(u_int16_t));
			if (skb == NULL)
				senderr(ENOMEM, is_tx_nobuf);
			*(u_int16_t *)frm = htole16(arg);	/* reason */
			skb_trim(skb, sizeof(u_int16_t));

			IEEE80211_NODE_STAT(ni, tx_disassoc);
			IEEE80211_NODE_STAT_SET(ni, tx_disassoc_code, arg);
			break;

		default:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
				"[%s] invalid mgmt frame type %u\n",
				swjtu_mesh_mac_sprintf(ni->ni_macaddr), type);
			senderr(EINVAL, is_tx_unknownmgt);
			/* NOTREACHED */
	}

	ret = __ieee80211_mgmt_output(ic, ni, skb, type);
	if (ret == 0)
	{
		if (timer)
			ic->ic_mgt_timer = timer;
	}
	else
	{
bad:
		ieee80211_free_node(ni);
	}
	return ret;
#undef senderr
}


/*
 * Send a null data frame to the specified node.
 */
int ieee80211_send_nulldata(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct sk_buff *skb;
	struct ieee80211_frame *wh;
	struct ieee80211_cb *cb;
	u_int8_t *frm;

	skb = _ieee80211_getmgtframe(&frm, 0);

	if (skb == NULL) {
		/* XXX debug msg */
		ic->ic_stats.is_tx_nobuf++;
		return ENOMEM;
	}
	cb  = (struct ieee80211_cb *)skb->cb;
	cb->ni = ieee80211_ref_node(ni);

	wh = (struct ieee80211_frame *)
		skb_push(skb, sizeof(struct ieee80211_frame));
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA |
		IEEE80211_FC0_SUBTYPE_NODATA;
	*(u_int16_t *)wh->i_dur = 0;
	*(u_int16_t *)wh->i_seq =
	    htole16(ni->ni_txseqs[0] << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseqs[0]++;

	/* XXX WDS */
	wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
	IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
	IEEE80211_ADDR_COPY(wh->i_addr3, ic->ic_myaddr);
	skb_trim(skb, sizeof(struct ieee80211_frame));

	IEEE80211_NODE_STAT(ni, tx_data);

	IF_ENQUEUE(&ic->ic_mgtq, skb);		/* cheat */
	(*dev->hard_start_xmit)(NULL, dev);

	return 0;
}

