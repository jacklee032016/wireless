/*
* $Id: meshconfig_cmds.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "meshconfig.h"

/* Map command line modifiers to the proper flags... */
typedef struct iwconfig_modifier
{
	const char		*cmd;		/* Command line shorthand */
	__u16			flag;		/* Flags to add */
	__u16			exclude;	/* Modifiers to exclude */
} iwconfig_modifier;

/* Modifiers for Power */
static const struct iwconfig_modifier	iwmod_power[] =
{
	{ "min",		MESHW_POWER_MIN,		MESHW_POWER_MAX },
	{ "max",	MESHW_POWER_MAX,		MESHW_POWER_MIN },
	{ "period",	MESHW_POWER_PERIOD,	MESHW_POWER_TIMEOUT | MESHW_POWER_SAVING },
	{ "timeout",	MESHW_POWER_TIMEOUT,	MESHW_POWER_PERIOD | MESHW_POWER_SAVING },
	{ "saving",	MESHW_POWER_SAVING,	MESHW_POWER_TIMEOUT | MESHW_POWER_PERIOD },
};
#define IWMOD_POWER_NUM	(sizeof(iwmod_power)/sizeof(iwmod_power[0]))

/* Modifiers for Retry */
static const struct iwconfig_modifier	iwmod_retry[] =
{
	{ "min",		MESHW_RETRY_MIN,		MESHW_RETRY_MAX },
	{ "max",	MESHW_RETRY_MAX,		MESHW_RETRY_MIN },
	{ "short",	MESHW_RETRY_SHORT,		MESHW_RETRY_LONG },
	{ "long",	MESHW_RETRY_LONG,		MESHW_RETRY_SHORT },
	{ "limit",	MESHW_RETRY_LIMIT,		MESHW_RETRY_LIFETIME },
	{ "lifetime",	MESHW_RETRY_LIFETIME,	MESHW_RETRY_LIMIT },
};
#define IWMOD_RETRY_NUM	(sizeof(iwmod_retry)/sizeof(iwmod_retry[0]))

/*
 * Parse command line modifiers. Return error or number arg parsed.
 * Modifiers must be at the beggining of command line.
 */
static int parse_modifiers(char *args[],int	count, __u16 *pout, /* Flags to write */
		const struct iwconfig_modifier modifier[],	int modnum)
{
	int		i = 0;
	int		k = 0;
	__u16		result = 0;	/* Default : no flag set */

	/* Get all modifiers and value types on the command line */
	do
	{
		for(k = 0; k < modnum; k++)
		{
			/* Check if matches */
			if(!strcasecmp(args[i], modifier[k].cmd))
			{/* Check for conflicting flags */
				if(result & modifier[k].exclude)
				{
					errarg = i;
					return(MESHWU_ERR_ARG_CONFLICT);
				}

				/* Just add it */
				result |= modifier[k].flag;
				++i;
				break;
			}
		}
	/* For as long as current arg matched and not out of args */	
	}while((i < count) && (k < modnum));

	/* Check there remains one arg for value */
	if(i >= count)
		return(MESHWU_ERR_ARG_NUM);

	/* Return result */
	*pout = result;
	return(i);
}


/** An error is indicated by a negative return value.
 * 0 and positive return values indicate the number of args consumed. */

