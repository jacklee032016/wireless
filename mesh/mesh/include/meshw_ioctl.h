/*
 * $Id: meshw_ioctl.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 * Cloned from linux/wireless.h, lizhijie
 */

#ifndef	__MESH_WIRELESS_IOCTL_H__
#define __MESH_WIRELESS_IOCTL_H__

/***************************** INCLUDES *****************************/

//#include <linux/types.h>		/* for "caddr_t" et al		*/
#include <linux/socket.h>		/* for "struct sockaddr" et al	*/

#ifndef	ARPHRD_ETHER
#define ARPHRD_ETHER 	1		/* Ethernet 10Mbps		*/
#endif


/* Wireless Identification */
#define MESHW_IO_W_COMMIT			0x8B00		/* Commit pending changes to driver */
#define MESHW_IO_R_NAME				0x8B01		/* get name == wireless protocol */
/* MESHW_IO_R_NAME is used to verify the presence of Wireless Extensions.
 * Common values : "IEEE 802.11-DS", "IEEE 802.11-FH", "IEEE 802.11b"...
 * Don't put the name of your driver there, it's useless. */

/* Basic operations */
#define MESHW_IO_W_NWID				0x8B02		/* set network id (pre-802.11) */
#define MESHW_IO_R_NWID				0x8B03		/* get network id (the cell) */
#define MESHW_IO_W_FREQ				0x8B04		/* set channel/frequency (Hz) */
#define MESHW_IO_R_FREQ				0x8B05		/* get channel/frequency (Hz) */
#define MESHW_IO_W_MODE				0x8B06		/* set operation mode */
#define MESHW_IO_R_MODE				0x8B07		/* get operation mode */
#define MESHW_IO_W_SENS				0x8B08		/* set sensitivity (dBm) */
#define MESHW_IO_R_SENS				0x8B09		/* get sensitivity (dBm) */

/* Informative stuff */
#define MESHW_IO_W_RANGE				0x8B0A		/* Unused */
#define MESHW_IO_R_RANGE				0x8B0B		/* Get range of parameters */
#define MESHW_IO_W_PRIV				0x8B0C		/* Unused */
#define MESHW_IO_R_PRIV				0x8B0D		/* get private ioctl interface info */
#define MESHW_IO_W_STATS				0x8B0E		/* Unused */
#define MESHW_IO_R_STATS				0x8B0F		/* Get /proc/net/wireless stats */
/* MESHW_IO_R_STATS is strictly used between user space and the kernel, and
 * is never passed to the driver (i.e. the driver will never see it). */

/* Spy support (statistics per MAC address - used for Mobile IP support) */
#define MESHW_IO_W_SPY				0x8B10		/* set spy addresses */
#define MESHW_IO_R_SPY					0x8B11		/* get spy info (quality of link) */
#define MESHW_IO_W_SPY_THR			0x8B12		/* set spy threshold (spy event) */
#define MESHW_IO_R_SPY_THR			0x8B13		/* get spy threshold */

/* Access Point manipulation */
#define MESHW_IO_W_AP_MAC			0x8B14		/* set access point MAC addresses */
#define MESHW_IO_R_AP_MAC				0x8B15		/* get access point MAC addresses */
#define MESHW_IO_R_AP_LIST			0x8B17		/* Deprecated in favor of scanning */
#define MESHW_IO_W_SCAN				0x8B18		/* trigger scanning (list cells) */
#define MESHW_IO_R_SCAN				0x8B19		/* get scanning results */

/* 802.11 specific support */
#define MESHW_IO_W_ESSID				0x8B1A		/* set ESSID (network name) */
#define MESHW_IO_R_ESSID				0x8B1B		/* get ESSID */
#define MESHW_IO_W_NICKNAME			0x8B1C		/* set node name/nickname */
#define MESHW_IO_R_NICKNAME			0x8B1D		/* get node name/nickname */
/* As the ESSID and NICKN are strings up to 32 bytes long, it doesn't fit
 * within the 'meshw_req' structure, so we need to use the 'data' member to
 * point to a string in user space, like it is done for RANGE... */

