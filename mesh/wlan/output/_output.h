/*
* $Id: _output.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef	___OUTPUT_H__
#define	___OUTPUT_H__

#include "wlan.h"

#ifdef IEEE80211_DEBUG
/*
 * Decide if an outbound management frame should be
 * printed when debugging is enabled.  This filters some
 * of the less interesting frames that come frequently
 * (e.g. beacons).
 */
static inline int output_doprint(struct ieee80211com *ic, int subtype)
{
	switch (subtype)
	{
		case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
			return (ic->ic_opmode == IEEE80211_M_IBSS);
	}
	return 1;
}
#endif

struct sk_buff *_ieee80211_getmgtframe(u_int8_t **frm, u_int pktlen);

u_int8_t *_ieee80211_add_wpa(u_int8_t *frm, struct ieee80211com *ic);

u_int8_t *_ieee80211_add_erp(u_int8_t *frm, struct ieee80211com *ic);
u_int8_t *_ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len);
u_int8_t *_ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs);
u_int8_t *_ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs);

u_int8_t *_ieee80211_add_wme_info(u_int8_t *frm, struct ieee80211_wme_state *wme);
u_int8_t *_ieee80211_add_wme_param(u_int8_t *frm, struct ieee80211_wme_state *wme);

#endif

