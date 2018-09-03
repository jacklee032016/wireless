/*
* $Id: _phy.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef 	___PHY_H__
#define	___PHY_H__

#include "wlan.h"

struct ieee80211com *_ieee80211_find_instance(MESH_DEVICE *dev);
void _ieee80211_add_vap(struct ieee80211com *ic);
void _ieee80211_remove_vap(struct ieee80211com *ic);

#endif