/* Other parameters useful in 802.11 and some other devices */
#define MESHW_IO_W_RATE				0x8B20		/* set default bit rate (bps) */
#define MESHW_IO_R_RATE				0x8B21		/* get default bit rate (bps) */
#define MESHW_IO_W_RTS				0x8B22		/* set RTS/CTS threshold (bytes) */
#define MESHW_IO_R_RTS					0x8B23		/* get RTS/CTS threshold (bytes) */
#define MESHW_IO_W_FRAG_THR			0x8B24		/* set fragmentation thr (bytes) */
#define MESHW_IO_R_FRAG_THR			0x8B25		/* get fragmentation thr (bytes) */
#define MESHW_IO_W_TX_POW			0x8B26		/* set transmit power (dBm) */
#define MESHW_IO_R_TX_POW				0x8B27		/* get transmit power (dBm) */
#define MESHW_IO_W_RETRY				0x8B28		/* set retry limits and lifetime */
#define MESHW_IO_R_RETRY				0x8B29		/* get retry limits and lifetime */

/* Encoding stuff (scrambling, hardware security, WEP...) */
#define MESHW_IO_W_ENCODE			0x8B2A		/* set encoding token & mode */
#define MESHW_IO_R_ENCODE				0x8B2B		/* get encoding token & mode */
/* Power saving stuff (power management, unicast and multicast) */
#define MESHW_IO_W_POWER_MGMT		0x8B2C		/* set Power Management settings */
#define MESHW_IO_R_POWER_MGMT		0x8B2D		/* get Power Management settings */


/* These 16 ioctl are wireless device private.
 * Each driver is free to use them for whatever purpose it chooses,
 * however the driver *must* export the description of those ioctls
 * with MESHW_IO_R_PRIV and *must* use arguments as defined below.
 * If you don't follow those rules, DaveM is going to hate you (reason :
 * it make mixed 32/64bit operation impossible).
 */
#define MESHW_IO_FIRST_PRIV			0x8BE0
#define MESHW_IO_LAST_PRIV			0x8BFF
/* Previously, we were using SIOCDEVPRIVATE, but we now have our
 * separate range because of collisions with other tools such as
 * 'mii-tool'.
 * We now have 32 commands, so a bit more space ;-).
 * Also, all 'odd' commands are only usable by root and don't return the
 * content of ifr/iwr to user (but you are not obliged to use the set/get
 * convention, just use every other two command).
 * And I repeat : you are not obliged to use them with iwspy, but you
 * must be compliant with it.
 */

/* The first and the last (range) */
#define MESHW_IO_FIRST					0x8B00
#define MESHW_IO_LAST					MESHW_IO_LAST_PRIV		/* 0x8BFF */

/* Even : get (world access), odd : set (root access) */
#define MESHW_IS_WRITE(cmd)			(!((cmd) & 0x1))
#define MESHW_IS_READ(cmd)			((cmd) & 0x1)

/* Those are *NOT* ioctls, do not issue request on them !!! */
/* Most events use the same identifier as ioctl requests */

#define MESHW_EV_TX_DROP				0x8C00		/* Packet dropped to excessive retry */
#define MESHW_EV_QUAL					0x8C01		/* Quality part of statistics (scan) */
#define MESHW_EV_CUSTOM				0x8C02		/* Driver specific ascii string */
#define MESHW_EV_NODE_REGISTERED		0x8C03		/* Discovered a new node (AP mode) */
#define MESHW_EV_NODE_EXPIRED		0x8C04		/* Expired a node (AP mode) */

#define MESHW_EV_FIRST					0x8C00

/*
 * The following is used with MESHW_IO_R_PRIV. It allow a driver to define
 * the interface (name, type of data) for its private ioctl.
 * Privates ioctl are MESHW_IO_FIRST_PRIV -> MESHW_IO_LAST_PRIV
 */

#define MESHW_PRIV_TYPE_MASK			0x7000	/* Type of arguments */
#define MESHW_PRIV_TYPE_NONE			0x0000
#define MESHW_PRIV_TYPE_BYTE			0x1000	/* Char as number */
#define MESHW_PRIV_TYPE_CHAR			0x2000	/* Char as character */
#define MESHW_PRIV_TYPE_INT			0x4000	/* 32 bits int */
#define MESHW_PRIV_TYPE_FLOAT			0x5000	/* struct meshw_freq */
#define MESHW_PRIV_TYPE_ADDR			0x6000	/* struct sockaddr */

