/*
* $Id: phy_ath.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#ifndef __PHY_ATH_H__
#define __PHY_ATH_H__

#define	AR_DEBUG

#ifdef AR_DEBUG
#define	IFF_DUMPPKTS(sc, _m) \
	((sc->sc_debug & _m) || ieee80211_msg_dumppkts(&sc->sc_ic))
enum {
	ATH_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	ATH_DEBUG_XMIT_DESC	= 0x00000002,	/* xmit descriptors */
	ATH_DEBUG_RECV		= 0x00000004,	/* basic recv operation */
	ATH_DEBUG_RECV_DESC	= 0x00000008,	/* recv descriptors */
	ATH_DEBUG_RATE		= 0x00000010,	/* rate control */
	ATH_DEBUG_RESET		= 0x00000020,	/* reset processing */
	ATH_DEBUG_MODE		= 0x00000040,	/* mode init/setup */
	ATH_DEBUG_BEACON 	= 0x00000080,	/* beacon handling */
	ATH_DEBUG_WATCHDOG 	= 0x00000100,	/* watchdog timeout */
	ATH_DEBUG_INTR		= 0x00001000,	/* ISR */
	ATH_DEBUG_TX_PROC	= 0x00002000,	/* tx ISR proc */
	ATH_DEBUG_RX_PROC	= 0x00004000,	/* rx ISR proc */
	ATH_DEBUG_BEACON_PROC	= 0x00008000,	/* beacon ISR proc */
	ATH_DEBUG_CALIBRATE	= 0x00010000,	/* periodic calibration */
	ATH_DEBUG_KEYCACHE	= 0x00020000,	/* key cache management */
	ATH_DEBUG_STATE		= 0x00040000,	/* 802.11 state transitions */
	ATH_DEBUG_NODE		= 0x00080000,	/* node management */
	ATH_DEBUG_LED		= 0x00100000,	/* led management */
	ATH_DEBUG_FATAL		= 0x80000000,	/* fatal errors */
	ATH_DEBUG_ANY		= 0xffffffff
};
void ath_printrxbuf(struct ath_buf *bf, int);
void ath_printtxbuf(struct ath_buf *bf, int);

#define	DPRINTF(sc, _m, _fmt, ...) do {		\
	if (sc->sc_debug & _m)					\
		printk(_fmt, __VA_ARGS__);			\
		} while (0)

#define	KEYPRINTF(sc, ix, hk, mac) do {				\
	if (sc->sc_debug & ATH_DEBUG_KEYCACHE)			\
		ath_keyprint(__func__, ix, hk, mac);		\
		} while (0)
		
#else
#define	IFF_DUMPPKTS(sc, _m)	0
#define	DPRINTF(sc, _m, _fmt, ...)
#define	KEYPRINTF(sc, k, ix, mac)
#endif

/* unalligned little endian access */     
#define LE_READ_2(p)							\
	((u_int16_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)							\
	((u_int32_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8) |	\
	  (((u_int8_t *)(p))[2] << 16) | (((u_int8_t *)(p))[3] << 24)))

enum
{
	ATH_LED_TX,
	ATH_LED_RX,
	ATH_LED_POLL,
};

int	_phy_init(MESH_DEVICE *);
int	_phy_reset(MESH_DEVICE *);

int	_phy_stop(MESH_DEVICE *);

void	_phy_initkeytable(struct ath_softc *);
int	ath_key_alloc(struct ieee80211com *,	const struct ieee80211_key *);
int	ath_key_delete(struct ieee80211com *, const struct ieee80211_key *);
int	ath_key_set(struct ieee80211com *, const struct ieee80211_key *,	const u_int8_t mac[IEEE80211_ADDR_LEN]);
void	ath_key_update_begin(struct ieee80211com *);
void	ath_key_update_end(struct ieee80211com *);

void	_phy_setslottime(struct ath_softc *);

int	ath_beacon_alloc(struct ath_softc *, struct ieee80211_node *);
void	ath_beacon_tasklet(MESH_DEVICE *);
void	ath_beacon_free(struct ath_softc *);
void	ath_beacon_config(struct ath_softc *);


int	_phy_rxbuf_init(struct ath_softc *, struct ath_buf *);

void	ath_setdefantenna(struct ath_softc *, u_int);
void	ath_rx_tasklet(TQUEUE_ARG data);
struct ath_txq *ath_txq_setup(struct ath_softc*, int qtype, int subtype);
int	ath_tx_setup(struct ath_softc *, int, int);
int	ath_wme_update(struct ieee80211com *);
void	ath_tx_cleanupq(struct ath_softc *, struct ath_txq *);
void	ath_tx_cleanup(struct ath_softc *);
int	_phy_start(struct sk_buff *, MESH_DEVICE *);
int	ath_tx_setup(struct ath_softc *, int ac, int txq);
int	ath_tx_start(MESH_DEVICE *, struct ieee80211_node *,struct ath_buf *, struct sk_buff *);

void	ath_draintxq(struct ath_softc *);

MESH_DEVICE_STATS *_phy_getstats(MESH_DEVICE *);

extern struct meshw_handler_def phy_meshw_handler_def;

#if 0
void	ath_led_event(struct ath_softc *, int);
void     ath_led_off(unsigned long arg);
#endif

