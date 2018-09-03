/*
 * $Id: meshlist.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "meshwu_lib.h"
#include <sys/time.h>

#define IW_SCAN_HACK		0x8000

/* Scan state and meta-information, used to decode events... */
typedef struct iwscan_state
{/* State */
	int			ap_num;		/* Access Point number 1->N */
	int			val_index;	/* Value in table 0->(N-1) */
} iwscan_state;

/***************************** SCANNING *****************************/
/*
 * This one behave quite differently from the others
 *
 * Note that we don't use the scanning capability of iwlib (functions
 * iw_process_scan() and iw_scan()). The main reason is that
 * iw_process_scan() return only a subset of the scan data to the caller,
 * for example custom elements and bitrates are ommited. Here, we
 * do the complete job...
 */

/* Parse, and display the results of a WPA or WPA2 IE. */
static void meshwu_print_ie_unknown(unsigned char *iebuf, int buflen)
{
	int	ielen = iebuf[1] + 2;
	int	i;
	if(ielen > buflen)
		ielen = buflen;

	printf("Unknown: ");
	for(i = 0; i < ielen; i++)
		printf("%02X", iebuf[i]);
	printf("\n");
}

/* Display the cipher type for the value passed in. */
static const char *	iw_cipher_name[] = 
{
	"None or same as Group ",
	"WEP-40 ",
	"TKIP ",
	"WRAP ",
	"CCMP ",
	"WEP-104 ",
	"Unknown ",
};
#define	IW_CIPHER_NUM	(sizeof(iw_cipher_name)/sizeof(iw_cipher_name[0]))

static inline void  meshwu_print_ie_cipher(unsigned char csuite)
{
	if(csuite > (IW_CIPHER_NUM - 1))
		csuite = IW_CIPHER_NUM - 1;
	printf(iw_cipher_name[csuite]);
}
 
/* Display the authentication suite for the value passed in. */
static const char *	iw_auth_name[] =
{
	"Reserved  ",
	"802.1X  ",
	"PSK  ",
	"Unknown ",
};
#define	IW_AUTH_NUM	(sizeof(iw_auth_name)/sizeof(iw_auth_name[0]))

static inline void  meshwu_print_ie_auth(unsigned char	asuite)
{
	if(asuite > (IW_AUTH_NUM - 1))
		asuite = IW_AUTH_NUM - 1;
	printf(iw_auth_name[asuite]);
}
 
/* Parse, and display the results of a WPA or WPA2 IE. *
 */
static inline void meshwu_print_ie_wpa(unsigned char *iebuf,int buflen)
{
	int			ielen = iebuf[1] + 2;
	int			offset = 2;	/* Skip the IE id, and the length. */
	unsigned char		wpa1_oui[3] = {0x00, 0x50, 0xf2};
	unsigned char		wpa2_oui[3] = {0x00, 0x0f, 0xac};
	unsigned char *	wpa_oui;
	int			i;
	uint16_t		ver = 0;
	uint16_t		cnt = 0;

	if(ielen > buflen)
		ielen = buflen;

	switch(iebuf[0])
	{
    case 0x30:		/* WPA2 */
      /* Check if we have enough data */
      if(ielen < 4)
	{
	  meshwu_print_ie_unknown(iebuf, buflen);
 	  return;
	}

      wpa_oui = wpa2_oui;
      break;

    case 0xdd:		/* WPA or else */
      wpa_oui = wpa1_oui;
 
      /* Not all IEs that start with 0xdd are WPA. 
       * So check that the OUI is valid. */
      if((ielen < 8)
	 || ((memcmp(&iebuf[offset], wpa_oui, 3) != 0)
	     && (iebuf[offset+3] == 0x01)))
 	{
	  meshwu_print_ie_unknown(iebuf, buflen);
 	  return;
 	}

       offset += 4;
       break;

    default:
      return;
    }
  
  /* Pick version number (little endian) */
  ver = iebuf[offset] | (iebuf[offset + 1] << 8);
  offset += 2;

  if(iebuf[0] == 0xdd)
    printf("WPA Version %d\n", ver);
  if(iebuf[0] == 0x30)
    printf("IEEE 802.11i/WPA2 Version %d\n", ver);

  /* From here, everything is technically optional. */

  /* Check if we are done */
  if(ielen < (offset + 4))
    {
      /* We have a short IE.  So we should assume TKIP/TKIP. */
      printf("                        Group Cipher : TKIP\n");
      printf("                        Pairwise Cipher : TKIP\n");
      return;
    }
 
  /* Next we have our group cipher. */
  if(memcmp(&iebuf[offset], wpa_oui, 3) != 0)
    {
      printf("                        Group Cipher : Proprietary\n");
    }
  else
    {
      printf("                        Group Cipher : ");
      meshwu_print_ie_cipher(iebuf[offset+3]);
      printf("\n");
    }
  offset += 4;

  /* Check if we are done */
  if(ielen < (offset + 2))
    {
      /* We don't have a pairwise cipher, or auth method. Assume TKIP. */
      printf("                        Pairwise Ciphers (1) : TKIP\n");
      return;
    }

  /* Otherwise, we have some number of pairwise ciphers. */
  cnt = iebuf[offset] | (iebuf[offset + 1] << 8);
  offset += 2;
  printf("                        Pairwise Ciphers (%d) : ", cnt);

  if(ielen < (offset + 4*cnt))
    return;

  for(i = 0; i < cnt; i++)
    {
      if(memcmp(&iebuf[offset], wpa_oui, 3) != 0)
 	{
 	  printf("Proprietary  ");
 	}
      else
	{
 	  meshwu_print_ie_cipher(iebuf[offset+3]);
 	}
      offset+=4;
    }
  printf("\n");
 
  /* Check if we are done */
  if(ielen < (offset + 2))
    return;

  /* Now, we have authentication suites. */
  cnt = iebuf[offset] | (iebuf[offset + 1] << 8);
  offset += 2;
  printf("                        Authentication Suites (%d) : ", cnt);

  if(ielen < (offset + 4*cnt))
    return;

  for(i = 0; i < cnt; i++)
    {
      if(memcmp(&iebuf[offset], wpa_oui, 3) != 0)
 	{
 	  printf("Proprietary  ");
 	}
      else
	{
 	  meshwu_print_ie_auth(iebuf[offset+3]);
 	}
       offset+=4;
     }
  printf("\n");
 
  /* Check if we are done */
  if(ielen < (offset + 1))
    return;

  /* Otherwise, we have capabilities bytes.
   * For now, we only care about preauth which is in bit position 1 of the
   * first byte.  (But, preauth with WPA version 1 isn't supposed to be 
   * allowed.) 8-) */
  if(iebuf[offset] & 0x01)
    {
      printf("                       Preauthentication Supported\n");
    }
}
 
/*
 * Process a generic IE and display the info in human readable form
 * for some of the most interesting ones.
 * For now, we only decode the WPA IEs.
 */
