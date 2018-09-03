/*
* $Id: meshwu_lib_80211.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "meshwu_lib.h"

/* Modes as human readable strings */
const char * const meshwu_operation_mode[] = 
{
	"Auto",
	"Ad-Hoc",
	"Managed",
	"Master",
	"Repeater",
	"Secondary",
	"Monitor"
};

/* Modulations as human readable strings */
const struct meshwu_modul_descr	meshwu_modul_list[] =
{
	/* Start with aggregate types, so that they display first */
	{ IW_MODUL_11AG, "11ag",	"IEEE 802.11a + 802.11g (2.4 & 5 GHz, up to 54 Mb/s)" },
	{ IW_MODUL_11AB, "11ab",  "IEEE 802.11a + 802.11b (2.4 & 5 GHz, up to 54 Mb/s)" },
	{ IW_MODUL_11G, "11g", "IEEE 802.11g (2.4 GHz, up to 54 Mb/s)" },
	{ IW_MODUL_11A, "11a", "IEEE 802.11a (5 GHz, up to 54 Mb/s)" },
	{ IW_MODUL_11B, "11b", "IEEE 802.11b (2.4 GHz, up to 11 Mb/s)" },

	/* Proprietary aggregates */
	{ IW_MODUL_TURBO | IW_MODUL_11A, "turboa", "Atheros turbo mode at 5 GHz (up to 108 Mb/s)" },
	{ IW_MODUL_TURBO | IW_MODUL_11G, "turbog", "Atheros turbo mode at 2.4 GHz (up to 108 Mb/s)" },
	{ IW_MODUL_PBCC | IW_MODUL_11B, "11+", "TI 802.11+ (2.4 GHz, up to 22 Mb/s)" },

	/* Individual modulations */
	{ IW_MODUL_OFDM_G, "OFDMg", "802.11g higher rates, OFDM at 2.4 GHz (up to 54 Mb/s)" },
	{ IW_MODUL_OFDM_A, "OFDMa", "802.11a, OFDM at 5 GHz (up to 54 Mb/s)" },
	{ IW_MODUL_CCK, "CCK", "802.11b higher rates (2.4 GHz, up to 11 Mb/s)" },
	{ IW_MODUL_DS, "DS", "802.11 Direct Sequence (2.4 GHz, up to 2 Mb/s)" },
	{ IW_MODUL_FH, "FH", "802.11 Frequency Hopping (2,4 GHz, up to 2 Mb/s)" },

	/* Proprietary modulations */
	{ IW_MODUL_TURBO, "turbo", "Atheros turbo mode, channel bonding (up to 108 Mb/s)" },
	{ IW_MODUL_PBCC, "PBCC",  "TI 802.11+ higher rates (2.4 GHz, up to 22 Mb/s)" },
	{ IW_MODUL_CUSTOM, "custom", "Driver specific modulation (check driver documentation)" },
};

#if __ARM_IXP__

/*  Convert a frequency to a channel (negative -> error) */
int meshwu_freq_to_channel(int freq, const struct meshw_range *range)
{
	int	ref_freq;
	int		k;
#if 0
	if(freq < KILO)
		return(-1);
#endif

	/* We compare the frequencies as double to ignore differences in encoding. Slower, but safer... */
	for(k = 0; k < range->num_frequency; k++)
	{
		GET_KHZ(ref_freq, range->freq[k] );
		if(freq == ref_freq)
			return(range->freq[k].i);
	}
	/* Not found */
	return(-2);
}

/* Convert a channel to a frequency (negative -> error) Return the channel on success */
int meshwu_channel_to_freq(int channel, const struct meshw_range *range)
{
	int		has_freq = 0;
	int		k;

	/* Check if the driver support only channels or if it has frequencies */
	for(k = 0; k < range->num_frequency; k++)
	{
		if((range->freq[k].e != 0) || (range->freq[k].m > (int) KILO))
			has_freq = 1;
	}
	if(!has_freq)
		return(-1);

	/* Find the correct frequency in the list */
	for(k = 0; k < range->num_frequency; k++)
	{
		if(range->freq[k].i == channel)
		{
			GET_KHZ(has_freq, range->freq[k]);
			return has_freq;
		}
	}

	return(-2);
}

#else
/*  Convert a frequency to a channel (negative -> error) */
int meshwu_freq_to_channel(double freq, const struct meshw_range *range)
{
	double	ref_freq;
	int		k;

	/* Check if it's a frequency or not already a channel */
	if(freq < KILO)
		return(-1);

	/* We compare the frequencies as double to ignore differences in encoding. Slower, but safer... */
	for(k = 0; k < range->num_frequency; k++)
	{
		ref_freq = meshwu_freq2float(&(range->freq[k]));
		if(freq == ref_freq)
			return(range->freq[k].i);
	}
	/* Not found */
	return(-2);
}

/* Convert a channel to a frequency (negative -> error) Return the channel on success */
int meshwu_channel_to_freq(int channel, double *pfreq,const struct meshw_range *range)
{
	int		has_freq = 0;
	int		k;

	/* Check if the driver support only channels or if it has frequencies */
	for(k = 0; k < range->num_frequency; k++)
	{
		if((range->freq[k].e != 0) || (range->freq[k].m > (int) KILO))
			has_freq = 1;
	}
	if(!has_freq)
		return(-1);

	/* Find the correct frequency in the list */
	for(k = 0; k < range->num_frequency; k++)
	{
		if(range->freq[k].i == channel)
		{
			*pfreq = meshwu_freq2float(&(range->freq[k]));
			return(channel);
		}
	}

	return(-2);
}
#endif

