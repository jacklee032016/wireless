/*
* $Id: wlan_input_mgmt.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_input.h"

/* Verify the existence and length of __elem or get out. */
#define IEEE80211_VERIFY_ELEMENT(__elem, __maxlen) do {			\
	if ((__elem) == NULL) {						\
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,		\
		    wh, ieee80211_mgt_subtype_name[subtype >>		\
			IEEE80211_FC0_SUBTYPE_SHIFT],			\
		    "%s", "no " #__elem );				\
		ic->ic_stats.is_rx_elem_missing++;			\
		return;							\
	}								\
	if ((__elem)[1] > (__maxlen)) {					\
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,		\
		    wh, ieee80211_mgt_subtype_name[subtype >>		\
			IEEE80211_FC0_SUBTYPE_SHIFT],			\
		    "bad " #__elem " len %d", (__elem)[1]);		\
		ic->ic_stats.is_rx_elem_toobig++;			\
		return;							\
	}								\
} while (0)

#define	IEEE80211_VERIFY_LENGTH(_len, _minlen) do {			\
	if ((_len) < (_minlen)) {					\
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,		\
		    wh, ieee80211_mgt_subtype_name[subtype >>		\
			IEEE80211_FC0_SUBTYPE_SHIFT],			\
		    "%s", "ie too short");				\
		ic->ic_stats.is_rx_elem_toosmall++;			\
		return;							\
	}								\
} while (0)

#define	ISPROBE(_st)	((_st) == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
#define	ISREASSOC(_st)	((_st) == IEEE80211_FC0_SUBTYPE_REASSOC_RESP)

static __inline int iswmeparam(const u_int8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_PARAM_OUI_SUBTYPE;
}

void __associate_response(struct ieee80211com *ic, struct sk_buff *skb,
	struct ieee80211_node *ni,	int subtype, int rssi, u_int32_t rstamp)
{
	u_int16_t capinfo, associd;
	u_int16_t status;

	struct ieee80211_frame *wh;
	u_int8_t *frm, *efrm;
	u_int8_t *rates, *xrates, *wpa, *wme;

	wh = (struct ieee80211_frame *) skb->data;
	frm = (u_int8_t *)&wh[1];
	efrm = skb->data + skb->len;

	if (ic->ic_opmode != IEEE80211_M_STA ||ic->ic_state != IEEE80211_S_ASSOC)
	{
		ic->ic_stats.is_rx_mgtdiscard++;
		return;
	}

	/*
	 * asresp frame format
	 *	[2] capability information
	 *	[2] status
	 *	[2] association ID
	 *	[tlv] supported rates
	 *	[tlv] extended supported rates
	 *	[tlv] WME
	 */
	IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
	ni = ic->ic_bss;
	capinfo = le16toh(*(u_int16_t *)frm);
	frm += 2;
	status = le16toh(*(u_int16_t *)frm);
	frm += 2;
	if (status != 0)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,  "[%s] %sassoc failed (reason %d)\n",  swjtu_mesh_mac_sprintf(wh->i_addr2), ISREASSOC(subtype) ?  "re" : "", status);

		if (ni != ic->ic_bss)	/* XXX never true? */
			ni->ni_fails++;

		ic->ic_stats.is_rx_auth_fail++;	/* XXX */
		return;
	}
	associd = le16toh(*(u_int16_t *)frm);
	frm += 2;

	rates = xrates = wpa = wme = NULL;

	while (frm < efrm)
	{
		switch (*frm)
		{
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			case IEEE80211_ELEMID_VENDOR:
				if (iswmeoui(frm))
					wme = frm;
				/* XXX Atheros OUI support */
				break;
		}
		frm += frm[1] + 2;
	}

	IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
	ieee80211_setup_rates(ic, ni, rates, xrates,	IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |IEEE80211_F_DONEGO | IEEE80211_F_DODEL);

	if (ni->ni_rates.rs_nrates == 0)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "[%s] %sassoc failed (rate set mismatch)\n", swjtu_mesh_mac_sprintf(wh->i_addr2), ISREASSOC(subtype) ?  "re" : "");

		if (ni != ic->ic_bss)	/* XXX never true? */
			ni->ni_fails++;
		ic->ic_stats.is_rx_assoc_norate++;
		return;
	}

	ni->ni_capinfo = capinfo;
	ni->ni_associd = associd;
	if (wme != NULL &&  _ieee80211_parse_wmeparams(ic, wme, wh) >= 0)
	{
		ni->ni_flags |= IEEE80211_NODE_QOS;
		ieee80211_wme_updateparams(ic);
	}
	else
		ni->ni_flags &= ~IEEE80211_NODE_QOS;
	/*
	 * Configure state now that we are associated.
	 *
	 * XXX may need different/additional driver callbacks?
	 */
	if ((ic->ic_curmode == IEEE80211_MODE_11A ||
	     ic->ic_curmode == IEEE80211_MODE_TURBO_A) ||
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE))
	{
		ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
		ic->ic_flags &= ~IEEE80211_F_USEBARKER;
	}
	else
	{
		ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
		ic->ic_flags |= IEEE80211_F_USEBARKER;
	}
	
	ieee80211_set_shortslottime(ic, (ic->ic_curmode == IEEE80211_MODE_11A ||
		 ic->ic_curmode == IEEE80211_MODE_TURBO_A) ||
		(ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
	/*
	 * Honor ERP protection.
	 *
	 * NB: ni_erp should zero for non-11g operation.
	 * XXX check ic_curmode anyway?
	 */
	if (ni->ni_erp & IEEE80211_ERP_USE_PROTECTION)
		ic->ic_flags |= IEEE80211_F_USEPROT;
	else
		ic->ic_flags &= ~IEEE80211_F_USEPROT;
	
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
	    "[%s] %sassoc success: %s preamble, %s slot time%s%s\n",
	    swjtu_mesh_mac_sprintf(wh->i_addr2),
	    ISREASSOC(subtype) ? "re" : "",
	    ic->ic_flags&IEEE80211_F_SHPREAMBLE ? "short" : "long",
	    ic->ic_flags&IEEE80211_F_SHSLOT ? "short" : "long",
	    ic->ic_flags&IEEE80211_F_USEPROT ? ", protection" : "",
	    ni->ni_flags & IEEE80211_NODE_QOS ? ", QoS" : ""
	);
	
	ieee80211_new_state(ic, IEEE80211_S_RUN, subtype);

	return;
}

