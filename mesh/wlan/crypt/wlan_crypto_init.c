/*
* $Id: wlan_crypto_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_crypto.h"

/* IEEE 802.11 generic crypto support. */

/*
 * Table of registered cipher modules. All ciphers are keep in this array 
 */
const struct ieee80211_cipher *wlanCiphers[IEEE80211_CIPHER_MAX];

static	int _ieee80211_crypto_delkey(struct ieee80211com *, struct ieee80211_key *);

/* Default "null" key management routines. */
static int null_key_alloc(struct ieee80211com *ic, const struct ieee80211_key *k)
{
	return IEEE80211_KEYIX_NONE;
}

static int null_key_delete(struct ieee80211com *ic, const struct ieee80211_key *k)
{
	return 1;
}

static int null_key_set(struct ieee80211com *ic, const struct ieee80211_key *k, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	return 1;
}

static void null_key_update(struct ieee80211com *ic)
{}


/* Setup crypto support in ieee80211com when init */
void ieee80211_crypto_attach(struct ieee80211com *ic)
{
	struct ieee80211_crypto_state *cs = &ic->ic_crypto;
	int i;

	/* NB: we assume everything is pre-zero'd */
	cs->cs_def_txkey = IEEE80211_KEYIX_NONE;
	wlanCiphers[IEEE80211_CIPHER_NONE] = &ieee80211_cipher_none;
	
	for (i = 0; i < IEEE80211_WEP_NKID; i++)
		ieee80211_crypto_resetkey(ic, &cs->cs_nw_keys[i],	IEEE80211_KEYIX_NONE);
	/*
	 * Initialize the driver key support routines to noop entries.
	 * This is useful especially for the cipher test modules.
	 */
	cs->cs_key_alloc = null_key_alloc;
	cs->cs_key_set = null_key_set;
	cs->cs_key_delete = null_key_delete;
	cs->cs_key_update_begin = null_key_update;
	cs->cs_key_update_end = null_key_update;
}

/*
 * Teardown crypto support.
 */
void ieee80211_crypto_detach(struct ieee80211com *ic)
{
	ieee80211_crypto_delglobalkeys(ic);
}

/*
 * Register a crypto cipher module.
 */
void ieee80211_crypto_register(const struct ieee80211_cipher *cip)
{
	if (cip->ic_cipher >= IEEE80211_CIPHER_MAX)
	{
		printf("%s: cipher %s has an invalid cipher index %u\n", __func__, cip->ic_name, cip->ic_cipher);
		return;
	}
	
	if (wlanCiphers[cip->ic_cipher] != NULL && wlanCiphers[cip->ic_cipher] != cip)
	{
		printf("%s: cipher %s registered with a different template\n", __func__, cip->ic_name);
		return;
	}
	wlanCiphers[cip->ic_cipher] = cip;
}

/*
 * Unregister a crypto cipher module.
 */
void ieee80211_crypto_unregister(const struct ieee80211_cipher *cip)
{
	if (cip->ic_cipher >= IEEE80211_CIPHER_MAX)
	{
		printf("%s: cipher %s has an invalid cipher index %u\n",	__func__, cip->ic_name, cip->ic_cipher);
		return;
	}
	
	if (wlanCiphers[cip->ic_cipher] != NULL && wlanCiphers[cip->ic_cipher] != cip)
	{
		printf("%s: cipher %s registered with a different template\n", __func__, cip->ic_name);
		return;
	}
	/* NB: don't complain about not being registered */
	/* XXX disallow if references */
	wlanCiphers[cip->ic_cipher] = NULL;
}

int ieee80211_crypto_available(u_int cipher)
{
	return cipher < IEEE80211_CIPHER_MAX && wlanCiphers[cipher] != NULL;
}

EXPORT_SYMBOL(ieee80211_crypto_available);
EXPORT_SYMBOL(ieee80211_crypto_attach);
EXPORT_SYMBOL(ieee80211_crypto_detach);
EXPORT_SYMBOL(ieee80211_crypto_register);
EXPORT_SYMBOL(ieee80211_crypto_unregister);
EXPORT_SYMBOL(ieee80211_crypto_newkey);
EXPORT_SYMBOL(ieee80211_crypto_delkey);
EXPORT_SYMBOL(ieee80211_crypto_delglobalkeys);
EXPORT_SYMBOL(ieee80211_crypto_setkey);
EXPORT_SYMBOL(ieee80211_crypto_encap);
EXPORT_SYMBOL(ieee80211_crypto_decap);

