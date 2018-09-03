/*
* $Id: meshwu_lib.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef	__MESHWU_LIB_H__
#define	__MESHWU_LIB_H__


#include "mesh_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

#if __ARM_IXP__
#else
/* Various versions information */
/* Recommended Wireless Extension version */
#define WE_VERSION	21
/* Maximum forward compatibility built in this version of WT */
#define WE_MAX_VERSION	21
/* Version of Wireless Tools */
#define WT_VERSION	29
#endif


#define KILO					1e3
#define MEGA				1e6
#define GIGA					1e9

/* For doing log10/exp10 without libm */
#define LOG10_MAGIC			1.25892541179

typedef struct meshw_statistics		iwstats;
typedef struct meshw_range		iwrange;
typedef struct meshw_param		iwparam;
typedef struct meshw_freq			iwfreq;
typedef struct meshw_quality		iwqual;
typedef struct meshw_priv_args	iwprivargs;
typedef struct sockaddr			sockaddr;

/* Structure for storing all wireless information for each device
 * This is a cut down version of the one above, containing only
 * the things *truly* needed to configure a card.
 * Don't add other junk, I'll remove it... */
typedef struct meshwu_config
{
	char				name[MESHNAMSIZ + 1];	/* Wireless/protocol name */
	
	int				has_nwid;
	iwparam			nwid;
	
	int				has_freq;
#if __ARM_IXP__
	int				freq;		/* KHz */
#else
	double			freq;
#endif
	int				freq_flags;
	
	int				has_key;
	unsigned char		key[MESHW_ENCODING_TOKEN_MAX];	/* Encoding key used */
	int				key_size;		/* Number of bytes */
	int				key_flags;		/* Various flags */

	int				has_essid;
	int				essid_on;
	char				essid[MESHW_ESSID_MAX_SIZE + 1];	/* ESSID (extended network) */

	int				has_mode;
	int				mode;			/* Operation mode */
} meshwu_config;

/* Structure for storing all wireless information for each device
 * This is pretty exhaustive... */
typedef struct meshwu_info
{
	struct meshwu_config	b;	/* Basic information */

	int					has_sens;
	iwparam				sens;			/* sensitivity */

	int					has_nickname;
	char					nickname[MESHW_ESSID_MAX_SIZE + 1]; /* NickName */

	int					has_ap_addr;
	sockaddr				ap_addr;		/* Access point address */

	int					has_bitrate;
	iwparam				bitrate;		/* Bit rate in bps */

	int					has_rts;
	iwparam				rts;			/* RTS threshold in bytes */

	int					has_frag;
	iwparam				frag;			/* Fragmentation threshold in bytes */

	int					has_power;
	iwparam				power;			/* Power management parameters */

	int					has_txpower;
	iwparam				txpower;		/* Transmit Power in dBm */

	int					has_retry;
	iwparam				retry;			/* Retry limit or lifetime */

	/* Stats */
	iwstats				stats;
	int					has_stats;
	iwrange				range;
	int					has_range;

	/* Auth params for WPA/802.1x/802.11i */
	int					auth_key_mgmt;
	int					has_auth_key_mgmt;
	int					auth_cipher_pairwise;
	int					has_auth_cipher_pairwise;
	int					auth_cipher_group;
	int					has_auth_cipher_group;
} meshwu_info;

/* Structure for storing an entry of a wireless scan.
 * This is only a subset of all possible information, the flexible
 * structure of scan results make it impossible to capture all
 * information in such a static structure. */
typedef struct meshwu_scan
{
	/* Linked list */
	struct meshwu_scan		*next;

	/* Cell identifiaction */
	int						has_ap_addr;
	sockaddr					ap_addr;		/* Access point address */

	/* Other information */
	struct meshwu_config		b;	/* Basic information */
	iwstats					stats;			/* Signal strength */
	int						has_stats;
} meshwu_scan;

/** Context used for non-blocking scan. */
typedef struct meshwu_scan_head
{
	meshwu_scan *	result;		/* Result of the scan */
	int				retry;		/* Retry level */
} meshwu_scan_head;

/* Structure used for parsing event streams, such as Wireless Events and scan results */
typedef struct meshwu_stream_descr
{
	char		*end;		/* End of the stream */
	char		*current;	/* Current event in stream of events */
	char		*value;		/* Current value in event */
} meshwu_stream_descr;

/* Prototype for handling display of each single interface on the
 * system - see iw_enum_devices() */
