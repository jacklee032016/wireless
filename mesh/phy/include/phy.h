/*
* $Id: phy.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef	__PHY_H__
#define	__PHY_H__

#define	PHY_VENDOR	"SWJTU(PHY)"

#define	PHY_DEBUG_INFO( _fmt, ...)	do{ 			\
	printk(KERN_INFO PHY_VENDOR " : " _fmt, ##__VA_ARGS__);		\
}while(0)


#include "mesh.h"

#include <linux/cache.h>
#include <linux/pci.h>

#include "wlan_if_media.h"
#include <ieee80211_var.h>

#include "phy_var.h"
#include "phy_bus_pci.h"
#include "phy_ath.h"

#endif

