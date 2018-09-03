/*
* $Id: wlan_ioctl.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#ifndef	__WLAN_IOCTL_H__
#define	__WLAN_IOCTL_H__

/* IEEE 802.11 ioctls. */

/* Per/node (station) statistics available when operating as an AP. */
struct ieee80211_nodestats
{
	u_int32_t		ns_rx_data;		/* rx data frames */
	u_int32_t		ns_rx_mgmt;		/* rx management frames */
	u_int32_t		ns_rx_ctrl;		/* rx control frames */
	
	u_int32_t		ns_rx_ucast;		/* rx unicast frames */
	u_int32_t		ns_rx_mcast;		/* rx multi/broadcast frames */
	
	u_int64_t		ns_rx_bytes;		/* rx data count (bytes) */
	u_int64_t		ns_rx_beacons;		/* rx beacon frames */
	u_int32_t		ns_rx_proberesp;	/* rx probe response frames */

#if WITH_MESH
	u_int32_t		ns_rx_mesh_mgmt;	/* rx action frame for mesh mgmt */
#endif

	u_int32_t		ns_rx_dup;		/* rx discard 'cuz dup */
	u_int32_t		ns_rx_noprivacy;	/* rx w/ wep but privacy off */
	u_int32_t		ns_rx_wepfail;		/* rx wep processing failed */
	u_int32_t		ns_rx_demicfail;	/* rx demic failed */
	u_int32_t		ns_rx_decap;		/* rx decapsulation failed */
	u_int32_t		ns_rx_defrag;		/* rx defragmentation failed */
	u_int32_t		ns_rx_disassoc;		/* rx disassociation */
	u_int32_t		ns_rx_deauth;		/* rx deauthentication */
	u_int32_t		ns_rx_decryptcrc;	/* rx decrypt failed on crc */
	u_int32_t		ns_rx_unauth;		/* rx on unauthorized port */
	u_int32_t		ns_rx_unencrypted;	/* rx unecrypted w/ privacy */

	u_int32_t		ns_tx_data;		/* tx data frames */
	u_int32_t		ns_tx_mgmt;		/* tx management frames */
	u_int32_t		ns_tx_ucast;		/* tx unicast frames */
	u_int32_t		ns_tx_mcast;		/* tx multi/broadcast frames */
	u_int64_t		ns_tx_bytes;		/* tx data count (bytes) */
	u_int32_t		ns_tx_probereq;		/* tx probe request frames */

#if WITH_MESH
	u_int32_t		ns_tx_mesh_mgmt;		/* tx action frames for mesg mgmt */
#endif

	u_int32_t		ns_tx_novlantag;	/* tx discard 'cuz no tag */
	u_int32_t		ns_tx_vlanmismatch;	/* tx discard 'cuz bad tag */

	u_int32_t		ns_ps_discard;		/* ps discard 'cuz of age */

	/* MIB-related state */
	u_int32_t		ns_tx_assoc;		/* [re]associations */
	u_int32_t		ns_tx_assoc_fail;	/* [re]association failures */
	u_int32_t		ns_tx_auth;		/* [re]authentications */
	u_int32_t		ns_tx_auth_fail;	/* [re]authentication failures*/
	u_int32_t		ns_tx_deauth;		/* deauthentications */
	u_int32_t		ns_tx_deauth_code;	/* last deauth reason */
	u_int32_t		ns_tx_disassoc;		/* disassociations */
	u_int32_t		ns_tx_disassoc_code;	/* last disassociation reason */
};

