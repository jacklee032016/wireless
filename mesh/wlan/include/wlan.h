/*
* $Id: wlan.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef	__WLAN_H__
#define	__WLAN_H__

#define	WLAN_VENDOR	"SWJTU(WLAN)"

#define	WLAN_DEBUG_INFO( _fmt, ...)	do{ 			\
	printk(KERN_INFO WLAN_VENDOR ": " _fmt, ##__VA_ARGS__);		\
}while(0)

#include "mesh.h"

#include "wlan_if_media.h"
#include <ieee80211_var.h>

#define	AGGRESSIVE_MODE_SWITCH_HYSTERESIS		3	/* pkts / 100ms */
#define	HIGH_PRI_SWITCH_THRESH					10	/* pkts / 100ms */

int ieee80211_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg);

#endif