typedef int (*meshwu_enum_handler)(int devFd, char *ifname, char *args[],int count);

/* Describe a modulation */
typedef struct meshwu_modul_descr
{
	unsigned int		mask;		/* Modulation bitmask */
	char				cmd[8];		/* Short name */
	char				*verbose;	/* Verbose description */
} meshwu_modul_descr;


int		meshwu_dbm2mwatt(int in);
int		meshwu_mwatt2dbm(int in);


char 	*meshwu_sawap_ntop(const struct sockaddr *sap,char *buf);
int 		meshwu_in_addr(int devFd, const char *ifname, char *bufp, struct sockaddr *sap);
int		meshwu_check_mac_addr_type(int devFd, const char *ifname);
int		meshwu_check_if_addr_type(int devFd, const char *ifname);

#if __ARM_IXP__
void meshwu_print_freq_value(char *buffer,int buflen, int freq);
void meshwu_print_freq(char *buffer,int buflen,int freq, int channel,int freq_flags);
int meshwu_freq_to_channel(int freq, const struct meshw_range *range);
int meshwu_channel_to_freq(int channel, const struct meshw_range *range);

#else
void meshwu_print_freq_value(char *buffer, int buflen,  double freq);
void meshwu_print_freq(char *buffer,int buflen,double freq, int channel,int freq_flags);
int 	meshwu_freq_to_channel(double freq,const struct meshw_range *range);
int	meshwu_channel_to_freq(int channel,double *pfreq, const struct meshw_range *range);
void meshwu_float2freq(double in, iwfreq *out);
double 	meshwu_freq2float(const iwfreq *in);
#endif
void meshwu_print_bitrate(char *buffer,int buflen,int bitrate);
void meshwu_print_txpower(char *buffer,int buflen, struct meshw_param *txpower);
void meshwu_print_stats(char *buffer,int buflen,const iwqual *qual,const iwrange *range,int has_range);
void meshwu_print_key(char *	buffer,int buflen,const unsigned char *key,int key_size,int key_flags);
void meshwu_print_pm_value(char *buffer,int buflen,int value,int flags,int we_version);
void meshwu_print_pm_mode(char *buffer, int buflen,int flags);
void meshwu_print_retry_value(char *buffer,int buflen,int value,int flags, int we_version);
void meshwu_print_timeval(char *buffer,int buflen, const struct timeval *timev,const struct timezone *tz);

int meshwu_get_basic_config(int	 devFd, const char *ifname, meshwu_config *info);
int meshwu_set_basic_config(int	devFd, const char *ifname, meshwu_config *info);
int meshwu_get_priv_size(int args);
int meshwu_get_priv_info(int	devFd, const char *ifname, iwprivargs **ppriv);
int meshwu_get_range_info(int devFd,  const char *ifname, iwrange *range);
int meshwu_get_stats(int	devFd,const char *ifname,iwstats *stats, const iwrange *range, int has_range);

int meshwu_in_key_full(int devFd, const char *ifname,const char *input,unsigned char *key, __u16 *flags);

void meshwu_init_event_stream(struct meshwu_stream_descr *stream,char *data,  int len);
int meshwu_extract_event_stream(struct meshwu_stream_descr *stream,struct meshw_event *iwe,int we_version);

#define ifr_name	ifr_ifrn.ifrn_name	/* interface name 	*/

#define	MESHU_SET_INFO(devFd, ifname,request,pwrq )	do{ 	\
	strncpy(pwrq->ifr_name, ifname, MESHNAMSIZ);				\
	return(ioctl(devFd, request, pwrq));						\
}while(0)
static inline int meshwu_set_ext(int devFd,const char *ifname, int	request,	struct meshw_req *	pwrq)		/* Fixed part of the request */
{
  /* Set device name */
  strncpy(pwrq->ifr_name, ifname, MESHNAMSIZ);
  /* Do the request */
  return(ioctl(devFd, request, pwrq));
}

/* Wrapper to extract some Wireless Parameter out of the driver */
#define	MESHU_GET_INFO(devFd, ifname,request,pwrq )	do{ 	\
	strncpy(pwrq->ifr_name, ifname, MESHNAMSIZ);				\
	return(ioctl(devFd, request, pwrq));						\
}while(0)

static inline int meshwu_get_ext(int devFd, const char *ifname, int request, struct meshw_req *pwrq)
{
	strncpy(pwrq->ifr_name, ifname, MESHNAMSIZ);
	return(ioctl(devFd, request, pwrq));
	/* Set device name */
}