/* Summary statistics. */
struct ieee80211_stats
{
	u_int32_t	is_rx_badversion;	/* rx frame with bad version */
	u_int32_t	is_rx_tooshort;		/* rx frame too short */
	u_int32_t	is_rx_wrongbss;		/* rx from wrong bssid */
	u_int32_t	is_rx_dup;		/* rx discard 'cuz dup */
	u_int32_t	is_rx_wrongdir;		/* rx w/ wrong direction */
	u_int32_t	is_rx_mcastecho;	/* rx discard 'cuz mcast echo */
	u_int32_t	is_rx_notassoc;		/* rx discard 'cuz sta !assoc */
	u_int32_t	is_rx_noprivacy;	/* rx w/ wep but privacy off */
	u_int32_t	is_rx_unencrypted;	/* rx w/o wep and privacy on */
	u_int32_t	is_rx_wepfail;		/* rx wep processing failed */
	u_int32_t	is_rx_decap;		/* rx decapsulation failed */
	u_int32_t	is_rx_mgtdiscard;	/* rx discard mgt frames */
	u_int32_t	is_rx_ctl;		/* rx discard ctrl frames */
	u_int32_t	is_rx_beacon;		/* rx beacon frames */
	u_int32_t	is_rx_rstoobig;		/* rx rate set truncated */
	u_int32_t	is_rx_elem_missing;	/* rx required element missing*/
	u_int32_t	is_rx_elem_toobig;	/* rx element too big */
	u_int32_t	is_rx_elem_toosmall;	/* rx element too small */
	u_int32_t	is_rx_elem_unknown;	/* rx element unknown */
	u_int32_t	is_rx_badchan;		/* rx frame w/ invalid chan */
	u_int32_t	is_rx_chanmismatch;	/* rx frame chan mismatch */
	u_int32_t	is_rx_nodealloc;	/* rx frame dropped */
	u_int32_t	is_rx_ssidmismatch;	/* rx frame ssid mismatch  */
	u_int32_t	is_rx_auth_unsupported;	/* rx w/ unsupported auth alg */
	u_int32_t	is_rx_auth_fail;	/* rx sta auth failure */
	u_int32_t	is_rx_auth_countermeasures;/* rx auth discard 'cuz CM */
	u_int32_t	is_rx_assoc_bss;	/* rx assoc from wrong bssid */
	u_int32_t	is_rx_assoc_notauth;	/* rx assoc w/o auth */
	u_int32_t	is_rx_assoc_capmismatch;/* rx assoc w/ cap mismatch */
	u_int32_t	is_rx_assoc_norate;	/* rx assoc w/ no rate match */
	u_int32_t	is_rx_assoc_badwpaie;	/* rx assoc w/ bad WPA IE */
	u_int32_t	is_rx_deauth;		/* rx deauthentication */
	u_int32_t	is_rx_disassoc;		/* rx disassociation */
	u_int32_t	is_rx_badsubtype;	/* rx frame w/ unknown subtype*/
	u_int32_t	is_rx_nobuf;		/* rx failed for lack of buf */
	u_int32_t	is_rx_decryptcrc;	/* rx decrypt failed on crc */
	u_int32_t	is_rx_ahdemo_mgt;	/* rx discard ahdemo mgt frame*/
	u_int32_t	is_rx_bad_auth;		/* rx bad auth request */
	u_int32_t	is_rx_unauth;		/* rx on unauthorized port */
	u_int32_t	is_rx_badkeyid;		/* rx w/ incorrect keyid */
	u_int32_t	is_rx_ccmpreplay;	/* rx seq# violation (CCMP) */
	u_int32_t	is_rx_ccmpformat;	/* rx format bad (CCMP) */
	u_int32_t	is_rx_ccmpmic;		/* rx MIC check failed (CCMP) */
	u_int32_t	is_rx_tkipreplay;	/* rx seq# violation (TKIP) */
	u_int32_t	is_rx_tkipformat;	/* rx format bad (TKIP) */
	u_int32_t	is_rx_tkipmic;		/* rx MIC check failed (TKIP) */
	u_int32_t	is_rx_tkipicv;		/* rx ICV check failed (TKIP) */
	u_int32_t	is_rx_badcipher;	/* rx failed 'cuz key type */
	u_int32_t	is_rx_nocipherctx;	/* rx failed 'cuz key !setup */
	u_int32_t	is_rx_acl;		/* rx discard 'cuz acl policy */
	u_int32_t	is_tx_nobuf;		/* tx failed for lack of buf */
	u_int32_t	is_tx_nonode;		/* tx failed for no node */
	u_int32_t	is_tx_unknownmgt;	/* tx of unknown mgt frame */
	u_int32_t	is_tx_badcipher;	/* tx failed 'cuz key type */
	u_int32_t	is_tx_nodefkey;		/* tx failed 'cuz no defkey */
	u_int32_t	is_tx_noheadroom;	/* tx failed 'cuz no space */
	u_int32_t	is_scan_active;		/* active scans started */
	u_int32_t	is_scan_passive;	/* passive scans started */
	u_int32_t	is_node_timeout;	/* nodes timed out inactivity */
	u_int32_t	is_crypto_nomem;	/* no memory for crypto ctx */
	u_int32_t	is_crypto_tkip;		/* tkip crypto done in s/w */
	u_int32_t	is_crypto_tkipenmic;	/* tkip en-MIC done in s/w */
	u_int32_t	is_crypto_tkipdemic;	/* tkip de-MIC done in s/w */
	u_int32_t	is_crypto_tkipcm;	/* tkip counter measures */
	u_int32_t	is_crypto_ccmp;		/* ccmp crypto done in s/w */
	u_int32_t	is_crypto_wep;		/* wep crypto done in s/w */
	u_int32_t	is_crypto_setkey_cipher;/* cipher rejected key */
	u_int32_t	is_crypto_setkey_nokey;	/* no key index for setkey */
	u_int32_t	is_crypto_delkey;	/* driver key delete failed */
	u_int32_t	is_crypto_badcipher;	/* unknown cipher */
	u_int32_t	is_crypto_nocipher;	/* cipher not available */
	u_int32_t	is_crypto_attachfail;	/* cipher attach failed */
	u_int32_t	is_crypto_swfallback;	/* cipher fallback to s/w */
	u_int32_t	is_crypto_keyfail;	/* driver key alloc failed */
	u_int32_t	is_crypto_enmicfail;	/* en-MIC failed */
	u_int32_t	is_ibss_capmismatch;	/* merge failed-cap mismatch */
	u_int32_t	is_ibss_norate;		/* merge failed-rate mismatch */
	u_int32_t	is_ps_unassoc;		/* ps-poll for unassoc. sta */
	u_int32_t	is_ps_badaid;		/* ps-poll w/ incorrect aid */
	u_int32_t	is_ps_qempty;		/* ps-poll w/ nothing to send */
};