void __associate_request(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni,	int subtype, int rssi, u_int32_t rstamp)
{
	u_int16_t capinfo, bintval;
	struct ieee80211_rsnparms rsn;
	u_int8_t reason;

	struct ieee80211_frame *wh;
	u_int8_t *frm, *efrm;
	u_int8_t *ssid, *rates, *xrates, *wpa, *wme;
	int reassoc, resp;

	wh = (struct ieee80211_frame *) skb->data;
	frm = (u_int8_t *)&wh[1];
	efrm = skb->data + skb->len;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP ||ic->ic_state != IEEE80211_S_RUN)
	{
		ic->ic_stats.is_rx_mgtdiscard++;
		return;
	}

	if (subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)
	{
		reassoc = 1;
		resp = IEEE80211_FC0_SUBTYPE_REASSOC_RESP;
	}
	else
	{
		reassoc = 0;
		resp = IEEE80211_FC0_SUBTYPE_ASSOC_RESP;
	}
	/*
	 * asreq frame format
	 *	[2] capability information
	 *	[2] listen interval
	 *	[6*] current AP address (reassoc only)
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] extended supported rates
	 *	[tlv] WPA or RSN
	 */
	IEEE80211_VERIFY_LENGTH(efrm - frm, (reassoc ? 10 : 4));
	
	if (!IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_bss->ni_bssid))
	{
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY, wh, ieee80211_mgt_subtype_name[subtype >>IEEE80211_FC0_SUBTYPE_SHIFT], "%s", "wrong bssid");

		ic->ic_stats.is_rx_assoc_bss++;
		return;
	}

	capinfo = le16toh(*(u_int16_t *)frm);
	frm += 2;

	bintval = le16toh(*(u_int16_t *)frm);	
	frm += 2;

	if (reassoc)
		frm += 6;	/* ignore current AP info */

	ssid = rates = xrates = wpa = wme = NULL;
	while (frm < efrm)
	{
		switch (*frm)
		{
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			/* XXX verify only one of RSN and WPA ie's? */
			case IEEE80211_ELEMID_RSN:
				wpa = frm;
				break;
			case IEEE80211_ELEMID_VENDOR:
				if (iswpaoui(frm))
				{
					if (ic->ic_flags & IEEE80211_F_WPA1)
						wpa = frm;
				}
				else if (iswmeinfo(frm))
					wme = frm;
				/* XXX Atheros OUI support */

				break;
		}
		frm += frm[1] + 2;
	}
	
	IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
	IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
	IEEE80211_VERIFY_SSID(ic->ic_bss, ssid);
	
	if ((ic->ic_flags & IEEE80211_F_HIDESSID) && ssid[1] == 0)
	{
		IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT, wh, ieee80211_mgt_subtype_name[subtype >>
			IEEE80211_FC0_SUBTYPE_SHIFT],  "%s", "no ssid with ssid suppression enabled");

		ic->ic_stats.is_rx_ssidmismatch++; /*XXX*/
		return;
	}

	if (ni == ic->ic_bss)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,  "[%s] deny %s request, sta not authenticated\n",
			swjtu_mesh_mac_sprintf(wh->i_addr2), reassoc ? "reassoc" : "assoc");
		
		ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);
		if (ni != NULL)
		{
			IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,  IEEE80211_REASON_ASSOC_NOT_AUTHED);
			ieee80211_free_node(ni);
		}
		ic->ic_stats.is_rx_assoc_notauth++;
		return;
	}
	
	/* assert right association security credentials */
	if (wpa == NULL && (ic->ic_flags & IEEE80211_F_WPA))
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC | IEEE80211_MSG_WPA,
			"[%s] no WPA/RSN IE in association request\n",	swjtu_mesh_mac_sprintf(wh->i_addr2));
		
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,	IEEE80211_REASON_RSN_REQUIRED);
		ieee80211_node_leave(ic, ni);
		/* XXX distinguish WPA/RSN? */
		ic->ic_stats.is_rx_assoc_badwpaie++;
		return;
	}
	
	if (wpa != NULL)
	{
		/*
		 * Parse WPA information element.  Note that
		 * we initialize the param block from the node
		 * state so that information in the IE overrides
		 * our defaults.  The resulting parameters are
		 * installed below after the association is assured.
		 */
		rsn = ni->ni_rsn;
		if (wpa[0] != IEEE80211_ELEMID_RSN)
			reason = _ieee80211_parse_wpa(ic, wpa, &rsn, wh);
		else
			reason = _ieee80211_parse_rsn(ic, wpa, &rsn, wh);
		
		if (reason != 0)
		{
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_DEAUTH, reason);
			ieee80211_node_leave(ic, ni);
			/* XXX distinguish WPA/RSN? */
			ic->ic_stats.is_rx_assoc_badwpaie++;
			return;
		}
		
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC | IEEE80211_MSG_WPA,
			"[%s] %s ie: mc %u/%u uc %u/%u key %u caps 0x%x\n",
			swjtu_mesh_mac_sprintf(wh->i_addr2),   wpa[0] != IEEE80211_ELEMID_RSN ?  "WPA" : "RSN",
			rsn.rsn_mcastcipher, rsn.rsn_mcastkeylen,   rsn.rsn_ucastcipher, rsn.rsn_ucastkeylen,  rsn.rsn_keymgmt, rsn.rsn_caps);
	
	}
	
	/* discard challenge after association */
	if (ni->ni_challenge != NULL)
	{
		kfree(ni->ni_challenge );
		ni->ni_challenge = NULL;
	}
	/* XXX some stations use the privacy bit for handling APs
	       that suport both encrypted and unencrypted traffic */
	/* NB: PRIVACY flag bits are assumed to match */
	if ((capinfo & IEEE80211_CAPINFO_ESS) == 0 ||
	    (capinfo & IEEE80211_CAPINFO_PRIVACY) ^
	    (ic->ic_flags & IEEE80211_F_PRIVACY))
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
		    "[%s] deny %s request, capability mismatch 0x%x\n",
		    swjtu_mesh_mac_sprintf(wh->i_addr2),
		    reassoc ? "reassoc" : "assoc", capinfo);
		IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_CAPINFO);
		ieee80211_node_leave(ic, ni);
		ic->ic_stats.is_rx_assoc_capmismatch++;
		return;
	}
	
	ieee80211_setup_rates(ic, ni, rates, xrates,	IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (ni->ni_rates.rs_nrates == 0)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY, "[%s] deny %s request, rate set mismatch\n",
		    		swjtu_mesh_mac_sprintf(wh->i_addr2), reassoc ? "reassoc" : "assoc");

		IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_BASIC_RATE);
		ieee80211_node_leave(ic, ni);
		ic->ic_stats.is_rx_assoc_norate++;
		return;
	}
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	ni->ni_intval = bintval;
	ni->ni_capinfo = capinfo;
	ni->ni_chan = ic->ic_bss->ni_chan;
	ni->ni_fhdwell = ic->ic_bss->ni_fhdwell;
	ni->ni_fhindex = ic->ic_bss->ni_fhindex;
	
	if (wpa != NULL)
	{
		/*
		 * Record WPA/RSN parameters for station, mark
		 * node as using WPA and record information element
		 * for applications that require it.
		 */
		ni->ni_rsn = rsn;
		_ieee80211_saveie(&ni->ni_wpa_ie, wpa);
	}
	else if (ni->ni_wpa_ie != NULL)
	{
		/*
		 * Flush any state from a previous association.
		 */
		FREE(ni->ni_wpa_ie, M_DEVBUF);
		ni->ni_wpa_ie = NULL;
	}
	
	if (wme != NULL) {
		/*
		 * Record WME parameters for station, mark node
		 * as capable of QoS and record information
		 * element for applications that require it.
		 */
		_ieee80211_saveie(&ni->ni_wme_ie, wme);
		ni->ni_flags |= IEEE80211_NODE_QOS;
	} 
	else if (ni->ni_wme_ie != NULL)
	{
		/*
		 * Flush any state from a previous association.
		 */
		kfree(ni->ni_wme_ie);
		ni->ni_wme_ie = NULL;
		ni->ni_flags &= ~IEEE80211_NODE_QOS;
	}
	
	ieee80211_node_join(ic, ni, resp);
	return ;
