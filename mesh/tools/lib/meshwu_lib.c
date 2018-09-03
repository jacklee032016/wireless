/*
* $Id: meshwu_lib.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "meshwu_lib.h"

#define IW15_MAX_FREQUENCIES	16
#define IW15_MAX_BITRATES	8
#define IW15_MAX_TXPOWER	8
#define IW15_MAX_ENCODING_SIZES	8
#define IW15_MAX_SPY		8
#define IW15_MAX_AP		8

/*	Struct meshw_range up to WE-15 */
struct	iw15_range
{
	__u32		throughput;
	__u32		min_nwid;
	__u32		max_nwid;
	__u16		num_channels;
	__u8		num_frequency;
	struct meshw_freq	freq[IW15_MAX_FREQUENCIES];
	__s32		sensitivity;
	struct meshw_quality	max_qual;
	__u8		num_bitrates;
	__s32		bitrate[IW15_MAX_BITRATES];
	__s32		min_rts;
	__s32		max_rts;
	__s32		min_frag;
	__s32		max_frag;
	__s32		min_pmp;
	__s32		max_pmp;
	__s32		min_pmt;
	__s32		max_pmt;
	__u16		pmp_flags;
	__u16		pmt_flags;
	__u16		pm_capa;
	__u16		encoding_size[IW15_MAX_ENCODING_SIZES];
	__u8		num_encoding_sizes;
	__u8		max_encoding_tokens;
	__u16		txpower_capa;
	__u8		num_txpower;
	__s32		txpower[IW15_MAX_TXPOWER];
	__u8		we_version_compiled;
	__u8		we_version_source;
	__u16		retry_capa;
	__u16		retry_flags;
	__u16		r_time_flags;
	__s32		min_retry;
	__s32		max_retry;
	__s32		min_r_time;
	__s32		max_r_time;
	struct meshw_quality	avg_qual;
};

/* Union for all the versions of iwrange.
 * Fortunately, I mostly only add fields at the end, and big-bang reorganisations are few. */
union	iw_range_raw
{
	struct iw15_range			range15;	/* WE 9->15 */
	struct meshw_range		range;		/* WE 16->current */
};

#if __ARM_IXP__
#else
/* Offsets in meshw_range struct */
#define iwr15_off(f)	( ((char *) &(((struct iw15_range *) NULL)->f)) - \
			  (char *) NULL)
#define iwr_off(f)	( ((char *) &(((struct meshw_range *) NULL)->f)) - \
			  (char *) NULL)

/* Disable runtime version warning in meshwu_get_range_info() */
int	iw_ignore_version = 0;
#endif

/*
 * Get essential wireless config from the device driver
 * We will call all the classical wireless ioctl on the driver through
 * the socket to know what is supported and to get the settings...
 * Note : compare to the version in iwconfig, we extract only
 * what's *really* needed to configure a device...
 */
int meshwu_get_basic_config(int devFd, const char *ifname, meshwu_config *info)
{
	struct meshw_req		wrq;
	memset((char *) info, 0, sizeof(struct meshwu_config));

	/* Get wireless name */
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_NAME, &wrq) < 0)
		return(-1);
	else
	{
		strncpy(info->name, wrq.u.name, MESHNAMSIZ);
		info->name[MESHNAMSIZ] = '\0';
	}

	/* Get network ID */
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_NWID, &wrq) >= 0)
	{
		info->has_nwid = 1;
		memcpy(&(info->nwid), &(wrq.u.nwid), sizeof(iwparam));
	}

	/* Get frequency / channel */
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_FREQ, &wrq) >= 0)
	{
		info->has_freq = 1;
#if __ARM_IXP__
		GET_KHZ(info->freq , wrq.u.freq);
#else
		info->freq = meshwu_freq2float(&(wrq.u.freq));
#endif
		info->freq_flags = wrq.u.freq.flags;
	}

	/* Get encryption information */
	wrq.u.data.pointer = (caddr_t) info->key;
	wrq.u.data.length = MESHW_ENCODING_TOKEN_MAX;
	wrq.u.data.flags = 0;
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_ENCODE, &wrq) >= 0)
	{
		info->has_key = 1;
		info->key_size = wrq.u.data.length;
		info->key_flags = wrq.u.data.flags;
	}

	/* Get ESSID */
	wrq.u.essid.pointer = (caddr_t) info->essid;
	wrq.u.essid.length = MESHW_ESSID_MAX_SIZE + 1;
	wrq.u.essid.flags = 0;
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_ESSID, &wrq) >= 0)
	{
		info->has_essid = 1;
		info->essid_on = wrq.u.data.flags;
	}

	/* Get operation mode */
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_MODE, &wrq) >= 0)
	{
		info->mode = wrq.u.mode;
		if((info->mode < MESHWU_NUM_OPER_MODE) && (info->mode >= 0))
			info->has_mode = 1;
	}

	return(0);
}