static inline void meshwu_print_gen_ie(unsigned char *	buffer,int buflen)
{
  int offset = 0;

  /* Loop on each IE, each IE is minimum 2 bytes */
  while(offset <= (buflen - 2))
    {
      printf("                    IE: ");

      /* Check IE type */
      switch(buffer[offset])
	{
	case 0xdd:	/* WPA1 (and other) */
	case 0x30:	/* WPA2 */
	  meshwu_print_ie_wpa(buffer + offset, buflen);
	  break;
	default:
	  meshwu_print_ie_unknown(buffer + offset, buflen);
	}
      /* Skip over this IE to the next one in the list. */
      offset += buffer[offset+1] + 2;
    }
}

/* Print one element from the scanning results */
static inline void print_scanning_token(struct meshwu_stream_descr *	stream,	/* Stream of events */
		     struct meshw_event *		event,	/* Extracted token */
		     struct iwscan_state *	state,
		     struct meshw_range *	iw_range,	/* Range info */
		     int		has_range)
{
  char		buffer[128];	/* Temporary buffer */

  /* Now, let's decode the event */
	switch(event->cmd)
	{
		case MESHW_IO_R_AP_MAC:
			printf("          Cell %02d - Address: %s\n", state->ap_num,
				meshwu_saether_ntop(&event->u.ap_addr, buffer));
			state->ap_num++;
			break;

		case MESHW_IO_R_NWID:
			if(event->u.nwid.disabled)
				printf("                    NWID:off/any\n");
			else
				printf("                    NWID:%X\n", event->u.nwid.value);
			break;

		case MESHW_IO_R_FREQ:
		{
			int		channel = -1;		/* Converted to channel */
#if __ARM_IXP__
			int			freq;
			GET_KHZ(freq, event->u.freq);
#else
			double		freq;
			freq = meshwu_freq2float(&(event->u.freq));
#endif
			/* Convert to channel if possible */
			if(has_range)
				channel = meshwu_freq_to_channel(freq, iw_range);
			meshwu_print_freq(buffer, sizeof(buffer), freq, channel, event->u.freq.flags);
			printf("                    %s\n", buffer);
		}	
		break;
		case MESHW_IO_R_MODE:
			printf("                    Mode:%s\n", meshwu_operation_mode[event->u.mode]);
			break;

		case MESHW_IO_R_NAME:
      printf("                    Protocol:%-1.16s\n", event->u.name);
      break;
    case MESHW_IO_R_ESSID:
      {
	char essid[MESHW_ESSID_MAX_SIZE+1];
	memset(essid, '\0', sizeof(essid));
	if((event->u.essid.pointer) && (event->u.essid.length))
	  memcpy(essid, event->u.essid.pointer, event->u.essid.length);
	if(event->u.essid.flags)
	  {
	    /* Does it have an ESSID index ? */
	    if((event->u.essid.flags & MESHW_ENCODE_INDEX) > 1)
	      printf("                    ESSID:\"%s\" [%d]\n", essid,
		     (event->u.essid.flags & MESHW_ENCODE_INDEX));
	    else
	      printf("                    ESSID:\"%s\"\n", essid);
	  }
	else
	  printf("                    ESSID:off/any/hidden\n");
      }
      break;
    case MESHW_IO_R_ENCODE:
      {
	unsigned char	key[MESHW_ENCODING_TOKEN_MAX];
	if(event->u.data.pointer)
	  memcpy(key, event->u.data.pointer, event->u.data.length);
	else
	  event->u.data.flags |= MESHW_ENCODE_NOKEY;
	printf("                    Encryption key:");
	if(event->u.data.flags & MESHW_ENCODE_DISABLED)
	  printf("off\n");
	else
	  {
	    /* Display the key */
	    meshwu_print_key(buffer, sizeof(buffer), key, event->u.data.length,
			 event->u.data.flags);
	    printf("%s", buffer);

	    /* Other info... */
	    if((event->u.data.flags & MESHW_ENCODE_INDEX) > 1)
	      printf(" [%d]", event->u.data.flags & MESHW_ENCODE_INDEX);
	    if(event->u.data.flags & MESHW_ENCODE_RESTRICTED)
	      printf("   Security mode:restricted");
	    if(event->u.data.flags & MESHW_ENCODE_OPEN)
	      printf("   Security mode:open");
	    printf("\n");
	  }
      }
      break;
    case MESHW_IO_R_RATE:
      if(state->val_index == 0)
	printf("                    Bit Rates:");
      else
	if((state->val_index % 5) == 0)
	  printf("\n                              ");
	else
	  printf("; ");
      meshwu_print_bitrate(buffer, sizeof(buffer), event->u.bitrate.value);
      printf("%s", buffer);
      /* Check for termination */
      if(stream->value == NULL)
	{
	  printf("\n");
	  state->val_index = 0;
	}
      else
	state->val_index++;
      break;
#if __ARM_IXP__
#else
	case SIOCGIWMODUL:
	{
		unsigned int	modul = event->u.param.value;
		int		i;
		int		n = 0;
		printf("                    Modulations :");
		for(i = 0; i < IW_SIZE_MODUL_LIST; i++)
		{
			if((modul & meshwu_modul_list[i].mask) == meshwu_modul_list[i].mask)
			{
				if((n++ % 8) == 7)
					printf("\n                        ");
				else
					printf(" ; ");
				printf("%s", meshwu_modul_list[i].cmd);
			}
		}
		printf("\n");
	}
	break;
#endif

    case MESHW_EV_QUAL:
      meshwu_print_stats(buffer, sizeof(buffer),
		     &event->u.qual, iw_range, has_range);
      printf("                    %s\n", buffer);
      break;
#if __ARM_IXP__	  
#else
	case IWEVGENIE:
		/* Informations Elements are complex, let's do only some of them */
		meshwu_print_gen_ie(event->u.data.pointer, event->u.data.length);
		break;
#endif	  
    case MESHW_EV_CUSTOM:
      {
	char custom[MESHW_CUSTOM_MAX+1];
	if((event->u.data.pointer) && (event->u.data.length))
	  memcpy(custom, event->u.data.pointer, event->u.data.length);
	custom[event->u.data.length] = '\0';
	printf("                    Extra:%s\n", custom);
      }
      break;
    default:
      printf("                    (Unknown Wireless Token 0x%04X)\n",
	     event->cmd);
   }	/* switch(event->cmd) */
}

