/*
* $Id: wlan_proto_acl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_protocol.h"

#if WITH_WLAN_AUTHBACK_ACL
/** Very simple-minded ACL module support. */
/* XXX just one for now */
static	const struct ieee80211_aclator *acl = NULL;

void ieee80211_aclator_register(const struct ieee80211_aclator *iac)
{
	WLAN_DEBUG_INFO( "wlan: %s acl policy registered\n", iac->iac_name);
	acl = iac;
}

void ieee80211_aclator_unregister(const struct ieee80211_aclator *iac)
{
	if (acl == iac)
		acl = NULL;
	WLAN_DEBUG_INFO("wlan: %s acl policy unregistered\n", iac->iac_name);
}

const struct ieee80211_aclator *ieee80211_aclator_get(const char *name)
{
#if 0
	if (acl == NULL)
		request_module("wlan_acl");
#endif	
	return acl && strcmp(acl->iac_name, name) == 0 ? acl : NULL;
}

EXPORT_SYMBOL(ieee80211_aclator_register);
EXPORT_SYMBOL(ieee80211_aclator_unregister);
EXPORT_SYMBOL(ieee80211_aclator_get);
#endif