static int set_essid_info(int devFd, char *ifname,char *args[],int	count)
{
	struct meshw_req		wrq;
	int			i = 1;
	char			essid[MESHW_ESSID_MAX_SIZE + 1];

	if((!strcasecmp(args[0], "off")) ||(!strcasecmp(args[0], "any")))
	{
		wrq.u.essid.flags = 0;
		essid[0] = '\0';
	}
	else if(!strcasecmp(args[0], "on"))
	{/* Get old essid */
		memset(essid, '\0', sizeof(essid));
		wrq.u.essid.pointer = (caddr_t) essid;
		wrq.u.essid.length = MESHW_ESSID_MAX_SIZE + 1;
		wrq.u.essid.flags = 0;

		if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_ESSID, &wrq) < 0)
			return(MESHWU_ERR_GET_EXT);
		wrq.u.essid.flags = 1;
	}
	else
	{
		i = 0;

		/* '-' or '--' allow to escape the ESSID string, allowing
		* to set it to the string "any" or "off".
		* This is a big ugly, but it will do for now */
		if((!strcmp(args[0], "-")) || (!strcmp(args[0], "--")))
		{
			if(++i >= count)
				return(MESHWU_ERR_ARG_NUM);
		}

		/* Check the size of what the user passed us to avoid buffer overflows */
		if(strlen(args[i]) > MESHW_ESSID_MAX_SIZE)
		{
			errmax = MESHW_ESSID_MAX_SIZE;
			return(MESHWU_ERR_ARG_SIZE);
		}
		else
		{
			int	temp;

			wrq.u.essid.flags = 1;
			strcpy(essid, args[i]);	/* Size checked, all clear */
			i++;

			/* Check for ESSID index */
			if((i < count) && (sscanf(args[i], "[%i]", &temp) == 1) &&
				(temp > 0) && (temp < MESHW_ENCODE_INDEX))
			{
				wrq.u.essid.flags = temp;
				++i;
			}
		}
	}

	/* Finally set the ESSID value */
	wrq.u.essid.pointer = (caddr_t) essid;
	wrq.u.essid.length = strlen(essid);
	wrq.u.essid.length++;

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_ESSID, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(i);
}

static int set_mode_info(int devFd, char *ifname,char *args[], int	count)
{
	struct meshw_req		wrq;
	int			k;

	count = count;

	if(sscanf(args[0], "%i", &k) != 1)
	{
		k = 0;
		while((k < MESHWU_NUM_OPER_MODE) && strncasecmp(args[0], meshwu_operation_mode[k], 3))
			k++;
	}

	if((k >= MESHWU_NUM_OPER_MODE) || (k < 0))
	{
		errarg = 0;
		return(MESHWU_ERR_ARG_TYPE);
	}

	wrq.u.mode = k;
	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_MODE, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	/* 1 arg */
	return(1);
}

static int set_freq_info(int devFd, char *ifname, char *args[], int count)
{
	struct meshw_req		wrq;
	int			i = 1;
#if __ARM_IXP__
	int		freq;
#else
	double		freq;
#endif

	if(!strcasecmp(args[0], "auto"))
	{
		wrq.u.freq.m = -1;
		wrq.u.freq.e = 0;
		wrq.u.freq.flags = 0;
	}
	else
	{
		if(!strcasecmp(args[0], "fixed"))
		{ /* Get old frequency */
			if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_FREQ, &wrq) < 0)
				return(MESHWU_ERR_GET_EXT);
			wrq.u.freq.flags = IW_FREQ_FIXED;
		}
		else
		{/* Should be a numeric value */
#if __ARM_IXP__		
			if(sscanf(args[0], "%d", &(freq)) != 1)
			{
				errarg = 0;
				return(MESHWU_ERR_ARG_TYPE);
			}

			if(index(args[0], 'G')) freq *= GIGA_INT;
			if(index(args[0], 'M')) freq *= MEGA_INT;
			if(index(args[0], 'k')) freq *= KILO_INT;

			SET_HZ(freq, wrq.u.freq);
#else
			if(sscanf(args[0], "%lg", &(freq)) != 1)
			{
				errarg = 0;
				return(MESHWU_ERR_ARG_TYPE);
			}

			if(index(args[0], 'G')) freq *= GIGA;
			if(index(args[0], 'M')) freq *= MEGA;
			if(index(args[0], 'k')) freq *= KILO;

			meshwu_float2freq(freq, &(wrq.u.freq));
#endif
			wrq.u.freq.flags = IW_FREQ_FIXED;

			/* Check for an additional argument */
			if((i < count) && (!strcasecmp(args[i], "auto")))
			{
				wrq.u.freq.flags = 0;
				++i;
			}

			if((i < count) && (!strcasecmp(args[i], "fixed")))
			{
				wrq.u.freq.flags = IW_FREQ_FIXED;
				++i;
			}
		}
	}

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_FREQ, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(i);
}