#define MESHW_PRIV_SIZE_FIXED			0x0800	/* Variable or fixed number of args */

#define MESHW_PRIV_SIZE_MASK			0x07FF	/* Max number of those args */

/*
 * Note : if the number of args is fixed and the size < 16 octets,
 * instead of passing a pointer we will put args in the meshw_req struct...
 */

/* Maximum frequencies in the range struct */
#define MESHW_MAX_FREQUENCIES		32
/* Note : if you have something like 80 frequencies,
 * don't increase this constant and don't fill the frequency list.
 * The user will be able to set by channel anyway... */

/* Maximum bit rates in the range struct */
#define MESHW_MAX_BITRATES			32

/* Maximum tx powers in the range struct */
#define MESHW_MAX_TXPOWER			8
/* Note : if you more than 8 TXPowers, just set the max and min or
 * a few of them in the struct meshw_range. */

/* Maximum of address that you may set with SPY */
#define MESHW_MAX_SPY					8

/* Maximum of address that you may get in the  list of access points in range */
#define MESHW_MAX_AP					64

/* Maximum size of the ESSID and NICKN strings */
#define MESHW_ESSID_MAX_SIZE			32

/* Modes of operation */
#define MESHW_MODE_AUTO				0	/* Let the driver decides */
#define MESHW_MODE_ADHOC				1	/* Single cell network */
#define MESHW_MODE_INFRA				2	/* Multi cell network, roaming, ... */
#define MESHW_MODE_MASTER			3	/* Synchronisation master or Access Point */
#define MESHW_MODE_REPEAT			4	/* Wireless Repeater (forwarder) */
#define MESHW_MODE_SECOND			5	/* Secondary master/repeater (backup) */
#define MESHW_MODE_MONITOR			6	/* Passive monitor (listen only) */

/* copy from WE21 */
/* Statistics flags (bitmask in updated) */
#define MESHW_QUAL_QUAL_UPDATED		0x01	/* Value was updated since last read */
#define MESHW_QUAL_LEVEL_UPDATED		0x02
#define MESHW_QUAL_NOISE_UPDATED	0x04
#define MESHW_QUAL_ALL_UPDATED		0x07
#define MESHW_QUAL_DBM				0x08	/* Level + Noise are dBm */
#define MESHW_QUAL_QUAL_INVALID		0x10	/* Driver doesn't provide value */
#define MESHW_QUAL_LEVEL_INVALID		0x20
#define MESHW_QUAL_NOISE_INVALID		0x40
#define MESHW_QUAL_ALL_INVALID		0x70

/* Maximum number of size of encoding token available
 * they are listed in the range structure */
#define MESHW_MAX_ENCODING_SIZES		8

/* Maximum size of the encoding token in bytes */
#define MESHW_ENCODING_TOKEN_MAX	32	/* 256 bits (for now) */

/* Flags for encoding (along with the token) */
#define MESHW_ENCODE_INDEX			0x00FF	/* Token index (if needed) */
#define MESHW_ENCODE_FLAGS			0xFF00	/* Flags defined below */
#define MESHW_ENCODE_MODE			0xF000	/* Modes defined below */
#define MESHW_ENCODE_DISABLED		0x8000	/* Encoding disabled */
#define MESHW_ENCODE_ENABLED			0x0000	/* Encoding enabled */
#define MESHW_ENCODE_RESTRICTED		0x4000	/* Refuse non-encoded packets */
#define MESHW_ENCODE_OPEN			0x2000	/* Accept non-encoded packets */
#define MESHW_ENCODE_NOKEY			0x0800  /* Key is write only, so not present */
#define MESHW_ENCODE_TEMP			0x0400  /* Temporary key */

