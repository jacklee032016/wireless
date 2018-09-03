/*
* $Id: _node.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef  ___NODE_H__
#define  ___NODE_H__

#include "wlan.h"

struct ieee80211_node *_ieee80211_find_node(struct ieee80211_node_table *nt,	const u_int8_t *macaddr);

void _ieee80211_free_allnodes(struct ieee80211_node_table *nt);

int _ieee80211_match_bss(struct ieee80211com *ic, struct ieee80211_node *ni);

void _node_reclaim(struct ieee80211_node_table *nt, struct ieee80211_node *ni);
void _ieee80211_setup_node(struct ieee80211_node_table *nt,	struct ieee80211_node *ni, const u_int8_t *macaddr);

void _ieee80211_free_allnodes_locked(struct ieee80211_node_table *nt);

/*
 * Set/change the channel.  The rate set is also updated as
 * to insure a consistent view by drivers.
 */
static inline void ieee80211_set_chan(struct ieee80211com *ic, struct ieee80211_node *ni, struct ieee80211_channel *chan)
{
	ni->ni_chan = chan;
	ni->ni_rates = ic->ic_sup_rates[ieee80211_chan2mode(ic, chan)];
}


#endif

