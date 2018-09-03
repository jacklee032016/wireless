/*
* $Id: wlan_input_mgmt_utils.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_input.h"

#ifdef IEEE80211_DEBUG
void ieee80211_ssid_mismatch(struct ieee80211com *ic, const char *tag,
	u_int8_t mac[IEEE80211_ADDR_LEN], u_int8_t *ssid)
{
	printf("[%s] discard %s frame, ssid mismatch: ", swjtu_mesh_mac_sprintf(mac), tag);
	ieee80211_print_essid(ssid + 2, ssid[1]);
	printf("\n");
}
#endif
/*
 * Convert a WPA cipher selector OUI to an internal
 * cipher algorithm.  Where appropriate we also
 * record any key length.
 */
static int __wpa_cipher(u_int8_t *sel, u_int8_t *keylen)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w)
	{
		case WPA_SEL(WPA_CSE_NULL):
			return IEEE80211_CIPHER_NONE;
		case WPA_SEL(WPA_CSE_WEP40):
			if (keylen)
				*keylen = 40 / NBBY;
			return IEEE80211_CIPHER_WEP;
		case WPA_SEL(WPA_CSE_WEP104):
			if (keylen)
				*keylen = 104 / NBBY;
			return IEEE80211_CIPHER_WEP;
		case WPA_SEL(WPA_CSE_TKIP):
			return IEEE80211_CIPHER_TKIP;
		case WPA_SEL(WPA_CSE_CCMP):
			return IEEE80211_CIPHER_AES_CCM;
	}
	return 32;		/* NB: so 1<< is discarded */
	
#undef WPA_SEL
}

/*
 * Convert a WPA key management/authentication algorithm
 * to an internal code.
 */
static int __wpa_keymgmt(u_int8_t *sel)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w)
	{
		case WPA_SEL(WPA_ASE_8021X_UNSPEC):
			return WPA_ASE_8021X_UNSPEC;
		case WPA_SEL(WPA_ASE_8021X_PSK):
			return WPA_ASE_8021X_PSK;
		case WPA_SEL(WPA_ASE_NONE):
			return WPA_ASE_NONE;
	}
	return 0;		/* NB: so is discarded */
#undef WPA_SEL
}

/*
 * Parse a WPA information element to collect parameters
 * and validate the parameters against what has been
 * configured for the system.
 */
int _ieee80211_parse_wpa(struct ieee80211com *ic, u_int8_t *frm,
	struct ieee80211_rsnparms *rsn, const struct ieee80211_frame *wh)
{
	u_int8_t len = frm[1];
	u_int32_t w;
	int n;

	/*
	 * Check the length once for fixed parts: OUI, type,
	 * version, mcast cipher, and 2 selector counts.
	 * Other, variable-length data, must be checked separately.
	 */
	KASSERT(ic->ic_flags & IEEE80211_F_WPA1, ("not WPA, flags 0x%x", ic->ic_flags));

	if (len < 14)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "WPA", "too short, len %u", len);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 6, len -= 4;		/* NB: len is payload only */
	/* NB: iswapoui already validated the OUI and type */

	w = LE_READ_2(frm);
	if (w != WPA_VERSION)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,  wh, "WPA", "bad version %u", w);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 2, len -= 2;

	/* multicast/group cipher */
	w = __wpa_cipher(frm, &rsn->rsn_mcastkeylen);
	if (w != rsn->rsn_mcastcipher)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,  wh, "WPA", "mcast cipher mismatch; got %u, expected %u",  w, rsn->rsn_mcastcipher);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 4, len -= 4;

	/* unicast ciphers */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4+2)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,  wh, "WPA", "ucast cipher data too short; len %u, n %u",  len, n);
		return IEEE80211_REASON_IE_INVALID;
	}
	
	w = 0;
	for (; n > 0; n--)
	{
		w |= 1<<__wpa_cipher(frm, &rsn->rsn_ucastkeylen);
		frm += 4, len -= 4;
	}

	w &= rsn->rsn_ucastcipherset;
	if (w == 0)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "WPA", "%s", "ucast cipher set empty");
		return IEEE80211_REASON_IE_INVALID;
	}
	
	if (w & (1<<IEEE80211_CIPHER_TKIP))
		rsn->rsn_ucastcipher = IEEE80211_CIPHER_TKIP;
	else
		rsn->rsn_ucastcipher = IEEE80211_CIPHER_AES_CCM;

	/* key management algorithms */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "WPA", "key mgmt alg data too short; len %u, n %u", len, n);
		return IEEE80211_REASON_IE_INVALID;
	}
	
	w = 0;
	for (; n > 0; n--)
	{
		w |= __wpa_keymgmt(frm);
		frm += 4, len -= 4;
	}

	w &= rsn->rsn_keymgmtset;
	if (w == 0)
	{
		IEEE80211_DISCARD_IE(ic,IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "WPA", "%s", "no acceptable key mgmt alg");
		return IEEE80211_REASON_IE_INVALID;
	}
	
	if (w & WPA_ASE_8021X_UNSPEC)
		rsn->rsn_keymgmt = WPA_ASE_8021X_UNSPEC;
	else
		rsn->rsn_keymgmt = WPA_ASE_8021X_PSK;

	if (len > 2)		/* optional capabilities */
		rsn->rsn_caps = LE_READ_2(frm);

	return 0;
}