/* added by lizhijie, 2006.09.15 */
#if __ARM_IXP__
static int set_channel_info(int devFd, char *ifname, char *args[], int count)
{
	struct meshw_req		wrq;
	int		i = 1;
	int		freq;
	
	if(!strcmp(args[i], "channel") )
	{
		if(++i >= count)
			return MESHWU_ERR_ARG_NUM;
//			ABORT_ARG_NUM("Set Channel", SIOCSIWFREQ);
		if(!strcasecmp(args[i], "auto"))
		{
			wrq.u.freq.m = -1;
			wrq.u.freq.e = 0;
			wrq.u.freq.flags = 0;
		}
		else
		{
			wrq.u.freq.m = atoi(args[i]);
			wrq.u.freq.e = 0;
			wrq.u.freq.flags = 0;
		}

//		IW_SET_EXT_ERR(skfd, ifname, SIOCSIWFREQ, &wrq,	 "Set Channel");
//		continue;
		if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_FREQ, &wrq) < 0)
			return(MESHWU_ERR_SET_EXT);

	}

	return (i);
}
#endif

static int set_bitrate_info(int	devFd, char *ifname, char *args[], int count)
{
	struct meshw_req		wrq;
	int			i = 1;

	wrq.u.bitrate.flags = 0;
	if(!strcasecmp(args[0], "auto"))
	{
		wrq.u.bitrate.value = -1;
		wrq.u.bitrate.fixed = 0;
	}
	else
	{
		if(!strcasecmp(args[0], "fixed"))
		{ /* Get old bitrate */
			if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RATE, &wrq) < 0)
				return(MESHWU_ERR_GET_EXT);

			wrq.u.bitrate.fixed = 1;
		}
		else
		{/* Should be a numeric value */
#if __ARM_IXP__		
			int		brate;
			if(sscanf(args[0], "%d", &(brate)) != 1)
			{
				errarg = 0;
				return(MESHWU_ERR_ARG_TYPE);
			}
			if(index(args[0], 'G'))
				brate *= GIGA_INT;
			if(index(args[0], 'M'))
				brate *= MEGA_INT;
			if(index(args[0], 'k'))
				brate *= KILO_INT;
#else
			double		brate;
			if(sscanf(args[0], "%lg", &(brate)) != 1)
			{
				errarg = 0;
				return(MESHWU_ERR_ARG_TYPE);
			}
			if(index(args[0], 'G'))
				brate *= GIGA;
			if(index(args[0], 'M'))
				brate *= MEGA;
			if(index(args[0], 'k'))
				brate *= KILO;
#endif			
			wrq.u.bitrate.value = (long) brate;
			wrq.u.bitrate.fixed = 1;

			/* Check for an additional argument */
			if((i < count) && (!strcasecmp(args[i], "auto")))
			{
				wrq.u.bitrate.fixed = 0;
				++i;
			}

			if((i < count) && (!strcasecmp(args[i], "fixed")))
			{
				wrq.u.bitrate.fixed = 1;
				++i;
			}
			if((i < count) && (!strcasecmp(args[i], "unicast")))
			{
				wrq.u.bitrate.flags |= IW_BITRATE_UNICAST;
				++i;
			}
			if((i < count) && (!strcasecmp(args[i], "broadcast")))
			{
				wrq.u.bitrate.flags |= IW_BITRATE_BROADCAST;
				++i;
			}
		}
	}

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_RATE, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	/* Var args */
	return(i);
}