/* Set essential wireless config in the device driver
 * We will call all the classical wireless ioctl on the driver through
 * the socket to know what is supported and to set the settings...
 * We support only the restricted set as above...
 */
int meshwu_set_basic_config(int devFd,const char *ifname,meshwu_config *info)
{
	struct meshw_req		wrq;
	int			ret = 0;

	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_NAME, &wrq) < 0)
		return(-2);

	/* Set the current mode of operation
	* Mode need to be first : some settings apply only in a specific mode (such as frequency).  */
	if(info->has_mode)
	{
		strncpy(wrq.ifr_name, ifname, MESHNAMSIZ);
		wrq.u.mode = info->mode;
		if(meshwu_get_ext(devFd, ifname, MESHW_IO_W_MODE, &wrq) < 0)
		{
			fprintf(stderr, "MESHW_IO_W_MODE: %s\n", strerror(errno));
			ret = -1;
		}
	}

	/* Set frequency / channel */
	if(info->has_freq)
	{
#if __ARM_IXP__
		SET_HZ(info->freq, wrq.u.freq);
#else
		meshwu_float2freq(info->freq, &(wrq.u.freq));
#endif
		if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_FREQ, &wrq) < 0)
		{
			fprintf(stderr, "MESHW_IO_W_FREQ: %s\n", strerror(errno));
			ret = -1;
		}
	}

	/* Set encryption information */
	if(info->has_key)
	{
		int		flags = info->key_flags;

		/* Check if there is a key index */
		if((flags & MESHW_ENCODE_INDEX) > 0)
		{/* Set the index */
			wrq.u.data.pointer = (caddr_t) NULL;
			wrq.u.data.flags = (flags & (MESHW_ENCODE_INDEX)) | MESHW_ENCODE_NOKEY;
			wrq.u.data.length = 0;

			if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_ENCODE, &wrq) < 0)
			{
				fprintf(stderr, "MESHW_IO_W_ENCODE(%d): %s\n",errno, strerror(errno));
				ret = -1;
			}
		}

		/* Mask out index to minimise probability of reject when setting key */
		flags = flags & (~MESHW_ENCODE_INDEX);

		/* Set the key itself (set current key in this case) */
		wrq.u.data.pointer = (caddr_t) info->key;
		wrq.u.data.length = info->key_size;
		wrq.u.data.flags = flags;

		/* Compatibility with WE<13 */
		if(flags & MESHW_ENCODE_NOKEY)
			wrq.u.data.pointer = NULL;

		if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_ENCODE, &wrq) < 0)
		{
			fprintf(stderr, "MESHW_IO_W_ENCODE(%d): %s\n", errno, strerror(errno));
			ret = -1;
		}
	}

	/* Set Network ID, if available (this is for non-802.11 cards) */
	if(info->has_nwid)
	{
		memcpy(&(wrq.u.nwid), &(info->nwid), sizeof(iwparam));
		wrq.u.nwid.fixed = 1;
		if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_NWID, &wrq) < 0)
		{
			fprintf(stderr, "MESHW_IO_W_NWID: %s\n", strerror(errno));
			ret = -1;
		}
	}

	/* Set ESSID (extended network), if available.
	* ESSID need to be last : most device re-perform the scanning/discovery
	* when this is set, and things like encryption keys are better be
	* defined if we want to discover the right set of APs/nodes. */
	if(info->has_essid)
	{

		wrq.u.essid.pointer = (caddr_t) info->essid;
		wrq.u.essid.length = strlen(info->essid);
		wrq.u.data.flags = info->essid_on;
		wrq.u.essid.length++;

		if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_ESSID, &wrq) < 0)
		{
			fprintf(stderr, "MESHW_IO_W_ESSID: %s\n", strerror(errno));
			ret = -1;
		}
	}

	return(ret);
}

