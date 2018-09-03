/*
* $Id: wlan_if_media.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 

#ifndef	__WLAN_IF_MEDIA_H__
#define	__WLAN_IF_MEDIA_H__

/*
 * Prototypes and definitions for BSD/OS-compatible network interface
 * media selection.
 *
 * Where it is safe to do so, this code strays slightly from the BSD/OS
 * design.  Software which uses the API (device drivers, basically)
 * shouldn't notice any difference.
 *
 * Many thanks to Matt Thomas for providing the information necessary
 * to implement this interface.
 */

struct wlan_ifmediareq
{
	char		wlan_ifm_name[MESHNAMSIZ];	/* if name, e.g. "en0" */
	int		wlan_ifm_current;		/* current media options */
	int		wlan_ifm_mask;		/* don't care mask */
	int		wlan_ifm_status;		/* media status */
	int		wlan_ifm_active;		/* active options */
	int		wlan_ifm_count;		/* # entries in ifm_ulist array */
	int		*wlan_ifm_ulist;		/* media words */
};

#define	SIOCSIFMEDIA	_IOWR('i', 55, struct ifreq)	/* set net media */
#define	SIOCGIFMEDIA	_IOWR('i', 56, struct wlan_ifmediareq) /* get net media */

#ifdef __KERNEL__

#include <sys/queue.h>

/*
 * Driver callbacks for media status and change requests.
 */

typedef	int (*wlan_ifm_change_cb_t)(MESH_DEVICE *);
typedef	void (*wlan_ifm_stat_cb_t)(MESH_DEVICE *, struct wlan_ifmediareq *req);

/*
 * In-kernel representation of a single supported media type.
 */
struct wlan_ifmedia_entry
{
	LIST_ENTRY(wlan_ifmedia_entry) 	wlan_ifm_list;
	int								wlan_ifm_media;	/* description of this media attachment */
	int								wlan_ifm_data;	/* for driver-specific use */
	void								*wlan_ifm_aux;	/* for driver-specific use */
};

/*
 * One of these goes into a network interface's softc structure.
 * It is used to keep general media state.
 */
struct wlan_ifmedia
{
	int						wlan_ifm_mask;	/* mask of changes we don't care about */
	int						wlan_ifm_media;	/* current user-set media word */
	struct wlan_ifmedia_entry 	*wlan_ifm_cur;	/* currently selected media */
	ATH_LIST_HEAD(, wlan_ifmedia_entry) wlan_ifm_list; /* list of all supported media */
	wlan_ifm_change_cb_t			wlan_ifm_change;	/* media change driver callback, called when set new media */
	wlan_ifm_stat_cb_t				wlan_ifm_status;	/* media status driver callback, called when get media status */
};

/* Initialize an interface's struct if_media field. */
void	wlan_ifmedia_init(struct wlan_ifmedia *ifm, int dontcare_mask, wlan_ifm_change_cb_t change_callback, wlan_ifm_stat_cb_t status_callback);

/* Remove all mediums from a struct ifmedia.  */
void	wlan_ifmedia_removeall( struct wlan_ifmedia *ifm);

/* Add one supported medium to a struct ifmedia. */
void	wlan_ifmedia_add(struct wlan_ifmedia *ifm, int mword, int data, void *aux);


/* Set default media type on initialization. */
void	wlan_ifmedia_set(struct wlan_ifmedia *ifm, int mword);

/* Common ioctl function for getting/setting media, called by driver. */
//int	wlan_ifmedia_ioctl(MESH_DEVICE *, struct ifreq *ifr, struct wlan_ifmedia *ifm, u_long cmd);
int	wlan_ifmedia_ioctl(MESH_DEVICE *, struct wlan_ifmediareq *ifr, struct wlan_ifmedia *ifm, u_long cmd);

#endif /*_KERNEL */

/*
 * if_media Options word:
 *	Bits	Use
 *	----	-------
 *	0-4	Media variant
 *	5-7	Media type
 *	8-15	Type specific options
 *	16-18	Mode (for multi-mode devices)
 *	19	RFU
 *	20-27	Shared (global) options
 *	28-31	Instance
 */

/*
 * Ethernet
 */
