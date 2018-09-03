/*
* $Id: meshwu_lib_tools.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "meshwu_lib.h"

int meshwu_dbm2mwatt(int in)
{
#ifdef MESHWU_WITHOUT_LIBM
	int		ip = in / 10;
	int		fp = in % 10;
	int		k;
	double	res = 1.0;

	/* Split integral and floating part to avoid accumulating rounding errors */
	for(k = 0; k < ip; k++)
		res *= 10;
	for(k = 0; k < fp; k++)
		res *= LOG10_MAGIC;
	return((int) res);
#else
	return((int) (floor(pow(10.0, (((double) in) / 10.0)))));
#endif
}

/* Convert a value in milliWatt to a value in dBm. */
int meshwu_mwatt2dbm(int	in)
{
#ifdef MESHWU_WITHOUT_LIBM
	double	fin = (double) in;
	int	res = 0;
	/* Split integral and floating part to avoid accumulating rounding errors */
	while(fin > 10.0)
	{
		res += 10;
		fin /= 10.0;
	}
	while(fin > 1.000001)	/* Eliminate rounding errors, take ceil */
	{
		res += 1;
		fin /= LOG10_MAGIC;
	}
	return(res);
#else
	return((int) (ceil(10.0 * log10((double) in))));
#endif
}

#if __ARM_IXP__
#else
void meshwu_float2freq(double in, iwfreq *out)
{
#ifdef MESHWU_WITHOUT_LIBM
	out->e = 0;
	while(in > 1e9)
	{
		in /= 10;
		out->e++;
	}
	out->m = (long) in;
#else
	out->e = (short) (floor(log10(in)));
	if(out->e > 8)
	{
		out->m = ((long) (floor(in / pow(10,out->e - 6)))) * 100;
		out->e -= 8;
	}
	else
	{
		out->m = (long) in;
		out->e = 0;
	}
#endif
}
/* Convert our internal representation of frequencies to a floating point. */
double meshwu_freq2float(const iwfreq *in)
{
#ifdef MESHWU_WITHOUT_LIBM
	int		i;
	double	res = (double) in->m;
	for(i = 0; i < in->e; i++)
		res *= 10;
	return(res);
#else
	return ((double) in->m) * pow(10,in->e);
#endif
}

#endif



/* Display an Wireless Access Point Socket Address in readable format.
 * Note : 0x44 is an accident of history, that's what the Orinoco/PrismII
 * chipset report, and the driver doesn't filter it. */
char *meshwu_sawap_ntop(const struct sockaddr *sap,char *buf)
{
	const struct ether_addr ether_zero = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
	const struct ether_addr ether_bcast = {{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }};
	const struct ether_addr ether_hack = {{ 0x44, 0x44, 0x44, 0x44, 0x44, 0x44 }};
	const struct ether_addr * ether_wap = (const struct ether_addr *) sap->sa_data;

	if(!meshwu_ether_cmp(ether_wap, &ether_zero))
		sprintf(buf, "Not-Associated");
	else if(!meshwu_ether_cmp(ether_wap, &ether_bcast))
			sprintf(buf, "Invalid");
	else if(!meshwu_ether_cmp(ether_wap, &ether_hack))
		sprintf(buf, "None");
	else
		meshu_ether_ntop(ether_wap, buf);

	return(buf);
}