/* Power management flags available (along with the value, if any) */
#define MESHW_POWER_ON				0x0000	/* No details... */
#define MESHW_POWER_TYPE				0xF000	/* Type of parameter */
#define MESHW_POWER_PERIOD			0x1000	/* Value is a period/duration of  */
#define MESHW_POWER_TIMEOUT			0x2000	/* Value is a timeout (to go asleep) */
#define MESHW_POWER_SAVING			0x4000	/* Value is relative (how aggressive), copy from WE21 */
#define MESHW_POWER_MODE				0x0F00	/* Power Management mode */
#define MESHW_POWER_UNICAST_R		0x0100	/* Receive only unicast messages */
#define MESHW_POWER_MULTICAST_R		0x0200	/* Receive only multicast messages */
#define MESHW_POWER_ALL_R				0x0300	/* Receive all messages though PM */
#define MESHW_POWER_FORCE_S			0x0400	/* Force PM procedure for sending unicast */
#define MESHW_POWER_REPEATER			0x0800	/* Repeat broadcast messages in PM period */
#define MESHW_POWER_MODIFIER			0x000F	/* Modify a parameter */
#define MESHW_POWER_MIN				0x0001	/* Value is a minimum  */
#define MESHW_POWER_MAX				0x0002	/* Value is a maximum */
#define MESHW_POWER_RELATIVE			0x0004	/* Value is not in seconds/ms/us */

/* Transmit Power flags available */
#define MESHW_TXPOW_TYPE				0x00FF	/* Type of value */
#define MESHW_TXPOW_DBM				0x0000	/* Value is in dBm */
#define MESHW_TXPOW_MWATT			0x0001	/* Value is in mW */
#define MESHW_TXPOW_RELATIVE			0x0002	/* Value is in arbitrary units, copy from we21 */
#define MESHW_TXPOW_RANGE			0x1000	/* Range of value between min/max */

/* Retry limits and lifetime flags available */
#define MESHW_RETRY_ON				0x0000	/* No details... */
#define MESHW_RETRY_TYPE				0xF000	/* Type of parameter */
#define MESHW_RETRY_LIMIT				0x1000	/* Maximum number of retries*/
#define MESHW_RETRY_LIFETIME			0x2000	/* Maximum duration of retries in us */
#define MESHW_RETRY_MODIFIER			0x000F	/* Modify a parameter */
#define MESHW_RETRY_MIN				0x0001	/* Value is a minimum  */
#define MESHW_RETRY_MAX				0x0002	/* Value is a maximum */
#define MESHW_RETRY_RELATIVE			0x0004	/* Value is not in seconds/ms/us */
/* copy from we21 */
#define MESHW_RETRY_SHORT				0x0010	/* Value is for short packets  */
#define MESHW_RETRY_LONG				0x0020	/* Value is for long packets */

/* Scanning request flags */
#define MESHW_SCAN_DEFAULT			0x0000	/* Default scan of the driver */
#define MESHW_SCAN_ALL_ESSID			0x0001	/* Scan all ESSIDs */
#define MESHW_SCAN_THIS_ESSID			0x0002	/* Scan only this ESSID */
#define MESHW_SCAN_ALL_FREQ			0x0004	/* Scan all Frequencies */
#define MESHW_SCAN_THIS_FREQ			0x0008	/* Scan only this Frequency */
#define MESHW_SCAN_ALL_MODE			0x0010	/* Scan all Modes */
#define MESHW_SCAN_THIS_MODE			0x0020	/* Scan only this Mode */
#define MESHW_SCAN_ALL_RATE			0x0040	/* Scan all Bit-Rates */
#define MESHW_SCAN_THIS_RATE			0x0080	/* Scan only this Bit-Rate */
/* Maximum size of returned data */
#define MESHW_SCAN_MAX_DATA			4096	/* In bytes */

/* Max number of char in custom event - use multiple of them if needed */
#define MESHW_CUSTOM_MAX				256		/* In bytes */

/****************************** TYPES ******************************/

/* --------------------------- SUBTYPES --------------------------- */
/*	Generic format for most parameters that fit in an int  */
struct	meshw_param
{
	__s32		value;		/* The value of the parameter itself */
	__u8		fixed;		/* Hardware should not use auto select */
	__u8		disabled;	/* Disable the feature */
	__u16		flags;		/* Various specifc flags (if any) */
};

/*	For all data larger than 16 octets, we need to use a
 *	pointer to memory allocated in user space. */
struct	meshw_point
{
	caddr_t		pointer;		/* Pointer to the data  (in user space) */
	__u16		length;		/* number of fields or size in bytes */
	__u16		flags;		/* Optional params */
};