#define	IFM_ETHER	0x00000020
#define	IFM_10_T	3		/* 10BaseT - RJ45 */
#define	IFM_10_2	4		/* 10Base2 - Thinnet */
#define	IFM_10_5	5		/* 10Base5 - AUI */
#define	IFM_100_TX	6		/* 100BaseTX - RJ45 */
#define	IFM_100_FX	7		/* 100BaseFX - Fiber */
#define	IFM_100_T4	8		/* 100BaseT4 - 4 pair cat 3 */
#define	IFM_100_VG	9		/* 100VG-AnyLAN */
#define	IFM_100_T2	10		/* 100BaseT2 */
#define	IFM_1000_SX	11		/* 1000BaseSX - multi-mode fiber */
#define	IFM_10_STP	12		/* 10BaseT over shielded TP */
#define	IFM_10_FL	13		/* 10BaseFL - Fiber */
#define	IFM_1000_LX	14		/* 1000baseLX - single-mode fiber */
#define	IFM_1000_CX	15		/* 1000baseCX - 150ohm STP */
#define	IFM_1000_T	16		/* 1000baseT - 4 pair cat 5 */
#define	IFM_HPNA_1	17		/* HomePNA 1.0 (1Mb/s) */
/* note 31 is the max! */

#define	IFM_ETH_MASTER	0x00000100	/* master mode (1000baseT) */

/*
 * Token ring
 */
#define	IFM_TOKEN	0x00000040
#define	IFM_TOK_STP4	3		/* Shielded twisted pair 4m - DB9 */
#define	IFM_TOK_STP16	4		/* Shielded twisted pair 16m - DB9 */
#define	IFM_TOK_UTP4	5		/* Unshielded twisted pair 4m - RJ45 */
#define	IFM_TOK_UTP16	6		/* Unshielded twisted pair 16m - RJ45 */
#define	IFM_TOK_STP100  7		/* Shielded twisted pair 100m - DB9 */
#define	IFM_TOK_UTP100  8		/* Unshielded twisted pair 100m - RJ45 */
#define	IFM_TOK_ETR	0x00000200	/* Early token release */
#define	IFM_TOK_SRCRT	0x00000400	/* Enable source routing features */
#define	IFM_TOK_ALLR	0x00000800	/* All routes / Single route bcast */
#define	IFM_TOK_DTR	0x00002000	/* Dedicated token ring */
#define	IFM_TOK_CLASSIC	0x00004000	/* Classic token ring */
#define	IFM_TOK_AUTO	0x00008000	/* Automatic Dedicate/Classic token ring */

/*
 * FDDI
 */
#define	IFM_FDDI	0x00000060
#define	IFM_FDDI_SMF	3		/* Single-mode fiber */
#define	IFM_FDDI_MMF	4		/* Multi-mode fiber */
#define	IFM_FDDI_UTP	5		/* CDDI / UTP */
#define	IFM_FDDI_DA	0x00000100	/* Dual attach / single attach */

/*
 * IEEE 802.11 Wireless
 */
#define	IFM_IEEE80211	0x00000080
/* NB: 0,1,2 are auto, manual, none defined below */
#define	IFM_IEEE80211_FH1	3	/* Frequency Hopping 1Mbps */
#define	IFM_IEEE80211_FH2	4	/* Frequency Hopping 2Mbps */
#define	IFM_IEEE80211_DS1	5	/* Direct Sequence 1Mbps */
#define	IFM_IEEE80211_DS2	6	/* Direct Sequence 2Mbps */
#define	IFM_IEEE80211_DS5	7	/* Direct Sequence 5.5Mbps */
#define	IFM_IEEE80211_DS11	8	/* Direct Sequence 11Mbps */
#define	IFM_IEEE80211_DS22	9	/* Direct Sequence 22Mbps */
#define	IFM_IEEE80211_OFDM6	10	/* OFDM 6Mbps */
#define	IFM_IEEE80211_OFDM9	11	/* OFDM 9Mbps */
#define	IFM_IEEE80211_OFDM12	12	/* OFDM 12Mbps */
#define	IFM_IEEE80211_OFDM18	13	/* OFDM 18Mbps */
#define	IFM_IEEE80211_OFDM24	14	/* OFDM 24Mbps */
#define	IFM_IEEE80211_OFDM36	15	/* OFDM 36Mbps */
#define	IFM_IEEE80211_OFDM48	16	/* OFDM 48Mbps */
#define	IFM_IEEE80211_OFDM54	17	/* OFDM 54Mbps */
#define	IFM_IEEE80211_OFDM72	18	/* OFDM 72Mbps */