/*
 * Max size of optional information elements.  We artificially
 * constrain this; it's limited only by the max frame size (and
 * the max parameter size of the wireless extensions).
 */
#define	IEEE80211_MAX_OPT_IE	256

/*
 * WPA/RSN get/set key request.  Specify the key/cipher
 * type and whether the key is to be used for sending and/or
 * receiving.  The key index should be set only when working
 * with global keys (use IEEE80211_KEYIX_NONE for ``no index'').
 * Otherwise a unicast/pairwise key is specified by the bssid
 * (on a station) or mac address (on an ap).  They key length
 * must include any MIC key data; otherwise it should be no
 more than IEEE80211_KEYBUF_SIZE.
 */
struct ieee80211req_key
{
	u_int8_t	ik_type;	/* key/cipher type */
	u_int8_t	ik_pad;
	u_int16_t	ik_keyix;	/* key index */
	u_int8_t	ik_keylen;	/* key length in bytes */
	u_int8_t	ik_flags;
/* NB: IEEE80211_KEY_XMIT and IEEE80211_KEY_RECV defined elsewhere */
#define	IEEE80211_KEY_DEFAULT	0x80	/* default xmit key */
	u_int8_t	ik_macaddr[IEEE80211_ADDR_LEN];
	u_int64_t	ik_keyrsc;	/* key receive sequence counter */
	u_int64_t	ik_keytsc;	/* key transmit sequence counter */
	u_int8_t	ik_keydata[IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE];
};

/*
 * Delete a key either by index or address.  Set the index
 * to IEEE80211_KEYIX_NONE when deleting a unicast key.
 */
struct ieee80211req_del_key
{
	u_int8_t	idk_keyix;	/* key index */
	u_int8_t	idk_macaddr[IEEE80211_ADDR_LEN];
};

/*
 * MLME state manipulation request.  IEEE80211_MLME_ASSOC
 * only makes sense when operating as a station.  The other
 * requests can be used when operating as a station or an
 * ap (to effect a station).
 */
struct ieee80211req_mlme
{
	u_int8_t	im_op;		/* operation to perform */
#define	IEEE80211_MLME_ASSOC				1	/* associate station */
#define	IEEE80211_MLME_DISASSOC			2	/* disassociate station */
#define	IEEE80211_MLME_DEAUTH			3	/* deauthenticate station */
#define	IEEE80211_MLME_AUTHORIZE			4	/* authorize station */
#define	IEEE80211_MLME_UNAUTHORIZE		5	/* unauthorize station */
#define	IEEE80211_MLME_CLEAR_STATS		6	/* clear station statistic */
	u_int16_t	im_reason;	/* 802.11 reason code */
	u_int8_t	im_macaddr[IEEE80211_ADDR_LEN];
};

