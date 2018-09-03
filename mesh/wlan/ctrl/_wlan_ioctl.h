/*
* $Id: _wlan_ioctl.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef	___WLAN_IOCTL_H__
#define	___WLAN_IOCTL_H__

#include "wlan.h"

#define	IS_UP(_dev) \
	(((_dev)->flags & (MESHF_RUNNING|MESHF_UP)) == (MESHF_RUNNING|MESHF_UP))
#define	IS_UP_AUTO(_ic) \
	(IS_UP((_ic)->ic_dev) && (_ic)->ic_roaming == IEEE80211_ROAMING_AUTO)


#endif