#define	IFM_IEEE80211_ADHOC	0x00000100	/* Operate in Adhoc mode */
#define	IFM_IEEE80211_HOSTAP	0x00000200	/* Operate in Host AP mode */
#define	IFM_IEEE80211_IBSS	0x00000400	/* Operate in IBSS mode */
#define	IFM_IEEE80211_IBSSMASTER 0x00000800	/* Operate as an IBSS master */
#define	IFM_IEEE80211_TURBO	0x00001000	/* Operate in turbo mode */
#define	IFM_IEEE80211_MONITOR	0x00002000	/* Operate in monitor mode */

/* operating mode for multi-mode devices */
#define	IFM_IEEE80211_11A	0x00010000	/* 5Ghz, OFDM mode */
#define	IFM_IEEE80211_11B	0x00020000	/* Direct Sequence mode */
#define	IFM_IEEE80211_11G	0x00030000	/* 2Ghz, CCK mode */
#define	IFM_IEEE80211_FH	0x00040000	/* 2Ghz, GFSK mode */

/*
 * Shared media sub-types
 */
#define	IFM_AUTO	0		/* Autoselect best media */
#define	IFM_MANUAL	1		/* Jumper/dipswitch selects media */
#define	IFM_NONE	2		/* Deselect all media */

/*
 * Shared options
 */
#define	IFM_FDX		0x00100000	/* Force full duplex */
#define	IFM_HDX		0x00200000	/* Force half duplex */
#define	IFM_FLAG0	0x01000000	/* Driver defined flag */
#define	IFM_FLAG1	0x02000000	/* Driver defined flag */
#define	IFM_FLAG2	0x04000000	/* Driver defined flag */
#define	IFM_LOOP	0x08000000	/* Put hardware in loopback */

/*
 * Masks
 */
#define	IFM_NMASK	0x000000e0	/* Network type */
#define	IFM_TMASK	0x0000001f	/* Media sub-type */
#define	IFM_IMASK	0xf0000000	/* Instance */
#define	IFM_ISHIFT	28			/* Instance shift */
#define	IFM_OMASK	0x0000ff00	/* Type specific options */
#define	IFM_MMASK	0x00070000	/* Mode */
#define	IFM_MSHIFT	16			/* Mode shift */
#define	IFM_GMASK	0x0ff00000	/* Global options */

/*
 * Status bits
 */
#define	IFM_AVALID	0x00000001	/* Active bit valid */
#define	IFM_ACTIVE	0x00000002	/* Interface attached to working net */

/*
 * Macros to extract various bits of information from the media word.
 */
#define	IFM_TYPE(x)         		((x) & IFM_NMASK)
#define	IFM_SUBTYPE(x)      		((x) & IFM_TMASK)
#define	IFM_TYPE_OPTIONS(x) 	((x) & IFM_OMASK)
#define	IFM_INST(x)         		(((x) & IFM_IMASK) >> IFM_ISHIFT)
#define	IFM_OPTIONS(x)			((x) & (IFM_OMASK|IFM_GMASK))
#define	IFM_MODE(x)	    		((x) & IFM_MMASK)

#define	IFM_INST_MAX			IFM_INST(IFM_IMASK)

/*
 * Macro to create a media word.
 */
#define	IFM_MAKEWORD(type, subtype, options, instance)			\
	((type) | (subtype) | (options) | ((instance) << IFM_ISHIFT))
	
#define	IFM_MAKEMODE(mode) \
	(((mode) << IFM_MSHIFT) & IFM_MMASK)

/*
 * NetBSD extension not defined in the BSDI API.  This is used in various
 * places to get the canonical description for a given type/subtype.
 *
 * NOTE: all but the top-level type descriptions must contain NO whitespace!
 * Otherwise, parsing these in ifconfig(8) would be a nightmare.
 */
struct wlan_ifmedia_description
{
	int			wlan_ifmt_word;		/* word value; may be masked */
	const char 	*wlan_ifmt_string;	/* description */
};