/*
 * Convert an RSN cipher selector OUI to an internal
 * cipher algorithm.  Where appropriate we also
 * record any key length.
 */
static int __rsn_cipher(u_int8_t *sel, u_int8_t *keylen)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w)
	{
		case RSN_SEL(RSN_CSE_NULL):
			return IEEE80211_CIPHER_NONE;
		case RSN_SEL(RSN_CSE_WEP40):
			if (keylen)
				*keylen = 40 / NBBY;
			return IEEE80211_CIPHER_WEP;
		case RSN_SEL(RSN_CSE_WEP104):
			if (keylen)
				*keylen = 104 / NBBY;
			return IEEE80211_CIPHER_WEP;
		case RSN_SEL(RSN_CSE_TKIP):
			return IEEE80211_CIPHER_TKIP;
		case RSN_SEL(RSN_CSE_CCMP):
			return IEEE80211_CIPHER_AES_CCM;
		case RSN_SEL(RSN_CSE_WRAP):
			return IEEE80211_CIPHER_AES_OCB;
	}
	return 32;		/* NB: so 1<< is discarded */
#undef WPA_SEL
}

/*
 * Convert an RSN key management/authentication algorithm
 * to an internal code.
 */
static int __rsn_keymgmt(u_int8_t *sel)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w)
	{
		case RSN_SEL(RSN_ASE_8021X_UNSPEC):
			return RSN_ASE_8021X_UNSPEC;
		case RSN_SEL(RSN_ASE_8021X_PSK):
			return RSN_ASE_8021X_PSK;
		case RSN_SEL(RSN_ASE_NONE):
			return RSN_ASE_NONE;
	}
	return 0;		/* NB: so is discarded */
#undef RSN_SEL
}

/*
 * Parse a WPA/RSN information element to collect parameters
 * and validate the parameters against what has been
 * configured for the system.
 */
int _ieee80211_parse_rsn(struct ieee80211com *ic, u_int8_t *frm,
	struct ieee80211_rsnparms *rsn, const struct ieee80211_frame *wh)
{
	u_int8_t len = frm[1];
	u_int32_t w;
	int n;

	/*
	 * Check the length once for fixed parts: 
	 * version, mcast cipher, and 2 selector counts.
	 * Other, variable-length data, must be checked separately.
	 */
	KASSERT(ic->ic_flags & IEEE80211_F_WPA2, ("not RSN, flags 0x%x", ic->ic_flags));