/* Input an Internet address and convert to binary. */
int __meshwu_in_inet(char *name, struct sockaddr *sap)
{
	struct hostent *hp;
	struct netent *np;
	struct sockaddr_in *sain = (struct sockaddr_in *) sap;

	/* Grmpf. -FvK */
	sain->sin_family = AF_INET;
	sain->sin_port = 0;

	/* Default is special, meaning 0.0.0.0. */
	if (!strcmp(name, "default"))
	{
		sain->sin_addr.s_addr = INADDR_ANY;
		return(1);
	}

	/* Try the NETWORKS database to see if this is a known network. */
	if ((np = getnetbyname(name)) != (struct netent *)NULL)
	{
		sain->sin_addr.s_addr = htonl(np->n_net);
		strcpy(name, np->n_name);
		return(1);
	}

	/* Always use the resolver (DNS name + IP addresses) */
	if ((hp = gethostbyname(name)) == (struct hostent *)NULL)
	{
		errno = h_errno;
		return(-1);
	}

	memcpy((char *) &sain->sin_addr, (char *) hp->h_addr_list[0], hp->h_length);
	strcpy(name, hp->h_name);
	return(0);
}

/* Input an address and convert to binary. */
int meshwu_in_addr(int devFd, const char *ifname, char *bufp, struct sockaddr *sap)
{
	/* Check if it is a hardware or IP address */
	if(index(bufp, ':') == NULL)
	{
		struct sockaddr	if_address;
		struct arpreq	arp_query;

		/* Check if we have valid interface address type */
		if(meshwu_check_if_addr_type(devFd, ifname) < 0)
		{
			fprintf(stderr, "%-8.16s  Interface doesn't support IP addresses\n", ifname);
			return(-1);
		}

		/* Read interface address */
		if(__meshwu_in_inet(bufp, &if_address) < 0)
		{
			fprintf(stderr, "Invalid interface address %s\n", bufp);
			return(-1);
		}

		/* Translate IP addresses to MAC addresses */
		memcpy((char *) &(arp_query.arp_pa), (char *) &if_address, sizeof(struct sockaddr));

		arp_query.arp_ha.sa_family = 0;
		arp_query.arp_flags = 0;

		/* The following restrict the search to the interface only */
		/* For old kernels which complain, just comment it... */

		strncpy(arp_query.arp_dev, ifname, MESHNAMSIZ);

		if((ioctl(devFd, SIOCGARP, &arp_query) < 0) || !(arp_query.arp_flags & ATF_COM))
		{
			fprintf(stderr, "Arp failed for %s on %s... (%d)\nTry to ping the address before setting it.\n", bufp, ifname, errno);
			return(-1);
		}

		/* Store new MAC address */
		memcpy((char *) sap, (char *) &(arp_query.arp_ha), sizeof(struct sockaddr));

#ifdef DEBUG
		{
			char buf[20];
			printf("IP Address %s => Hw Address = %s\n", bufp, meshwu_saether_ntop(sap, buf));
		}
#endif
	}
	else	/* If it's an hardware address */
	{
		/* Check if we have valid mac address type */
		if(meshwu_check_mac_addr_type(devFd, ifname) < 0)
		{
			fprintf(stderr, "%-8.16s  Interface doesn't support MAC addresses\n", ifname);
			return(-1);
		}

		/* Get the hardware address */
		if(meshwu_saether_aton(bufp, sap) == 0)
		{
			fprintf(stderr, "Invalid hardware address %s\n", bufp);
			return(-1);
		}
	}

#ifdef DEBUG
	{
		char buf[20];
		printf("Hw Address = %s\n", meshwu_saether_ntop(sap, buf));
	}
#endif

	return(0);
}


