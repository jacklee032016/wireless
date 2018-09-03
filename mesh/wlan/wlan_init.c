/*
* $Id: wlan_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "wlan.h"

/* Print a console message with the device name prepended. */
void mdev_printf(MESH_DEVICE *mdev, const char *fmt, ...)
{
	va_list ap;
	char buf[512];		/* XXX */

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	printk("%s(%s): %s", MESH_VENDOR, mdev->name, buf);
}
EXPORT_SYMBOL(mdev_printf);



/*
 * Module glue.
 */
#define 	WLAN_VERSION		"SWJTU-0.01"
static	char *wlanVersion = WLAN_VERSION " (EXPERIMENTAL)";
static	char *wlanDevInfo = "wlan";

MODULE_AUTHOR("Errno Consulting, LiZhijie, SWJTU");
MODULE_DESCRIPTION("802.11 wireless LAN protocol support");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Wireless Lab/SWJTU");
#endif

static int __init init_wlan(void)
{
	WLAN_DEBUG_INFO("%s: %s\n", wlanDevInfo, wlanVersion);
	return 0;
}
module_init(init_wlan);

static void __exit exit_wlan(void)
{
	WLAN_DEBUG_INFO( "%s: driver unloaded\n", wlanDevInfo);
}
module_exit(exit_wlan);