	if (len < 10)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "RSN", "too short, len %u", len);
		return IEEE80211_REASON_IE_INVALID;
	}
	
	frm += 2;		/* skip id+len */
	w = LE_READ_2(frm);
	if (w != RSN_VERSION)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "RSN", "bad version %u", w);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 2, len -= 2;

	/* multicast/group cipher */
	w = __rsn_cipher(frm, &rsn->rsn_mcastkeylen);
	if (w != rsn->rsn_mcastcipher)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "RSN", "mcast cipher mismatch; got %u, expected %u", w, rsn->rsn_mcastcipher);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 4, len -= 4;

	/* unicast ciphers */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;

	if (len < n*4+2)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "RSN", "ucast cipher data too short; len %u, n %u",  len, n);
		return IEEE80211_REASON_IE_INVALID;
	}

	w = 0;
	for (; n > 0; n--)
	{
		w |= 1<<__rsn_cipher(frm, &rsn->rsn_ucastkeylen);
		frm += 4, len -= 4;
	}

	w &= rsn->rsn_ucastcipherset;
	if (w == 0)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,  wh, "RSN", "%s", "ucast cipher set empty");
		return IEEE80211_REASON_IE_INVALID;
	}

	if (w & (1<<IEEE80211_CIPHER_TKIP))
		rsn->rsn_ucastcipher = IEEE80211_CIPHER_TKIP;
	else
		rsn->rsn_ucastcipher = IEEE80211_CIPHER_AES_CCM;

	/* key management algorithms */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "RSN", "key mgmt alg data too short; len %u, n %u", len, n);
		return IEEE80211_REASON_IE_INVALID;
	}

	w = 0;
	for (; n > 0; n--)
	{
		w |= __rsn_keymgmt(frm);
		frm += 4, len -= 4;
	}

	w &= rsn->rsn_keymgmtset;
	if (w == 0)
	{
		IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA, wh, "RSN", "%s", "no acceptable key mgmt alg");
		return IEEE80211_REASON_IE_INVALID;
	}
	
	if (w & RSN_ASE_8021X_UNSPEC)
		rsn->rsn_keymgmt = RSN_ASE_8021X_UNSPEC;
	else
		rsn->rsn_keymgmt = RSN_ASE_8021X_PSK;

	/* optional RSN capabilities */
	if (len > 2)
		rsn->rsn_caps = LE_READ_2(frm);
	/* XXXPMKID */

	return 0;
}

int _ieee80211_parse_wmeparams(struct ieee80211com *ic, u_int8_t *frm,	const struct ieee80211_frame *wh)
{
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)

	struct ieee80211_wme_state *wme = &ic->ic_wme;
	u_int len = frm[1], qosinfo;
	int i;

	if (len < sizeof(struct ieee80211_wme_param)-2)
	{
		IEEE80211_DISCARD_IE(ic,IEEE80211_MSG_ELEMID | IEEE80211_MSG_WME,wh, "WME", "too short, len %u", len);
		return -1;
	}
	
	qosinfo = frm[ offsetof(struct ieee80211_wme_param, param_qosInfo) ];
	qosinfo &= WME_QOSINFO_COUNT;
	/* XXX do proper check for wraparound */
	if (qosinfo == wme->wme_wmeChanParams.cap_info)
		return 0;
	frm += offsetof(struct ieee80211_wme_param, params_acParams);
	for (i = 0; i < WME_NUM_AC; i++)
	{
		struct wmeParams *wmep =
			&wme->wme_wmeChanParams.cap_wmeParams[i];
		/* NB: ACI not used */
		wmep->wmep_acm = MS(frm[0], WME_PARAM_ACM);
		wmep->wmep_aifsn = MS(frm[0], WME_PARAM_AIFSN);
		wmep->wmep_logcwmin = MS(frm[1], WME_PARAM_LOGCWMIN);
		wmep->wmep_logcwmax = MS(frm[1], WME_PARAM_LOGCWMAX);
		wmep->wmep_txopLimit = LE_READ_2(frm+2);
		frm += 4;
	}
	wme->wme_wmeChanParams.cap_info = qosinfo;
	return 1;
#undef MS
}

void _ieee80211_saveie(u_int8_t **iep, const u_int8_t *ie)
{
	u_int ielen = ie[1]+2;
	/*
	 * Record information element for later use.
	 */
	if (*iep == NULL || (*iep)[1] != ie[1])
	{
		if (*iep != NULL)
			kfree(*iep);
		MALLOC(*iep, void*, ielen, M_DEVBUF, M_NOWAIT);
	}

	if (*iep != NULL)
		memcpy(*iep, ie, ielen);
	/* XXX note failure */
}