//		break;
}

void __probe_request(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni,	int subtype, int rssi, u_int32_t rstamp)
{
	u_int8_t rate;

	struct ieee80211_frame *wh;
	u_int8_t *frm, *efrm;
	u_int8_t *ssid, *rates, *xrates;
	int allocbs;

	wh = (struct ieee80211_frame *) skb->data;
	frm = (u_int8_t *)&wh[1];
	efrm = skb->data + skb->len;


	if (ic->ic_opmode == IEEE80211_M_STA ||ic->ic_state != IEEE80211_S_RUN)
	{
		ic->ic_stats.is_rx_mgtdiscard++;
		return;
	}
	
	if (IEEE80211_IS_MULTICAST(wh->i_addr2))
	{
		/* frame must be directed */
		ic->ic_stats.is_rx_mgtdiscard++;	/* XXX stat */
		return;
	}

	/*
	 * prreq frame format
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] extended supported rates
	 */
	ssid = rates = xrates = NULL;
	while (frm < efrm)
	{
		switch (*frm)
		{
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
		}
		frm += frm[1] + 2;
	}

	IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
	IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
	IEEE80211_VERIFY_SSID(ic->ic_bss, ssid);

	if ((ic->ic_flags & IEEE80211_F_HIDESSID) && ssid[1] == 0)
	{
		IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT, wh, ieee80211_mgt_subtype_name[subtype >>
			IEEE80211_FC0_SUBTYPE_SHIFT], "%s", "no ssid with ssid suppression enabled");
		ic->ic_stats.is_rx_ssidmismatch++; /*XXX*/
		return;
	}

	if (ni == ic->ic_bss)
	{
		if (ic->ic_opmode == IEEE80211_M_IBSS)
		{
			/*
			 * XXX Cannot tell if the sender is operating
			 * in ibss mode.  But we need a new node to
			 * send the response so blindly add them to the
			 * neighbor table.
			 */
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT,  "[%s] creating adhoc node from probe request\n", swjtu_mesh_mac_sprintf(wh->i_addr2));

			ni = ieee80211_fakeup_adhoc_node(&ic->ic_sta, wh->i_addr2);
		}
		else
			ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);

		if (ni == NULL)
			return;
		allocbs = 1;
	}
	else
		allocbs = 0;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "[%s] recv probe req\n", swjtu_mesh_mac_sprintf(wh->i_addr2));
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	
	rate = ieee80211_setup_rates(ic, ni, rates, xrates,  IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE | IEEE80211_F_DONEGO | IEEE80211_F_DODEL);

	if (rate & IEEE80211_RATE_BASIC)
	{
		IEEE80211_DISCARD(ic, IEEE80211_MSG_XRATE,wh, ieee80211_mgt_subtype_name[subtype >> IEEE80211_FC0_SUBTYPE_SHIFT], "%s", "recv'd rate set invalid");
	}
	else
	{
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_PROBE_RESP, 0);
	}
	
	if (allocbs && ic->ic_opmode != IEEE80211_M_IBSS)
	{
		/* reclaim immediately */
		ieee80211_free_node(ni);
	}

	return;
}