/* Size (in bytes) of various events */
static const int priv_type_size[] =
{
	0,							/* MESHW_PRIV_TYPE_NONE */
	1,							/* MESHW_PRIV_TYPE_BYTE */
	1,							/* MESHW_PRIV_TYPE_CHAR */
	0,							/* Not defined */
	sizeof(__u32),					/* MESHW_PRIV_TYPE_INT */
	sizeof(struct meshw_freq),		/* MESHW_PRIV_TYPE_FLOAT */
	sizeof(struct sockaddr),		/* MESHW_PRIV_TYPE_ADDR */
	0,							/* Not defined */
};

/* Max size in bytes of an private argument. */
int meshwu_get_priv_size(int	args)
{
	int	num = args & MESHW_PRIV_SIZE_MASK;
	int	type = (args & MESHW_PRIV_TYPE_MASK) >> 12;

	return(num * priv_type_size[type]);
}

/*
 * Note : there is one danger using this function. If it return 0, you still need to free() the buffer. Beware.
 */
int meshwu_get_priv_info(int devFd, const char *ifname, iwprivargs **ppriv)
{
	struct meshw_req		wrq;
	iwprivargs 	*priv = NULL;	/* Not allocated yet */
	int			maxpriv = 64;// changed as 64, not 16; lzj 2006.09.25	/* Minimum for compatibility WE<13 */
	iwprivargs 	*newpriv;

	/* Some driver may return a very large number of ioctls. Some
	* others a very small number. We now use a dynamic allocation
	* of the array to satisfy everybody. Of course, as we don't know
	* in advance the size of the array, we try various increasing sizes. */

	do
	{/* (Re)allocate the buffer */
//		newpriv = realloc(priv, maxpriv * sizeof(priv[0]));
		newpriv = realloc(priv, maxpriv * sizeof(iwprivargs));
		if(newpriv == NULL)
		{
			fprintf(stderr, "%s: Allocation failed\n", __FUNCTION__);
			break;
		}
		priv = newpriv;

		/* Ask the driver if it's large enough */
		wrq.u.data.pointer = (caddr_t) priv;
		wrq.u.data.length = 64;//maxpriv;lzj 
		wrq.u.data.flags = 0;

		if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_PRIV, &wrq) >= 0)
		{
			/* Success. Pass the buffer by pointer */
			*ppriv = priv;
			/* Return the number of ioctls */
			return(wrq.u.data.length);
		}

		/* Only E2BIG means the buffer was too small, abort on other errors */
		if(errno != E2BIG)
		{
			/* Most likely "not supported". Don't barf. */
			break;
		}

		/* Failed. We probably need a bigger buffer. Check if the kernel
		* gave us any hints. */
		if(wrq.u.data.length > maxpriv)
			maxpriv = wrq.u.data.length;
		else
			maxpriv *= 2;
	}
	while(maxpriv < 1000);

	if(priv)
		free(priv);

	*ppriv = NULL;
	return(-1);
}

/* Get the range information out of the driver */
int meshwu_get_range_info(int devFd, const char *ifname, iwrange *range)
{
	struct meshw_req		wrq;
	char					buffer[sizeof(iwrange) * 2];
	union iw_range_raw 	*range_raw;

	bzero(buffer, sizeof(buffer));
	wrq.u.data.pointer = (caddr_t) buffer;
	wrq.u.data.length = sizeof(buffer);
	wrq.u.data.flags = 0;
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RANGE, &wrq) < 0)
		return(-1);

	/* Point to the buffer */
	range_raw = (union iw_range_raw *) buffer;

	/* For new versions, we can check the version directly, for old versions
	* we use magic. 300 bytes is a also magic number, don't touch... */
	if(wrq.u.data.length < 300)
	{
		/* That's v10 or earlier. Ouch ! Let's make a guess...*/
		range_raw->range.we_version_compiled = 9;
	}
#if __ARM_IXP__
#else
	/* Check how it needs to be processed */
	if(range_raw->range.we_version_compiled > 15)
