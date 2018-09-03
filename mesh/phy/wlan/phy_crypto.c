/*
* $Id: phy_crypto.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#include "phy.h"

#ifdef AR_DEBUG
static void ath_keyprint(const char *tag, u_int ix, const HAL_KEYVAL *hk, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	static const char *ciphers[] =
	{
		"WEP",
		"AES-OCB",
		"AES-CCM",
		"CKIP",
		"TKIP",
		"CLR",
	};
	int i, n;

	printk("%s: [%02u] %-7s ", tag, ix, ciphers[hk->kv_type]);
	for (i = 0, n = hk->kv_len; i < n; i++)
		printk("%02x", hk->kv_val[i]);
	printk(" mac %s", swjtu_mesh_mac_sprintf(mac));
	if (hk->kv_type == HAL_CIPHER_TKIP)
	{
		printk(" mic ");
		for (i = 0; i < sizeof(hk->kv_mic); i++)
			printk("%02x", hk->kv_mic[i]);
	}
	printk("\n");
}
#endif

/*
 * Set a TKIP key into the hardware.  This handles the
 * potential distribution of key state to multiple key
 * cache slots for TKIP.
 */
static int ath_keyset_tkip(struct ath_softc *sc, const struct ieee80211_key *k, HAL_KEYVAL *hk, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
#define	IEEE80211_KEY_XR	(IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV)
	static const u_int8_t zerobssid[IEEE80211_ADDR_LEN];
	struct ath_hal *ah = sc->sc_ah;

	KASSERT(k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP,	("got a non-TKIP key, cipher %u", k->wk_cipher->ic_cipher));
	KASSERT(sc->sc_splitmic, ("key cache !split"));
	
	if ((k->wk_flags & IEEE80211_KEY_XR) == IEEE80211_KEY_XR)
	{
		/*
		 * TX key goes at first index, RX key at +32.
		 * The hal handles the MIC keys at index+64.
		 */
		memcpy(hk->kv_mic, k->wk_txmic, sizeof(hk->kv_mic));
		KEYPRINTF(sc, k->wk_keyix, hk, zerobssid);
		if (!ath_hal_keyset(ah, k->wk_keyix, hk, zerobssid))
			return 0;

		memcpy(hk->kv_mic, k->wk_rxmic, sizeof(hk->kv_mic));
		KEYPRINTF(sc, k->wk_keyix+32, hk, mac);
		/* XXX delete tx key on failure? */
		return ath_hal_keyset(ah, k->wk_keyix+32, hk, mac);
	}
	else if (k->wk_flags & IEEE80211_KEY_XR)
	{
		/*
		 * TX/RX key goes at first index.
		 * The hal handles the MIC keys are index+64.
		 */
		KASSERT(k->wk_keyix < IEEE80211_WEP_NKID, ("group key at index %u", k->wk_keyix));

		memcpy(hk->kv_mic, k->wk_flags & IEEE80211_KEY_XMIT ?	k->wk_txmic : k->wk_rxmic, sizeof(hk->kv_mic));
		KEYPRINTF(sc, k->wk_keyix, hk, zerobssid);
		return ath_hal_keyset(ah, k->wk_keyix, hk, zerobssid);
	}
	/* XXX key w/o xmit/recv; need this for compression? */
	return 0;
#undef IEEE80211_KEY_XR
}

/*
 * Set a net80211 key into the hardware.  This handles the
 * potential distribution of key state to multiple key
 * cache slots for TKIP with hardware MIC support.
 */
static int ath_keyset(struct ath_softc *sc, const struct ieee80211_key *k, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	static const u_int8_t ciphermap[] =
	{
		HAL_CIPHER_WEP,		/* IEEE80211_CIPHER_WEP */
		HAL_CIPHER_TKIP,	/* IEEE80211_CIPHER_TKIP */
		HAL_CIPHER_AES_OCB,	/* IEEE80211_CIPHER_AES_OCB */
		HAL_CIPHER_AES_CCM,	/* IEEE80211_CIPHER_AES_CCM */
		(u_int8_t) -1,		/* 4 is not allocated */
		HAL_CIPHER_CKIP,	/* IEEE80211_CIPHER_CKIP */
		HAL_CIPHER_CLR,		/* IEEE80211_CIPHER_NONE */
	};
	struct ath_hal *ah = sc->sc_ah;
	const struct ieee80211_cipher *cip = k->wk_cipher;
	HAL_KEYVAL hk;

	memset(&hk, 0, sizeof(hk));
	/*
	 * Software crypto uses a "clear key" so non-crypto
	 * state kept in the key cache are maintained and
	 * so that rx frames have an entry to match.
	 */
	if ((k->wk_flags & IEEE80211_KEY_SWCRYPT) == 0)
	{
		KASSERT(cip->ic_cipher < N(ciphermap), ("invalid cipher type %u", cip->ic_cipher));
		hk.kv_type = ciphermap[cip->ic_cipher];
		hk.kv_len = k->wk_keylen;
		memcpy(hk.kv_val, k->wk_key, k->wk_keylen);
	}
	else
		hk.kv_type = HAL_CIPHER_CLR;

	if (hk.kv_type == HAL_CIPHER_TKIP && (k->wk_flags & IEEE80211_KEY_SWMIC) == 0 &&
	    sc->sc_splitmic)
	{
		return ath_keyset_tkip(sc, k, &hk, mac);
	}
	else
	{
		KEYPRINTF(sc, k->wk_keyix, &hk, mac);
		return ath_hal_keyset(ah, k->wk_keyix, &hk, mac);
	}
#undef N
}