/* Convert a passphrase into a key
 * ### NOT IMPLEMENTED ###
 * Return size of the key, or 0 (no key) or -1 (error) */
static int __meshwu_pass_key(const char *input,unsigned char *key)
{
	input = input; key = key;
	fprintf(stderr, "Error: Passphrase not implemented\n");
	return(-1);
}

/* Parse a key from the command line.
 * Return size of the key, or 0 (no key) or -1 (error)
 * If the key is too long, it's simply truncated... */
static int __meshwu_in_key(const char *input, unsigned char *key)
{
	int		keylen = 0;
	/* Check the type of key */
	if(!strncmp(input, "s:", 2))
	{/* First case : as an ASCII string (Lucent/Agere cards) */
		keylen = strlen(input + 2);		/* skip "s:" */
		if(keylen > MESHW_ENCODING_TOKEN_MAX)
			keylen = MESHW_ENCODING_TOKEN_MAX;
		memcpy(key, input + 2, keylen);
	}
	else if(!strncmp(input, "p:", 2))
	{/* Second case : as a passphrase (PrismII cards) */
		return(__meshwu_pass_key(input + 2, key));		/* skip "p:" */
	}
	else
	{
		const char 	*p;
		int		dlen;	/* Digits sequence length */
		unsigned char	out[MESHW_ENCODING_TOKEN_MAX];

		/* Third case : as hexadecimal digits */
		p = input;
		dlen = -1;

		/* Loop until we run out of chars in input or overflow the output */
		while(*p != '\0')
		{
			int	temph;
			int	templ;
			int	count;
			/* No more chars in this sequence */
			if(dlen <= 0)
			{/* Skip separator */
				if(dlen == 0)
					p++;
				/* Calculate num of char to next separator */
				dlen = strcspn(p, "-:;.,");
			}

			/* Get each char separatly (and not by two) so that we don't
			* get confused by 'enc' (=> '0E'+'0C') and similar */
			count = sscanf(p, "%1X%1X", &temph, &templ);
			if(count < 1)
				return(-1);		/* Error -> non-hex char */
			/* Fixup odd strings such as '123' is '01'+'23' and not '12'+'03'*/
			if(dlen % 2)
				count = 1;
			/* Put back two chars as one byte and output */
			if(count == 2)
				templ |= temph << 4;
			else
				templ = temph;
			out[keylen++] = (unsigned char) (templ & 0xFF);
			/* Check overflow in output */
			if(keylen >= MESHW_ENCODING_TOKEN_MAX)
				break;

			/* Move on to next chars */
			p += count;
			dlen -= count;
		}

		/* We use a temporary output buffer 'out' so that if there is
		* an error, we don't overwrite the original key buffer.
		* Because of the way iwconfig loop on multiple key/enc arguments
		* until it finds an error in here, this is necessary to avoid
		* silently corrupting the encryption key... */
		memcpy(key, out, keylen);
	}
	
#ifdef DEBUG
	{
		char buf[MESHW_ENCODING_TOKEN_MAX * 3];
		meshwu_print_key(buf, sizeof(buf), key, keylen, 0);
		printf("Got key : %d [%s]\n", keylen, buf);
	}
#endif

	return(keylen);
}

/* Parse a key from the command line. Return size of the key, or 0 (no key) or -1 (error) */
int meshwu_in_key_full(int devFd, const char *ifname,const char *input,unsigned char *key, __u16 *flags)
{
	int		keylen = 0;
	char *	p;
	if(!strncmp(input, "l:", 2))
	{
		struct meshw_range	range;
		/* Extra case : as a login (user:passwd - Cisco LEAP) */
		keylen = strlen(input + 2) + 1;		/* skip "l:", add '\0' */
		/* Most user/password is 8 char, so 18 char total, < 32 */
		if(keylen > MESHW_ENCODING_TOKEN_MAX)
			keylen = MESHW_ENCODING_TOKEN_MAX;
		memcpy(key, input + 2, keylen);

		/* Separate the two strings */
		p = strchr((char *) key, ':');
		if(p == NULL)
		{
			fprintf(stderr, "Error: Invalid login format\n");
			return(-1);
		}
		*p = '\0';
		/* Extract range info */
		if(meshwu_get_range_info(devFd, ifname, &range) < 0)
			/* Hum... Maybe we should return an error ??? */
			memset(&range, 0, sizeof(range));

		if(range.we_version_compiled > 15)
		{
			printf("flags = %X, index = %X\n", *flags, range.encoding_login_index);
			if((*flags & MESHW_ENCODE_INDEX) == 0)
			{/* Extract range info */
				if(meshwu_get_range_info(devFd, ifname, &range) < 0)
					memset(&range, 0, sizeof(range));
				printf("flags = %X, index = %X\n", *flags, range.encoding_login_index);

				/* Set the index the driver expects */
				*flags |= range.encoding_login_index & MESHW_ENCODE_INDEX;
			}
			printf("flags = %X, index = %X\n", *flags, range.encoding_login_index);
		}
	}
	else	/* Simpler routine above */
		keylen = __meshwu_in_key(input, key);

	return(keylen);
}

