/*
* $Id: wlan_crypto.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#ifndef	__WLAN_CRYPTO_H__
#define	__WLAN_CRYPTO_H__

/*
 * 802.11 protocol crypto-related definitions.
 */
#define	IEEE80211_KEYBUF_SIZE	16
#define	IEEE80211_MICBUF_SIZE	(8+8)	/* space for both tx+rx keys */


struct ieee80211_cipher;

/*
 * Crypto key state.  There is sufficient room for all supported
 * ciphers (see below).  The underlying ciphers are handled
 * separately through loadable cipher modules that register with
 * the generic crypto support.  A key has a reference to an instance
 * of the cipher; any per-key state is hung off wk_private by the
 * cipher when it is attached.  Ciphers are automatically called
 * to detach and cleanup any such state when the key is deleted.
 *
 * The generic crypto support handles encap/decap of cipher-related
 * frame contents for both hardware- and software-based implementations.
 * A key requiring software crypto support is automatically flagged and
 * the cipher is expected to honor this and do the necessary work.
 * Ciphers such as TKIP may also support mixed hardware/software
 * encrypt/decrypt and MIC processing.
 */
/* XXX need key index typedef */
/* XXX pack better? */
/* XXX 48-bit rsc/tsc */
struct ieee80211_key
{
	u_int8_t	wk_keylen;	/* key length in bytes */
	u_int8_t	wk_flags;
#define	IEEE80211_KEY_XMIT		0x01	/* key used for xmit */
#define	IEEE80211_KEY_RECV		0x02	/* key used for recv */
#define	IEEE80211_KEY_GROUP		0x04	/* key used for WPA group operation */
#define	IEEE80211_KEY_SWCRYPT	0x10	/* host-based encrypt/decrypt, SW : software based */
#define	IEEE80211_KEY_SWMIC		0x20	/* host-based enmic/demic */
	u_int16_t	wk_keyix;	/* key index */
	u_int8_t	wk_key[IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE];
#define	wk_txmic	wk_key+IEEE80211_KEYBUF_SIZE+0	/* XXX can't () right */
#define	wk_rxmic	wk_key+IEEE80211_KEYBUF_SIZE+8	/* XXX can't () right */
	u_int64_t	wk_keyrsc;	/* key receive sequence counter */
	u_int64_t	wk_keytsc;	/* key transmit sequence counter */
	
	const struct ieee80211_cipher *wk_cipher;
	void		*wk_private;	/* private cipher state */
};

#define	IEEE80211_KEY_COMMON 		/* common flags passed in by apps */\
	(IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV | IEEE80211_KEY_GROUP)

/*
 * NB: these values are ordered carefully; there are lots of
 * of implications in any reordering.  In particular beware
 * that 4 is not used to avoid conflicting with IEEE80211_F_PRIVACY.
 */
#define	IEEE80211_CIPHER_WEP		0
#define	IEEE80211_CIPHER_TKIP		1
#define	IEEE80211_CIPHER_AES_OCB	2
#define	IEEE80211_CIPHER_AES_CCM	3
#define	IEEE80211_CIPHER_CKIP		5
#define	IEEE80211_CIPHER_NONE		6	/* pseudo value */

#define	IEEE80211_CIPHER_MAX		(IEEE80211_CIPHER_NONE+1)

#define	IEEE80211_KEYIX_NONE	((u_int16_t) -1)

#if defined(__KERNEL__) || defined(_KERNEL)

struct ieee80211com;
struct ieee80211_node;
struct sk_buff;

/*
 * Crypto state kept in each ieee80211com.  Some of this
 * can/should be shared when virtual AP's are supported.
 *
 * XXX save reference to ieee80211com to properly encapsulate state.
 * XXX split out crypto capabilities from ic_caps
 */
struct ieee80211_crypto_state
{
	struct ieee80211_key	cs_nw_keys[IEEE80211_WEP_NKID];
	u_int16_t		cs_def_txkey;	/* default/group tx key index */

	int			(*cs_key_alloc)(struct ieee80211com *,	const struct ieee80211_key *);
	int			(*cs_key_delete)(struct ieee80211com *, const struct ieee80211_key *);
	int			(*cs_key_set)(struct ieee80211com *, const struct ieee80211_key *,	const u_int8_t mac[IEEE80211_ADDR_LEN]);
	void			(*cs_key_update_begin)(struct ieee80211com *);
	void			(*cs_key_update_end)(struct ieee80211com *);
};