/* MAC ACL operations */
enum
{
	IEEE80211_MACCMD_POLICY_OPEN	= 0,	/* set policy: no ACL's */
	IEEE80211_MACCMD_POLICY_ALLOW	= 1,	/* set policy: allow traffic */
	IEEE80211_MACCMD_POLICY_DENY	= 2,	/* set policy: deny traffic */
	IEEE80211_MACCMD_FLUSH			= 3,	/* flush ACL database */
	IEEE80211_MACCMD_DETACH			= 4	/* detach ACL policy */
};

/*
 * Set the active channel list.  Note this list is
 * intersected with the available channel list in
 * calculating the set of channels actually used in
 * scanning.
 */
struct ieee80211req_chanlist
{
	u_int8_t	ic_channels[IEEE80211_CHAN_BYTES];
};

/* Get the active channel list info. */
struct ieee80211req_chaninfo
{
	u_int	ic_nchans;
	struct ieee80211_channel ic_chans[IEEE80211_CHAN_MAX];
};

/* Retrieve the WPA/RSN information element for an associated station. */
struct ieee80211req_wpaie
{
	u_int8_t	wpa_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t	wpa_ie[IEEE80211_MAX_OPT_IE];
};

/* Retrieve per-node statistics. */
struct ieee80211req_sta_stats
{
	union
	{
		/* NB: explicitly force 64-bit alignment */
		u_int8_t	macaddr[IEEE80211_ADDR_LEN];
		u_int64_t	pad;
	} is_u;

	struct ieee80211_nodestats is_stats;
};

/*
 * Station information block; the mac address is used
 * to retrieve other data like stats, unicast key, etc.
 */
struct ieee80211req_sta_info
{
	u_int16_t	isi_len;		/* length (mult of 4) */
	u_int16_t	isi_freq;		/* MHz */
	u_int16_t	isi_flags;		/* channel flags */
	u_int16_t	isi_state;		/* state flags */
	u_int8_t	isi_authmode;		/* authentication algorithm */
	u_int8_t	isi_rssi;
	u_int8_t	isi_capinfo;		/* capabilities */
	u_int8_t	isi_erp;		/* ERP element */
	u_int8_t	isi_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t	isi_nrates;
						/* negotiated rates */
	u_int8_t	isi_rates[IEEE80211_RATE_MAXSIZE];
	u_int8_t	isi_txrate;		/* index to isi_rates[] */
	u_int16_t	isi_ie_len;		/* IE length */
	u_int16_t	isi_associd;		/* assoc response */
	u_int16_t	isi_txpower;		/* current tx power */
	u_int16_t	isi_vlan;		/* vlan tag */
	u_int16_t	isi_txseqs[17];		/* seq to be transmitted */
	u_int16_t	isi_rxseqs[17];		/* seq previous for qos frames*/
	u_int16_t	isi_inact;		/* inactivity timer */
	/* XXX frag state? */
	/* variable length IE data */
};

/*
 * Retrieve per-station information; to retrieve all
 * specify a mac address of ff:ff:ff:ff:ff:ff.
 */
struct ieee80211req_sta_req
{
	union
	{
		/* NB: explicitly force 64-bit alignment */
		u_int8_t	macaddr[IEEE80211_ADDR_LEN];
		u_int64_t	pad;
	} is_u;

	struct ieee80211req_sta_info info[1];	/* variable length */
};

/* Get/set per-station tx power cap. */
struct ieee80211req_sta_txpow
{
	u_int8_t	it_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t	it_txpow;
};

/*
 * WME parameters are set and return using i_val and i_len.
 * i_val holds the value itself.  i_len specifies the AC
 * and, as appropriate, then high bit specifies whether the
 * operation is to be applied to the BSS or ourself.
 */
#define	IEEE80211_WMEPARAM_SELF	0x0000		/* parameter applies to self */
#define	IEEE80211_WMEPARAM_BSS	0x8000		/* parameter applies to BSS */
#define	IEEE80211_WMEPARAM_VAL	0x7fff		/* parameter value */

/*
 * Wireless Extensions API, private ioctl interfaces.
 *
 * NB: Even-numbered ioctl numbers have set semantics and are privileged!
 *     (regardless of the incorrect comment in wireless.h!)
 */
