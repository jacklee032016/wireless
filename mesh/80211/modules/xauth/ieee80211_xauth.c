/*
* $Id: ieee80211_xauth.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/*
 * External authenticator placeholder module.
 *
 * This support is optional; it is only used when the 802.11 layer's
 * authentication mode is set to use 802.1x or WPA is enabled separately
 * (for WPA-PSK).  If compiled as a module this code does not need
 * to be present unless 802.1x/WPA is in use.
 *
 * The authenticator hooks into the 802.11 layer.  At present we use none
 * of the available callbacks--the user mode authenticator process works
 * entirely from messages about stations joining and leaving.
 */
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/sysctl.h>
#include <linux/in.h>

#include "if_media.h"
#include "if_llc.h"
#include "if_ethersubr.h"

#include <ieee80211_var.h>

/*
 * Module glue.
 */

MODULE_AUTHOR("Errno Consulting, Sam Leffler");
MODULE_DESCRIPTION("802.11 wireless support: external (user mode) authenticator");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

/*
 * One module handles everything for now.  May want
 * to split things up for embedded applications.
 */
static const struct ieee80211_authenticator xauth = {
	.ia_name	= "external",
	.ia_attach	= NULL,
	.ia_detach	= NULL,
	.ia_node_join	= NULL,
	.ia_node_leave	= NULL,
};

static int __init
init_ieee80211_xauth(void)
{
	ieee80211_authenticator_register(IEEE80211_AUTH_8021X, &xauth);
	ieee80211_authenticator_register(IEEE80211_AUTH_WPA, &xauth);
	return 0;
}
module_init(init_ieee80211_xauth);

static void __exit
exit_ieee80211_xauth(void)
{
	ieee80211_authenticator_unregister(IEEE80211_AUTH_8021X);
	ieee80211_authenticator_unregister(IEEE80211_AUTH_WPA);
}
module_exit(exit_ieee80211_xauth);