extern	void		ieee80211_crypto_attach(struct ieee80211com *);
extern	void		ieee80211_crypto_detach(struct ieee80211com *);
extern	int		ieee80211_crypto_newkey(struct ieee80211com *, int cipher, int flags, struct ieee80211_key *);
extern	int		ieee80211_crypto_delkey(struct ieee80211com *,struct ieee80211_key *);
extern	int		ieee80211_crypto_setkey(struct ieee80211com *,struct ieee80211_key *, const u_int8_t macaddr[IEEE80211_ADDR_LEN]);
extern	void		ieee80211_crypto_delglobalkeys(struct ieee80211com *);

/*
 * Template for a supported cipher.  Ciphers register with the
 * crypto code and are typically loaded as separate modules
 * (the null cipher is always present).
 * XXX may need refcnts
 */
struct ieee80211_cipher
{
	const char *ic_name;		/* printable name */
	u_int	ic_cipher;		/* IEEE80211_CIPHER_*, eg. index in cipher array */
	u_int	ic_header;		/* size of privacy header (bytes) */
	u_int	ic_trailer;		/* size of privacy trailer (bytes) */
	u_int	ic_miclen;		/* size of mic trailer (bytes) */
	
	void*	(*ic_attach)(struct ieee80211com *, struct ieee80211_key *);
	void		(*ic_detach)(struct ieee80211_key *);
	int		(*ic_setkey)(struct ieee80211_key *);
	
	int		(*ic_encap)(struct ieee80211_key *, struct sk_buff *, u_int8_t keyid);
	int		(*ic_decap)(struct ieee80211_key *, struct sk_buff *);
	
	int		(*ic_enmic)(struct ieee80211_key *, struct sk_buff *);
	int		(*ic_demic)(struct ieee80211_key *, struct sk_buff *);
};
extern	const struct ieee80211_cipher ieee80211_cipher_none;

extern	void ieee80211_crypto_register(const struct ieee80211_cipher *);
extern	void ieee80211_crypto_unregister(const struct ieee80211_cipher *);
extern	int ieee80211_crypto_available(u_int cipher);

extern	struct ieee80211_key *ieee80211_crypto_encap(struct ieee80211com *,
		struct ieee80211_node *, struct sk_buff *);
extern	struct ieee80211_key *ieee80211_crypto_decap(struct ieee80211com *,
		struct ieee80211_node *, struct sk_buff *);

/* Check and remove any MIC when rx skb. */
static inline int ieee80211_crypto_demic(struct ieee80211com *ic, struct ieee80211_key *k, 	struct sk_buff *skb)
{
	const struct ieee80211_cipher *cip = k->wk_cipher;
	return (cip->ic_miclen > 0 ? (*cip->ic_demic)(k, skb) : 1);
}

/* Add any MIC when tx skb */
static inline int ieee80211_crypto_enmic(struct ieee80211com *ic, struct ieee80211_key *k, struct sk_buff *skb)
{
	const struct ieee80211_cipher *cip = k->wk_cipher;
	return (cip->ic_miclen > 0 ? (*cip->ic_enmic)(k, skb) : 1);
}

/* 
 * Reset key state to an unused state.  The crypto
 * key allocation mechanism insures other state (e.g.
 * key data) is properly setup before a key is used.
 */
static inline void ieee80211_crypto_resetkey(struct ieee80211com *ic, struct ieee80211_key *k, u_int16_t ix)
{
	k->wk_cipher = &ieee80211_cipher_none;;
	k->wk_private = k->wk_cipher->ic_attach(ic, k);
	k->wk_keyix = ix;
	k->wk_flags = IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV;
}

/*
 * Crypt-related notification methods.
 */
extern	void ieee80211_notify_replay_failure(struct ieee80211com *, 	const struct ieee80211_frame *, const struct ieee80211_key *, u_int64_t rsc);
extern	void ieee80211_notify_michael_failure(struct ieee80211com *,  const struct ieee80211_frame *, u_int keyix);
#endif /* defined(__KERNEL__) || defined(_KERNEL) */

#endif

