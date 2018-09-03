/*
* $Id: wlan_proto_authen.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_protocol.h"

/************************************************
 * Simple-minded authenticator module support.
 ************************************************/

#define	IEEE80211_AUTH_MAX		(IEEE80211_AUTH_WPA+1)
/* XXX well-known names */
static const char *auth_modnames[IEEE80211_AUTH_MAX] = 
{
	"wlan_internal",	/* IEEE80211_AUTH_NONE */
	"wlan_internal",	/* IEEE80211_AUTH_OPEN */
	"wlan_internal",	/* IEEE80211_AUTH_SHARED */
	"wlan_xauth",		/* IEEE80211_AUTH_8021X	 */
	"wlan_internal",	/* IEEE80211_AUTH_AUTO */
	"wlan_xauth",		/* IEEE80211_AUTH_WPA */
};
static const struct ieee80211_authenticator *authenticators[IEEE80211_AUTH_MAX];

static const struct ieee80211_authenticator auth_internal = 
{
	.ia_name			= "wlan_internal",
	/* handles */	
	.ia_attach		= NULL,
	.ia_detach		= NULL,
	.ia_node_join		= NULL,
	.ia_node_leave	= NULL,
};

//SYSINIT(wlan_auth, SI_SUB_DRIVERS, SI_ORDER_FIRST, ieee80211_auth_setup, NULL);	// TODO

const struct ieee80211_authenticator *ieee80211_authenticator_get(int auth)
{
	if (auth >= IEEE80211_AUTH_MAX)
		return NULL;
#if 0	
	if (authenticators[auth] == NULL)
		request_module(auth_modnames[auth]);	/* only 2.6 support */
#endif	
	return authenticators[auth];
}

void ieee80211_authenticator_register(int type, const struct ieee80211_authenticator *auth)
{
	if (type >= IEEE80211_AUTH_MAX)
		return;
	authenticators[type] = auth;
}

void ieee80211_authenticator_unregister(int type)
{
	if (type >= IEEE80211_AUTH_MAX)
		return;
	authenticators[type] = NULL;
}

/* Setup internal authenticators once; they are never unregistered. */
void ieee80211_auth_setup(void)
{/* dummy authenticator */
	ieee80211_authenticator_register(IEEE80211_AUTH_OPEN, 		&auth_internal);
	ieee80211_authenticator_register(IEEE80211_AUTH_SHARED, 		&auth_internal);
	ieee80211_authenticator_register(IEEE80211_AUTH_AUTO, 		&auth_internal);
}

EXPORT_SYMBOL(ieee80211_authenticator_register);
EXPORT_SYMBOL(ieee80211_authenticator_unregister);
EXPORT_SYMBOL(ieee80211_auth_setup);