void __beacon_probe_response(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni,	int subtype, int rssi, u_int32_t rstamp)
{
	u_int8_t *tstamp, *country;
	u_int8_t chan, bchan, fhindex, erp;
	u_int16_t capinfo, bintval, timoff;
	u_int16_t fhdwell;

	struct ieee80211_frame *wh;
	u_int8_t *frm, *efrm;
	u_int8_t *ssid, *rates, *xrates, *wpa, *wme;

	wh = (struct ieee80211_frame *) skb->data;
	frm = (u_int8_t *)&wh[1];
	efrm = skb->data + skb->len;

	/*
	 * We process beacon/probe response frames:
	 *    o when scanning, or
	 *    o station mode when associated (to collect state
	 *      updates such as 802.11g slot time), or
	 *    o adhoc mode (to discover neighbors)
	 * Frames otherwise received are discarded.
	 */ 
	if (!((ic->ic_flags & IEEE80211_F_SCAN) 
		|| (ic->ic_opmode == IEEE80211_M_STA && ni->ni_associd) 
		|| ic->ic_opmode == IEEE80211_M_IBSS) )
	{
		ic->ic_stats.is_rx_mgtdiscard++;
		return;
	}
	/*
	 * beacon/probe response frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] capability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] country information
	 *	[tlv] parameter set (FH/DS)
	 *	[tlv] erp information
	 *	[tlv] extended supported rates
	 *	[tlv] WME
	 *	[tlv] WPA or RSN
	 */
	IEEE80211_VERIFY_LENGTH(efrm - frm, 12);
	tstamp  = frm;
	frm += 8;
	
	bintval = le16toh(*(u_int16_t *)frm);
	frm += 2;

	capinfo = le16toh(*(u_int16_t *)frm);	
	frm += 2;
	
	ssid = rates = xrates = country = wpa = wme = NULL;
	bchan = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
	chan = bchan;
	fhdwell = 0;
	fhindex = 0;
	erp = 0;
	timoff = 0;

	while (frm < efrm)
	{
		switch (*frm)
		{
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_COUNTRY:
				country = frm;
				break;
			case IEEE80211_ELEMID_FHPARMS:
				if (ic->ic_phytype == IEEE80211_T_FH)
				{
					fhdwell = LE_READ_2(&frm[2]);
					chan = IEEE80211_FH_CHAN(frm[4], frm[5]);
					fhindex = frm[6];
				}
				break;
			case IEEE80211_ELEMID_DSPARMS:
				/*
				 * XXX hack this since depending on phytype
				 * is problematic for multi-mode devices.
				 */
				if (ic->ic_phytype != IEEE80211_T_FH)
					chan = frm[2];
				break;
			case IEEE80211_ELEMID_TIM:
				/* XXX ATIM? */
				timoff = frm - skb->data;
				break;
			case IEEE80211_ELEMID_IBSSPARMS:
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			case IEEE80211_ELEMID_ERP:
				if (frm[1] != 1)
				{
					IEEE80211_DISCARD_IE(ic,  IEEE80211_MSG_ELEMID, wh, "ERP", "bad len %u", frm[1]);
					ic->ic_stats.is_rx_elem_toobig++;
					break;
				}
				
				erp = frm[2];
				break;
			case IEEE80211_ELEMID_RSN:
				wpa = frm;
				break;
			case IEEE80211_ELEMID_VENDOR:
				if (iswpaoui(frm))
					wpa = frm;
				else if (iswmeparam(frm) || iswmeinfo(frm))
					wme = frm;
				/* XXX Atheros OUI support */
				break;
			default:
				IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID, wh, "unhandled", "id %u, len %u", *frm, frm[1]);
				ic->ic_stats.is_rx_elem_unknown++;
				break;
		}
		frm += frm[1] + 2;
	}

	IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
	IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
	if (
#if IEEE80211_CHAN_MAX < 255
	    chan > IEEE80211_CHAN_MAX ||
#endif
	    isclr(ic->ic_chan_active, chan))
	{
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID, wh, ieee80211_mgt_subtype_name[subtype >>
			IEEE80211_FC0_SUBTYPE_SHIFT],"invalid channel %u", chan);
		
		ic->ic_stats.is_rx_badchan++;
		return;
	}

	if (chan != bchan && ic->ic_phytype != IEEE80211_T_FH)
	{
		/*
		 * Frame was received on a channel different from the
		 * one indicated in the DS params element id;
		 * silently discard it.
		 *
		 * NB: this can happen due to signal leakage.
		 *     But we should take it for FH phy because
		 *     the rssi value should be correct even for
		 *     different hop pattern in FH.
		 */
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID, wh, ieee80211_mgt_subtype_name[subtype >>IEEE80211_FC0_SUBTYPE_SHIFT], "for off-channel %u", chan);
		ic->ic_stats.is_rx_chanmismatch++;
		return;
	}

	/*
	 * Count frame now that we know it's to be processed.
	 */
	if (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
	{
		ic->ic_stats.is_rx_beacon++;		/* XXX remove */
		IEEE80211_NODE_STAT(ni, rx_beacons);
	}
	else
		IEEE80211_NODE_STAT(ni, rx_proberesp);

	/*
	 * When operating in station mode, check for state updates.
	 * Be careful to ignore beacons received while doing a
	 * background scan.  We consider only 11g/WMM stuff right now.
	 */
	if (ic->ic_opmode == IEEE80211_M_STA &&
	    ni->ni_associd != 0 &&
	    ((ic->ic_flags & IEEE80211_F_SCAN) == 0 ||
	     IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_bssid)))
	{
		if (ni->ni_erp != erp)
		{
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "[%s] erp change: was 0x%x, now 0x%x\n", swjtu_mesh_mac_sprintf(wh->i_addr2), ni->ni_erp, erp);

			if (erp & IEEE80211_ERP_USE_PROTECTION)
				ic->ic_flags |= IEEE80211_F_USEPROT;
			else
				ic->ic_flags &= ~IEEE80211_F_USEPROT;

			ni->ni_erp = erp;
			/* XXX statistic */
		}

		if ((ni->ni_capinfo ^ capinfo) & IEEE80211_CAPINFO_SHORT_SLOTTIME)
		{
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "[%s] capabilities change: before 0x%x, now 0x%x\n", swjtu_mesh_mac_sprintf(wh->i_addr2), ni->ni_capinfo, capinfo);
			/*
			 * NB: we assume short preamble doesn't
			 *     change dynamically
			 */
			ieee80211_set_shortslottime(ic,
				(ic->ic_curmode == IEEE80211_MODE_11A || 
				 ic->ic_curmode == IEEE80211_MODE_TURBO_A) ||
				(ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
			ni->ni_capinfo = capinfo;
			/* XXX statistic */
		}
		
		if (wme != NULL && _ieee80211_parse_wmeparams(ic, wme, wh) > 0)
			ieee80211_wme_updateparams(ic);
		/* NB: don't need the rest of this */
		if ((ic->ic_flags & IEEE80211_F_SCAN) == 0)
			return;
	}

	if (ni == ic->ic_bss)
	{
#if 0//def IEEE80211_DEBUG
		if (ieee80211_msg_scan(ic))
			dump_probe_beacon(subtype, 1,
			    wh->i_addr2, chan, bchan, capinfo,
			    bintval, erp, ssid, country);
#endif
		/*
		 * Create a new entry.  If scanning the entry goes
		 * in the scan cache.  Otherwise, be particular when
		 * operating in adhoc mode--only take nodes marked
		 * as ibss participants so we don't populate our
		 * neighbor table with unintersting sta's.
		 */
		if ((ic->ic_flags & IEEE80211_F_SCAN) == 0)
		{
			if ((capinfo & IEEE80211_CAPINFO_IBSS) == 0)
				return;
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT, "[%s] creating adhoc node from beacon/probe response\n",  swjtu_mesh_mac_sprintf(wh->i_addr2));

			ni = ieee80211_fakeup_adhoc_node(&ic->ic_sta, wh->i_addr2);
		}
		else
			ni = ieee80211_dup_bss(&ic->ic_scan, wh->i_addr2);

		if (ni == NULL)
			return;

		ni->ni_esslen = ssid[1];
		memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
		memcpy(ni->ni_essid, ssid + 2, ssid[1]);
	}
	else if (ssid[1] != 0 && (ISPROBE(subtype) || ni->ni_esslen == 0))
	{
		/*
		 * Update ESSID at probe response to adopt
		 * hidden AP by Lucent/Cisco, which announces
		 * null ESSID in beacon.
		 */
#if 0//def IEEE80211_DEBUG
		if (ieee80211_msg_scan(ic) ||
		    ieee80211_msg_debug(ic))
			dump_probe_beacon(subtype, 0,
			    wh->i_addr2, chan, bchan, capinfo,
			    bintval, erp, ssid, country);
#endif
		ni->ni_esslen = ssid[1];
		memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
		memcpy(ni->ni_essid, ssid + 2, ssid[1]);
	}

	ni->ni_scangen = ic->ic_scan.nt_scangen;
	IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	memcpy(ni->ni_tstamp.data, tstamp, sizeof(ni->ni_tstamp));
	ni->ni_intval = bintval;
	ni->ni_capinfo = capinfo;
	ni->ni_chan = &ic->ic_channels[chan];
	ni->ni_fhdwell = fhdwell;
	ni->ni_fhindex = fhindex;
	ni->ni_erp = erp;
	/*
	 * Record the byte offset from the mac header to
	 * the start of the TIM information element for
	 * use by hardware and/or to speedup software
	 * processing of beacon frames.
	 */
	ni->ni_timoff = timoff;
	/*
	 * Record optional information elements that might be
	 * used by applications or drivers.
	 */
	if (wme != NULL)
		_ieee80211_saveie(&ni->ni_wme_ie, wme);
	if (wpa != NULL)
		_ieee80211_saveie(&ni->ni_wpa_ie, wpa);

	/* NB: must be after ni_chan is setup */
	ieee80211_setup_rates(ic, ni, rates, xrates, IEEE80211_F_DOSORT);

	return ;
}