/* Perform a scanning on one device  */
static int print_scanning_info(int devFd,char *ifname,char *args[], int count)
{
	struct meshw_req		wrq;
#if __ARM_IXP__
#else
	struct iw_scan_req    scanopt;		/* Options for 'set' */
#endif
	int			scanflags = 0;		/* Flags for scan */
  unsigned char *	buffer = NULL;		/* Results */
  int			buflen = MESHW_SCAN_MAX_DATA; /* Min for compat WE<17 */
  struct meshw_range	range;
  int			has_range;
  struct timeval	tv;				/* Select timeout */
  int			timeout = 15000000;		/* 15s */

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Get range stuff */
  has_range = (meshwu_get_range_info(devFd, ifname, &range) >= 0);

  /* Check if the interface could support scanning. */
  if((!has_range) || (range.we_version_compiled < 14))
    {
      fprintf(stderr, "%-8.16s  Interface doesn't support scanning.\n\n",
	      ifname);
      return(-1);
    }

  /* Init timeout value -> 250ms between set and first get */
  tv.tv_sec = 0;
  tv.tv_usec = 250000;

#if __ARM_IXP__
#else
	memset(&scanopt, 0, sizeof(scanopt));
#endif

  /* Parse command line arguments and extract options.
   * Note : when we have enough options, we should use the parser
   * from iwconfig... */
	while(count > 0)
	{
		/* One arg is consumed (the option name) */
		count--;

		/* Check for Active Scan (scan with specific essid)*/
		if(!strncmp(args[0], "essid", 5))
		{
			if(count < 1)
			{
				fprintf(stderr, "Too few arguments for scanning option [%s]\n", args[0]);
				return(-1);
			}
			args++;
			count--;

#if __ARM_IXP__
#else
			/* Store the ESSID in the scan options */
			scanopt.essid_len = strlen(args[0]);
			memcpy(scanopt.essid, args[0], scanopt.essid_len);
			/* Initialise BSSID as needed */
			if(scanopt.bssid.sa_family == 0)
			{
				scanopt.bssid.sa_family = ARPHRD_ETHER;
				memset(scanopt.bssid.sa_data, 0xff, ETH_ALEN);
			}
#endif
			/* Scan only this ESSID */
			scanflags |= MESHW_SCAN_THIS_ESSID;
		}
		else if(!strncmp(args[0], "last", 4))
		{/* Check for last scan result (do not trigger scan) */
		/* Hack */
			scanflags |= IW_SCAN_HACK;
		}
		else
		{
			fprintf(stderr, "Invalid scanning option [%s]\n", args[0]);
			return(-1);
		}

		/* Next arg */
		args++;
	}


#if __ARM_IXP__
#else
	/* Check if we have scan options */
	if(scanflags)
	{
		wrq.u.data.pointer = (caddr_t) &scanopt;
		wrq.u.data.length = sizeof(scanopt);
		wrq.u.data.flags = scanflags;
	}
	else
#endif
	{
		wrq.u.data.pointer = NULL;
		wrq.u.data.flags = 0;
		wrq.u.data.length = 0;
	}

  /* If only 'last' was specified on command line, don't trigger a scan */
  if(scanflags == IW_SCAN_HACK)
    {
      /* Skip waiting */
      tv.tv_usec = 0;
    }
  else
    {
      /* Initiate Scanning */
      if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_SCAN, &wrq) < 0)
	{
	  if((errno != EPERM) || (scanflags != 0))
	    {
	      fprintf(stderr, "%-8.16s  Interface doesn't support scanning : %s\n\n",
		      ifname, strerror(errno));
	      return(-1);
	    }
	  /* If we don't have the permission to initiate the scan, we may
	   * still have permission to read left-over results.
	   * But, don't wait !!! */
#if 0
	  /* Not cool, it display for non wireless interfaces... */
	  fprintf(stderr, "%-8.16s  (Could not trigger scanning, just reading left-over results)\n", ifname);
#endif
	  tv.tv_usec = 0;
	}
    }
  timeout -= tv.tv_usec;

  /* Forever */
  while(1)
    {
      fd_set		rfds;		/* File descriptors for select */
      int		last_fd;	/* Last fd */
      int		ret;

      /* Guess what ? We must re-generate rfds each time */
      FD_ZERO(&rfds);
      last_fd = -1;

      /* In here, add the rtnetlink fd in the list */

      /* Wait until something happens */
      ret = select(last_fd + 1, &rfds, NULL, NULL, &tv);

      /* Check if there was an error */
      if(ret < 0)
	{
	  if(errno == EAGAIN || errno == EINTR)
	    continue;
	  fprintf(stderr, "Unhandled signal - exiting...\n");
	  return(-1);
	}

      /* Check if there was a timeout */
      if(ret == 0)
	{
	  unsigned char *	newbuf;

	realloc:
	  /* (Re)allocate the buffer - realloc(NULL, len) == malloc(len) */
	  newbuf = realloc(buffer, buflen);
	  if(newbuf == NULL)
	    {
	      if(buffer)
		free(buffer);
	      fprintf(stderr, "%s: Allocation failed\n", __FUNCTION__);
	      return(-1);
	    }
	  buffer = newbuf;

	  /* Try to read the results */
	  wrq.u.data.pointer = buffer;
	  wrq.u.data.flags = 0;
	  wrq.u.data.length = buflen;
	  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_SCAN, &wrq) < 0)
	    {
	      /* Check if buffer was too small (WE-17 only) */
	      if((errno == E2BIG) && (range.we_version_compiled > 16))
		{
		  /* Some driver may return very large scan results, either
		   * because there are many cells, or because they have many
		   * large elements in cells (like MESHW_EV_CUSTOM). Most will
		   * only need the regular sized buffer. We now use a dynamic
		   * allocation of the buffer to satisfy everybody. Of course,
		   * as we don't know in advance the size of the array, we try
		   * various increasing sizes. Jean II */

		  /* Check if the driver gave us any hints. */
		  if(wrq.u.data.length > buflen)
		    buflen = wrq.u.data.length;
		  else
		    buflen *= 2;

		  /* Try again */
		  goto realloc;
		}

	      /* Check if results not available yet */
	      if(errno == EAGAIN)
		{
		  /* Restart timer for only 100ms*/
		  tv.tv_sec = 0;
		  tv.tv_usec = 100000;
		  timeout -= tv.tv_usec;
		  if(timeout > 0)
		    continue;	/* Try again later */
		}

	      /* Bad error */
	      free(buffer);
	      fprintf(stderr, "%-8.16s  Failed to read scan data : %s\n\n",
		      ifname, strerror(errno));
	      return(-2);
	    }
	  else
	    /* We have the results, go to process them */
	    break;
	}

      /* In here, check if event and event type
       * if scan event, read results. All errors bad & no reset timeout */
    }

  if(wrq.u.data.length)
    {
      struct meshw_event		iwe;
      struct meshwu_stream_descr	stream;
      struct iwscan_state	state = { .ap_num = 1, .val_index = 0 };
      int			ret;
      
#if 0
      /* Debugging code. In theory useless, because it's debugged ;-) */
      int	i;
      printf("Scan result %d [%02X", wrq.u.data.length, buffer[0]);
      for(i = 1; i < wrq.u.data.length; i++)
	printf(":%02X", buffer[i]);
      printf("]\n");
#endif
      printf("%-8.16s  Scan completed :\n", ifname);
      meshwu_init_event_stream(&stream, (char *) buffer, wrq.u.data.length);
      do
	{
	  /* Extract an event and print it */
	  ret = meshwu_extract_event_stream(&stream, &iwe,
					range.we_version_compiled);
	  if(ret > 0)
	    print_scanning_token(&stream, &iwe, &state,
				 &range, has_range);
	}
      while(ret > 0);
      printf("\n");
    }
  else
    printf("%-8.16s  No scan results\n", ifname);

  free(buffer);
  return(0);
}