#endif
	{ /* This is our native format, that's easy... */
		/* Copy stuff at the right place, ignore extra */
		memcpy((char *) range, buffer, sizeof(iwrange));
	}

#if __ARM_IXP__	
#else
	else
	{
		/* Zero unknown fields */
		bzero((char *) range, sizeof(struct meshw_range));
		/* Initial part unmoved */
		memcpy((char *) range, buffer, iwr15_off(num_channels));
		/* Frequencies pushed futher down towards the end */
		memcpy((char *) range + iwr_off(num_channels), buffer + iwr15_off(num_channels),  iwr15_off(sensitivity) - iwr15_off(num_channels));

		/* This one moved up */
		memcpy((char *) range + iwr_off(sensitivity), buffer + iwr15_off(sensitivity), iwr15_off(num_bitrates) - iwr15_off(sensitivity));
		/* This one goes after avg_qual */
		memcpy((char *) range + iwr_off(num_bitrates),buffer + iwr15_off(num_bitrates), iwr15_off(min_rts) - iwr15_off(num_bitrates));

		/* Number of bitrates has changed, put it after */
		memcpy((char *) range + iwr_off(min_rts), buffer + iwr15_off(min_rts), iwr15_off(txpower_capa) - iwr15_off(min_rts));

		/* Added encoding_login_index, put it after */
		memcpy((char *) range + iwr_off(txpower_capa), buffer + iwr15_off(txpower_capa), iwr15_off(txpower) - iwr15_off(txpower_capa));

		/* Hum... That's an unexpected glitch. Bummer. */
		memcpy((char *) range + iwr_off(txpower),buffer + iwr15_off(txpower), iwr15_off(avg_qual) - iwr15_off(txpower));

		/* Avg qual moved up next to max_qual */
		memcpy((char *) range + iwr_off(avg_qual), buffer + iwr15_off(avg_qual),  sizeof(struct meshw_quality));
	}

	/* We are now checking much less than we used to do, because we can
	* accomodate more WE version. But, there are still cases where things will break... */
	if(!iw_ignore_version)
	{/* We don't like very old version (unfortunately kernel 2.2.X) */
		if(range->we_version_compiled <= 10)
		{
			fprintf(stderr, "Warning: Driver for device %s has been compiled with an ancient version\n", ifname);
			fprintf(stderr, "of Wireless Extension, while this program support version 11 and later.\n");
			fprintf(stderr, "Some things may be broken...\n\n");
		}

		/* We don't like future versions of WE, because we can't cope with the unknown */
		if(range->we_version_compiled > WE_MAX_VERSION)
		{
			fprintf(stderr, "Warning: Driver for device %s has been compiled with version %d\n", ifname, range->we_version_compiled);
			fprintf(stderr, "of Wireless Extension, while this program supports up to version %d.\n", WE_VERSION);
			fprintf(stderr, "Some things may be broken...\n\n");
		}

		/* Driver version verification */
		if((range->we_version_compiled > 10) && (range->we_version_compiled < range->we_version_source))
			{
			fprintf(stderr, "Warning: Driver for device %s recommend version %d of Wireless Extension,\n", ifname, range->we_version_source);
			fprintf(stderr, "but has been compiled with version %d, therefore some driver features\n", range->we_version_compiled);
			fprintf(stderr, "may not be available...\n\n");
		}

		/* Note : we are only trying to catch compile difference, not source.
		* If the driver source has not been updated to the latest, it doesn't
		* matter because the new fields are set to zero */
	}
#endif

	return(0);
}

/* Note : strtok not thread safe, not used in WE-12 and later. */
int meshwu_get_stats(int	devFd,const char *ifname,iwstats *stats, const iwrange *range, int has_range)
{
	if((has_range) && (range->we_version_compiled > 11))
	{
		struct meshw_req		wrq;
		wrq.u.data.pointer = (caddr_t) stats;
		wrq.u.data.length = sizeof(struct meshw_statistics);
		wrq.u.data.flags = 1;		/* Clear updated flag */
		
		strncpy(wrq.ifr_name, ifname, MESHNAMSIZ);
		if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_STATS, &wrq) < 0)
			return(-1);

		return(0);
	}

	return 0;
}


