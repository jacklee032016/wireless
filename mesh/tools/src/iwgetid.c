/*
 *	Wireless Tools
 * Just print the ESSID or NWID...
 */

#include "iwlib.h"

#include <getopt.h>

/*
 * Note on Pcmcia Schemes :
 * ----------------------
 *	The purpose of this tool is to use the ESSID discovery mechanism
 * to select the appropriate Pcmcia Scheme. The card tell us which
 * ESSID it has found, and we can then select the appropriate Pcmcia
 * Scheme for this ESSID (Wireless config (encrypt keys) and IP config).
 *	The way to do it is as follows :
 *			cardctl scheme "essidany"
 *			delay 100
 *			$scheme = iwgetid --scheme
 *			cardctl scheme $scheme
 *	Of course, you need to add a scheme called "essidany" with the
 * following setting :
 *			essidany,*,*,*)
 *				ESSID="any"
 *				IPADDR="10.0.0.1"
 *
 *	This can also be integrated int he Pcmcia scripts.
 *	Some drivers don't activate the card up to "ifconfig up".
 * Therefore, they wont scan ESSID up to this point, so we can't
 * read it reliably in Pcmcia scripts.
 *	I guess the proper way to write the network script is as follows :
 *			if($scheme == "iwgetid") {
 *				iwconfig $name essid any
 *				iwconfig $name nwid any
 *				ifconfig $name up
 *				delay 100
 *				$scheme = iwgetid $name --scheme
 *				ifconfig $name down
 *			}
 *
 *	This is pseudo code, but you get an idea...
 *	The "ifconfig up" activate the card.
 *	The "delay" is necessary to let time for the card scan the
 * frequencies and associate with the AP.
 *	The "ifconfig down" is necessary to allow the driver to optimise
 * the wireless parameters setting (minimise number of card resets).
 *
 *	Another cute idea is to have a list of Pcmcia Schemes to try
 * and to keep the first one that associate (AP address != 0). This
 * would be necessary for closed networks and cards that can't
 * discover essid...
 *
 */

/**************************** CONSTANTS ****************************/

#define FORMAT_DEFAULT	0	/* Nice looking display for the user */
#define FORMAT_SCHEME	1	/* To be used as a Pcmcia Scheme */
#define FORMAT_RAW	2	/* Raw value, for shell scripts */
#define WTYPE_ESSID	0	/* Display ESSID or NWID */
#define WTYPE_AP	1	/* Display AP/Cell Address */
#define WTYPE_FREQ	2	/* Display frequency/channel */
#define WTYPE_CHANNEL	3	/* Display channel (converted from freq) */
#define WTYPE_MODE	4	/* Display mode */
#define WTYPE_PROTO	5	/* Display protocol name */

/************************ DISPLAY ESSID/NWID ************************/

/*------------------------------------------------------------------*/
/*
 * Display the ESSID if possible
 */
static int
print_essid(int			devFd,
	    const char *	ifname,
	    int			format)
{
  struct meshw_req		wrq;
  char			essid[MESHW_ESSID_MAX_SIZE + 1];	/* ESSID */
  char			pessid[MESHW_ESSID_MAX_SIZE + 1];	/* Pcmcia format */
  unsigned int		i;
  unsigned int		j;

  /* Make sure ESSID is always NULL terminated */
  memset(essid, 0, sizeof(essid));

  /* Get ESSID */
  wrq.u.essid.pointer = (caddr_t) essid;
  wrq.u.essid.length = MESHW_ESSID_MAX_SIZE + 1;
  wrq.u.essid.flags = 0;
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_ESSID, &wrq) < 0)
    return(-1);

  switch(format)
    {
    case FORMAT_SCHEME:
      /* Strip all white space and stuff */
      j = 0;
      for(i = 0; i < strlen(essid); i++)
	if(isalnum(essid[i]))
	  pessid[j++] = essid[i];
      pessid[j] = '\0';
      if((j == 0) || (j > 32))
	return(-2);
      printf("%s\n", pessid);
      break;
    case FORMAT_RAW:
      printf("%s\n", essid);
      break;
    default:
      printf("%-8.16s  ESSID:\"%s\"\n", ifname, essid);
      break;
    }

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Display the NWID if possible
 */
static int
print_nwid(int		devFd,
	   const char *	ifname,
	   int		format)
{
  struct meshw_req		wrq;

  /* Get network ID */
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_NWID, &wrq) < 0)
    return(-1);

  switch(format)
    {
    case FORMAT_SCHEME:
      /* Prefix with nwid to avoid name space collisions */
      printf("nwid%X\n", wrq.u.nwid.value);
      break;
    case FORMAT_RAW:
      printf("%X\n", wrq.u.nwid.value);
      break;
    default:
      printf("%-8.16s  NWID:%X\n", ifname, wrq.u.nwid.value);
      break;
    }

  return(0);
}

/**************************** AP ADDRESS ****************************/

/*------------------------------------------------------------------*/
/*
 * Display the AP Address if possible
 */