/* Print the number of channels and available frequency for the device */
static int print_freq_info(int devFd,char *ifname,char *args[], int count)
{
	struct meshw_req		wrq;
	struct meshw_range	range;
#if __ARM_IXP__
	int			freq;
#else
	double		freq;
#endif
	int			k;
	int			channel;
	char			buffer[128];

	args = args; count = count;

	if(meshwu_get_range_info(devFd, ifname, &range) < 0)
		fprintf(stderr, "%-8.16s  no frequency information.\n\n", ifname);
	else
	{
		if(range.num_frequency > 0)
		{
			printf("%-8.16s  %d channels in total; available frequencies :\n", ifname, range.num_channels);
			for(k = 0; k < range.num_frequency; k++)
			{
#if __ARM_IXP__			
				GET_MHZ(freq, range.freq[k]);
				printf("          Channel %.2d : %d MHz\n", range.freq[k].i, freq);//range.freq[k].m );
//				meshwu_print_freq_value(buffer, sizeof(buffer), freq*KILO_INT);
#else
				freq = meshwu_freq2float(&(range.freq[k]));
				meshwu_print_freq_value(buffer, sizeof(buffer), freq);
				printf("          Channel %.2d : %s\n", range.freq[k].i, buffer);
#endif
			}
		}
		else
			printf("%-8.16s  %d channels\n", ifname, range.num_channels);

		/* Get current frequency / channel and display it */
		if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_FREQ, &wrq) >= 0)
		{
#if __ARM_IXP__
			GET_KHZ(freq, wrq.u.freq);
#else
			freq = meshwu_freq2float(&(wrq.u.freq));
#endif
			channel = meshwu_freq_to_channel(freq, &range);
			meshwu_print_freq(buffer, sizeof(buffer), freq, channel, wrq.u.freq.flags);
			printf("          Current %s\n\n", buffer);
		}
	}
	return(0);
}

/* Print the number of available bitrates for the device  */
static int print_bitrate_info(int devFd,char *ifname,char *args[], int count)
{
  struct meshw_req		wrq;
  struct meshw_range	range;
  int			k;
  char			buffer[128];

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Extract range info */
  if(meshwu_get_range_info(devFd, ifname, &range) < 0)
      fprintf(stderr, "%-8.16s  no bit-rate information.\n\n",
		      ifname);
  else
    {
      if((range.num_bitrates > 0) && (range.num_bitrates <= MESHW_MAX_BITRATES))
	{
	  printf("%-8.16s  %d available bit-rates :\n",
		 ifname, range.num_bitrates);
	  /* Print them all */
	  for(k = 0; k < range.num_bitrates; k++)
	    {
	      meshwu_print_bitrate(buffer, sizeof(buffer), range.bitrate[k]);
	      /* Maybe this should be %10s */
	      printf("\t  %s\n", buffer);
	    }
	}
      else
	printf("%-8.16s  unknown bit-rate information.\n", ifname);

      /* Get current bit rate */
      if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RATE, &wrq) >= 0)
	{
	  meshwu_print_bitrate(buffer, sizeof(buffer), wrq.u.bitrate.value);
	  printf("          Current Bit Rate%c%s\n",
		 (wrq.u.bitrate.fixed ? '=' : ':'), buffer);
	}

      /* Try to get the broadcast bitrate if it exist... */
      if(range.bitrate_capa & IW_BITRATE_BROADCAST)
	{
	  wrq.u.bitrate.flags = IW_BITRATE_BROADCAST;
	  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RATE, &wrq) >= 0)
	    {
	      meshwu_print_bitrate(buffer, sizeof(buffer), wrq.u.bitrate.value);
	      printf("          Broadcast Bit Rate%c%s\n",
		     (wrq.u.bitrate.fixed ? '=' : ':'), buffer);
	    }
	}

      printf("\n");
    }
  return(0);
}

/************************* ENCRYPTION KEYS *************************/
/* Print the number of available encryption key for the device */
static int print_keys_info(int devFd,char *ifname,char *args[], int count)
{
  struct meshw_req		wrq;
  struct meshw_range	range;
  unsigned char		key[MESHW_ENCODING_TOKEN_MAX];
  int			k;
  char			buffer[128];

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Extract range info */
  if(meshwu_get_range_info(devFd, ifname, &range) < 0)
      fprintf(stderr, "%-8.16s  no encryption keys information.\n\n",
		      ifname);
  else
    {
      printf("%-8.16s  ", ifname);
      /* Print key sizes */
      if((range.num_encoding_sizes > 0) &&
	 (range.num_encoding_sizes < MESHW_MAX_ENCODING_SIZES))
	{
	  printf("%d key sizes : %d", range.num_encoding_sizes,
		 range.encoding_size[0] * 8);
	  /* Print them all */
	  for(k = 1; k < range.num_encoding_sizes; k++)
	    printf(", %d", range.encoding_size[k] * 8);
	  printf("bits\n          ");
	}
      /* Print the keys and associate mode */
      printf("%d keys available :\n", range.max_encoding_tokens);
      for(k = 1; k <= range.max_encoding_tokens; k++)
	{
	  wrq.u.data.pointer = (caddr_t) key;
	  wrq.u.data.length = MESHW_ENCODING_TOKEN_MAX;
	  wrq.u.data.flags = k;
	  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_ENCODE, &wrq) < 0)
	    {
	      fprintf(stderr, "Error reading wireless keys (MESHW_IO_R_ENCODE): %s\n", strerror(errno));
	      break;
	    }
	  if((wrq.u.data.flags & MESHW_ENCODE_DISABLED) ||
	     (wrq.u.data.length == 0))
	    printf("\t\t[%d]: off\n", k);
	  else
	    {
	      /* Display the key */
	      meshwu_print_key(buffer, sizeof(buffer),
			   key, wrq.u.data.length, wrq.u.data.flags);
	      printf("\t\t[%d]: %s", k, buffer);

	      /* Other info... */
	      printf(" (%d bits)", wrq.u.data.length * 8);
	      printf("\n");
	    }
	}
      /* Print current key and mode */
      wrq.u.data.pointer = (caddr_t) key;
      wrq.u.data.length = MESHW_ENCODING_TOKEN_MAX;
      wrq.u.data.flags = 0;	/* Set index to zero to get current */
      if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_ENCODE, &wrq) >= 0)
	{
	  /* Note : if above fails, we have already printed an error
	   * message int the loop above */
	  printf("          Current Transmit Key: [%d]\n",
		 wrq.u.data.flags & MESHW_ENCODE_INDEX);
	  if(wrq.u.data.flags & MESHW_ENCODE_RESTRICTED)
	    printf("          Security mode:restricted\n");
	  if(wrq.u.data.flags & MESHW_ENCODE_OPEN)
	    printf("          Security mode:open\n");
	}
