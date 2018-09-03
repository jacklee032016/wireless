/*
* $Id: wlan_output_wpa.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_output.h"

#define	WPA_OUI_BYTES		0x00, 0x50, 0xf2
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDSELECTOR(frm, sel) do {		\
	memcpy(frm, sel, 4);			\
	frm += 4;				\
} while (0)

static u_int8_t * __ieee80211_setup_wpa_ie(struct ieee80211com *ic, u_int8_t *ie)
{
	static const u_int8_t oui[4] = { WPA_OUI_BYTES, WPA_OUI_TYPE };
	static const u_int8_t cipher_suite[][4] = {
		{ WPA_OUI_BYTES, WPA_CSE_WEP40 },	/* NB: 40-bit */
		{ WPA_OUI_BYTES, WPA_CSE_TKIP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX WRAP */
		{ WPA_OUI_BYTES, WPA_CSE_CCMP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX CKIP */
		{ WPA_OUI_BYTES, WPA_CSE_NULL },
	};
	static const u_int8_t wep104_suite[4] =
		{ WPA_OUI_BYTES, WPA_CSE_WEP104 };
	static const u_int8_t key_mgt_unspec[4] =
		{ WPA_OUI_BYTES, WPA_ASE_8021X_UNSPEC };
	static const u_int8_t key_mgt_psk[4] =
		{ WPA_OUI_BYTES, WPA_ASE_8021X_PSK };
	const struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	u_int8_t *frm = ie;
	u_int8_t *selcnt;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 0;				/* length filled in below */
	memcpy(frm, oui, sizeof(oui));		/* WPA OUI */
	frm += sizeof(oui);
	ADDSHORT(frm, WPA_VERSION);

	/* XXX filter out CKIP */

	/* multicast cipher */
	if (rsn->rsn_mcastcipher == IEEE80211_CIPHER_WEP &&
	    rsn->rsn_mcastkeylen >= 13)
		ADDSELECTOR(frm, wep104_suite);
	else
		ADDSELECTOR(frm, cipher_suite[rsn->rsn_mcastcipher]);

	/* unicast cipher list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_AES_CCM)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	}
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_TKIP)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
	}

	/* authenticator selector list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_UNSPEC) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_unspec);
	}
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_PSK) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_psk);
	}

	/* optional capabilities */
	if (rsn->rsn_caps != 0)
		ADDSHORT(frm, rsn->rsn_caps);

	/* calculate element length */
	ie[1] = frm - ie - 2;
	KASSERT(ie[1]+2 <= (int)sizeof(struct ieee80211_ie_wpa),
		("WPA IE too big, %u > %zu",
		ie[1]+2, sizeof(struct ieee80211_ie_wpa)));
	return frm;
}
#undef ADDSHORT
#undef ADDSELECTOR
#undef WPA_OUI_BYTES


#define	RSN_OUI_BYTES		0x00, 0x0f, 0xac
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDSELECTOR(frm, sel) do {		\
	memcpy(frm, sel, 4);			\
	frm += 4;				\
} while (0)

static u_int8_t * __ieee80211_setup_rsn_ie(struct ieee80211com *ic, u_int8_t *ie)
{
	static const u_int8_t cipher_suite[][4] = {
		{ RSN_OUI_BYTES, RSN_CSE_WEP40 },	/* NB: 40-bit */
		{ RSN_OUI_BYTES, RSN_CSE_TKIP },
		{ RSN_OUI_BYTES, RSN_CSE_WRAP },
		{ RSN_OUI_BYTES, RSN_CSE_CCMP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX CKIP */
		{ RSN_OUI_BYTES, RSN_CSE_NULL },
	};
	static const u_int8_t wep104_suite[4] =
		{ RSN_OUI_BYTES, RSN_CSE_WEP104 };
	static const u_int8_t key_mgt_unspec[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_UNSPEC };
	static const u_int8_t key_mgt_psk[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_PSK };
	const struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	u_int8_t *frm = ie;
	u_int8_t *selcnt;

	*frm++ = IEEE80211_ELEMID_RSN;
	*frm++ = 0;				/* length filled in below */
	ADDSHORT(frm, RSN_VERSION);

	/* XXX filter out CKIP */

	/* multicast cipher */
	if (rsn->rsn_mcastcipher == IEEE80211_CIPHER_WEP &&
	    rsn->rsn_mcastkeylen >= 13)
		ADDSELECTOR(frm, wep104_suite);
	else
		ADDSELECTOR(frm, cipher_suite[rsn->rsn_mcastcipher]);

	/* unicast cipher list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_AES_CCM)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	}
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_TKIP)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
	}

	/* authenticator selector list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_UNSPEC) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_unspec);
	}
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_PSK) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_psk);
	}

	/* optional capabilities */
	if (rsn->rsn_caps != 0)
		ADDSHORT(frm, rsn->rsn_caps);
	/* XXX PMKID */

	/* calculate element length */
	ie[1] = frm - ie - 2;
	KASSERT(ie[1]+2 <= (int)sizeof(struct ieee80211_ie_wpa),
		("RSN IE too big, %u > %zu",
		ie[1]+2, sizeof(struct ieee80211_ie_wpa)));
	return frm;
}
#undef ADDSELECTOR
#undef ADDSHORT
#undef RSN_OUI_BYTES

/* Add a WPA/RSN element to a frame. */
u_int8_t *_ieee80211_add_wpa(u_int8_t *frm, struct ieee80211com *ic)
{

	KASSERT(ic->ic_flags & IEEE80211_F_WPA, ("no WPA/RSN!"));
	if (ic->ic_flags & IEEE80211_F_WPA2)
		frm = __ieee80211_setup_rsn_ie(ic, frm);
	if (ic->ic_flags & IEEE80211_F_WPA1)
		frm = __ieee80211_setup_wpa_ie(ic, frm);
	return frm;
}