/* Fill the hardware key cache with key entries. */
void _phy_initkeytable(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	MESH_DEVICE *dev = ic->ic_dev;
	struct ath_hal *ah = sc->sc_ah;
	const u_int8_t *bssid;
	int i;

	/* XXX maybe should reset all keys when !PRIVACY */
	if (ic->ic_state == IEEE80211_S_SCAN)
		bssid = dev->broadcast;
	else
		bssid = ic->ic_bss->ni_bssid;

	for (i = 0; i < IEEE80211_WEP_NKID; i++)
	{
		struct ieee80211_key *k = &ic->ic_nw_keys[i];

		if (k->wk_keylen == 0)
		{
			ath_hal_keyreset(ah, i);
			DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: reset key %u\n", __func__, i);
		}
		else
		{
			ath_keyset(sc, k, bssid);
		}
	}
}

/*
 * Allocate tx/rx key slots for TKIP.  We allocate two slots for
 * each key, one for decrypt/encrypt and the other for the MIC.
 */
static u_int16_t __key_alloc_2pair(struct ath_softc *sc)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	u_int i, keyix;

	KASSERT(sc->sc_splitmic, ("key cache !split"));
	/* XXX could optimize */
	for (i = 0; i < N(sc->sc_keymap)/4; i++)
	{
		u_int8_t b = sc->sc_keymap[i];
		if (b != 0xff)
		{
			/* One or more slots in this byte are free. */
			keyix = i*NBBY;
			while (b & 1)
			{
		again:
				keyix++;
				b >>= 1;
			}
			/* XXX IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV */
			if (isset(sc->sc_keymap, keyix+32) ||
			    isset(sc->sc_keymap, keyix+64) ||
			    isset(sc->sc_keymap, keyix+32+64))
			{
				/* full pair unavailable */
				/* XXX statistic */
				if (keyix == (i+1)*NBBY)
				{
					/* no slots were appropriate, advance */
					continue;
				}
				goto again;
			}
			setbit(sc->sc_keymap, keyix);
			setbit(sc->sc_keymap, keyix+64);
			setbit(sc->sc_keymap, keyix+32);
			setbit(sc->sc_keymap, keyix+32+64);
			DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: key pair %u,%u %u,%u\n", __func__, keyix, keyix+64, keyix+32, keyix+32+64);
			return keyix;
		}
	}
	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of pair space\n", __func__);
	return IEEE80211_KEYIX_NONE;
#undef N
}

/* Allocate a single key cache slot.  */
static u_int16_t __key_alloc_single(struct ath_softc *sc)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	u_int i, keyix;

	/* XXX try i,i+32,i+64,i+32+64 to minimize key pair conflicts */
	for (i = 0; i < N(sc->sc_keymap); i++)
	{
		u_int8_t b = sc->sc_keymap[i];
		if (b != 0xff)
		{
			/*
			 * One or more slots are free.
			 */
			keyix = i*NBBY;
			while (b & 1)
				keyix++, b >>= 1;
			setbit(sc->sc_keymap, keyix);
			DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: key %u\n", __func__, keyix);
			return keyix;
		}
	}
	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of space\n", __func__);
	return IEEE80211_KEYIX_NONE;
#undef N
}

/*
 * Allocate one or more key cache slots for a uniacst key.  The
 * key itself is needed only to identify the cipher.  For hardware
 * TKIP with split cipher+MIC keys we allocate two key cache slot
 * pairs so that we can setup separate TX and RX MIC keys.  Note
 * that the MIC key for a TKIP key at slot i is assumed by the
 * hardware to be at slot i+64.  This limits TKIP keys to the first
 * 64 entries.
 */