#if __ARM_IXP__
#else
      /* Print WPA/802.1x/802.11i security parameters */
      if(range.we_version_compiled > 17)
	{
	  /* Display advance encryption capabilities */
	  if(range.enc_capa)
	    {
	      const char *	auth_string[] = { "WPA",
						  "WPA2",
						  "CIPHER TKIP",
						  "CIPHER CCMP" };
	      const int		auth_num = (sizeof(auth_string) /
					    sizeof(auth_string[1]));
	      int		i;
	      int		mask = 0x1;

	      printf("          Authentication capabilities :\n");
	      for(i = 0; i < auth_num; i++)
		{
		  if(range.enc_capa & mask)
		    printf("\t\t%s\n", auth_string[i]);
		  mask <<= 1;
		}
	    }

	  /* Current values for authentication */
	  wrq.u.param.flags = IW_AUTH_KEY_MGMT;
	  if(meshwu_get_ext(devFd, ifname, SIOCGIWAUTH, &wrq) >= 0)
	      printf("          Current key_mgmt:0x%X\n",
		     wrq.u.param.value);

	  wrq.u.param.flags = IW_AUTH_CIPHER_PAIRWISE;
	  if(meshwu_get_ext(devFd, ifname, SIOCGIWAUTH, &wrq) >= 0)
	      printf("          Current cipher_pairwise:0x%X\n",
		     wrq.u.param.value);

	  wrq.u.param.flags = IW_AUTH_CIPHER_GROUP;
	  if(meshwu_get_ext(devFd, ifname, SIOCGIWAUTH, &wrq) >= 0)
	    printf("          Current cipher_group:0x%X\n",
		   wrq.u.param.value);
	}

     printf("\n\n");
#endif
    }

  return(0);
}

/* Print Power Management info for each device */
static int get_pm_value(int		devFd,
	     char *		ifname,
	     struct meshw_req *	pwrq,
	     int		flags,
	     char *		buffer,
	     int		buflen,
	     int		we_version_compiled)
{
  /* Get Another Power Management value */
  pwrq->u.power.flags = flags;
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_POWER_MGMT, pwrq) >= 0)
    {
      /* Let's check the value and its type */
      if(pwrq->u.power.flags & MESHW_POWER_TYPE)
	{
	  meshwu_print_pm_value(buffer, buflen,
			    pwrq->u.power.value, pwrq->u.power.flags,
			    we_version_compiled);
	  printf("\n                 %s", buffer);
	}
    }
  return(pwrq->u.power.flags);
}

/* Print Power Management range for each type */
static void
print_pm_value_range(char *		name,
		     int		mask,
		     int		iwr_flags,
		     int		iwr_min,
		     int		iwr_max,
		     char *		buffer,
		     int		buflen,
		     int		we_version_compiled)
{
  if(iwr_flags & mask)
    {
      int	flags = (iwr_flags & ~(MESHW_POWER_MIN | MESHW_POWER_MAX));
      /* Display if auto or fixed */
      printf("%s %s ; ",
	     (iwr_flags & MESHW_POWER_MIN) ? "Auto " : "Fixed",
	     name);
      /* Print the range */
      meshwu_print_pm_value(buffer, buflen,
			iwr_min, flags | MESHW_POWER_MIN,
			we_version_compiled);
      printf("%s\n                          ", buffer);
      meshwu_print_pm_value(buffer, buflen,
			iwr_max, flags | MESHW_POWER_MAX,
			we_version_compiled);
      printf("%s\n          ", buffer);
    }
}

/*
 * Power Management types of values
 */
static const unsigned int pm_type_flags[] = {
  MESHW_POWER_PERIOD,
  MESHW_POWER_TIMEOUT,
  MESHW_POWER_SAVING,
};
static const int pm_type_flags_size = (sizeof(pm_type_flags)/sizeof(pm_type_flags[0]));

/* Print Power Management info for each device */
static int print_pm_info(int devFd,char *ifname,char *args[], int count)
{
  struct meshw_req		wrq;
  struct meshw_range	range;
  char			buffer[128];

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Extract range info */
  if((meshwu_get_range_info(devFd, ifname, &range) < 0) ||
     (range.we_version_compiled < 10))
      fprintf(stderr, "%-8.16s  no power management information.\n\n",
		      ifname);
  else
    {
      printf("%-8.16s  ", ifname);

      /* Display modes availables */
      if(range.pm_capa & MESHW_POWER_MODE)
	{
	  printf("Supported modes :\n          ");
	  if(range.pm_capa & (MESHW_POWER_UNICAST_R | MESHW_POWER_MULTICAST_R))
	    printf("\t\to Receive all packets (unicast & multicast)\n          ");
	  if(range.pm_capa & MESHW_POWER_UNICAST_R)
	    printf("\t\to Receive Unicast only (discard multicast)\n          ");
	  if(range.pm_capa & MESHW_POWER_MULTICAST_R)
	    printf("\t\to Receive Multicast only (discard unicast)\n          ");
	  if(range.pm_capa & MESHW_POWER_FORCE_S)
	    printf("\t\to Force sending using Power Management\n          ");
	  if(range.pm_capa & MESHW_POWER_REPEATER)
	    printf("\t\to Repeat multicast\n          ");
	}
      /* Display min/max period availables */
      print_pm_value_range("period ", MESHW_POWER_PERIOD,
			   range.pmp_flags, range.min_pmp, range.max_pmp,
			   buffer, sizeof(buffer), range.we_version_compiled);
      /* Display min/max timeout availables */
      print_pm_value_range("timeout", MESHW_POWER_TIMEOUT,
			   range.pmt_flags, range.min_pmt, range.max_pmt,
			   buffer, sizeof(buffer), range.we_version_compiled);
      /* Display min/max saving availables */
      print_pm_value_range("saving ", MESHW_POWER_SAVING,
			   range.pms_flags, range.min_pms, range.max_pms,
			   buffer, sizeof(buffer), range.we_version_compiled);

      /* Get current Power Management settings */
      wrq.u.power.flags = 0;
      if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_POWER_MGMT, &wrq) >= 0)
	{
	  int	flags = wrq.u.power.flags;

	  /* Is it disabled ? */
	  if(wrq.u.power.disabled)
	    printf("Current mode:off\n");
	  else
	    {
	      unsigned int	pm_type = 0;
	      unsigned int	pm_mask = 0;
	      unsigned int	remain_mask = range.pm_capa & MESHW_POWER_TYPE;
	      int		i = 0;

	      /* Let's check the mode */
	      meshwu_print_pm_mode(buffer, sizeof(buffer), flags);
	      printf("Current %s", buffer);

	      /* Let's check if nothing (simply on) */
	      if((flags & MESHW_POWER_MODE) == MESHW_POWER_ON)
		printf("mode:on");

	      /* Let's check the value and its type */
	      if(wrq.u.power.flags & MESHW_POWER_TYPE)
		{
		  meshwu_print_pm_value(buffer, sizeof(buffer),
				    wrq.u.power.value, wrq.u.power.flags,
				    range.we_version_compiled);
		  printf("\n                 %s", buffer);
		}

	      while(1)
		{
		  /* Deal with min/max for the current value */
		  pm_mask = 0;
		  /* If we have been returned a MIN value, ask for the MAX */
		  if(flags & MESHW_POWER_MIN)
		    pm_mask = MESHW_POWER_MAX;
		  /* If we have been returned a MAX value, ask for the MIN */
		  if(flags & MESHW_POWER_MAX)
		    pm_mask = MESHW_POWER_MIN;
		  /* If we have something to ask for... */
		  if(pm_mask)
		    {
		      pm_mask |= pm_type;
		      get_pm_value(devFd, ifname, &wrq, pm_mask,
				   buffer, sizeof(buffer),
				   range.we_version_compiled);
		    }

		  /* Remove current type from mask */
		  remain_mask &= ~(wrq.u.power.flags);

		  /* Check what other types we still have to read */
		  while(i < pm_type_flags_size)
		    {
		      pm_type = remain_mask & pm_type_flags[i];
		      if(pm_type)
			break;
		      i++;
		    }
		  /* Nothing anymore : exit the loop */
		  if(!pm_type)
		    break;

		  /* Ask for this other type of value */
		  flags = get_pm_value(devFd, ifname, &wrq, pm_type,
				       buffer, sizeof(buffer),
				       range.we_version_compiled);
		  /* Loop back for min/max */
		}
	      printf("\n");
	    }
	}
      printf("\n");
    }
  return(0);
}