/* Check if interface support the right MAC address type... */
int meshwu_check_mac_addr_type(int devFd, const char *ifname)
{
	struct ifreq		ifr;

	/* Get the type of hardware address */
	strncpy(ifr.ifr_name, ifname, MESHNAMSIZ);
	if((ioctl(devFd, SIOCGIFHWADDR, &ifr) < 0) || ((ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
		&& (ifr.ifr_hwaddr.sa_family != ARPHRD_IEEE80211)))
	{/* Deep trouble... */
		fprintf(stderr, "Interface %s doesn't support MAC addresses\n", ifname);
		return(-1);
	}

#ifdef DEBUG
	{
		char buf[20];
		printf("Hardware : %d - %s\n", ifr.ifr_hwaddr.sa_family, meshwu_saether_ntop(&ifr.ifr_hwaddr, buf));
	}
#endif

	return(0);
}

/* Check if interface support the right interface address type... */
int meshwu_check_if_addr_type(int devFd, const char *ifname)
{
	struct ifreq		ifr;

	/* Get the type of interface address */
	strncpy(ifr.ifr_name, ifname, MESHNAMSIZ);
	if((ioctl(devFd, SIOCGIFADDR, &ifr) < 0) || (ifr.ifr_addr.sa_family !=  AF_INET))
	{/* Deep trouble... */
		fprintf(stderr, "Interface %s doesn't support IP addresses\n", ifname);
		return(-1);
	}
#ifdef DEBUG
	printf("Interface : %d - 0x%lX\n", ifr.ifr_addr.sa_family, *((unsigned long *) ifr.ifr_addr.sa_data));
#endif

	return(0);
}

#if __ARM_IXP__
void meshwu_print_freq_value(char *buffer,int buflen, int freq)
{
#if 0
	if(freq < KILO_INT)
		snprintf(buffer, buflen, "%d", freq);
	else
#endif		
	{
		char	scale;
		int	divisor;

		if(freq >= GIGA_INT)
		{
			scale = 'G';
			divisor = GIGA_INT;
		}
		else
		{
			if(freq >= MEGA_INT)
			{
				scale = 'M';
				divisor = MEGA_INT;
			}
			else
			{
				scale = 'k';
				divisor = KILO_INT;
			}
		}
		snprintf(buffer, buflen, "%d %cHz", freq / divisor, scale);
	}
}

/* Output a frequency with proper scaling */
void meshwu_print_freq(char *buffer,int buflen,int freq, int channel,int freq_flags)
{
	char	sep = ((freq_flags & IW_FREQ_FIXED) ? '=' : ':');
	char	vbuf[16];

	/* Print the frequency/channel value */
	meshwu_print_freq_value(vbuf, sizeof(vbuf), freq);
	/* Check if channel only */
	if(freq < KILO_INT)
		snprintf(buffer, buflen, "Channel%c%s", sep, vbuf);
	else
	{/* Frequency. Check if we have a channel as well */
		if(channel >= 0)
			snprintf(buffer, buflen, "Frequency%c%s (Channel %d)",sep, vbuf, channel);
		else
			snprintf(buffer, buflen, "Frequency%c%s", sep, vbuf);
	}
}

void meshwu_print_bitrate(char *buffer,int buflen,int bitrate)
{
	int	rate = bitrate;
	char		scale;
	int		divisor;

	if(rate >= GIGA_INT)
	{
		scale = 'G';
		divisor = GIGA_INT;
	}
	else
	{
		if(rate >= MEGA_INT)
		{
			scale = 'M';
			divisor = MEGA_INT;
		}
		else
		{
			scale = 'k';
			divisor = KILO_INT;
		}
	}
	snprintf(buffer, buflen, "%d %cb/s", rate / divisor, scale);
}

#else
void meshwu_print_freq_value(char *buffer,int buflen,double freq)
{
	if(freq < KILO)
		snprintf(buffer, buflen, "%g", freq);
	else
	{
		char	scale;
		int	divisor;

		if(freq >= GIGA)
		{
			scale = 'G';
			divisor = GIGA;
		}
		else
		{
			if(freq >= MEGA)
			{
				scale = 'M';
				divisor = MEGA;
			}
			else
			{
				scale = 'k';
				divisor = KILO;
			}
		}
		snprintf(buffer, buflen, "%g %cHz", freq / divisor, scale);
	}
}

/* Output a frequency with proper scaling */
void meshwu_print_freq(char *buffer,int buflen,double freq, int channel,int freq_flags)
{
	char	sep = ((freq_flags & IW_FREQ_FIXED) ? '=' : ':');
	char	vbuf[16];

	/* Print the frequency/channel value */
	meshwu_print_freq_value(vbuf, sizeof(vbuf), freq);
	/* Check if channel only */
	if(freq < KILO)
		snprintf(buffer, buflen, "Channel%c%s", sep, vbuf);
	else
	{/* Frequency. Check if we have a channel as well */
		if(channel >= 0)
			snprintf(buffer, buflen, "Frequency%c%s (Channel %d)",sep, vbuf, channel);
		else
			snprintf(buffer, buflen, "Frequency%c%s", sep, vbuf);
	}
}

void meshwu_print_bitrate(char *buffer,int buflen,int bitrate)
{
	double	rate = bitrate;
	char		scale;
	int		divisor;

	if(rate >= GIGA)
	{
		scale = 'G';
		divisor = GIGA;
	}
	else
	{
		if(rate >= MEGA)
		{
			scale = 'M';
			divisor = MEGA;
		}
		else
		{
			scale = 'k';
			divisor = KILO;
		}
	}
	snprintf(buffer, buflen, "%g %cb/s", rate / divisor, scale);
}
#endif

/* Output a txpower with proper conversion */
void meshwu_print_txpower(char *buffer,int buflen, struct meshw_param *txpower)
{
	int		dbm;
	/* Check if disabled */
	if(txpower->disabled)
	{
		snprintf(buffer, buflen, "off");
	}
	else
	{/* Check for relative values */
		if(txpower->flags & MESHW_TXPOW_RELATIVE)
		{
			snprintf(buffer, buflen, "%d", txpower->value);
		}
		else
		{/* Convert everything to dBm */
			if(txpower->flags & MESHW_TXPOW_MWATT)
				dbm = meshwu_mwatt2dbm(txpower->value);
			else
				dbm = txpower->value;

			/* Display */
			snprintf(buffer, buflen, "%d dBm", dbm);
		}
	}
}

void meshwu_print_stats(char *buffer,int buflen,const iwqual *qual,const iwrange *range,int has_range)
{
	int		len;
  /* People are very often confused by the 8 bit arithmetic happening
   * here.
   * All the values here are encoded in a 8 bit integer. 8 bit integers
   * are either unsigned [0 ; 255], signed [-128 ; +127] or
   * negative [-255 ; 0].
   * Further, on 8 bits, 0x100 == 256 == 0.
   *
   * Relative/percent values are always encoded unsigned, between 0 and 255.
   * Absolute/dBm values are always encoded negative, between -255 and 0.
   *
   * How do we separate relative from absolute values ?
   * The old way is to use the range to do that. As of WE-19, we have
   * an explicit MESHW_QUAL_DBM flag in updated...
   * The range allow to specify the real min/max of the value. As the
   * range struct only specify one bound of the value, we assume that
   * the other bound is 0 (zero).
   * For relative values, range is [0 ; range->max].
   * For absolute values, range is [range->max ; 0].
   *
   * Let's take two example :
   * 1) value is 75%. qual->value = 75 ; range->max_qual.value = 100
   * 2) value is -54dBm. noise floor of the radio is -104dBm.
   *    qual->value = -54 = 202 ; range->max_qual.value = -104 = 152
   *
   * Jean II
   */

  /* Just do it...
   * The old way to detect dBm require both the range and a non-null
   * level (which confuse the test). The new way can deal with level of 0
   * because it does an explicit test on the flag. */
  if(has_range && ((qual->level != 0) || (qual->updated & MESHW_QUAL_DBM)))
    {
      /* Deal with quality : always a relative value */
      if(!(qual->updated & MESHW_QUAL_QUAL_INVALID))
	{
	  len = snprintf(buffer, buflen, "Quality%c%d/%d  ",
			 qual->updated & MESHW_QUAL_QUAL_UPDATED ? '=' : ':',
			 qual->qual, range->max_qual.qual);
	  buffer += len;
	  buflen -= len;
	}

      /* Check if the statistics are in dBm or relative */
      if((qual->updated & MESHW_QUAL_DBM)
	 || (qual->level > range->max_qual.level))
	{
	  /* Deal with signal level in dBm  (absolute power measurement) */
	  if(!(qual->updated & MESHW_QUAL_LEVEL_INVALID))
	    {
	      len = snprintf(buffer, buflen, "Signal level%c%d dBm  ",
			     qual->updated & MESHW_QUAL_LEVEL_UPDATED ? '=' : ':',
			     qual->level - 0x100);
	      buffer += len;
	      buflen -= len;
	    }

	  /* Deal with noise level in dBm (absolute power measurement) */
	  if(!(qual->updated & MESHW_QUAL_NOISE_INVALID))
	    {
	      len = snprintf(buffer, buflen, "Noise level%c%d dBm",
			     qual->updated & MESHW_QUAL_NOISE_UPDATED ? '=' : ':',
			     qual->noise - 0x100);
	    }
	}
      else
	{
	  /* Deal with signal level as relative value (0 -> max) */
	  if(!(qual->updated & MESHW_QUAL_LEVEL_INVALID))
	    {
	      len = snprintf(buffer, buflen, "Signal level%c%d/%d  ",
			     qual->updated & MESHW_QUAL_LEVEL_UPDATED ? '=' : ':',
			     qual->level, range->max_qual.level);
	      buffer += len;
	      buflen -= len;
	    }

	  /* Deal with noise level as relative value (0 -> max) */
	  if(!(qual->updated & MESHW_QUAL_NOISE_INVALID))
	    {
	      len = snprintf(buffer, buflen, "Noise level%c%d/%d",
			     qual->updated & MESHW_QUAL_NOISE_UPDATED ? '=' : ':',
			     qual->noise, range->max_qual.noise);
	    }
	}
    }
  else
    {
      /* We can't read the range, so we don't know... */
      snprintf(buffer, buflen,
	       "Quality:%d  Signal level:%d  Noise level:%d",
	       qual->qual, qual->level, qual->noise);
    }
}

void meshwu_print_key(char *	buffer,int buflen,const unsigned char *key,int key_size,int key_flags)
{
  int	i;

  /* Check buffer size -> 1 bytes => 2 digits + 1/2 separator */
  if((key_size * 3) > buflen)
    {
      snprintf(buffer, buflen, "<too big>");
      return;
    }

  /* Is the key present ??? */
  if(key_flags & MESHW_ENCODE_NOKEY)
    {
      /* Nope : print on or dummy */
      if(key_size <= 0)
	strcpy(buffer, "on");			/* Size checked */
      else
	{
	  strcpy(buffer, "**");			/* Size checked */
	  buffer +=2;
	  for(i = 1; i < key_size; i++)
	    {
	      if((i & 0x1) == 0)
		strcpy(buffer++, "-");		/* Size checked */
	      strcpy(buffer, "**");		/* Size checked */
	      buffer +=2;
	    }
	}
    }
  else
    {
      /* Yes : print the key */
      sprintf(buffer, "%.2X", key[0]);		/* Size checked */
      buffer +=2;
      for(i = 1; i < key_size; i++)
	{
	  if((i & 0x1) == 0)
	    strcpy(buffer++, "-");		/* Size checked */
	  sprintf(buffer, "%.2X", key[i]);	/* Size checked */
	  buffer +=2;
	}
    }
}

void meshwu_print_pm_value(char *buffer,int buflen,int value,int flags,int we_version)
{
	/* Check size */
	if(buflen < 25)
	{
		snprintf(buffer, buflen, "<too big>");
		return;
	}
	buflen -= 25;

	/* Modifiers */
	if(flags & MESHW_POWER_MIN)
	{
		strcpy(buffer, " min");
		buffer += 4;
	}
	if(flags & MESHW_POWER_MAX)
	{
		strcpy(buffer, " max");
		buffer += 4;
	}

	/* Type */
	if(flags & MESHW_POWER_TIMEOUT)
	{
		strcpy(buffer, " timeout:");
		buffer += 9;
	}
	else
	{
		if(flags & MESHW_POWER_SAVING)
		{
			strcpy(buffer, " saving:");
			buffer += 8;
		}
		else
		{
			strcpy(buffer, " period:");
			buffer += 8;
		}
	}

	/* Display value without units */
	if(flags & MESHW_POWER_RELATIVE)
	{
		if(we_version < 21)
			value /= MEGA;
		snprintf(buffer, buflen, "%d", value);
	}
	else
	{/* Display value with units */
		if(value >= (int) MEGA)
			snprintf(buffer, buflen, "%gs", ((double) value) / MEGA);
		else
			if(value >= (int) KILO)
				snprintf(buffer, buflen, "%gms", ((double) value) / KILO);
			else
				snprintf(buffer, buflen, "%dus", value);
	}
}

void meshwu_print_pm_mode(char *buffer, int buflen,int flags)
{
	if(buflen < 28)
	{
		snprintf(buffer, buflen, "<too big>");
		return;
	}

	/* Print the proper mode... */
	switch(flags & MESHW_POWER_MODE)
	{
		case MESHW_POWER_UNICAST_R:
			strcpy(buffer, "mode:Unicast only received");
			break;
		case MESHW_POWER_MULTICAST_R:
			strcpy(buffer, "mode:Multicast only received");
			break;
		case MESHW_POWER_ALL_R:
			strcpy(buffer, "mode:All packets received");
			break;
		case MESHW_POWER_FORCE_S:
			strcpy(buffer, "mode:Force sending");
			break;
		case MESHW_POWER_REPEATER:
			strcpy(buffer, "mode:Repeat multicasts");
			break;
		default:
			strcpy(buffer, "");
			break;
	}
}

void meshwu_print_retry_value(char *buffer,int buflen,int value,int flags, int we_version)
{
	if(buflen < 20)
	{
		snprintf(buffer, buflen, "<too big>");
		return;
	}
	buflen -= 20;

	/* Modifiers */
	if(flags & MESHW_RETRY_MIN)
	{
		strcpy(buffer, " min");
		buffer += 4;
	}
	if(flags & MESHW_RETRY_MAX)
	{
		strcpy(buffer, " max");
		buffer += 4;
	}
	if(flags & MESHW_RETRY_SHORT)
	{
		strcpy(buffer, " short");
		buffer += 6;
	}
	if(flags & MESHW_RETRY_LONG)
	{
		strcpy(buffer, "  long");
		buffer += 6;
	}

	/* Type lifetime of limit */
	if(flags & MESHW_RETRY_LIFETIME)
	{
		strcpy(buffer, " lifetime:");
		buffer += 10;

		/* Display value without units */
		if(flags & MESHW_POWER_RELATIVE)
		{
			if(we_version < 21)
				value /= MEGA;
			snprintf(buffer, buflen, "%d", value);
		}
		else
		{/* Display value with units */
			if(value >= (int) MEGA)
				snprintf(buffer, buflen, "%gs", ((double) value) / MEGA);
			else
				if(value >= (int) KILO)
					snprintf(buffer, buflen, "%gms", ((double) value) / KILO);
				else
					snprintf(buffer, buflen, "%dus", value);
		}
	}
	else
		snprintf(buffer, buflen, " limit:%d", value);
}

/* Inspired from irdadump... */
void meshwu_print_timeval(char *buffer,int buflen, const struct timeval *timev,const struct timezone *tz)
{
	int s;
	s = (timev->tv_sec - tz->tz_minuteswest * 60) % 86400;
	snprintf(buffer, buflen, "%02d:%02d:%02d.%06u", 
		s / 3600, (s % 3600) / 60, s % 60, (u_int32_t) timev->tv_usec);
}