static int set_enc_info(int devFd, char *ifname, char *args[],int count)
{
	struct meshw_req		wrq;
	int			i = 1;
	unsigned char		key[MESHW_ENCODING_TOKEN_MAX];

	if(!strcasecmp(args[0], "on"))
	{/* Get old encryption information */
		wrq.u.data.pointer = (caddr_t) key;
		wrq.u.data.length = MESHW_ENCODING_TOKEN_MAX;
		wrq.u.data.flags = 0;
		if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_ENCODE, &wrq) < 0)
			return(MESHWU_ERR_GET_EXT);
		wrq.u.data.flags &= ~MESHW_ENCODE_DISABLED;	/* Enable */
	}
	else
	{
		int	gotone = 0;
		int	oldone;
		int	keylen;
		int	temp;

		wrq.u.data.pointer = (caddr_t) NULL;
		wrq.u.data.flags = 0;
		wrq.u.data.length = 0;
		i = 0;

		/* Allow arguments in any order (it's safe) */
		do
		{
			oldone = gotone;

			/* -- Check for the key -- */
			if(i < count)
			{
				keylen = meshwu_in_key_full(devFd, ifname, args[i], key, &wrq.u.data.flags);
				if(keylen > 0)
				{
					wrq.u.data.length = keylen;
					wrq.u.data.pointer = (caddr_t) key;
					++i;
					gotone++;
				}
			}

			/* -- Check for token index -- */
			if((i < count) && (sscanf(args[i], "[%i]", &temp) == 1) &&
				(temp > 0) && (temp < MESHW_ENCODE_INDEX))
			{
				wrq.u.encoding.flags |= temp;
				++i;
				gotone++;
			}

			/* -- Check the various flags -- */
			if((i < count) && (!strcasecmp(args[i], "off")))
			{
				wrq.u.data.flags |= MESHW_ENCODE_DISABLED;
				++i;
				gotone++;
			}

			if((i < count) && (!strcasecmp(args[i], "open")))
			{
				wrq.u.data.flags |= MESHW_ENCODE_OPEN;
				++i;
				gotone++;
			}
			if((i < count) && (!strncasecmp(args[i], "restricted", 5)))
			{
				wrq.u.data.flags |= MESHW_ENCODE_RESTRICTED;
				++i;
				gotone++;
			}
			if((i < count) && (!strncasecmp(args[i], "temporary", 4)))
			{
				wrq.u.data.flags |= MESHW_ENCODE_TEMP;
				++i;
				gotone++;
			}

		}	while(gotone != oldone);

		/* Pointer is absent in new API */
		if(wrq.u.data.pointer == NULL)
			wrq.u.data.flags |= MESHW_ENCODE_NOKEY;

		/* Check if we have any invalid argument */
		if(!gotone)
		{
			errarg = 0;
			return(MESHWU_ERR_ARG_TYPE);
		}
	}

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_ENCODE, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	/* Var arg */
	return(i);
}

static int set_power_info(int	devFd, char *ifname, char *args[], int count)
{
	struct meshw_req		wrq;
	int			i = 1;

	if(!strcasecmp(args[0], "off"))
		wrq.u.power.disabled = 1;	/* i.e. max size */
	else  if(!strcasecmp(args[0], "on"))
	{/* Get old Power info */
		wrq.u.power.flags = 0;
		if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_POWER_MGMT, &wrq) < 0)
			return(MESHWU_ERR_GET_EXT);
		wrq.u.power.disabled = 0;
	}
	else
	{
		double		temp;
		int		gotone = 0;

		/* Parse modifiers */
		i = parse_modifiers(args, count, &wrq.u.power.flags,  iwmod_power, IWMOD_POWER_NUM);
		if(i < 0)
			return(i);

		wrq.u.power.disabled = 0;

		/* Is there any value to grab ? */
		if(sscanf(args[i], "%lg", &(temp)) == 1)
		{
			struct meshw_range	range;
			int			flags;

			/* Extract range info to handle properly 'relative' */
			if(meshwu_get_range_info(devFd, ifname, &range) < 0)
				memset(&range, 0, sizeof(range));

			/* Get the flags to be able to do the proper conversion */
			switch(wrq.u.power.flags & MESHW_POWER_TYPE)
			{
				case MESHW_POWER_SAVING:
					flags = range.pms_flags;
					break;

				case MESHW_POWER_TIMEOUT:
					flags = range.pmt_flags;
					break;
				default:
					flags = range.pmp_flags;
					break;
			}

			/* Check if time or relative */
			if(flags & MESHW_POWER_RELATIVE)
			{
				if(range.we_version_compiled < 21)
					temp *= MEGA;
				else
					wrq.u.power.flags |= MESHW_POWER_RELATIVE;
			}
			else
			{
				temp *= MEGA;	/* default = s */
				if(index(args[i], 'u'))
					temp /= MEGA;
				if(index(args[i], 'm'))
					temp /= KILO;
			}

			wrq.u.power.value = (long) temp;

			/* Set some default type if none */
			if((wrq.u.power.flags & MESHW_POWER_TYPE) == 0)
				wrq.u.power.flags |= MESHW_POWER_PERIOD;
			++i;
			gotone = 1;
		}

		/* Now, check the mode */
		if(i < count)
		{
			if(!strcasecmp(args[i], "all"))
				wrq.u.power.flags |= MESHW_POWER_ALL_R;
			if(!strncasecmp(args[i], "unicast", 4))
				wrq.u.power.flags |= MESHW_POWER_UNICAST_R;
			if(!strncasecmp(args[i], "multicast", 5))
				wrq.u.power.flags |= MESHW_POWER_MULTICAST_R;
			if(!strncasecmp(args[i], "force", 5))
				wrq.u.power.flags |= MESHW_POWER_FORCE_S;
			if(!strcasecmp(args[i], "repeat"))
				wrq.u.power.flags |= MESHW_POWER_REPEATER;
			if(wrq.u.power.flags & MESHW_POWER_MODE)
			{
				++i;
				gotone = 1;
			}
		}

		if(!gotone)
		{
			errarg = i;
			return(MESHWU_ERR_ARG_TYPE);
		}
	}

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_POWER_MGMT, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	/* Var args */
	return(i);
}