static inline char *meshwu_saether_ntop(const struct sockaddr *sap, char* bufp)
{
	meshu_ether_ntop((const struct ether_addr *) sap->sa_data, bufp);
	return bufp;
}

/* Input an Ethernet Socket Address and convert to binary. */
static inline int meshwu_saether_aton(const char *bufp, struct sockaddr *sap)
{
	sap->sa_family = ARPHRD_ETHER;
	return meshu_ether_aton(bufp, (struct ether_addr *) sap->sa_data);
}

/* Create an Ethernet broadcast address */
static inline void meshwu_broad_ether(struct sockaddr *sap)
{
	sap->sa_family = ARPHRD_ETHER;
	memset((char *) sap->sa_data, 0xFF, ETH_ALEN);
}

/* Create an Ethernet NULL address */
static inline void meshwu_null_ether(struct sockaddr *sap)
{
	sap->sa_family = ARPHRD_ETHER;
	memset((char *) sap->sa_data, 0x00, ETH_ALEN);
}

/* Compare two ethernet addresses */
static inline int meshwu_ether_cmp(const struct ether_addr* eth1, const struct ether_addr* eth2)
{
	return memcmp(eth1, eth2, sizeof(*eth1));
}



/* not defined constant in our IOCTL files */
/* Frequency flags */
#define IW_FREQ_AUTO		0x00	/* Let the driver decides */
#define IW_FREQ_FIXED		0x01	/* Force a specific value */

/* Modulations bitmasks */
#define IW_MODUL_ALL		0x00000000	/* Everything supported */
#define IW_MODUL_FH		0x00000001	/* Frequency Hopping */
#define IW_MODUL_DS		0x00000002	/* Original Direct Sequence */
#define IW_MODUL_CCK		0x00000004	/* 802.11b : 5.5 + 11 Mb/s */
#define IW_MODUL_11B		(IW_MODUL_DS | IW_MODUL_CCK)
#define IW_MODUL_PBCC		0x00000008	/* TI : 5.5 + 11 + 22 Mb/s */
#define IW_MODUL_OFDM_A		0x00000010	/* 802.11a : 54 Mb/s */
#define IW_MODUL_11A		(IW_MODUL_OFDM_A)
#define IW_MODUL_11AB		(IW_MODUL_11B | IW_MODUL_11A)
#define IW_MODUL_OFDM_G		0x00000020	/* 802.11g : 54 Mb/s */
#define IW_MODUL_11G		(IW_MODUL_11B | IW_MODUL_OFDM_G)
#define IW_MODUL_11AG		(IW_MODUL_11G | IW_MODUL_11A)
#define IW_MODUL_TURBO		0x00000040	/* ATH : bonding, 108 Mb/s */
/* In here we should define MIMO stuff. Later... */
#define IW_MODUL_CUSTOM		0x40000000	/* Driver specific */

/* Bitrate flags available */
#define IW_BITRATE_TYPE		0x00FF	/* Type of value */
#define IW_BITRATE_UNICAST	0x0001	/* Maximum/Fixed unicast bitrate */
#define IW_BITRATE_BROADCAST	0x0002	/* Fixed broadcast bitrate */

/* Modes as human readable strings */
extern const char * const	meshwu_operation_mode[];
#define MESHWU_NUM_OPER_MODE		7

/* Modulations as human readable strings */
extern const struct meshwu_modul_descr	meshwu_modul_list[];
#define IW_SIZE_MODUL_LIST	16


#if __ARM_IXP__
/* for atheros driver */
#define	KILO_INT		1000
#define	MEGA_INT		(KILO_INT*KILO_INT)
#define	GIGA_INT		(KILO_INT*MEGA_INT)

#define	GET_MHZ(value, iwfreq)	\
	do{ 	\
	value = iwfreq.m/KILO_INT;value= value*10/KILO_INT;	\
		}while(0);

#define	GET_KHZ(value, iwfreq)	\
	do{ int i =0; value = iwfreq.m/KILO_INT;	\
		for(i=0;i<iwfreq.e; i++) value = value*10; \
		}while(0)

#define	SET_HZ(hz, iwfreq )	\
	do{iwfreq.e = 1;iwfreq.m = hz*10;}while(0)

#endif/* __ARM_IXP__ */

#define 	trace			printf( "%s_%s()_%d lines\n", __FILE__, __FUNCTION__, __LINE__ )

#ifdef __cplusplus
}
#endif

#endif