#ifndef WE_ESSENTIAL
/************************** TRANSMIT POWER **************************/
/* Print the number of available transmit powers for the device */
static int print_txpower_info(int devFd,char *ifname,char *args[], int count)
{
  struct meshw_req		wrq;
  struct meshw_range	range;
  int			dbm;
  int			mwatt;
  int			k;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Extract range info */
  if((meshwu_get_range_info(devFd, ifname, &range) < 0) ||
     (range.we_version_compiled < 10))
      fprintf(stderr, "%-8.16s  no transmit-power information.\n\n",
		      ifname);
  else
    {
      if((range.num_txpower <= 0) || (range.num_txpower > MESHW_MAX_TXPOWER))
	printf("%-8.16s  unknown transmit-power information.\n\n", ifname);
      else
	{
	  printf("%-8.16s  %d available transmit-powers :\n",
		 ifname, range.num_txpower);
	  /* Print them all */
	  for(k = 0; k < range.num_txpower; k++)
	    {
	      /* Check for relative values */
	      if(range.txpower_capa & MESHW_TXPOW_RELATIVE)
		{
		  printf("\t  %d (no units)\n", range.txpower[k]);
		}
	      else
		{
		  if(range.txpower_capa & MESHW_TXPOW_MWATT)
		    {
		      dbm = meshwu_mwatt2dbm(range.txpower[k]);
		      mwatt = range.txpower[k];
		    }
		  else
		    {
		      dbm = range.txpower[k];
		      mwatt = meshwu_dbm2mwatt(range.txpower[k]);
		    }
		  printf("\t  %d dBm  \t(%d mW)\n", dbm, mwatt);
		}
	    }
	}

      /* Get current Transmit Power */
      if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_TX_POW, &wrq) >= 0)
	{
	  printf("          Current Tx-Power");
	  /* Disabled ? */
	  if(wrq.u.txpower.disabled)
	    printf(":off\n\n");
	  else
	    {
	      /* Fixed ? */
	      if(wrq.u.txpower.fixed)
		printf("=");
	      else
		printf(":");
	      /* Check for relative values */
	      if(wrq.u.txpower.flags & MESHW_TXPOW_RELATIVE)
		{
		  /* I just hate relative value, because they are
		   * driver specific, so not very meaningfull to apps.
		   * But, we have to support that, because
		   * this is the way hardware is... */
		  printf("\t  %d (no units)\n", wrq.u.txpower.value);
		}
	      else
		{
		  if(wrq.u.txpower.flags & MESHW_TXPOW_MWATT)
		    {
		      dbm = meshwu_mwatt2dbm(wrq.u.txpower.value);
		      mwatt = wrq.u.txpower.value;
		    }
		  else
		    {
		      dbm = wrq.u.txpower.value;
		      mwatt = meshwu_dbm2mwatt(wrq.u.txpower.value);
		    }
		  printf("%d dBm  \t(%d mW)\n\n", dbm, mwatt);
		}
	    }
	}
    }
  return(0);
}

/*********************** RETRY LIMIT/LIFETIME ***********************/
/* Print one retry value */
static int get_retry_value(int		devFd,
		char *		ifname,
		struct meshw_req *	pwrq,
		int		flags,
		char *		buffer,
		int		buflen,
		int		we_version_compiled)
{
  /* Get Another retry value */
  pwrq->u.retry.flags = flags;
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RETRY, pwrq) >= 0)
    {
      /* Let's check the value and its type */
      if(pwrq->u.retry.flags & MESHW_RETRY_TYPE)
	{
	  meshwu_print_retry_value(buffer, buflen,
			       pwrq->u.retry.value, pwrq->u.retry.flags,
			       we_version_compiled);
	  printf("%s\n                 ", buffer);
	}
    }
  return(pwrq->u.retry.flags);
}

/*
 * Print Power Management range for each type
 */
static void print_retry_value_range(char *		name,
			int		mask,
			int		iwr_flags,
			int		iwr_min,
			int		iwr_max,
			char *		buffer,
			int		buflen,
			int		we_version_compiled)
{
  if(iwr_flags & mask)
    {
      int	flags = (iwr_flags & ~(MESHW_RETRY_MIN | MESHW_RETRY_MAX));
      /* Display if auto or fixed */
      printf("%s %s ; ",
	     (iwr_flags & MESHW_POWER_MIN) ? "Auto " : "Fixed",
	     name);
      /* Print the range */
      meshwu_print_retry_value(buffer, buflen,
			   iwr_min, flags | MESHW_POWER_MIN,
			   we_version_compiled);
      printf("%s\n                           ", buffer);
      meshwu_print_retry_value(buffer, buflen,
			   iwr_max, flags | MESHW_POWER_MAX,
			   we_version_compiled);
      printf("%s\n          ", buffer);
    }
}

