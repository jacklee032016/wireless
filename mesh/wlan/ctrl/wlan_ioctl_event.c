/*
* $Id: wlan_ioctl_event.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_wlan_ioctl.h"
#include "if_ethersubr.h"

void ieee80211_notify_node_join(struct ieee80211com *ic, struct ieee80211_node *ni, int newassoc)
{
	union meshw_req_data wreq;

	memset(&wreq, 0, sizeof(wreq));
	if (ni == ic->ic_bss)
	{
		if (newassoc)
			meshif_carrier_on(ic->ic_dev);
		IEEE80211_ADDR_COPY(wreq.addr.sa_data, ni->ni_bssid);
		wreq.addr.sa_family = ARPHRD_ETHER;
		wireless_send_event(ic->ic_dev, MESHW_IO_R_AP_MAC, &wreq, NULL);
	}
	else
	{
		/* fire off wireless for new and reassoc station
		 * please note that this is ok atm because there is no
		 * difference in handling in hostapd.
		 * If needed we will change to use an MESHW_EV_CUSTOM event.
		 */
		IEEE80211_ADDR_COPY(wreq.addr.sa_data, ni->ni_macaddr);
		wreq.addr.sa_family = ARPHRD_ETHER;
		wireless_send_event(ic->ic_dev, MESHW_EV_NODE_REGISTERED, &wreq, NULL);
	}
}

void ieee80211_notify_traffic_statistic(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	static const char * tag = "STA-TRAFFIC-STAT";
	union meshw_req_data wrqu;
	char buf[128]; /* max message < 125 byte */

	snprintf(buf, sizeof(buf), "%s\nmac=%s\n"
		"rx_packets=%u\ntx_packets=%u\n"
		"rx_bytes=%u\ntx_bytes=%u\n",
		tag, swjtu_mesh_mac_sprintf(ni->ni_macaddr),
		ni->ni_stats.ns_rx_data, ni->ni_stats.ns_tx_data,
		(u_int32_t)ni->ni_stats.ns_rx_bytes,
		(u_int32_t)ni->ni_stats.ns_tx_bytes);
 
	memset(&wrqu, 0, sizeof(wrqu));
	wrqu.data.length = strlen(buf);
	wireless_send_event(ic->ic_dev, MESHW_EV_CUSTOM, &wrqu, buf);
}
 
void ieee80211_notify_node_leave(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	union meshw_req_data wreq;

	if (ni == ic->ic_bss)
	{
		meshif_carrier_off(ic->ic_dev);
		memset(wreq.ap_addr.sa_data, 0, ETHER_ADDR_LEN);
		wreq.ap_addr.sa_family = ARPHRD_ETHER;
		wireless_send_event(ic->ic_dev, MESHW_IO_R_AP_MAC, &wreq, NULL);
	}
	else
	{
		/* sending message about last traffic statistics of station */
		ieee80211_notify_traffic_statistic(ic, ni);
		/* fire off wireless event station leaving */
		memset(&wreq, 0, sizeof(wreq));
		IEEE80211_ADDR_COPY(wreq.addr.sa_data, ni->ni_macaddr);
		wreq.addr.sa_family = ARPHRD_ETHER;
		wireless_send_event(ic->ic_dev, MESHW_EV_NODE_EXPIRED, &wreq, NULL);
	}
}

void ieee80211_notify_scan_done(struct ieee80211com *ic)
{
	union meshw_req_data wreq;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "%s: notify scan done\n", ic->ic_dev->name);

	/* dispatch wireless event indicating scan completed */
	wreq.data.length = 0;
	wreq.data.flags = 0;
	wireless_send_event(ic->ic_dev, MESHW_IO_R_SCAN, &wreq, NULL);
}

void ieee80211_notify_replay_failure(struct ieee80211com *ic,
	const struct ieee80211_frame *wh, const struct ieee80211_key *k,
	u_int64_t rsc)
{
	static const char * tag = "MLME-REPLAYFAILURE.indication";
	union meshw_req_data wrqu;
	char buf[128];

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_CRYPTO,
		"[%s] %s replay detected <rsc %llu, csc %llu>\n",
		swjtu_mesh_mac_sprintf(wh->i_addr2), k->wk_cipher->ic_name,	rsc, k->wk_keyrsc);

	if (ic->ic_dev == NULL)		/* NB: for cipher test modules */
		return;

	/* TODO: needed parameters: count, keyid, key type, src address, TSC */
	snprintf(buf, sizeof(buf), "%s(keyid=%d %scast addr=%s)", tag,
	    k->wk_keyix, IEEE80211_IS_MULTICAST(wh->i_addr1) ?  "broad" : "uni",
	    swjtu_mesh_mac_sprintf(wh->i_addr1));
	memset(&wrqu, 0, sizeof(wrqu));
	wrqu.data.length = strlen(buf);
	wireless_send_event(ic->ic_dev, MESHW_EV_CUSTOM, &wrqu, buf);
}

void ieee80211_notify_michael_failure(struct ieee80211com *ic,
	const struct ieee80211_frame *wh, u_int keyix)
{
	static const char * tag = "MLME-MICHAELMICFAILURE.indication";
	union meshw_req_data wrqu;
	char buf[128];

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_CRYPTO,"[%s] Michael MIC verification failed <keyidx %d>\n",
	       swjtu_mesh_mac_sprintf(wh->i_addr2), keyix);
	ic->ic_stats.is_rx_tkipmic++;

	if (ic->ic_dev == NULL)		/* NB: for cipher test modules */
		return;

	/* TODO: needed parameters: count, keyid, key type, src address, TSC */
	snprintf(buf, sizeof(buf), "%s(keyid=%d %scast addr=%s)", tag,
	    keyix, IEEE80211_IS_MULTICAST(wh->i_addr1) ?  "broad" : "uni",
	    swjtu_mesh_mac_sprintf(wh->i_addr1));
	memset(&wrqu, 0, sizeof(wrqu));
	wrqu.data.length = strlen(buf);
	wireless_send_event(ic->ic_dev, MESHW_EV_CUSTOM, &wrqu, buf);
}

