/*
* $Id: ieee80211_input.h,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
*/

#ifndef  __IEEE80211_INPUT_H__
#define __IEEE80211_INPUT_H__

#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/* IEEE 802.11 input handling */
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/if_vlan.h>

#include "if_llc.h"
#include "if_ethersubr.h"
#include "if_media.h"

#include <ieee80211_var.h>

#ifdef IEEE80211_DEBUG

/*
 * Decide if a received management frame should be
 * printed when debugging is enabled.  This filters some
 * of the less interesting frames that come frequently
 * (e.g. beacons).
 */
static __inline int doprint(struct ieee80211com *ic, int subtype)
{
	switch (subtype)
	{
		case IEEE80211_FC0_SUBTYPE_BEACON:
			return (ic->ic_flags & IEEE80211_F_SCAN);
		case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
			return (ic->ic_opmode == IEEE80211_M_IBSS);
	}
	return 1;
}

/*
 * Emit a debug message about discarding a frame or information
 * element.  One format is for extracting the mac address from
 * the frame header; the other is for when a header is not
 * available or otherwise appropriate.
 */
#define	IEEE80211_DISCARD(_ic, _m, _wh, _type, _fmt, ...) do {		\
	if ((_ic)->ic_debug & (_m))					\
		ieee80211_discard_frame(_ic, _wh, _type, _fmt, __VA_ARGS__);\
} while (0)
#define	IEEE80211_DISCARD_IE(_ic, _m, _wh, _type, _fmt, ...) do {	\
	if ((_ic)->ic_debug & (_m))					\
		ieee80211_discard_ie(_ic, _wh, _type, _fmt, __VA_ARGS__);\
} while (0)
#define	IEEE80211_DISCARD_MAC(_ic, _m, _mac, _type, _fmt, ...) do {	\
	if ((_ic)->ic_debug & (_m))					\
		ieee80211_discard_mac(_ic, _mac, _type, _fmt, __VA_ARGS__);\
} while (0)

const u_int8_t *ieee80211_getbssid(struct ieee80211com *,	const struct ieee80211_frame *);
void ieee80211_discard_frame(struct ieee80211com *,
	const struct ieee80211_frame *, const char *type, const char *fmt, ...);
void ieee80211_discard_ie(struct ieee80211com *,
	const struct ieee80211_frame *, const char *type, const char *fmt, ...);
void ieee80211_discard_mac(struct ieee80211com *,
	const u_int8_t mac[IEEE80211_ADDR_LEN], const char *type,
	const char *fmt, ...);
#else
#define	IEEE80211_DISCARD(_ic, _m, _wh, _type, _fmt, ...)
#define	IEEE80211_DISCARD_IE(_ic, _m, _wh, _type, _fmt, ...)
#define	IEEE80211_DISCARD_MAC(_ic, _m, _mac, _type, _fmt, ...)
#endif /* IEEE80211_DEBUG */



#ifdef IEEE80211_DEBUG

#define	IEEE80211_VERIFY_SSID(_ni, _ssid) do {				\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {	\
		if (ieee80211_msg_input(ic))				\
			ieee80211_ssid_mismatch(ic, 			\
			    ieee80211_mgt_subtype_name[subtype >>	\
				IEEE80211_FC0_SUBTYPE_SHIFT],		\
				wh->i_addr2, _ssid);			\
		ic->ic_stats.is_rx_ssidmismatch++;			\
		return;							\
	}								\
} while (0)
#else /* !IEEE80211_DEBUG */
#define	IEEE80211_VERIFY_SSID(_ni, _ssid) do {				\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {	\
		ic->ic_stats.is_rx_ssidmismatch++;			\
		return;							\
	}								\
} while (0)
#endif /* !IEEE80211_DEBUG */

/* unalligned little endian access */     
#define LE_READ_2(p)					\
	((u_int16_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)					\
	((u_int32_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8) |		\
	  (((const u_int8_t *)(p))[2] << 16) |		\
	  (((const u_int8_t *)(p))[3] << 24)))

static __inline int iswpaoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}

static __inline int iswmeoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI);
}


static __inline int iswmeinfo(const u_int8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_INFO_OUI_SUBTYPE;
}

static __inline int isatherosoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI);
}


void ieee80211_auth_open(struct ieee80211com *ic, struct ieee80211_frame *wh,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp, u_int16_t seq,
    u_int16_t status);
void ieee80211_auth_shared(struct ieee80211com *ic, struct ieee80211_frame *wh,
    u_int8_t *frm, u_int8_t *efrm, struct ieee80211_node *ni, int rssi,
    u_int32_t rstamp, u_int16_t seq, u_int16_t status);


#ifdef IEEE80211_DEBUG
void ieee80211_ssid_mismatch(struct ieee80211com *ic, const char *tag,
	u_int8_t mac[IEEE80211_ADDR_LEN], u_int8_t *ssid);
#endif
int ieee80211_parse_wpa(struct ieee80211com *ic, u_int8_t *frm,
	struct ieee80211_rsnparms *rsn, const struct ieee80211_frame *wh);
int ieee80211_parse_rsn(struct ieee80211com *ic, u_int8_t *frm,
	struct ieee80211_rsnparms *rsn, const struct ieee80211_frame *wh);
int ieee80211_parse_wmeparams(struct ieee80211com *ic, u_int8_t *frm,
	const struct ieee80211_frame *wh);
void ieee80211_saveie(u_int8_t **iep, const u_int8_t *ie);


int ieee80211_setup_rates(struct ieee80211com *ic, struct ieee80211_node *ni,
	u_int8_t *rates, u_int8_t *xrates, int flags);
#ifdef IEEE80211_DEBUG
void dump_probe_beacon(u_int8_t subtype, int isnew,
	const u_int8_t mac[IEEE80211_ADDR_LEN],
	u_int8_t chan, u_int8_t bchan, u_int16_t capinfo, u_int16_t bintval,
	u_int8_t erp, u_int8_t *ssid, u_int8_t *country);
#endif
void ieee80211_node_pwrsave(struct ieee80211_node *ni, int enable);
void ieee80211_recv_pspoll(struct ieee80211com *ic,
	struct ieee80211_node *ni, struct sk_buff *skb0);

int ieee80211_input_data(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni, int rssi, u_int32_t rstamp);
int ieee80211_input_mgmt(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni, int rssi, u_int32_t rstamp);

#endif