void ieee80211_recv_mgmt(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni, int subtype, int rssi, u_int32_t rstamp)
{
	struct ieee80211_frame *wh;
	u_int8_t *frm, *efrm;

	wh = (struct ieee80211_frame *) skb->data;
	frm = (u_int8_t *)&wh[1];
	efrm = skb->data + skb->len;
	
	switch (subtype) 
	{
#if 0//WITH_MESH
		case IEEE80211_FC0_SUBTYPE_ACTION:
			return action_frame_rx(ic, skb, ni, subtype, rssi, rstamp);
#endif

		case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		case IEEE80211_FC0_SUBTYPE_BEACON: 
			return __beacon_probe_response(ic, skb, ni, subtype, rssi, rstamp);

		case IEEE80211_FC0_SUBTYPE_PROBE_REQ: 
			return __probe_request( ic, skb, ni, subtype, rssi, rstamp);

		case IEEE80211_FC0_SUBTYPE_AUTH:
		{
			u_int16_t algo, seq, status;
			/*
			 * auth frame format
			 *	[2] algorithm
			 *	[2] sequence
			 *	[2] status
			 *	[tlv*] challenge
			 */
			IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
			algo   = le16toh(*(u_int16_t *)frm);
			seq    = le16toh(*(u_int16_t *)(frm + 2));
			status = le16toh(*(u_int16_t *)(frm + 4));
			
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,  "[%s] recv auth frame with algorithm %d seq %d\n", swjtu_mesh_mac_sprintf(wh->i_addr2), algo, seq);

#if WITH_WLAN_AUTHBACK_ACL
			/* Consult the ACL policy module if setup. */
			if (ic->ic_acl != NULL && !ic->ic_acl->iac_check(ic, wh->i_addr2))
			{
				IEEE80211_DISCARD(ic, IEEE80211_MSG_ACL, wh, "auth", "%s", "disallowed by ACL");
				ic->ic_stats.is_rx_acl++;
				return;
			}
#endif

			if (ic->ic_flags & IEEE80211_F_COUNTERM)
			{
				IEEE80211_DISCARD(ic, IEEE80211_MSG_AUTH | IEEE80211_MSG_CRYPTO, wh, "auth", "%s", "TKIP countermeasures enabled");

				ic->ic_stats.is_rx_auth_countermeasures++;
				if (ic->ic_opmode == IEEE80211_M_HOSTAP)
				{
					IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_AUTH, IEEE80211_REASON_MIC_FAILURE);
				}
				return;
			}
			
			if (algo == IEEE80211_AUTH_ALG_SHARED)
				_ieee80211_auth_shared(ic, wh, frm + 6, efrm, ni, rssi,  rstamp, seq, status);
			else if (algo == IEEE80211_AUTH_ALG_OPEN)
				_ieee80211_auth_open(ic, wh, ni, rssi, rstamp, seq, status);
			else
			{
				IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY, wh, "auth", "unsupported alg %d", algo);
				ic->ic_stats.is_rx_auth_unsupported++;
				
				if (ic->ic_opmode == IEEE80211_M_HOSTAP)
				{
					/* XXX not right */
					IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_AUTH, (seq+1) | (IEEE80211_STATUS_ALG<<16));
				}
				return;
			} 
			break;
		}

		case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
		case IEEE80211_FC0_SUBTYPE_REASSOC_REQ: 
			return __associate_request(ic, skb, ni, subtype, rssi, rstamp);

		case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
		case IEEE80211_FC0_SUBTYPE_REASSOC_RESP: 
			return __associate_response(ic, skb, ni, subtype, rssi, rstamp);

		case IEEE80211_FC0_SUBTYPE_DEAUTH:
		{
			u_int16_t reason;

			if (ic->ic_state == IEEE80211_S_SCAN)
			{
				ic->ic_stats.is_rx_mgtdiscard++;
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH, "[%s] station DEAUTH in bad state.\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr));
				return;
			}
			/*
			 * deauth frame format
			 *	[2] reason
			 */
			IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
			reason = le16toh(*(u_int16_t *)frm);
			ic->ic_stats.is_rx_deauth++;
			IEEE80211_NODE_STAT(ni, rx_deauth);
			
			switch (ic->ic_opmode)
			{
				case IEEE80211_M_STA:
					ieee80211_new_state(ic, IEEE80211_S_AUTH,
					    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
					break;
				case IEEE80211_M_HOSTAP:
					if (ni != ic->ic_bss)
					{
						IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH, "station %s deauthenticated by peer (reason %d)\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr), reason);
						ieee80211_node_leave(ic, ni);
					}
					break;
				default:
					ic->ic_stats.is_rx_mgtdiscard++;
					break;
			}
			break;
		}

		case IEEE80211_FC0_SUBTYPE_DISASSOC:
		{
			u_int16_t reason;

			if (ic->ic_state != IEEE80211_S_RUN && ic->ic_state != IEEE80211_S_AUTH)
			{		// TODO: IEEE80211_S_ASSOC?
				ic->ic_stats.is_rx_mgtdiscard++;
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "[%s] station DISASSOC in bad state.\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr));
				return;
			}
			/*
			 * disassoc frame format
			 *	[2] reason
			 */
			IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
			reason = le16toh(*(u_int16_t *)frm);
			ic->ic_stats.is_rx_disassoc++;
			IEEE80211_NODE_STAT(ni, rx_disassoc);
			switch (ic->ic_opmode)
			{
				case IEEE80211_M_STA:
					ieee80211_new_state(ic, IEEE80211_S_ASSOC, wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
					break;
				case IEEE80211_M_HOSTAP:
					if (ni != ic->ic_bss)
					{
						IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "[%s] sta disassociated by peer (reason %d)\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr), reason);
						ieee80211_node_leave(ic, ni);
					}
					break;
				default:
					ic->ic_stats.is_rx_mgtdiscard++;
					break;
			}
			break;
		}
		
		default:
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY,   wh, "mgt", "subtype 0x%x not handled", subtype);
			ic->ic_stats.is_rx_badsubtype++;
			break;
	}
}