/*
 *	A frequency
 *	For numbers lower than 10^9, we encode the number in 'm' and
 *	set 'e' to 0
 *	For number greater than 10^9, we divide it by the lowest power
 *	of 10 to get 'm' lower than 10^9, with 'm'= f / (10^'e')...
 *	The power of 10 is in 'e', the result of the division is in 'm'.
 */
struct	meshw_freq
{
	__s32		m;		/* Mantissa */
	__s16		e;		/* Exponent */
	__u8		i;		/* List index (when in range struct) */
#if 0	
	__u8		pad;		/* Unused - just for alignement */
#else
	__u8		flags;		/* Flags (fixed/auto) */
#endif
};

/*	Quality of the link */
struct	meshw_quality
{
	__u8		qual;		/* link quality (%retries, SNR, %missed beacons or better...) */
	__u8		level;		/* signal level (dBm) */
	__u8		noise;		/* noise level (dBm) */
	__u8		updated;	/* Flags to know if updated */
};

#if 1
/*
 *	Packet discarded in the wireless adapter due to
 *	"wireless" specific problems...
 *	Note : the list of counter and statistics in net_device_stats
 *	is already pretty exhaustive, and you should use that first.
 *	This is only additional stats...
 */
struct	meshw_discarded
{
	__u32		nwid;		/* Rx : Wrong nwid/essid */
	__u32		code;		/* Rx : Unable to code/decode (WEP) */
	__u32		fragment;	/* Rx : Can't perform MAC reassembly */
	__u32		retries;	/* Tx : Max MAC retries num reached */
	__u32		misc;		/* Others cases */
};

/*
 *	Packet/Time period missed in the wireless adapter due to
 *	"wireless" specific problems...
 */
struct	meshw_missed
{
	__u32		beacon;		/* Missed beacons/superframe */
};
#endif

/*	Quality range (for spy threshold) */
struct	meshw_thrspy
{
	struct sockaddr			addr;		/* Source address (hw/mac) */
	struct meshw_quality		qual;		/* Quality of the link */
	struct meshw_quality		low;		/* Low threshold */
	struct meshw_quality		high;		/* High threshold */
};

/* ------------------------ WIRELESS STATS ------------------------ */
/*
 * Wireless statistics (used for /proc/net/wireless)
 */
struct	meshw_statistics
{
	__u16					status;		/* Status	 * - device dependent for now */

	struct meshw_quality		qual;		/* Quality of the link * (instant/mean/max) */
#if 1						 	
	struct meshw_discarded	discard;	/* Packet discarded counts */
	struct meshw_missed		miss;		/* Packet missed counts */
#endif	
};

/* ------------------------ IOCTL REQUEST ------------------------ */
/*
 * This structure defines the payload of an ioctl, and is used 
 * below.
 *
 * Note that this structure should fit on the memory footprint
 * of meshw_req (which is the same as ifreq), which mean a max size of
 * 16 octets = 128 bits. Warning, pointers might be 64 bits wide...
 * You should check this when increasing the structures defined
 * above in this file...
 */
union	meshw_req_data
{
	/* Config - generic */
	char						name[MESHNAMSIZ];
	/* Name : used to verify the presence of  wireless extensions.
	 * Name of the protocol/provider... */

	struct meshw_point		essid;		/* Extended network name */
	struct meshw_param		nwid;		/* network id (or domain - the cell) */
	struct meshw_freq		freq;		/* frequency or channel :
					 * 0-1000 = channel
					 * > 1000 = frequency in Hz */

	struct meshw_param		sens;		/* signal level threshold */
	struct meshw_param		bitrate;	/* default bit rate */
	struct meshw_param		txpower;	/* default transmit power */
	struct meshw_param		rts;		/* RTS threshold threshold */
	struct meshw_param		frag;		/* Fragmentation threshold */
	__u32					mode;		/* Operation mode */
	struct meshw_param		retry;		/* Retry limits & lifetime */

	struct meshw_point		encoding;	/* Encoding stuff : tokens */
	struct meshw_param		power;		/* PM duration/timeout */
	struct meshw_quality 		qual;		/* Quality part of statistics */

	/* sockaddr is in the format of 'sa_family, sa_data[14]' '*/
	struct sockaddr			ap_addr;	/* Access point address */
	struct sockaddr			addr;		/* Destination address (hw/mac) */