static int set_nick_info(int devFd,char *ifname,char *args[], int count)
{
	struct meshw_req		wrq;

	count = count;

	if(strlen(args[0]) > MESHW_ESSID_MAX_SIZE)
	{
		errmax = MESHW_ESSID_MAX_SIZE;
		return(MESHWU_ERR_ARG_SIZE);
	}

	wrq.u.essid.pointer = (caddr_t) args[0];
	wrq.u.essid.length = strlen(args[0]);
	wrq.u.essid.length++;

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_NICKNAME, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	/* 1 args */
	return(1);
}

static int set_nwid_info(int devFd, char *ifname, char *args[], int count)
{
	struct meshw_req		wrq;
	unsigned long		temp;

	count = count;

	if((!strcasecmp(args[0], "off")) ||(!strcasecmp(args[0], "any")))
		wrq.u.nwid.disabled = 1;
	else if(!strcasecmp(args[0], "on"))
	{/* Get old nwid */
		if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_NWID, &wrq) < 0)
			return(MESHWU_ERR_GET_EXT);
		wrq.u.nwid.disabled = 0;
	}
	else if(sscanf(args[0], "%lX", &(temp)) != 1)
	{
		errarg = 0;
		return(MESHWU_ERR_ARG_TYPE);
	}
	else
	{
		wrq.u.nwid.value = temp;
		wrq.u.nwid.disabled = 0;
	}

	wrq.u.nwid.fixed = 1;

	/* Set new nwid */
	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_NWID, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	/* 1 arg */
	return(1);
}

static int set_apaddr_info(int	 devFd, char *ifname, char *args[],int count)
{
	struct meshw_req		wrq;

	count = count;

	if((!strcasecmp(args[0], "auto")) || (!strcasecmp(args[0], "any")))
	{/* Send a broadcast address */
		meshwu_broad_ether(&(wrq.u.ap_addr));
	}
	else
	{
		if(!strcasecmp(args[0], "off"))
		{ /* Send a NULL address */
			meshwu_null_ether(&(wrq.u.ap_addr));
		}
		else
		{
			/* Get the address and check if the interface supports it */
			if(meshwu_in_addr(devFd, ifname, args[0], &(wrq.u.ap_addr)) < 0)
			{
				errarg = 0;
				return(MESHWU_ERR_ARG_TYPE);
			}
		}
	}

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_AP_MAC, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(1);
}