void	_phy_update_txpow(struct ath_softc *);


void	ath_setcurmode(struct ath_softc *, enum ieee80211_phymode);

#ifdef CONFIG_SYSCTL
void	ath_dynamic_sysctl_register(struct ath_softc *);
void	ath_dynamic_sysctl_unregister(struct ath_softc *);
#endif /* CONFIG_SYSCTL */


/*
 * For packet capture, define the same physical layer packet header 
 * structure as used in the wlan-ng driver 
 */
enum
{
	DIDmsg_lnxind_wlansniffrm		= 0x00000044,
	DIDmsg_lnxind_wlansniffrm_hosttime	= 0x00010044,
	DIDmsg_lnxind_wlansniffrm_mactime	= 0x00020044,
	DIDmsg_lnxind_wlansniffrm_channel	= 0x00030044,
	DIDmsg_lnxind_wlansniffrm_rssi		= 0x00040044,
	DIDmsg_lnxind_wlansniffrm_sq		= 0x00050044,
	DIDmsg_lnxind_wlansniffrm_signal	= 0x00060044,
	DIDmsg_lnxind_wlansniffrm_noise		= 0x00070044,
	DIDmsg_lnxind_wlansniffrm_rate		= 0x00080044,
	DIDmsg_lnxind_wlansniffrm_istx		= 0x00090044,
	DIDmsg_lnxind_wlansniffrm_frmlen	= 0x000A0044
};

enum
{
	P80211ENUM_msgitem_status_no_value	= 0x00
};

enum
{
	P80211ENUM_truth_false			= 0x00,
	P80211ENUM_truth_true			= 0x01
};

typedef struct
{
	u_int32_t did;
	u_int16_t status;
	u_int16_t len;
	u_int32_t data;
} p80211item_uint32_t;

typedef struct
{
	u_int32_t msgcode;
	u_int32_t msglen;
#define WLAN_DEVNAMELEN_MAX 16
	u_int8_t devname[WLAN_DEVNAMELEN_MAX];
	p80211item_uint32_t hosttime;
	p80211item_uint32_t mactime;
	p80211item_uint32_t channel;
	p80211item_uint32_t rssi;
	p80211item_uint32_t sq;
	p80211item_uint32_t signal;
	p80211item_uint32_t noise;
	p80211item_uint32_t rate;
	p80211item_uint32_t istx;
	p80211item_uint32_t frmlen;
} wlan_ng_prism2_header;


#ifdef HAS_VMAC

// register with VMAC 
void ath_vmac_register(struct ath_softc* sc);
// unregister from VMAC
void ath_vmac_unregister(struct ath_softc* sc);

// ath_tx_raw_start works like ath_tx_start except it doesn't
// do encapsulation...
int ath_tx_raw_start(MESH_DEVICE *, struct ieee80211_node *, struct ath_buf *, struct sk_buff *);

// ath_cu_softmac_handle_rx,txdone are used to handle packet rx
// or txdone signals, either in the top half (set "intop" to 1)
// or bottom half (set "intop" to 0)
int ath_cu_softmac_handle_rx(MESH_DEVICE* dev,int intop);
int ath_cu_softmac_handle_txdone(MESH_DEVICE* dev, int intop);


// The ath_cu_softmac_*_tasklet functions are optionally scheduled to
// run as tasklets if there's too much stuff to do right away.
void ath_cu_softmac_rx_tasklet(TQUEUE_ARG data);
void ath_cu_softmac_txdone_tasklet(TQUEUE_ARG data);
void ath_cu_softmac_work_tasklet(TQUEUE_ARG data);
// ath_cu_softmac_set_phocus_state directs the state of
// the "phocus" antenna if attached.
void vmac_ath_set_phocus_state(u_int16_t state,int16_t settle);

// Encapsulate/decapsulate the required extra header gunk
//static struct sk_buff* vmac_ath_encapsulate(void *data,struct sk_buff* skb);
//static struct sk_buff* vmac_ath_decapsulate(struct ath_softc* sc,struct sk_buff* skb);
// returns length of header or -1 if unknown packet type
int ath_cu_softmac_getheaderlen(struct ath_softc*,struct sk_buff*);


//int ath_vmac_mac_start(struct sk_buff* skb, struct net_device *dev);

#endif

extern int ath_countrycode;	/* country code */
extern int ath_regdomain;			/* regulatory domain */

extern int ath_dwelltime;		/* 5 channels/second */
extern int ath_calinterval;		/* calibrate every 30 secs */
extern int ath_outdoor;		/* enable outdoor use */
extern int ath_xchanmode;		/* enable extended channels */

extern int countrycode ;
extern int outdoor ;
extern int xchanmode ;

/* in ***_net.c */
void _phy_init_mesh_dev(MESH_DEVICE *dev);

/* in ***_ioctrl.c */
int _phy_ioctl_diag(struct ath_softc *sc, struct ath_diag *ad);

/* in ath_pci.c */
extern	int ath_ioctl_ethtool(struct ath_softc *sc, int cmd, void *addr);

u_int32_t _phy_calcrxfilter(struct ath_softc *sc, enum ieee80211_state state);

#ifdef AR_DEBUG
extern int ath_debug;
#endif

#endif