static int print_retry_info(int devFd,char *ifname,char *args[], int count)
{
  struct meshw_req		wrq;
  struct meshw_range	range;
  char			buffer[128];

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Extract range info */
  if((meshwu_get_range_info(devFd, ifname, &range) < 0) ||
     (range.we_version_compiled < 11))
    fprintf(stderr, "%-8.16s  no retry limit/lifetime information.\n\n",
	    ifname);
  else
    {
      printf("%-8.16s  ", ifname);

      /* Display min/max limit availables */
      print_retry_value_range("limit   ", MESHW_RETRY_LIMIT, range.retry_flags,
			      range.min_retry, range.max_retry,
			      buffer, sizeof(buffer),
			      range.we_version_compiled);
      /* Display min/max lifetime availables */
      print_retry_value_range("lifetime", MESHW_RETRY_LIFETIME, 
			      range.r_time_flags,
			      range.min_r_time, range.max_r_time,
			      buffer, sizeof(buffer),
			      range.we_version_compiled);

      /* Get current retry settings */
      wrq.u.retry.flags = 0;
      if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RETRY, &wrq) >= 0)
	{
	  int	flags = wrq.u.retry.flags;

	  /* Is it disabled ? */
	  if(wrq.u.retry.disabled)
	    printf("Current mode:off\n          ");
	  else
	    {
	      unsigned int	retry_type = 0;
	      unsigned int	retry_mask = 0;
	      unsigned int	remain_mask = range.retry_capa & MESHW_RETRY_TYPE;

	      /* Let's check the mode */
	      printf("Current mode:on\n                 ");

	      /* Let's check the value and its type */
	      if(wrq.u.retry.flags & MESHW_RETRY_TYPE)
		{
		  meshwu_print_retry_value(buffer, sizeof(buffer),
				       wrq.u.retry.value, wrq.u.retry.flags,
				       range.we_version_compiled);
		  printf("%s\n                 ", buffer);
		}

	      while(1)
		{
		  /* Deal with min/max/short/long for the current value */
		  retry_mask = 0;
		  /* If we have been returned a MIN value, ask for the MAX */
		  if(flags & MESHW_RETRY_MIN)
		    retry_mask = MESHW_RETRY_MAX;
		  /* If we have been returned a MAX value, ask for the MIN */
		  if(flags & MESHW_RETRY_MAX)
		    retry_mask = MESHW_RETRY_MIN;
		  /* Same for SHORT and LONG */
		  if(flags & MESHW_RETRY_SHORT)
		    retry_mask = MESHW_RETRY_LONG;
		  if(flags & MESHW_RETRY_LONG)
		    retry_mask = MESHW_RETRY_SHORT;
		  /* If we have something to ask for... */
		  if(retry_mask)
		    {
		      retry_mask |= retry_type;
		      get_retry_value(devFd, ifname, &wrq, retry_mask,
				      buffer, sizeof(buffer),
				      range.we_version_compiled);
		    }

		  /* And if we have both a limit and a lifetime,
		   * ask the other one */
		  remain_mask &= ~(wrq.u.retry.flags);
		  retry_type = remain_mask;
		  /* Nothing anymore : exit the loop */
		  if(!retry_type)
		    break;

		  /* Ask for this other type of value */
		  flags = get_retry_value(devFd, ifname, &wrq, retry_type,
					  buffer, sizeof(buffer),
					  range.we_version_compiled);
		  /* Loop back for min/max/short/long */
		}
	    }
	}
      printf("\n");
    }
  return(0);
}

/*
 * Note : now that we have scanning support, this is depracted and
 * won't survive long. Actually, next version it's out !
 */
/*
 * Display the list of ap addresses and the associated stats
 * Exacly the same as the spy list, only with different IOCTL and messages
 */
static int print_ap_info(int devFd,char *ifname,char *args[], int count)
{
  struct meshw_req		wrq;
  char		buffer[(sizeof(struct meshw_quality) +
			sizeof(struct sockaddr)) * MESHW_MAX_AP];
  char		temp[128];
  struct sockaddr *	hwa;
  struct meshw_quality *	qual;
  iwrange	range;
  int		has_range = 0;
  int		has_qual = 0;
  int		n;
  int		i;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Collect stats */
  wrq.u.data.pointer = (caddr_t) buffer;
  wrq.u.data.length = MESHW_MAX_AP;
  wrq.u.data.flags = 0;
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_AP_LIST, &wrq) < 0)
    {
      fprintf(stderr, "%-8.16s  Interface doesn't have a list of Peers/Access-Points\n\n", ifname);
      return(-1);
    }

  /* Number of addresses */
  n = wrq.u.data.length;
  has_qual = wrq.u.data.flags;

  /* The two lists */
  hwa = (struct sockaddr *) buffer;
  qual = (struct meshw_quality *) (buffer + (sizeof(struct sockaddr) * n));

  /* Check if we have valid mac address type */
  if(meshwu_check_mac_addr_type(devFd, ifname) < 0)
    {
      fprintf(stderr, "%-8.16s  Interface doesn't support MAC addresses\n\n", ifname);
      return(-2);
    }

  /* Get range info if we can */
  if(meshwu_get_range_info(devFd, ifname, &(range)) >= 0)
    has_range = 1;

  /* Display it */
  if(n == 0)
    printf("%-8.16s  No Peers/Access-Point in range\n", ifname);
  else
    printf("%-8.16s  Peers/Access-Points in range:\n", ifname);
  for(i = 0; i < n; i++)
    {
      if(has_qual)
	{
	  /* Print stats for this address */
	  printf("    %s : ", meshwu_saether_ntop(&hwa[i], temp));
	  meshwu_print_stats(temp, sizeof(buffer), &qual[i], &range, has_range);
	  printf("%s\n", temp);
	}
      else
	/* Only print the address */
	printf("    %s\n", meshwu_saether_ntop(&hwa[i], temp));
    }
  printf("\n");
  return(0);
}

#if __ARM_IXP__
#else
static const char *	event_capa_req[] =
{
  [MESHW_IO_W_NWID	- MESHW_IO_FIRST] = "Set NWID (kernel generated)",
  [MESHW_IO_W_FREQ	- MESHW_IO_FIRST] = "Set Frequency/Channel (kernel generated)",
  [MESHW_IO_R_FREQ	- MESHW_IO_FIRST] = "New Frequency/Channel",
  [MESHW_IO_W_MODE	- MESHW_IO_FIRST] = "Set Mode (kernel generated)",
  [MESHW_IO_R_SPY_THR - MESHW_IO_FIRST] = "Spy threshold crossed",
  [MESHW_IO_R_AP_MAC	- MESHW_IO_FIRST] = "New Access Point/Cell address - roaming",
  [MESHW_IO_R_SCAN	- MESHW_IO_FIRST] = "Scan request completed",
  [MESHW_IO_W_ESSID	- MESHW_IO_FIRST] = "Set ESSID (kernel generated)",
  [MESHW_IO_R_ESSID	- MESHW_IO_FIRST] = "New ESSID",
  [MESHW_IO_R_RATE	- MESHW_IO_FIRST] = "New bit-rate",
  [MESHW_IO_W_ENCODE - MESHW_IO_FIRST] = "Set Encoding (kernel generated)",
  [MESHW_IO_R_POWER_MGMT	- MESHW_IO_FIRST] = NULL,
};

static const char *	event_capa_evt[] =
{
  [MESHW_EV_TX_DROP	- MESHW_EV_FIRST] = "Tx packet dropped - retry exceeded",
  [MESHW_EV_CUSTOM	- MESHW_EV_FIRST] = "Custom driver event",
  [MESHW_EV_NODE_REGISTERED - MESHW_EV_FIRST] = "Registered node",
  [MESHW_EV_NODE_EXPIRED	- MESHW_EV_FIRST] = "Expired node",
};