static int
print_ap(int		devFd,
	 const char *	ifname,
	 int		format)
{
  struct meshw_req		wrq;
  char			buffer[64];

  /* Get AP Address */
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_AP_MAC, &wrq) < 0)
    return(-1);

  /* Print */
  meshu_ether_ntop((const struct ether_addr *) wrq.u.ap_addr.sa_data, buffer);
  switch(format)
    {
    case FORMAT_SCHEME:
      /* I think ':' are not problematic, because Pcmcia scripts
       * seem to handle them properly... */
    case FORMAT_RAW:
      printf("%s\n", buffer);
      break;
    default:
      printf("%-8.16s  Access Point/Cell: %s\n", ifname, buffer);
      break;
    }

  return(0);
}

/****************************** OTHER ******************************/

/*------------------------------------------------------------------*/
/*
 * Display the frequency (or channel) if possible
 */
static int
print_freq(int		devFd,
	   const char *	ifname,
	   int		format)
{
  struct meshw_req		wrq;
  double		freq;
  char			buffer[64];

  /* Get frequency / channel */
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_FREQ, &wrq) < 0)
    return(-1);

  /* Print */
  freq = meshwu_freq2float(&(wrq.u.freq));
  switch(format)
    {
    case FORMAT_SCHEME:
      /* Prefix with freq to avoid name space collisions */
      printf("freq%g\n", freq);
      break;
    case FORMAT_RAW:
      printf("%g\n", freq);
      break;
    default:
      meshwu_print_freq(buffer, sizeof(buffer), freq, -1, wrq.u.freq.flags);
      printf("%-8.16s  %s\n", ifname, buffer);
      break;
    }

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Display the channel (converted from frequency) if possible
 */
static int
print_channel(int		devFd,
	      const char *	ifname,
	      int		format)
{
  struct meshw_req		wrq;
  struct meshw_range	range;
  double		freq;
  int			channel;

  /* Get frequency / channel */
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_FREQ, &wrq) < 0)
    return(-1);

  /* Convert to channel */
  if(meshwu_get_range_info(devFd, ifname, &range) < 0)
    return(-2);
  freq = meshwu_freq2float(&(wrq.u.freq));
  if(freq < KILO)
    channel = (int) freq;
  else
    {
      channel = meshwu_freq_to_channel(freq, &range);
      if(channel < 0)
	return(-3);
    }

  /* Print */
  switch(format)
    {
    case FORMAT_SCHEME:
      /* Prefix with freq to avoid name space collisions */
      printf("channel%d\n", channel);
      break;
    case FORMAT_RAW:
      printf("%d\n", channel);
      break;
    default:
      printf("%-8.16s  Channel:%d\n", ifname, channel);
      break;
    }

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Display the mode if possible
 */
static int
print_mode(int		devFd,
	   const char *	ifname,
	   int		format)
{
  struct meshw_req		wrq;

  /* Get frequency / channel */
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_MODE, &wrq) < 0)
    return(-1);
  if(wrq.u.mode >= MESHWU_NUM_OPER_MODE)
    return(-2);

  /* Print */
  switch(format)
    {
    case FORMAT_SCHEME:
      /* Strip all white space and stuff */
      if(wrq.u.mode == MESHW_MODE_ADHOC)
	printf("AdHoc\n");
      else
	printf("%s\n", meshwu_operation_mode[wrq.u.mode]);
      break;
    case FORMAT_RAW:
      printf("%d\n", wrq.u.mode);
      break;
    default:
      printf("%-8.16s  Mode:%s\n", ifname, meshwu_operation_mode[wrq.u.mode]);
      break;
    }

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Display the ESSID if possible
 */
static int
print_protocol(int		devFd,
	       const char *	ifname,
	       int		format)
{
  struct meshw_req		wrq;
  char			proto[MESHNAMSIZ + 1];	/* Protocol */
  char			pproto[MESHNAMSIZ + 1];	/* Pcmcia format */
  unsigned int		i;
  unsigned int		j;

  /* Get Protocol name */
  if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_NAME, &wrq) < 0)
    return(-1);
  strncpy(proto, wrq.u.name, MESHNAMSIZ);
  proto[MESHNAMSIZ] = '\0';

  switch(format)
    {
    case FORMAT_SCHEME:
      /* Strip all white space and stuff */
      j = 0;
      for(i = 0; i < strlen(proto); i++)
	if(isalnum(proto[i]))
	  pproto[j++] = proto[i];
      pproto[j] = '\0';
      if((j == 0) || (j > 32))
	return(-2);
      printf("%s\n", pproto);
      break;
    case FORMAT_RAW:
      printf("%s\n", proto);
      break;
    default:
      printf("%-8.16s  Protocol Name:\"%s\"\n", ifname, proto);
      break;
    }

  return(0);
}

/******************************* MAIN ********************************/

/*------------------------------------------------------------------*/
/*
 * Check options and call the proper handler
 */
