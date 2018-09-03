/*
* $Id: wlan_output_misc.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_output.h"

/*
 * Add a supported rates element id to a frame.
 */
u_int8_t *_ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = rs->rs_nrates;
	if (nrates > IEEE80211_RATE_SIZE)
		nrates = IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	return frm + nrates;
}

/*
 * Add an extended supported rates element id to a frame.
 */
u_int8_t *_ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	/*
	 * Add an extended supported rates element if operating in 11g mode.
	 */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		int nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
		*frm++ = IEEE80211_ELEMID_XRATES;
		*frm++ = nrates;
		memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
		frm += nrates;
	}
	return frm;
}

/* 
 * Add an ssid elemet to a frame.
 */
u_int8_t *_ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return frm + len;
}

/*
 * Add an erp element to a frame.
 */
u_int8_t *_ieee80211_add_erp(u_int8_t *frm, struct ieee80211com *ic)
{
	u_int8_t erp;

	*frm++ = IEEE80211_ELEMID_ERP;
	*frm++ = 1;
	erp = 0;
	if (ic->ic_nonerpsta != 0)
		erp |= IEEE80211_ERP_NON_ERP_PRESENT;
	if (ic->ic_flags & IEEE80211_F_USEPROT)
		erp |= IEEE80211_ERP_USE_PROTECTION;
	if (ic->ic_flags & IEEE80211_F_USEBARKER)
		erp |= IEEE80211_ERP_LONG_PREAMBLE;
	*frm++ = erp;
	return frm;
}



#define	WME_OUI_BYTES		0x00, 0x50, 0xf2
/*
 * Add a WME information element to a frame.
 */
u_int8_t *_ieee80211_add_wme_info(u_int8_t *frm, struct ieee80211_wme_state *wme)
{
	static const struct ieee80211_wme_info info = {
		.wme_id		= IEEE80211_ELEMID_VENDOR,
		.wme_len	= sizeof(struct ieee80211_wme_info) - 2,
		.wme_oui	= { WME_OUI_BYTES },
		.wme_type	= WME_OUI_TYPE,
		.wme_subtype	= WME_INFO_OUI_SUBTYPE,
		.wme_version	= WME_VERSION,
		.wme_info	= 0,
	};
	memcpy(frm, &info, sizeof(info));
	return frm + sizeof(info); 
}

/*
 * Add a WME parameters element to a frame.
 */
#define	SM(_v, _f)	(((_v) << _f##_S) & _f)
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)

u_int8_t *_ieee80211_add_wme_param(u_int8_t *frm, struct ieee80211_wme_state *wme)
{
	/* NB: this works 'cuz a param has an info at the front */
	static const struct ieee80211_wme_info param = {
		.wme_id		= IEEE80211_ELEMID_VENDOR,
		.wme_len	= sizeof(struct ieee80211_wme_param) - 2,
		.wme_oui	= { WME_OUI_BYTES },
		.wme_type	= WME_OUI_TYPE,
		.wme_subtype	= WME_PARAM_OUI_SUBTYPE,
		.wme_version	= WME_VERSION,
	};
	int i;

	memcpy(frm, &param, sizeof(param));
	frm += offsetof(struct ieee80211_wme_info, wme_info);
	*frm++ = wme->wme_bssChanParams.cap_info;	/* AC info */
	*frm++ = 0;					/* reserved field */
	for (i = 0; i < WME_NUM_AC; i++) {
		const struct wmeParams *ac =
		       &wme->wme_bssChanParams.cap_wmeParams[i];
		*frm++ = SM(i, WME_PARAM_ACI)
		       | SM(ac->wmep_acm, WME_PARAM_ACM)
		       | SM(ac->wmep_aifsn, WME_PARAM_AIFSN)
		       ;
		*frm++ = SM(ac->wmep_logcwmax, WME_PARAM_LOGCWMAX)
		       | SM(ac->wmep_logcwmin, WME_PARAM_LOGCWMIN)
		       ;
		ADDSHORT(frm, ac->wmep_txopLimit);
	}
	return frm;
}