	struct meshw_param		param;		/* Other small parameters */
	struct meshw_point		data;		/* Other large parameters */
};

/*
 * The structure to exchange data for ioctl.
 * This structure is the same as 'struct ifreq', but (re)defined for
 * convenience...
 * Do I need to remind you about structure size (32 octets) ?
 */
struct	meshw_req 
{
	union
	{
		char	ifrn_name[MESHNAMSIZ];	/* if name, e.g. "eth0" */
	} ifr_ifrn;

	/* Data part (defined just above) */
	union	meshw_req_data	u;
};

/* -------------------------- IOCTL DATA -------------------------- */
/*
 *	For those ioctl which want to exchange mode data that what could
 *	fit in the above structure...
 */

/* Range of parameters */

struct	meshw_range
{
	/* Informative stuff (to choose between different interface) */
	__u32		throughput;	/* To give an idea... */
	/* In theory this value should be the maximum benchmarked
	 * TCP/IP throughput, because with most of these devices the
	 * bit rate is meaningless (overhead an co) to estimate how
	 * fast the connection will go and pick the fastest one.
	 * I suggest people to play with Netperf or any benchmark...
	 */

	/* NWID (or domain id) */
	__u32		min_nwid;	/* Minimal NWID we are able to set */
	__u32		max_nwid;	/* Maximal NWID we are able to set */

	/* Old Frequency (backward compat - moved lower ) */
	__u16		old_num_channels;
	__u8		old_num_frequency;
	/* Filler to keep "version" at the same offset */
	__s32		old_freq[6];

	/* signal level threshold range */
	__s32	sensitivity;

	/* Quality of link & SNR stuff */
	/* Quality range (link, level, noise)
	 * If the quality is absolute, it will be in the range [0 ; max_qual],
	 * if the quality is dBm, it will be in the range [max_qual ; 0].
	 * Don't forget that we use 8 bit arithmetics... */
	struct meshw_quality	max_qual;	/* Quality of the link */
	/* This should contain the average/typical values of the quality
	 * indicator. This should be the threshold between a "good" and
	 * a "bad" link (example : monitor going from green to orange).
	 * Currently, user space apps like quality monitors don't have any
	 * way to calibrate the measurement. With this, they can split
	 * the range between 0 and max_qual in different quality level
	 * (using a geometric subdivision centered on the average).
	 * I expect that people doing the user space apps will feedback
	 * us on which value we need to put in each driver... */
	struct meshw_quality	avg_qual;	/* Quality of the link */

	/* Rates */
	__u8		num_bitrates;	/* Number of entries in the list */
	__s32		bitrate[MESHW_MAX_BITRATES];	/* list, in bps */

	/* RTS threshold */
	__s32		min_rts;	/* Minimal RTS threshold */
	__s32		max_rts;	/* Maximal RTS threshold */

	/* Frag threshold */
	__s32		min_frag;	/* Minimal frag threshold */
	__s32		max_frag;	/* Maximal frag threshold */

	/* Power Management duration & timeout */
	__s32		min_pmp;	/* Minimal PM period */
	__s32		max_pmp;	/* Maximal PM period */
	__s32		min_pmt;	/* Minimal PM timeout */
	__s32		max_pmt;	/* Maximal PM timeout */
	__u16		pmp_flags;	/* How to decode max/min PM period */
	__u16		pmt_flags;	/* How to decode max/min PM timeout */
	__u16		pm_capa;	/* What PM options are supported */

	/* Encoder stuff */
	__u16	encoding_size[MESHW_MAX_ENCODING_SIZES];	/* Different token sizes */
	__u8	num_encoding_sizes;	/* Number of entry in the list */
	__u8	max_encoding_tokens;	/* Max number of tokens */
	/* For drivers that need a "login/passwd" form */
	__u8	encoding_login_index;	/* token index for login token */

	/* Transmit power */
	__u16		txpower_capa;	/* What options are supported */
	__u8		num_txpower;	/* Number of entries in the list */
	__s32		txpower[MESHW_MAX_TXPOWER];	/* list, in bps */

	/* Wireless Extension version info */
	__u8		we_version_compiled;	/* Must be WIRELESS_EXT */
	__u8		we_version_source;	/* Last update of source */