static int set_txpower_info(int devFd, char *ifname,char *args[], int count)
{
	struct meshw_req		wrq;
	int			i = 1;

	args = args; count = count;

	/* Prepare the request */
	wrq.u.txpower.value = -1;
	wrq.u.txpower.fixed = 1;
	wrq.u.txpower.disabled = 0;
	wrq.u.txpower.flags = MESHW_TXPOW_DBM;

	if(!strcasecmp(args[0], "off"))
		wrq.u.txpower.disabled = 1;	/* i.e. turn radio off */
	else if(!strcasecmp(args[0], "auto"))
		wrq.u.txpower.fixed = 0;	/* i.e. use power control */
	else
	{
		if(!strcasecmp(args[0], "on"))
		{
			/* Get old tx-power */
			if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_TX_POW, &wrq) < 0)
				return(MESHWU_ERR_GET_EXT);
			wrq.u.txpower.disabled = 0;
		}
		else
		{
			if(!strcasecmp(args[0], "fixed"))
			{/* Get old tx-power */
				if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_TX_POW, &wrq) < 0)
					return(MESHWU_ERR_GET_EXT);
				wrq.u.txpower.fixed = 1;
				wrq.u.txpower.disabled = 0;
			}
			else	
			{/* Should be a numeric value */
				int		power;
				int		ismwatt = 0;
				struct meshw_range	range;

				/* Extract range info to do proper conversion */
				if(meshwu_get_range_info(devFd, ifname, &range) < 0)
					memset(&range, 0, sizeof(range));

				/* Get the value */
				if(sscanf(args[0], "%i", &(power)) != 1)
				{
					errarg = 0;
					return(MESHWU_ERR_ARG_TYPE);
				}

				/* Check if milliWatt
				* We authorise a single 'm' as a shorthand for 'mW',
				* on the other hand a 'd' probably means 'dBm'... */
				ismwatt = ((index(args[0], 'm') != NULL) && (index(args[0], 'd') == NULL));

				/* We could check 'W' alone... Another time... */
				/* Convert */
				if(range.txpower_capa & MESHW_TXPOW_RELATIVE)
				{ /* Can't convert */
					if(ismwatt)
					{
						errarg = 0;
						return(MESHWU_ERR_ARG_TYPE);
					}
					wrq.u.txpower.flags = MESHW_TXPOW_RELATIVE;
				}
				else if(range.txpower_capa & MESHW_TXPOW_MWATT)
				{
					if(!ismwatt)
						power = meshwu_dbm2mwatt(power);

					wrq.u.txpower.flags = MESHW_TXPOW_MWATT;
				}
				else
				{
					if(ismwatt)
						power = meshwu_mwatt2dbm(power);
					wrq.u.txpower.flags = MESHW_TXPOW_DBM;
				}
				wrq.u.txpower.value = power;

				/* Check for an additional argument */
				if((i < count) && (!strcasecmp(args[i], "auto")))
				{
					wrq.u.txpower.fixed = 0;
					++i;
				}

				if((i < count) && (!strcasecmp(args[i+1], "fixed")))
				{
					wrq.u.txpower.fixed = 1;
					++i;
				}
			}
		}
	}

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_TX_POW, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(i);
}


static int set_sens_info(int devFd, char *ifname, char *args[], int count)
{
	struct meshw_req		wrq;
	int			temp;

	count = count;

	if(sscanf(args[0], "%i", &(temp)) != 1)
	{
		errarg = 0;
		return(MESHWU_ERR_ARG_TYPE);
	}
	wrq.u.sens.value = temp;

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_SENS, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(1);
}

static int set_retry_info(int devFd, char *ifname,char *args[], int count)
{
	struct meshw_req		wrq;
	int			i = 0;
	double		temp;

	/* Parse modifiers */
	i = parse_modifiers(args, count, &wrq.u.retry.flags, iwmod_retry, IWMOD_RETRY_NUM);
	if(i < 0)
		return(i);

	/* Add default type if none */
	if((wrq.u.retry.flags & MESHW_RETRY_TYPE) == 0)
		wrq.u.retry.flags |= MESHW_RETRY_LIMIT;

	wrq.u.retry.disabled = 0;

	/* Is there any value to grab ? */
	if(sscanf(args[i], "%lg", &(temp)) != 1)
	{
		errarg = i;
		return(MESHWU_ERR_ARG_TYPE);
	}

	/* Limit is absolute, on the other hand lifetime is seconds */
	if(wrq.u.retry.flags & MESHW_RETRY_LIFETIME)
	{
		struct meshw_range	range;
		/* Extract range info to handle properly 'relative' */
		if(meshwu_get_range_info(devFd, ifname, &range) < 0)
			memset(&range, 0, sizeof(range));

		if(range.r_time_flags & MESHW_RETRY_RELATIVE)
		{
			if(range.we_version_compiled < 21)
				temp *= MEGA;
			else
				wrq.u.retry.flags |= MESHW_RETRY_RELATIVE;
		}
		else
		{
			/* Normalise lifetime */
			temp *= MEGA;	/* default = s */
			if(index(args[i], 'u'))
				temp /= MEGA;
			if(index(args[i], 'm'))
				temp /= KILO;
		}
	}

	wrq.u.retry.value = (long) temp;
	++i;

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_RETRY, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(i);
}