#undef ISREASSOC
#undef ISPROBE
#undef IEEE80211_VERIFY_LENGTH
#undef IEEE80211_VERIFY_ELEMENT


int _ieee80211_input_mgmt(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
	struct ieee80211_frame *wh;
	struct ieee80211_key *key;
	int hdrsize;
	u_int8_t dir, type, subtype;

	wh = (struct ieee80211_frame *)skb->data;
	hdrsize = ieee80211_hdrsize(wh);
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	IEEE80211_NODE_STAT(ni, rx_mgmt);
	if (dir != IEEE80211_FC1_DIR_NODS)
	{
		ic->ic_stats.is_rx_wrongdir++;
		goto err;
	}
	
	if (skb->len < sizeof(struct ieee80211_frame))
	{
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY, ni->ni_macaddr, "mgt too short: len", "%u", skb->len);
		ic->ic_stats.is_rx_tooshort++;
		goto out;
	}
	
#if 0//def IEEE80211_DEBUG
	if ((ieee80211_msg_debug(ic) && doprint(ic, subtype)) ||ieee80211_msg_dumppkts(ic))
	{
		mdev_printf(ic->ic_dev, "received %s from %s rssi %d\n", ieee80211_mgt_subtype_name[subtype >>
			IEEE80211_FC0_SUBTYPE_SHIFT], swjtu_mesh_mac_sprintf(wh->i_addr2), rssi);
	}
