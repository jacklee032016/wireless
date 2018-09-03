/*
* $Id: _crypto.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
* local defination for wlan crypto 
*/

#ifndef 	___CRYPTO_H__
#define	___CRYPTO_H__

#include "wlan.h"
#include "wlan_crypto.h"

/* Write-arounds for common operations. */
static inline void cipher_detach(struct ieee80211_key *key)
{
	key->wk_cipher->ic_detach(key);
}

static inline void *cipher_attach(struct ieee80211com *ic, struct ieee80211_key *key)
{
	return key->wk_cipher->ic_attach(ic, key);
}

/* 
 * Wrappers for driver key management methods.
 */
static inline int dev_key_alloc(struct ieee80211com *ic, const struct ieee80211_key *key)
{
	return ic->ic_crypto.cs_key_alloc(ic, key);
}

static inline int dev_key_delete(struct ieee80211com *ic,	const struct ieee80211_key *key)
{
	return ic->ic_crypto.cs_key_delete(ic, key);
}

static inline int dev_key_set(struct ieee80211com *ic, const struct ieee80211_key *key, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	return ic->ic_crypto.cs_key_set(ic, key, mac);
}

extern const struct ieee80211_cipher *wlanCiphers[];

#endif