static int set_rts_info(int devFd, char *	ifname, char *args[], int count)
{
	struct meshw_req		wrq;

	count = count;

	wrq.u.rts.value = -1;
	wrq.u.rts.fixed = 1;
	wrq.u.rts.disabled = 0;

	if(!strcasecmp(args[0], "off"))
		wrq.u.rts.disabled = 1;	/* i.e. max size */
	else if(!strcasecmp(args[0], "auto"))
		wrq.u.rts.fixed = 0;
	else
	{
		if(!strcasecmp(args[0], "fixed"))
		{/* Get old RTS threshold */
			if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RTS, &wrq) < 0)
				return(MESHWU_ERR_GET_EXT);

			wrq.u.rts.fixed = 1;
		}
		else
		{/* Should be a numeric value */
			long	temp;
			if(sscanf(args[0], "%li", (unsigned long *) &(temp)) != 1)
			{
				errarg = 0;
				return(MESHWU_ERR_ARG_TYPE);
			}
			wrq.u.rts.value = temp;
		}
	}

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_RTS, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(1);
}

static int set_frag_info(int devFd, char *ifname, char *args[], int count)
{
	struct meshw_req		wrq;

	count = count;
	wrq.u.frag.value = -1;
	wrq.u.frag.fixed = 1;
	wrq.u.frag.disabled = 0;
	if(!strcasecmp(args[0], "off"))
		wrq.u.frag.disabled = 1;	/* i.e. max size */
	else if(!strcasecmp(args[0], "auto"))
		wrq.u.frag.fixed = 0;
	else
	{
		if(!strcasecmp(args[0], "fixed"))
		{/* Get old fragmentation threshold */
			if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_FRAG_THR, &wrq) < 0)
				return(MESHWU_ERR_GET_EXT);
			wrq.u.frag.fixed = 1;
		}
		else
		{/* Should be a numeric value */
			long	temp;
			if(sscanf(args[0], "%li", &(temp))!= 1)
			{
				errarg = 0;
				return(MESHWU_ERR_ARG_TYPE);
			}
			wrq.u.frag.value = temp;
		}
	}

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_FRAG_THR, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(1);
}

#if 0
static int set_modulation_info(int devFd, char *ifname, char *args[], int count)
{
	struct meshw_req		wrq;
	int			i = 1;

	/* Avoid "Unused parameter" warning */
	args = args;
	count = count;
	if(!strcasecmp(args[0], "auto"))
		wrq.u.param.fixed = 0;	/* i.e. use any modulation */
	else
	{
		if(!strcasecmp(args[0], "fixed"))
		{ /* Get old modulation */
			if(meshwu_get_ext(devFd, ifname, SIOCGIWMODUL, &wrq) < 0)
				return(MESHWU_ERR_GET_EXT);
			wrq.u.param.fixed = 1;
		}
		else
		{
			int		k;

			/* Allow multiple modulations, combine them together */
			wrq.u.param.value = 0x0;
			i = 0;
			do
			{
				for(k = 0; k < IW_SIZE_MODUL_LIST; k++)
				{
					if(!strcasecmp(args[i], meshwu_modul_list[k].cmd))
					{
						wrq.u.param.value |= meshwu_modul_list[k].mask;
						++i;
						break;
					}
				}
			/* For as long as current arg matched and not out of args */
			}while((i < count) && (k < IW_SIZE_MODUL_LIST));

			/* Check we got something */
			if(i == 0)
			{
				errarg = 0;
				return(MESHWU_ERR_ARG_TYPE);
			}

			/* Check for an additional argument */
			if((i < count) && (!strcasecmp(args[i], "auto")))
			{
				wrq.u.param.fixed = 0;
				++i;
			}
			if((i < count) && (!strcasecmp(args[i], "fixed")))
			{
				wrq.u.param.fixed = 1;
				++i;
			}
		}
	}

	if(meshwu_set_ext(devFd, ifname, SIOCSIWMODUL, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(i);
}
#endif


static int set_commit_info(int devFd,char *ifname, char *args[],int count)
{
	struct meshw_req		wrq;

	args = args; count = count;

	if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_COMMIT, &wrq) < 0)
		return(MESHWU_ERR_SET_EXT);

	return(0);
}


