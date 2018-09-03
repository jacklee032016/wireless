/*
* $Id: wlan_crypto_none.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
/* IEEE 802.11 NULL crypto cipher support.  */

#include "_crypto.h"

static void *none_attach(struct ieee80211com *ic, struct ieee80211_key *k)
{
	return ic;		/* for diagnostics+stats */
}

static void none_detach(struct ieee80211_key *k)
{
	(void) k;
}

static int none_setkey(struct ieee80211_key *k)
{
	(void) k;
	return 1;
}

static int none_encap(struct ieee80211_key *k, struct sk_buff *skb, u_int8_t keyid)
{
	struct ieee80211com *ic = k->wk_private;
#ifdef IEEE80211_DEBUG
	struct ieee80211_frame *wh = (struct ieee80211_frame *)skb->data;
#endif

	/*
	 * The specified key is not setup; this can
	 * happen, at least, when changing keys.
	 */
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_CRYPTO, "[%s] key id %u is not set (encap)\n",	swjtu_mesh_mac_sprintf(wh->i_addr1), keyid>>6);
	ic->ic_stats.is_tx_badcipher++;
	return 0;
}

static int none_decap(struct ieee80211_key *k, struct sk_buff *skb)
{
	struct ieee80211com *ic = k->wk_private;
#ifdef IEEE80211_DEBUG
	struct ieee80211_frame *wh = (struct ieee80211_frame *)skb->data;
	const u_int8_t *ivp = (const u_int8_t *)&wh[1];
#endif

	/*
	 * The specified key is not setup; this can
	 * happen, at least, when changing keys.
	 */
	/* XXX useful to know dst too */
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_CRYPTO, "[%s] key id %u is not set (decap)\n",  swjtu_mesh_mac_sprintf(wh->i_addr2), ivp[IEEE80211_WEP_IVLEN] >> 6);
	ic->ic_stats.is_rx_badkeyid++;
	return 0;
}

static int none_enmic(struct ieee80211_key *k, struct sk_buff *skb)
{
	struct ieee80211com *ic = k->wk_private;

	ic->ic_stats.is_tx_badcipher++;
	return 0;
}

static int none_demic(struct ieee80211_key *k, struct sk_buff *skb)
{
	struct ieee80211com *ic = k->wk_private;

	ic->ic_stats.is_rx_badkeyid++;
	return 0;
}

const struct ieee80211_cipher ieee80211_cipher_none =
{
	.ic_name	= "NONE",
	.ic_cipher	= IEEE80211_CIPHER_NONE,
	.ic_header	= 0,
	.ic_trailer	= 0,
	.ic_miclen	= 0,
	.ic_attach	= none_attach,
	.ic_detach	= none_detach,
	.ic_setkey	= none_setkey,
	.ic_encap	= none_encap,
	.ic_decap	= none_decap,
	.ic_enmic	= none_enmic,
	.ic_demic	= none_demic,
};
EXPORT_SYMBOL(ieee80211_cipher_none);