/* Print the event capability for the device */
static int print_event_capa_info(int devFd,char *ifname,char *args[], int count)
{
  struct meshw_range	range;
  int			cmd;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Extract range info */
  if((meshwu_get_range_info(devFd, ifname, &range) < 0) ||
     (range.we_version_compiled < 10))
      fprintf(stderr, "%-8.16s  no wireless event capability information.\n\n",
		      ifname);
  else
    {
#if 0
      /* Debugging ;-) */
      for(cmd = 0x8B00; cmd < 0x8C0F; cmd++)
	{
	  int idx = IW_EVENT_CAPA_INDEX(cmd);
	  int mask = IW_EVENT_CAPA_MASK(cmd);
	  printf("0x%X - %d - %X\n", cmd, idx, mask);
	}
#endif

      printf("%-8.16s  Wireless Events supported :\n", ifname);

      for(cmd = MESHW_IO_FIRST; cmd <= MESHW_IO_R_POWER_MGMT; cmd++)
	{
	  int idx = IW_EVENT_CAPA_INDEX(cmd);
	  int mask = IW_EVENT_CAPA_MASK(cmd);
	  if(range.event_capa[idx] & mask)
	    printf("          0x%04X : %s\n",
		   cmd, event_capa_req[cmd - MESHW_IO_FIRST]);
	}
      for(cmd = MESHW_EV_FIRST; cmd <= MESHW_EV_NODE_EXPIRED; cmd++)
	{
	  int idx = IW_EVENT_CAPA_INDEX(cmd);
	  int mask = IW_EVENT_CAPA_MASK(cmd);
	  if(range.event_capa[idx] & mask)
	    printf("          0x%04X : %s\n",
		   cmd, event_capa_evt[cmd - MESHW_EV_FIRST]);
	}
      printf("\n");
    }
  return(0);
}

/* Print Modulation info for each device  */
static int print_modul_info(int devFd,char *ifname,char *args[], int count)
{
  struct meshw_req		wrq;
  struct meshw_range	range;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Extract range info */
  if((meshwu_get_range_info(devFd, ifname, &range) < 0) ||
     (range.we_version_compiled < 11))
    fprintf(stderr, "%-8.16s  no modulation information.\n\n",
	    ifname);
  else
    {
      if(range.modul_capa == 0x0)
	printf("%-8.16s  unknown modulation information.\n\n", ifname);
      else
	{
	  int i;
	  printf("%-8.16s  Modulations available :\n", ifname);

	  /* Display each modulation available */
	  for(i = 0; i < IW_SIZE_MODUL_LIST; i++)
	    {
	      if((range.modul_capa & meshwu_modul_list[i].mask)
		 == meshwu_modul_list[i].mask)
		printf("              %-8s: %s\n",
		       meshwu_modul_list[i].cmd, meshwu_modul_list[i].verbose);
	    }

	  /* Get current modulations settings */
	  wrq.u.param.flags = 0;
	  if(meshwu_get_ext(devFd, ifname, SIOCGIWMODUL, &wrq) >= 0)
	    {
	      unsigned int	modul = wrq.u.param.value;
	      int		n = 0;

	      printf("          Current modulations %c",
		     wrq.u.param.fixed ? '=' : ':');

	      /* Display each modulation enabled */
	      for(i = 0; i < IW_SIZE_MODUL_LIST; i++)
		{
		  if((modul & meshwu_modul_list[i].mask) == meshwu_modul_list[i].mask)
		    {
		      if((n++ % 8) == 0)
			printf("\n              ");
		      else
			printf(" ; ");
		      printf("%s", meshwu_modul_list[i].cmd);
		    }
		}

	      printf("\n");
	    }
	  printf("\n");
	}
    }
  return(0);
}
#endif /* ARM_IXP */

#endif	/* WE_ESSENTIAL */

typedef struct iwlist_entry
{
	const char *cmd;
	meshwu_enum_handler	fn;
	int			max_count;
	const char *		argsname;
} iwlist_cmd;

static const struct iwlist_entry iwlist_cmds[] =
{
	{ "scanning",		print_scanning_info,	-1, "[essid NNN] [last]" },
	{ "frequency",	print_freq_info,	0, NULL },
	{ "channel",		print_freq_info,	0, NULL },
	{ "bitrate",		print_bitrate_info,	0, NULL },
	{ "rate",		print_bitrate_info,	0, NULL },
	{ "encryption",	print_keys_info,	0, NULL },
	{ "key",		print_keys_info,	0, NULL },
	{ "power",		print_pm_info,		0, NULL },
#ifndef WE_ESSENTIAL
	{ "txpower",		print_txpower_info,	0, NULL },
	{ "retry",		print_retry_info,	0, NULL },
	{ "ap",		print_ap_info,		0, NULL },
	{ "accesspoints",	print_ap_info,		0, NULL },
	{ "peers",		print_ap_info,		0, NULL },
#if __ARM_IXP__
#else
	{ "event",		print_event_capa_info,	0, NULL },
	{ "modulation",	print_modul_info,	0, NULL },
#endif	
#endif	/* WE_ESSENTIAL */
	{ NULL, NULL, 0, 0 },
};

/* Find the most appropriate command matching the command line */
static inline const iwlist_cmd *find_command(const char *	cmd)
{
	const iwlist_cmd *	found = NULL;
	int			ambig = 0;
	unsigned int		len = strlen(cmd);
	int			i;

	for(i = 0; iwlist_cmds[i].cmd != NULL; ++i)
	{
		if(strncasecmp(iwlist_cmds[i].cmd, cmd, len) != 0)
			continue;

		if(len == strlen(iwlist_cmds[i].cmd))
			return &iwlist_cmds[i];

		/* Partial match */
		if(found == NULL)/* First time */
			found = &iwlist_cmds[i];
		else	 if (iwlist_cmds[i].fn != found->fn)/* Another time */
			ambig = 1;
	}

	if(found == NULL)
	{
		fprintf(stderr, "meshlist: unknown command `%s'\n", cmd);
		return NULL;
	}

	if(ambig)
	{
		fprintf(stderr, "iwlist: command `%s' is ambiguous\n", cmd);
		return NULL;
	}

	return found;
}

static void meshlist_usage(int status)
{
	FILE* f = status ? stderr : stdout;
	int i;

	fprintf(f,   "Usage: meshlist [interface] %s %s\n", iwlist_cmds[0].cmd,
		iwlist_cmds[0].argsname);
	for(i = 1; iwlist_cmds[i].cmd != NULL; ++i)
		fprintf(f, "              [interface] %s\n", iwlist_cmds[i].cmd);
	exit(status);
}

int main(int argc,char **argv)
{
	int devFd;
	char *dev;
	char *cmd;
	char **args;
	int count;
	const iwlist_cmd *iwcmd;

	if(argc < 2)
		meshlist_usage(1);

	if((argc == 2) && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
		meshlist_usage(0);
	if(argc == 2)
	{
		cmd = argv[1];
		dev = NULL;
		args = NULL;
		count = 0;
	}
	else
	{
		cmd = argv[2];
		dev = argv[1];
		args = argv + 3;
		count = argc - 3;
	}

	iwcmd = find_command(cmd);
	if(iwcmd == NULL)
		return 1;

	if((iwcmd->max_count >= 0) && (count > iwcmd->max_count))
	{
		fprintf(stderr, "meshlist: command `%s' needs fewer arguments (max %d)\n", iwcmd->cmd, iwcmd->max_count);
		return 1;
	}

	if((devFd = meshu_device_open(1)) < 0)
	{
		perror("MESH device file failed");
		return -1;
	}

	if (dev)
		(*iwcmd->fn)(devFd, dev, args, count);

	close(devFd);
	return 0;
}