struct iwconfig_entry iwconfig_cmds[] = 
{
	{
		"essid",
		set_essid_info,
		1,
		MESHW_IO_W_ESSID,
		"Set ESSID",
		"{NNN|any|on|off}"
	},
	{
		"mode",
		set_mode_info,
		1,
		MESHW_IO_W_MODE,
		"Set Mode",
		"{managed|ad-hoc|master|...}"
	},
	{
		"freq",
		set_freq_info,
		1,
		MESHW_IO_W_FREQ,
		"Set Frequency",
		"N.NNN[k|M|G]"
	},
	{
		"channel",
#if __ARM_IXP__
		set_channel_info,
#else
		set_freq_info,
#endif		
		1,
		MESHW_IO_W_FREQ,
		"Set Frequency",
		"N"
	},
	{
		"bit",
		set_bitrate_info,
		1,
		MESHW_IO_W_RATE,
		"Set Bit Rate",
		"{N[k|M|G]|auto|fixed}"
	},
	{
		"rate",
		set_bitrate_info,
		1,
		MESHW_IO_W_RATE,
		"Set Bit Rate",
		"{N[k|M|G]|auto|fixed}"
	},
	{
		"enc",
		set_enc_info,
		1,
		MESHW_IO_W_ENCODE,
		"Set Encode",
		"{NNNN-NNNN|off}"
	},
	{
		"key",
		set_enc_info,
		1,
		MESHW_IO_W_ENCODE,
		"Set Encode",
		"{NNNN-NNNN|off}"
	},
	{
		"power",
		set_power_info,
		1,
		MESHW_IO_W_POWER_MGMT,
		"Set Power Management",
		"{period N|timeout N|saving N|off}"
	},
	
	{
		"nick",
		set_nick_info,
		1,
		MESHW_IO_W_NICKNAME,
		"Set Nickname",
		"NNN"
	},
	{
		"nwid",
		set_nwid_info,
		1,
		MESHW_IO_W_NWID,
		"Set NWID",
		"{NN|on|off}"
	},
	{
		"ap",
		set_apaddr_info,
		1,
		MESHW_IO_W_AP_MAC,
		"Set AP Address",
		"{N|off|auto}"
	},
	{
		"txpower",
		set_txpower_info,
		1,
		MESHW_IO_W_TX_POW,
		"Set Tx Power",
		"{NmW|NdBm|off|auto}"
	},
	{
		"sens",
		set_sens_info,
		1,
		MESHW_IO_W_SENS,
		"Set Sensitivity",
		"N"
	},
	{
		"retry",
		set_retry_info,
		1,
		MESHW_IO_W_RETRY,
		"Set Retry Limit",
		"{limit N|lifetime N}"
	},
	{
		"rts",
		set_rts_info,
		1,
		MESHW_IO_W_RTS,
		"Set RTS Threshold",
		"{N|auto|fixed|off}"
	},
	{
		"frag",
		set_frag_info,
		1,
		MESHW_IO_W_FRAG_THR,
		"Set Fragmentation Threshold",
		"{N|auto|fixed|off}"
	},
#if 0	
	{
		"modulation",
		set_modulation_info,
		1,
		SIOCGIWMODUL,
		"Set Modulation",
		"{11g|11a|CCK|OFDMg|...}"
	},
#endif
 	{
 		"commit",
		set_commit_info,
		0,
		MESHW_IO_W_COMMIT,
		"Commit changes",
		""
	},
	{ NULL, NULL, 0, 0, NULL, NULL },
};