static int
print_one_device(int		devFd,
		 int		format,
		 int		wtype,
		 const char*	ifname)
{
  int ret;

  /* Check wtype */
  switch(wtype)
    {
    case WTYPE_AP:
      /* Try to print an AP */
      ret = print_ap(devFd, ifname, format);
      break;

    case WTYPE_CHANNEL:
      /* Try to print channel */
      ret = print_channel(devFd, ifname, format);
      break;

    case WTYPE_FREQ:
      /* Try to print frequency */
      ret = print_freq(devFd, ifname, format);
      break;

    case WTYPE_MODE:
      /* Try to print the mode */
      ret = print_mode(devFd, ifname, format);
      break;

    case WTYPE_PROTO:
      /* Try to print the protocol */
      ret = print_protocol(devFd, ifname, format);
      break;

    default:
      /* Try to print an ESSID */
      ret = print_essid(devFd, ifname, format);
      if(ret < 0)
	{
	  /* Try to print a nwid */
	  ret = print_nwid(devFd, ifname, format);
	}
    }

  return(ret);
}

/*------------------------------------------------------------------*/
/*
 * Try the various devices until one return something we can use
 *
 * Note : we can't use iw_enum_devices() because we want a different
 * behaviour :
 *	1) Stop at the first valid wireless device
 *	2) Only go through active devices
 */
static int
scan_devices(int		devFd,
	     int		format,
	     int		wtype)
{
  char		buff[1024];
  struct ifconf ifc;
  struct ifreq *ifr;
  int		i;

  /* Get list of active devices */
  ifc.ifc_len = sizeof(buff);
  ifc.ifc_buf = buff;
  if(ioctl(devFd, SIOCGIFCONF, &ifc) < 0)
    {
      perror("SIOCGIFCONF");
      return(-1);
    }
  ifr = ifc.ifc_req;

  /* Print the first match */
  for(i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++)
    {
      if(print_one_device(devFd, format, wtype, ifr->ifr_name) >= 0)
	return 0;
    }
  return(-1);
}

/*------------------------------------------------------------------*/
/*
 * helper
 */
static void
iw_usage(int status)
{
  fputs("Usage iwgetid [OPTIONS] [ifname]\n"
	"  Options are:\n"
	"    -a,--ap       Print the access point address\n"
	"    -c,--channel  Print the current channel\n"
	"    -f,--freq     Print the current frequency\n"
	"    -m,--mode     Print the current mode\n"
	"    -p,--protocol Print the protocol name\n"
	"    -r,--raw      Format the output as raw value for shell scripts\n"
	"    -s,--scheme   Format the output as a PCMCIA scheme identifier\n"
	"    -h,--help     Print this message\n",
	status ? stderr : stdout);
  exit(status);
}

static const struct option long_opts[] = {
  { "ap", no_argument, NULL, 'a' },
  { "channel", no_argument, NULL, 'c' },
  { "freq", no_argument, NULL, 'f' },
  { "mode", no_argument, NULL, 'm' },
  { "protocol", no_argument, NULL, 'p' },
  { "help", no_argument, NULL, 'h' },
  { "raw", no_argument, NULL, 'r' },
  { "scheme", no_argument, NULL, 's' },
  { NULL, 0, NULL, 0 }
};

/*------------------------------------------------------------------*/
/*
 * The main !
 */
int
main(int	argc,
     char **	argv)
{
  int	devFd;			/* generic raw socket desc.	*/
  int	format = FORMAT_DEFAULT;
  int	wtype = WTYPE_ESSID;
  int	opt;
  int	ret = -1;

  /* Check command line arguments */
  while((opt = getopt_long(argc, argv, "acfhmprs", long_opts, NULL)) > 0)
    {
      switch(opt)
	{
	case 'a':
	  /* User wants AP/Cell Address */
	  wtype = WTYPE_AP;
	  break;

	case 'c':
	  /* User wants channel only */
	  wtype = WTYPE_CHANNEL;
	  break;

	case 'f':
	  /* User wants frequency/channel */
	  wtype = WTYPE_FREQ;
	  break;

	case 'm':
	  /* User wants the mode */
	  wtype = WTYPE_MODE;
	  break;

	case 'p':
	  /* User wants the protocol */
	  wtype = WTYPE_PROTO;
	  break;

	case 'h':
	  iw_usage(0);
	  break;

	case 'r':
	  /* User wants a Raw format */
	  format = FORMAT_RAW;
	  break;

	case 's':
	  /* User wants a Scheme format */
	  format = FORMAT_SCHEME;
	  break;

	default:
	  iw_usage(1);
	  break;
	}
    }
  if(optind + 1 < argc) {
    fputs("Too many arguments.\n", stderr);
    iw_usage(1);
  }

  /* Create a channel to the NET kernel. */
  if((devFd = meshu_device_open()) < 0)
    {
      perror("socket");
      return(-1);
    }

  /* Check if first argument is a device name */
  if(optind < argc)
    {
      /* Yes : query only this device */
      ret = print_one_device(devFd, format, wtype, argv[optind]);
    }
  else
    {
      /* No : query all devices and print first found */
      ret = scan_devices(devFd, format, wtype);
    }

  fflush(stdout);
  iw_sockets_close(devFd);
  return(ret);
}