#define	IFM_TYPE_DESCRIPTIONS {						\
	{ IFM_ETHER,		"Ethernet" },				\
	{ IFM_TOKEN,		"Token ring" },				\
	{ IFM_FDDI,		"FDDI" },				\
	{ IFM_IEEE80211,	"IEEE 802.11 Wireless Ethernet" },	\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_ETHERNET_DESCRIPTIONS {				\
	{ IFM_10_T,	"10baseT/UTP" },				\
	{ IFM_10_2,	"10base2/BNC" },				\
	{ IFM_10_5,	"10base5/AUI" },				\
	{ IFM_100_TX,	"100baseTX" },					\
	{ IFM_100_FX,	"100baseFX" },					\
	{ IFM_100_T4,	"100baseT4" },					\
	{ IFM_100_VG,	"100baseVG" },					\
	{ IFM_100_T2,	"100baseT2" },					\
	{ IFM_10_STP,	"10baseSTP" },					\
	{ IFM_10_FL,	"10baseFL" },					\
	{ IFM_1000_SX,	"1000baseSX" },					\
	{ IFM_1000_LX,	"1000baseLX" },					\
	{ IFM_1000_CX,	"1000baseCX" },					\
	{ IFM_1000_T,	"1000baseTX" },					\
	{ IFM_1000_T,	"1000baseT" },					\
	{ IFM_HPNA_1,	"homePNA" },					\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_ETHERNET_ALIASES {					\
	{ IFM_10_T,	"UTP" },					\
	{ IFM_10_T,	"10UTP" },					\
	{ IFM_10_2,	"BNC" },					\
	{ IFM_10_2,	"10BNC" },					\
	{ IFM_10_5,	"AUI" },					\
	{ IFM_10_5,	"10AUI" },					\
	{ IFM_100_TX,	"100TX" },					\
	{ IFM_100_T4,	"100T4" },					\
	{ IFM_100_VG,	"100VG" },					\
	{ IFM_100_T2,	"100T2" },					\
	{ IFM_10_STP,	"10STP" },					\
	{ IFM_10_FL,	"10FL" },					\
	{ IFM_1000_SX,	"1000SX" },					\
	{ IFM_1000_LX,	"1000LX" },					\
	{ IFM_1000_CX,	"1000CX" },					\
	{ IFM_1000_T,	"1000TX" },					\
	{ IFM_1000_T,	"1000T" },					\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_ETHERNET_OPTION_DESCRIPTIONS {			\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_TOKENRING_DESCRIPTIONS {				\
	{ IFM_TOK_STP4,	"DB9/4Mbit" },					\
	{ IFM_TOK_STP16, "DB9/16Mbit" },				\
	{ IFM_TOK_UTP4,	"UTP/4Mbit" },					\
	{ IFM_TOK_UTP16, "UTP/16Mbit" },				\
	{ IFM_TOK_STP100, "STP/100Mbit" },				\
	{ IFM_TOK_UTP100, "UTP/100Mbit" },				\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_TOKENRING_ALIASES {					\
	{ IFM_TOK_STP4,	"4STP" },					\
	{ IFM_TOK_STP16, "16STP" },					\
	{ IFM_TOK_UTP4,	"4UTP" },					\
	{ IFM_TOK_UTP16, "16UTP" },					\
	{ IFM_TOK_STP100, "100STP" },					\
	{ IFM_TOK_UTP100, "100UTP" },					\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_TOKENRING_OPTION_DESCRIPTIONS {			\
	{ IFM_TOK_ETR,	"EarlyTokenRelease" },				\
	{ IFM_TOK_SRCRT, "SourceRouting" },				\
	{ IFM_TOK_ALLR,	"AllRoutes" },					\
	{ IFM_TOK_DTR,	"Dedicated" },					\
	{ IFM_TOK_CLASSIC,"Classic" },					\
	{ IFM_TOK_AUTO,	" " },						\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_FDDI_DESCRIPTIONS {					\
	{ IFM_FDDI_SMF, "Single-mode" },				\
	{ IFM_FDDI_MMF, "Multi-mode" },					\
	{ IFM_FDDI_UTP, "UTP" },					\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_FDDI_ALIASES {					\
	{ IFM_FDDI_SMF,	"SMF" },					\
	{ IFM_FDDI_MMF,	"MMF" },					\
	{ IFM_FDDI_UTP,	"CDDI" },					\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_FDDI_OPTION_DESCRIPTIONS {				\
	{ IFM_FDDI_DA, "Dual-attach" },					\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_IEEE80211_DESCRIPTIONS {				\
	{ IFM_IEEE80211_FH1, "FH/1Mbps" },				\
	{ IFM_IEEE80211_FH2, "FH/2Mbps" },				\
	{ IFM_IEEE80211_DS1, "DS/1Mbps" },				\
	{ IFM_IEEE80211_DS2, "DS/2Mbps" },				\
	{ IFM_IEEE80211_DS5, "DS/5.5Mbps" },				\
	{ IFM_IEEE80211_DS11, "DS/11Mbps" },				\
	{ IFM_IEEE80211_DS22, "DS/22Mbps" },				\
	{ IFM_IEEE80211_OFDM6, "OFDM/6Mbps" },				\
	{ IFM_IEEE80211_OFDM9, "OFDM/9Mbps" },				\
	{ IFM_IEEE80211_OFDM12, "OFDM/12Mbps" },			\
	{ IFM_IEEE80211_OFDM18, "OFDM/18Mbps" },			\
	{ IFM_IEEE80211_OFDM24, "OFDM/24Mbps" },			\
	{ IFM_IEEE80211_OFDM36, "OFDM/36Mbps" },			\
	{ IFM_IEEE80211_OFDM48, "OFDM/48Mbps" },			\
	{ IFM_IEEE80211_OFDM54, "OFDM/54Mbps" },			\
	{ IFM_IEEE80211_OFDM72, "OFDM/72Mbps" },			\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_IEEE80211_ALIASES {					\
	{ IFM_IEEE80211_FH1, "FH1" },					\
	{ IFM_IEEE80211_FH2, "FH2" },					\
	{ IFM_IEEE80211_FH1, "FrequencyHopping/1Mbps" },		\
	{ IFM_IEEE80211_FH2, "FrequencyHopping/2Mbps" },		\
	{ IFM_IEEE80211_DS1, "DS1" },					\
	{ IFM_IEEE80211_DS2, "DS2" },					\
	{ IFM_IEEE80211_DS5, "DS5.5" },					\
	{ IFM_IEEE80211_DS11, "DS11" },					\
	{ IFM_IEEE80211_DS22, "DS22" },					\
	{ IFM_IEEE80211_DS1, "DirectSequence/1Mbps" },			\
	{ IFM_IEEE80211_DS2, "DirectSequence/2Mbps" },			\
	{ IFM_IEEE80211_DS5, "DirectSequence/5.5Mbps" },		\
	{ IFM_IEEE80211_DS11, "DirectSequence/11Mbps" },		\
	{ IFM_IEEE80211_DS22, "DirectSequence/22Mbps" },		\
	{ IFM_IEEE80211_OFDM6, "OFDM6" },				\
	{ IFM_IEEE80211_OFDM9, "OFDM9" },				\
	{ IFM_IEEE80211_OFDM12, "OFDM12" },				\
	{ IFM_IEEE80211_OFDM18, "OFDM18" },				\
	{ IFM_IEEE80211_OFDM24, "OFDM24" },				\
	{ IFM_IEEE80211_OFDM36, "OFDM36" },				\
	{ IFM_IEEE80211_OFDM48, "OFDM48" },				\
	{ IFM_IEEE80211_OFDM54, "OFDM54" },				\
	{ IFM_IEEE80211_OFDM72, "OFDM72" },				\
	{ IFM_IEEE80211_DS1, "CCK1" },					\
	{ IFM_IEEE80211_DS2, "CCK2" },					\
	{ IFM_IEEE80211_DS5, "CCK5.5" },				\
	{ IFM_IEEE80211_DS11, "CCK11" },				\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_IEEE80211_OPTION_DESCRIPTIONS {			\
	{ IFM_IEEE80211_ADHOC, "adhoc" },				\
	{ IFM_IEEE80211_HOSTAP, "hostap" },				\
	{ IFM_IEEE80211_IBSS, "ibss" },					\
	{ IFM_IEEE80211_IBSSMASTER, "ibss-master" },			\
	{ IFM_IEEE80211_TURBO, "turbo" },				\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_IEEE80211_MODE_DESCRIPTIONS {			\
	{ IFM_IEEE80211_11A, "11a" },					\
	{ IFM_IEEE80211_11B, "11b" },					\
	{ IFM_IEEE80211_11G, "11g" },					\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_SHARED_DESCRIPTIONS {				\
	{ IFM_AUTO,	"autoselect" },					\
	{ IFM_MANUAL,	"manual" },					\
	{ IFM_NONE,	"none" },					\
	{ 0, NULL },							\
}

#define	IFM_SUBTYPE_SHARED_ALIASES {					\
	{ IFM_AUTO,	"auto" },					\
	{ 0, NULL },							\
}

#define	IFM_SHARED_OPTION_DESCRIPTIONS {				\
	{ IFM_FDX,	"full-duplex" },				\
	{ IFM_HDX,	"half-duplex" },				\
	{ IFM_FLAG0,	"flag0" },					\
	{ IFM_FLAG1,	"flag1" },					\
	{ IFM_FLAG2,	"flag2" },					\
	{ IFM_LOOP,	"hw-loopback" },				\
	{ 0, NULL },							\
}

#endif