#endif

	if (wh->i_fc[1] & IEEE80211_FC1_WEP)
	{
		if (subtype != IEEE80211_FC0_SUBTYPE_AUTH)
		{
			/*
			 * Only shared key auth frames with a challenge
			 * should be decrypted, discard all others.
			 */
			IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,	wh, ieee80211_mgt_subtype_name[subtype >>IEEE80211_FC0_SUBTYPE_SHIFT],  "%s", "WEP set but not permitted");
			ic->ic_stats.is_rx_mgtdiscard++; /* XXX */
			goto out;
		}
		
		if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0)
		{
			/*
			 * Discard encrypted frames when privacy is off.
			 */
			IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,  wh, "mgt", "%s", "WEP set but PRIVACY off");
			ic->ic_stats.is_rx_noprivacy++;
			goto out;
		}
		
		key = ieee80211_crypto_decap(ic, ni, skb);
		if (key == NULL)
		{
			/* NB: stats+msgs handled in crypto_decap */
			goto out;
		}
		wh = (struct ieee80211_frame *)skb->data;
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		
	}

	/* call ieee80211_recv_mgmt */
	(*ic->ic_recv_mgmt)(ic, skb, ni, subtype, rssi, rstamp);
	dev_kfree_skb(skb);
	
	return type;
err:
	ic->ic_devstats->rx_errors++;
out:
	if (skb != NULL)
		dev_kfree_skb(skb);
	return type;

}