int ath_key_alloc(struct ieee80211com *ic, const struct ieee80211_key *k)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ath_softc *sc = (struct ath_softc *)dev->priv;

	/*
	 * Group key allocation must be handled specially for
	 * parts that do not support multicast key cache search
	 * functionality.  For those parts the key id must match
	 * the h/w key index so lookups find the right key.  On
	 * parts w/ the key search facility we install the sender's
	 * mac address (with the high bit set) and let the hardware
	 * find the key w/o using the key id.  This is preferred as
	 * it permits us to support multiple users for adhoc and/or
	 * multi-station operation.
	 */
	if ((k->wk_flags & IEEE80211_KEY_GROUP) && !sc->sc_mcastkey)
	{
		u_int keyix;

		if (!(&ic->ic_nw_keys[0] <= k &&
		      k < &ic->ic_nw_keys[IEEE80211_WEP_NKID]))
		{
			/* should not happen */
			DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: bogus group key\n", __func__);
			return IEEE80211_KEYIX_NONE;
		}
		keyix = k - ic->ic_nw_keys;
		/*
		 * XXX we pre-allocate the global keys so
		 * have no way to check if they've already been allocated.
		 */
		return keyix;
	}

	/*
	 * We allocate two pair for TKIP when using the h/w to do
	 * the MIC.  For everything else, including software crypto,
	 * we allocate a single entry.  Note that s/w crypto requires
	 * a pass-through slot on the 5211 and 5212.  The 5210 does
	 * not support pass-through cache entries and we map all
	 * those requests to slot 0.
	 */
	if (k->wk_flags & IEEE80211_KEY_SWCRYPT)
	{
		return __key_alloc_single(sc);
	}
	else if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP &&
	    (k->wk_flags & IEEE80211_KEY_SWMIC) == 0 && sc->sc_splitmic)
	{
		return __key_alloc_2pair(sc);
	}
	else
	{
		return __key_alloc_single(sc);
	}
}

/* Delete an entry in the key cache allocated by ath_key_alloc. */
int ath_key_delete(struct ieee80211com *ic, const struct ieee80211_key *k)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	struct ath_hal *ah = sc->sc_ah;
	const struct ieee80211_cipher *cip = k->wk_cipher;
	u_int keyix = k->wk_keyix;

	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: delete key %u\n", __func__, keyix);

	ath_hal_keyreset(ah, keyix);
	/*
	 * Handle split tx/rx keying required for TKIP with h/w MIC.
	 */
	if (cip->ic_cipher == IEEE80211_CIPHER_TKIP &&
	    (k->wk_flags & IEEE80211_KEY_SWMIC) == 0 && sc->sc_splitmic)
		ath_hal_keyreset(ah, keyix+32);		/* RX key */
	if (keyix >= IEEE80211_WEP_NKID)
	{
		/*
		 * Don't touch keymap entries for global keys so
		 * they are never considered for dynamic allocation.
		 */
		clrbit(sc->sc_keymap, keyix);
		if (cip->ic_cipher == IEEE80211_CIPHER_TKIP &&
		    (k->wk_flags & IEEE80211_KEY_SWMIC) == 0 &&
		    sc->sc_splitmic)
		{
			clrbit(sc->sc_keymap, keyix+64);	/* TX key MIC */
			clrbit(sc->sc_keymap, keyix+32);	/* RX key */
			clrbit(sc->sc_keymap, keyix+32+64);	/* RX key MIC */
		}
	}
	return 1;
}

/* Set the key cache contents for the specified key.  Key cache
 * slot(s) must already have been allocated by ath_key_alloc. */
int ath_key_set(struct ieee80211com *ic, const struct ieee80211_key *k, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ath_softc *sc = (struct ath_softc *)dev->priv;

	return ath_keyset(sc, k, mac);
}

/*
 * Block/unblock tx+rx processing while a key change is done.
 * We assume the caller serializes key management operations
 * so we only need to worry about synchronization with other
 * uses that originate in the driver.
 */
void ath_key_update_begin(struct ieee80211com *ic)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	
	DPRINTF(sc, ATH_DEBUG_FATAL, "%lu %s (%s)\n", jiffies, __func__, dev->name);
	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s:\n", __func__);
	/*
	 * When called from the rx tasklet we cannot use
	 * tasklet_disable because it will block waiting
	 * for us to complete execution.
	 *
	 * XXX Using in_softirq is not right since we might
	 * be called from other soft irq contexts than
	 * ath_rx_tasklet.
	 * TODO: can cause bugs
	 */
#if 1
	if (!in_softirq())
		tasklet_disable(&sc->sc_rxtq);
#endif

	meshif_stop_queue(dev);	// TODO: find a way to not block mgmt frames

#if 0	
	if (sc->sc_rawdev_enabled) 
		meshif_stop_queue(&sc->sc_rawdev);
#endif

}

void ath_key_update_end(struct ieee80211com *ic)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ath_softc *sc = dev->priv;

	DPRINTF(sc, ATH_DEBUG_FATAL, "%lu %s (%s)\n", jiffies, __func__, dev->name);
	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s:\n", __func__);
	
	meshif_start_queue(dev);
#if 0	
	if (sc->sc_rawdev_enabled) 
		meshif_start_queue(&sc->sc_rawdev);
#endif

#if 1
	if (!in_softirq())		/* NB: see above */
		tasklet_enable(&sc->sc_rxtq);
#endif
}