#define	IEEE80211_IOCTL_SETPARAM		(MESHW_IO_FIRST_PRIV+0)
#define	IEEE80211_IOCTL_GETPARAM		(MESHW_IO_FIRST_PRIV+1)
#define	IEEE80211_IOCTL_SETKEY		(MESHW_IO_FIRST_PRIV+2)
#define	IEEE80211_IOCTL_DELKEY		(MESHW_IO_FIRST_PRIV+4)
#define	IEEE80211_IOCTL_SETMLME		(MESHW_IO_FIRST_PRIV+6)
#define	IEEE80211_IOCTL_SETOPTIE		(MESHW_IO_FIRST_PRIV+8)
#define	IEEE80211_IOCTL_GETOPTIE		(MESHW_IO_FIRST_PRIV+9)
#define	IEEE80211_IOCTL_ADDMAC		(MESHW_IO_FIRST_PRIV+10)
#define	IEEE80211_IOCTL_DELMAC		(MESHW_IO_FIRST_PRIV+12)
#define	IEEE80211_IOCTL_CHANLIST		(MESHW_IO_FIRST_PRIV+14)
#ifdef HAS_VMAC
#define 	IEEE80211_IOCTL_SETREG          	(MESHW_IO_FIRST_PRIV+16)
#define 	IEEE80211_IOCTL_GETREG          	(MESHW_IO_FIRST_PRIV+17)
#endif

enum
{
	IEEE80211_PARAM_TURBO				= 1,		/* turbo mode */
	IEEE80211_PARAM_MODE					= 2,		/* phy mode (11a, 11b, etc.) */
	IEEE80211_PARAM_AUTHMODE			= 3,		/* authentication mode */
	IEEE80211_PARAM_PROTMODE			= 4,		/* 802.11g protection */
	IEEE80211_PARAM_MCASTCIPHER			= 5,		/* multicast/default cipher */
	IEEE80211_PARAM_MCASTKEYLEN			= 6,		/* multicast key length */
	IEEE80211_PARAM_UCASTCIPHERS		= 7,		/* unicast cipher suites */
	IEEE80211_PARAM_UCASTCIPHER			= 8,		/* unicast cipher */
	IEEE80211_PARAM_UCASTKEYLEN			= 9,		/* unicast key length */
	IEEE80211_PARAM_WPA					= 10,	/* WPA mode (0,1,2) */
	IEEE80211_PARAM_ROAMING				= 12,	/* roaming mode */
	IEEE80211_PARAM_PRIVACY				= 13,	/* privacy invoked */
	IEEE80211_PARAM_COUNTERMEASURES	= 14,	/* WPA/TKIP countermeasures */
	IEEE80211_PARAM_DROPUNENCRYPTED	= 15,	/* discard unencrypted frames */
	IEEE80211_PARAM_DRIVER_CAPS			= 16,	/* driver capabilities */
	IEEE80211_PARAM_MACCMD				= 17,	/* MAC ACL operation */
	IEEE80211_PARAM_WME					= 18,	/* WME mode (on, off) */
	IEEE80211_PARAM_HIDESSID				= 19,	/* hide SSID mode (on, off) */
	IEEE80211_PARAM_APBRIDGE				= 20,	/* AP inter-sta bridging */
	IEEE80211_PARAM_KEYMGTALGS			= 21,	/* key management algorithms */
	IEEE80211_PARAM_RSNCAPS				= 22,	/* RSN capabilities */
	IEEE80211_PARAM_INACT				= 23,	/* station inactivity timeout */
	IEEE80211_PARAM_INACT_AUTH			= 24,	/* station auth inact timeout */
	IEEE80211_PARAM_INACT_INIT			= 25		/* station init inact timeout */
};

/* SIOCDEVPRIVATE is 0x89F0 and defined in linux/socketios.h  */
#define	SIOCG80211STATS					(SIOCDEVPRIVATE+2)
/* NB: require in+out parameters so cannot use wireless extensions, yech */
#define	IEEE80211_IOCTL_GETKEY			(SIOCDEVPRIVATE+3)
#define	IEEE80211_IOCTL_GETWPAIE			(SIOCDEVPRIVATE+4)
#define	IEEE80211_IOCTL_GETSTASTATS		(SIOCDEVPRIVATE+5)


#endif 

