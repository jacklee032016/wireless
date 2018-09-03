/*
* $Id: radar.h,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#ifndef __RADAR_H__
#define __RADAR_H__

extern struct ieee80211_channel *radar_handle_interference(struct ieee80211com *ic);
extern void radar_init(struct ieee80211com *ic);

#endif