	/* Retry limits and lifetime */
	__u16		retry_capa;	/* What retry options are supported */
	__u16		retry_flags;	/* How to decode max/min retry limit */
	__u16		r_time_flags;	/* How to decode max/min retry life */
	__s32		min_retry;	/* Minimal number of retries */
	__s32		max_retry;	/* Maximal number of retries */
	__s32		min_r_time;	/* Minimal retry lifetime */
	__s32		max_r_time;	/* Maximal retry lifetime */

	/* Frequency */
	__u16		num_channels;	/* Number of channels [0; num - 1] */
	__u8		num_frequency;	/* Number of entry in the list */
	struct meshw_freq	freq[MESHW_MAX_FREQUENCIES];	/* list */
	/* Note : this frequency list doesn't need to fit channel numbers,
	 * because each entry contain its channel index */

	/* following field is copied from WE21,lzj */
	__u32		enc_capa;	/* IW_ENC_CAPA_* bit field */

	/* More power management stuff */
	__s32		min_pms;	/* Minimal PM saving */
	__s32		max_pms;	/* Maximal PM saving */
	__u16		pms_flags;	/* How to decode max/min PM saving */

	/* All available modulations for driver (hw may support less) */
	__s32		modul_capa;	/* IW_MODUL_* bit field */

	/* More bitrate stuff */
	__u32		bitrate_capa;	/* Types of bitrates supported */
};

/* Private ioctl interface information */
 
struct	meshw_priv_args
{
	__u32		cmd;		/* Number of the ioctl to issue */
	__u16		set_args;	/* Type and number of args */
	__u16		get_args;	/* Type and number of args */
	char		name[MESHNAMSIZ];	/* Name of the extension */
};

/* ----------------------- WIRELESS EVENTS ----------------------- */
/*
 * Wireless events are carried through the rtnetlink socket to user
 * space. They are encapsulated in the IFLA_WIRELESS field of
 * a RTM_NEWLINK message.
 */

/* A Wireless Event. Contains basically the same data as the ioctl... */
struct meshw_event
{
	__u16					len;			/* Real lenght of this stuff */
	__u16					cmd;			/* Wireless IOCTL */
	union meshw_req_data		u;		/* IOCTL fixed payload */
};

/* Size of the Event prefix (including padding and alignement junk) */
#define MESHW_EV_LCP_LEN			(sizeof(struct meshw_event) - sizeof(union meshw_req_data))
/* Size of the various events */
#define MESHW_EV_CHAR_LEN			(MESHW_EV_LCP_LEN + MESHNAMSIZ)
#define MESHW_EV_UINT_LEN			(MESHW_EV_LCP_LEN + sizeof(__u32))
#define MESHW_EV_FREQ_LEN			(MESHW_EV_LCP_LEN + sizeof(struct meshw_freq))
#define MESHW_EV_POINT_LEN		(MESHW_EV_LCP_LEN + sizeof(struct meshw_point))
#define MESHW_EV_PARAM_LEN		(MESHW_EV_LCP_LEN + sizeof(struct meshw_param))
#define MESHW_EV_ADDR_LEN			(MESHW_EV_LCP_LEN + sizeof(struct sockaddr))
#define MESHW_EV_QUAL_LEN			(MESHW_EV_LCP_LEN + sizeof(struct meshw_quality))

/* iw_point events are special. First, the payload (extra data) come at 
* the end of the event, so they are bigger than IW_EV_POINT_LEN. Second, 
* we omit the pointer, so start at an offset. */

#if 0
/* *	For all data larger than 16 octets, we need to use a 
*	pointer to memory allocated in user space. 
*/
struct	iw_point
{
	void __user	*pointer;	/* Pointer to the data  (in user space) */  
	__u16		length;		/* number of fields or size in bytes */  
	__u16		flags;		/* Optional params */
};

#define IW_EV_POINT_OFF (((char *) &(((struct iw_point *) NULL)->length)) - \
	(char *) NULL)

#define IW_EV_POINT_LEN	(MESHW_EV_LCP_LEN + sizeof(struct iw_point) - \
	IW_EV_POINT_OFF)
/* Note : in the case of meshw_point, the extra data will come at the end of the event */
#endif

#define		MESHW_MAIN_VERSION		0
#define		MESHW_MINOR_VERSION		5

#endif

