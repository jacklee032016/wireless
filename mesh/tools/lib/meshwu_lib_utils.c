/*
 * MESH Wireless Tools
 * $Id: meshwu_lib_utils.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "meshwu_lib.h"

/*
 * Fun stuff with protocol identifiers (MESHW_IO_R_NAME).
 * We assume that drivers are returning sensible values in there,
 * which is not always the case :-(
 */

/*------------------------------------------------------------------*/
/*
 * Compare protocol identifiers.
 * We don't want to know if the two protocols are the exactly same,
 * but if they interoperate at some level, and also if they accept the
 * same type of config (ESSID vs NWID, freq...).
 * This is supposed to work around the alphabet soup.
 * Return 1 if protocols are compatible, 0 otherwise
 */
int iw_protocol_compare(const char *	protocol1,
		    const char *	protocol2)
{
  const char *	dot11 = "IEEE 802.11";
  const char *	dot11_ds = "Dbg";
  const char *	dot11_5g = "a";

  /* If the strings are the same -> easy */
  if(!strncmp(protocol1, protocol2, MESHNAMSIZ))
    return(1);

  /* Are we dealing with one of the 802.11 variant ? */
  if( (!strncmp(protocol1, dot11, strlen(dot11))) &&
      (!strncmp(protocol2, dot11, strlen(dot11))) )
    {
      const char *	sub1 = protocol1 + strlen(dot11);
      const char *	sub2 = protocol2 + strlen(dot11);
      unsigned int	i;
      int		isds1 = 0;
      int		isds2 = 0;
      int		is5g1 = 0;
      int		is5g2 = 0;

      /* Check if we find the magic letters telling it's DS compatible */
      for(i = 0; i < strlen(dot11_ds); i++)
	{
	  if(strchr(sub1, dot11_ds[i]) != NULL)
	    isds1 = 1;
	  if(strchr(sub2, dot11_ds[i]) != NULL)
	    isds2 = 1;
	}
      if(isds1 && isds2)
	return(1);

      /* Check if we find the magic letters telling it's 5GHz compatible */
      for(i = 0; i < strlen(dot11_5g); i++)
	{
	  if(strchr(sub1, dot11_5g[i]) != NULL)
	    is5g1 = 1;
	  if(strchr(sub2, dot11_5g[i]) != NULL)
	    is5g2 = 1;
	}
      if(is5g1 && is5g2)
	return(1);
    }
  /* Not compatible */
  return(0);
}




/*********************** ADDRESS SUBROUTINES ************************/
/*
 * This section is mostly a cut & past from net-tools-1.2.0
 * (Well... This has evolved over the years)
 * manage address display and input...
 */

#if 0
/*------------------------------------------------------------------*/
/*
 * Check if interface support the right address types...
 */
int
iw_check_addr_type(int		devFd,
		   char *	ifname)
{
  /* Check the interface address type */
  if(meshwu_check_if_addr_type(devFd, ifname) < 0)
    return(-1);

  /* Check the interface address type */
  if(meshwu_check_mac_addr_type(devFd, ifname) < 0)
    return(-1);

  return(0);
}
#endif

#if 0
/*------------------------------------------------------------------*/
/*
 * Ask the kernel for the MAC address of an interface.
 */
int
iw_get_mac_addr(int			devFd,
		const char *		ifname,
		struct ether_addr *	eth,
		unsigned short *	ptype)
{
  struct ifreq	ifr;
  int		ret;

  /* Prepare request */
  bzero(&ifr, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, ifname, MESHNAMSIZ);

  /* Do it */
  ret = ioctl(devFd, SIOCGIFHWADDR, &ifr);

  memcpy(eth->ether_addr_octet, ifr.ifr_hwaddr.sa_data, 6); 
  *ptype = ifr.ifr_hwaddr.sa_family;
  return(ret);
}
#endif

/*------------------------------------------------------------------*/

/************************* MISC SUBROUTINES **************************/




/************************ EVENT SUBROUTINES ************************/
/*
 * The Wireless Extension API 14 and greater define Wireless Events,
 * that are used for various events and scanning.
 * Those functions help the decoding of events, so are needed only in
 * this case.
 */

/* -------------------------- CONSTANTS -------------------------- */

/* Type of headers we know about (basically union iwreq_data) */
#define IW_HEADER_TYPE_NULL	0	/* Not available */
#define IW_HEADER_TYPE_CHAR	2	/* char [MESHNAMSIZ] */
#define IW_HEADER_TYPE_UINT	4	/* __u32 */
#define IW_HEADER_TYPE_FREQ	5	/* struct meshw_freq */
#define IW_HEADER_TYPE_ADDR	6	/* struct sockaddr */
#define IW_HEADER_TYPE_POINT	8	/* struct iw_point */
#define IW_HEADER_TYPE_PARAM	9	/* struct meshw_param */
#define IW_HEADER_TYPE_QUAL	10	/* struct meshw_quality */

/* Handling flags */
/* Most are not implemented. I just use them as a reminder of some
 * cool features we might need one day ;-) */
#define IW_DESCR_FLAG_NONE	0x0000	/* Obvious */
/* Wrapper level flags */
#define IW_DESCR_FLAG_DUMP	0x0001	/* Not part of the dump command */
#define IW_DESCR_FLAG_EVENT	0x0002	/* Generate an event on SET */
#define IW_DESCR_FLAG_RESTRICT	0x0004	/* GET : request is ROOT only */
				/* SET : Omit payload from generated iwevent */
#define IW_DESCR_FLAG_NOMAX	0x0008	/* GET : no limit on request size */
/* Driver level flags */
#define IW_DESCR_FLAG_WAIT	0x0100	/* Wait for driver event */

/* ---------------------------- TYPES ---------------------------- */

/*
 * Describe how a standard IOCTL looks like.
 */
struct iw_ioctl_description
{
	__u8	header_type;		/* NULL, iw_point or other */
	__u8	token_type;		/* Future */
	__u16	token_size;		/* Granularity of payload */
	__u16	min_tokens;		/* Min acceptable token number */
	__u16	max_tokens;		/* Max acceptable token number */
	__u32	flags;			/* Special handling of the request */
};


/* Meta-data about all the standard Wireless Extension request we know about. */
static const struct iw_ioctl_description standard_ioctl_descr[] =
{
	[MESHW_IO_W_COMMIT	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_NULL,
	},

	[MESHW_IO_R_NAME	- MESHW_IO_FIRST] = 
	{
		.header_type	= IW_HEADER_TYPE_CHAR,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[MESHW_IO_W_NWID	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[MESHW_IO_R_NWID	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[MESHW_IO_W_FREQ	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_FREQ,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[MESHW_IO_R_FREQ	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_FREQ,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[MESHW_IO_W_MODE	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_UINT,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[MESHW_IO_R_MODE	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_UINT,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[MESHW_IO_W_SENS	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_R_SENS	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_W_RANGE	- MESHW_IO_FIRST] = 
	{
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[MESHW_IO_R_RANGE	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= sizeof(struct meshw_range),
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[MESHW_IO_W_PRIV	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[MESHW_IO_R_PRIV	- MESHW_IO_FIRST] =
	{ /* (handled directly by us) */
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[MESHW_IO_W_STATS	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[MESHW_IO_R_STATS	- MESHW_IO_FIRST] =
	{ /* (handled directly by us) */
		.header_type	= IW_HEADER_TYPE_NULL,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[MESHW_IO_W_SPY	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct sockaddr),
		.max_tokens	= MESHW_MAX_SPY,
	},
	[MESHW_IO_R_SPY	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct sockaddr) +
				  sizeof(struct meshw_quality),
		.max_tokens	= MESHW_MAX_SPY,
	},
	[MESHW_IO_W_SPY_THR	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct meshw_thrspy),
		.min_tokens	= 1,
		.max_tokens	= 1,
	},
	[MESHW_IO_R_SPY_THR	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct meshw_thrspy),
		.min_tokens	= 1,
		.max_tokens	= 1,
	},
	[MESHW_IO_W_AP_MAC	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_ADDR,
	},
	[MESHW_IO_R_AP_MAC	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_ADDR,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
#if __ARM_IXP__
#else
	[MESHW_IO_W_MLME	- MESHW_IO_FIRST] = 
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_mlme),
		.max_tokens	= sizeof(struct iw_mlme),
	},
#endif	
	[MESHW_IO_R_AP_LIST	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct sockaddr) + sizeof(struct meshw_quality),
		.max_tokens	= MESHW_MAX_AP,
		.flags		= IW_DESCR_FLAG_NOMAX,
	},
#if __ARM_IXP__
#else
	[MESHW_IO_W_SCAN	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= 0,
		.max_tokens	= sizeof(struct iw_scan_req),
	},
#endif
	[MESHW_IO_R_SCAN	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= MESHW_SCAN_MAX_DATA,
		.flags		= IW_DESCR_FLAG_NOMAX,
	},
	[MESHW_IO_W_ESSID	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= MESHW_ESSID_MAX_SIZE + 1,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[MESHW_IO_R_ESSID	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= MESHW_ESSID_MAX_SIZE + 1,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[MESHW_IO_W_NICKNAME	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= MESHW_ESSID_MAX_SIZE + 1,
	},
	[MESHW_IO_R_NICKNAME	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= MESHW_ESSID_MAX_SIZE + 1,
	},
	[MESHW_IO_W_RATE	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_R_RATE	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_W_RTS	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_R_RTS	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_W_FRAG_THR	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_R_FRAG_THR	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_W_TX_POW	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_R_TX_POW	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_W_RETRY	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_R_RETRY	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_W_ENCODE	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= MESHW_ENCODING_TOKEN_MAX,
		.flags		= IW_DESCR_FLAG_EVENT | IW_DESCR_FLAG_RESTRICT,
	},
	[MESHW_IO_R_ENCODE	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= MESHW_ENCODING_TOKEN_MAX,
		.flags		= IW_DESCR_FLAG_DUMP | IW_DESCR_FLAG_RESTRICT,
	},
	[MESHW_IO_W_POWER_MGMT	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[MESHW_IO_R_POWER_MGMT	- MESHW_IO_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_PARAM,
	},

#if 0	
	[SIOCSIWMODUL	- MESHW_IO_FIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWMODUL	- MESHW_IO_FIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWGENIE	- MESHW_IO_FIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[SIOCGIWGENIE	- MESHW_IO_FIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[SIOCSIWAUTH	- MESHW_IO_FIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWAUTH	- MESHW_IO_FIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWENCODEEXT - MESHW_IO_FIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_encode_ext),
		.max_tokens	= sizeof(struct iw_encode_ext) +
				  MESHW_ENCODING_TOKEN_MAX,
	},
	[SIOCGIWENCODEEXT - MESHW_IO_FIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_encode_ext),
		.max_tokens	= sizeof(struct iw_encode_ext) +
				  MESHW_ENCODING_TOKEN_MAX,
	},
	[SIOCSIWPMKSA - MESHW_IO_FIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_pmksa),
		.max_tokens	= sizeof(struct iw_pmksa),
	},
#endif	
};
static const unsigned int standard_ioctl_num = (sizeof(standard_ioctl_descr) /
						sizeof(struct iw_ioctl_description));

/*
 * Meta-data about all the additional standard Wireless Extension events
 * we know about.
 */
static const struct iw_ioctl_description standard_event_descr[] =
{
	[MESHW_EV_TX_DROP	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_ADDR,
	},
	[MESHW_EV_QUAL	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_QUAL,
	},
	[MESHW_EV_CUSTOM	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= MESHW_CUSTOM_MAX,
	},
	[MESHW_EV_NODE_REGISTERED	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_ADDR,
	},
	[MESHW_EV_NODE_EXPIRED	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_ADDR, 
	},
#if 0	
	[IWEVGENIE	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[IWEVMICHAELMICFAILURE	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT, 
		.token_size	= 1,
		.max_tokens	= sizeof(struct iw_michaelmicfailure),
	},
	[IWEVASSOCREQIE	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[IWEVASSOCRESPIE	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[IWEVPMKIDCAND	- MESHW_EV_FIRST] =
	{
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= sizeof(struct iw_pmkid_cand),
	},
#endif	
};
static const unsigned int standard_event_num = (sizeof(standard_event_descr) /
						sizeof(struct iw_ioctl_description));

/* Size (in bytes) of various events */
static const int event_type_size[] = {
	MESHW_EV_LCP_LEN,		/* IW_HEADER_TYPE_NULL */
	0,
	MESHW_EV_CHAR_LEN,		/* IW_HEADER_TYPE_CHAR */
	0,
	MESHW_EV_UINT_LEN,		/* IW_HEADER_TYPE_UINT */
	MESHW_EV_FREQ_LEN,		/* IW_HEADER_TYPE_FREQ */
	MESHW_EV_ADDR_LEN,		/* IW_HEADER_TYPE_ADDR */
	0,
	MESHW_EV_POINT_LEN,	/* Without variable payload */
	MESHW_EV_PARAM_LEN,	/* IW_HEADER_TYPE_PARAM */
	MESHW_EV_QUAL_LEN,		/* IW_HEADER_TYPE_QUAL */
};

/*
 * Initialise the struct meshwu_stream_descr so that we can extract
 * individual events from the event stream.
 */
void meshwu_init_event_stream(struct meshwu_stream_descr *stream, char *data,int len)
{
	/* Cleanup */
	memset((char *) stream, '\0', sizeof(struct meshwu_stream_descr));

	/* Set things up */
	stream->current = data;
	stream->end = data + len;
}

/* Extract the next event from the event stream. */
int meshwu_extract_event_stream(struct meshwu_stream_descr *	stream,	/* Stream of events */
			struct meshw_event *	iwe,	/* Extracted event */
			int we_version)
{
	const struct iw_ioctl_description *descr = NULL;
	int		event_type = 0;
	unsigned int	event_len = 1;		/* Invalid */
	char *	pointer;
	/* Don't "optimise" the following variable, it will crash */
	unsigned	cmd_index;		/* *MUST* be unsigned */

	/* Unused for now. Will be later on... */
	we_version = we_version;

	/* Check for end of stream */
	if((stream->current + MESHW_EV_LCP_LEN) > stream->end)
		return(0);

#if DEBUG
  printf("DBG - stream->current = %p, stream->value = %p, stream->end = %p\n",
	 stream->current, stream->value, stream->end);
#endif

  /* Extract the event header (to get the event id).
   * Note : the event may be unaligned, therefore copy... */
  memcpy((char *) iwe, stream->current, MESHW_EV_LCP_LEN);

#if DEBUG
  printf("DBG - iwe->cmd = 0x%X, iwe->len = %d\n",
	 iwe->cmd, iwe->len);
#endif

  /* Check invalid events */
  if(iwe->len <= MESHW_EV_LCP_LEN)
    return(-1);

  /* Get the type and length of that event */
  if(iwe->cmd <= MESHW_IO_LAST)
    {
      cmd_index = iwe->cmd - MESHW_IO_FIRST;
      if(cmd_index < standard_ioctl_num)
	descr = &(standard_ioctl_descr[cmd_index]);
    }
  else
    {
      cmd_index = iwe->cmd - MESHW_EV_FIRST;
      if(cmd_index < standard_event_num)
	descr = &(standard_event_descr[cmd_index]);
    }
  if(descr != NULL)
    event_type = descr->header_type;
  /* Unknown events -> event_type=0 => MESHW_EV_LCP_LEN */
  event_len = event_type_size[event_type];
#if __ARM_IXP__
#else
	/* Fixup for earlier version of WE */
	if((we_version <= 18) && (event_type == IW_HEADER_TYPE_POINT))
		event_len += MESHW_EV_POINT_OFF;
#endif

  /* Check if we know about this event */
  if(event_len <= MESHW_EV_LCP_LEN)
    {
      /* Skip to next event */
      stream->current += iwe->len;
      return(2);
    }
  event_len -= MESHW_EV_LCP_LEN;

  /* Set pointer on data */
  if(stream->value != NULL)
    pointer = stream->value;			/* Next value in event */
  else
    pointer = stream->current + MESHW_EV_LCP_LEN;	/* First value in event */

#if DEBUG
  printf("DBG - event_type = %d, event_len = %d, pointer = %p\n",
	 event_type, event_len, pointer);
#endif

  /* Copy the rest of the event (at least, fixed part) */
  if((pointer + event_len) > stream->end)
    {
      /* Go to next event */
      stream->current += iwe->len;
      return(-2);
    }
	/* Fixup for WE-19 and later : pointer no longer in the stream */
#if __ARM_IXP__
#else
	if((we_version > 18) && (event_type == IW_HEADER_TYPE_POINT))
		memcpy((char *) iwe + MESHW_EV_LCP_LEN + MESHW_EV_POINT_OFF, pointer, event_len);
	else
#endif		
		memcpy((char *) iwe + MESHW_EV_LCP_LEN, pointer, event_len);

  /* Skip event in the stream */
  pointer += event_len;

  /* Special processing for iw_point events */
  if(event_type == IW_HEADER_TYPE_POINT)
    {
      /* Check the length of the payload */
      unsigned int	extra_len = iwe->len - (event_len + MESHW_EV_LCP_LEN);
      if(extra_len > 0)
	{
	  /* Set pointer on variable part (warning : non aligned) */
	  iwe->u.data.pointer = pointer;

	  /* Check that we have a descriptor for the command */
	  if(descr == NULL)
	    /* Can't check payload -> unsafe... */
	    iwe->u.data.pointer = NULL;	/* Discard paylod */
	  else
	    {
	      /* Those checks are actually pretty hard to trigger,
	       * because of the checks done in the kernel... */

	      /* Discard bogus events which advertise more tokens than
	       * what they carry... */
	      unsigned int	token_len = iwe->u.data.length * descr->token_size;
	      if(token_len > extra_len)
		iwe->u.data.pointer = NULL;	/* Discard paylod */
	      /* Check that the advertised token size is not going to
	       * produce buffer overflow to our caller... */
	      if((iwe->u.data.length > descr->max_tokens)
		 && !(descr->flags & IW_DESCR_FLAG_NOMAX))
		iwe->u.data.pointer = NULL;	/* Discard paylod */
	      /* Same for underflows... */
	      if(iwe->u.data.length < descr->min_tokens)
		iwe->u.data.pointer = NULL;	/* Discard paylod */
#if DEBUG
	      printf("DBG - extra_len = %d, token_len = %d, token = %d, max = %d, min = %d\n",
		     extra_len, token_len, iwe->u.data.length, descr->max_tokens, descr->min_tokens);
#endif
	    }
	}
      else
	/* No data */
	iwe->u.data.pointer = NULL;

      /* Go to next event */
      stream->current += iwe->len;
    }
  else
    {
      /* Is there more value in the event ? */
      if((pointer + event_len) <= (stream->current + iwe->len))
	/* Go to next value */
	stream->value = pointer;
      else
	{
	  /* Go to next event */
	  stream->value = NULL;
	  stream->current += iwe->len;
	}
    }
  return(1);
}

/*********************** SCANNING SUBROUTINES ***********************/
/*
 * The Wireless Extension API 14 and greater define Wireless Scanning.
 * The normal API is complex, this is an easy API that return
 * a subset of the scanning results. This should be enough for most
 * applications that want to use Scanning.
 * If you want to have use the full/normal API, check iwlist.c...
 *
 * Precaution when using scanning :
 * The scanning operation disable normal network traffic, and therefore
 * you should not abuse of scan.
 * The scan need to check the presence of network on other frequencies.
 * While you are checking those other frequencies, you can *NOT* be on
 * your normal frequency to listen to normal traffic in the cell.
 * You need typically in the order of one second to actively probe all
 * 802.11b channels (do the maths). Some cards may do that in background,
 * to reply to scan commands faster, but they still have to do it.
 * Leaving the cell for such an extended period of time is pretty bad.
 * Any kind of streaming/low latency traffic will be impacted, and the
 * user will perceive it (easily checked with telnet). People trying to
 * send traffic to you will retry packets and waste bandwidth. Some
 * applications may be sensitive to those packet losses in weird ways,
 * and tracing those weird behavior back to scanning may take time.
 * If you are in ad-hoc mode, if two nodes scan approx at the same
 * time, they won't see each other, which may create associations issues.
 * For those reasons, the scanning activity should be limited to
 * what's really needed, and continuous scanning is a bad idea.
 * Jean II
 */

/*
 * Process/store one element from the scanning results in wireless_scan
 */
static inline meshwu_scan *iw_process_scanning_token(struct meshw_event *event, meshwu_scan *wscan)
{
	meshwu_scan *oldwscan;

	switch(event->cmd)
	{
		case MESHW_IO_R_AP_MAC:
			/* New cell description. Allocate new cell descriptor, zero it. */
			oldwscan = wscan;
			wscan = (meshwu_scan *) malloc(sizeof(meshwu_scan));
			if(wscan == NULL)
				return(wscan);
			/* Link at the end of the list */
			if(oldwscan != NULL)
				oldwscan->next = wscan;

			/* Reset it */
			bzero(wscan, sizeof(meshwu_scan));
			/* Save cell identifier */
			wscan->has_ap_addr = 1;
			memcpy(&(wscan->ap_addr), &(event->u.ap_addr), sizeof (sockaddr));
			break;
		case MESHW_IO_R_NWID:
			wscan->b.has_nwid = 1;
			memcpy(&(wscan->b.nwid), &(event->u.nwid), sizeof(iwparam));
			break;

		case MESHW_IO_R_FREQ:
			wscan->b.has_freq = 1;
#if __ARM_IXP__			
			GET_KHZ(wscan->b.freq, event->u.freq);
#else
			wscan->b.freq = meshwu_freq2float(&(event->u.freq));
#endif
			wscan->b.freq_flags = event->u.freq.flags;
			break;

		case MESHW_IO_R_MODE:
      wscan->b.mode = event->u.mode;
      if((wscan->b.mode < MESHWU_NUM_OPER_MODE) && (wscan->b.mode >= 0))
	wscan->b.has_mode = 1;
      break;
    case MESHW_IO_R_ESSID:
      wscan->b.has_essid = 1;
      wscan->b.essid_on = event->u.data.flags;
      memset(wscan->b.essid, '\0', MESHW_ESSID_MAX_SIZE+1);
      if((event->u.essid.pointer) && (event->u.essid.length))
	memcpy(wscan->b.essid, event->u.essid.pointer, event->u.essid.length);
      break;
    case MESHW_IO_R_ENCODE:
      wscan->b.has_key = 1;
      wscan->b.key_size = event->u.data.length;
      wscan->b.key_flags = event->u.data.flags;
      if(event->u.data.pointer)
	memcpy(wscan->b.key, event->u.essid.pointer, event->u.data.length);
      else
	wscan->b.key_flags |= MESHW_ENCODE_NOKEY;
      break;
    case MESHW_EV_QUAL:
      /* We don't get complete stats, only qual */
      wscan->has_stats = 1;
      memcpy(&wscan->stats.qual, &event->u.qual, sizeof(struct meshw_quality));
      break;
    case MESHW_IO_R_RATE:
      /* Scan may return a list of bitrates. Should we really bother with
       * an array of bitrates ? Or only the maximum bitrate ? Jean II */
    case MESHW_EV_CUSTOM:
      /* How can we deal with those sanely ? Jean II */
    default:
      break;
   }	/* switch(event->cmd) */

  return(wscan);
}

/*------------------------------------------------------------------*/
/*
 * Initiate the scan procedure, and process results.
 * This is a non-blocking procedure and it will return each time
 * it would block, returning the amount of time the caller should wait
 * before calling again.
 * Return -1 for error, delay to wait for (in ms), or 0 for success.
 * Error code is in errno
 */
int
iw_process_scan(int			devFd,
		char *			ifname,
		int			we_version,
		meshwu_scan_head *	context)
{
  struct meshw_req		wrq;
  unsigned char *	buffer = NULL;		/* Results */
  int			buflen = MESHW_SCAN_MAX_DATA; /* Min for compat WE<17 */
  unsigned char *	newbuf;

  /* Don't waste too much time on interfaces (150 * 100 = 15s) */
  context->retry++;
  if(context->retry > 150)
    {
      errno = ETIME;
      return(-1);
    }

  /* If we have not yet initiated scanning on the interface */
  if(context->retry == 1)
    {
      /* Initiate Scan */
      wrq.u.data.pointer = NULL;		/* Later */
      wrq.u.data.flags = 0;
      wrq.u.data.length = 0;
      if(meshwu_set_ext(devFd, ifname, MESHW_IO_W_SCAN, &wrq) < 0)
	return(-1);
      /* Success : now, just wait for event or results */
      return(250);	/* Wait 250 ms */
    }

 realloc:
  /* (Re)allocate the buffer - realloc(NULL, len) == malloc(len) */
  newbuf = realloc(buffer, buflen);
  if(newbuf == NULL)
    {
      /* man says : If realloc() fails the original block is left untouched */
      if(buffer)
	free(buffer);
      errno = ENOMEM;
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
      if((errno == E2BIG) && (we_version > 16))
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
	  free(buffer);
	  /* Wait for only 100ms from now on */
	  return(100);	/* Wait 100 ms */
	}

      free(buffer);
      /* Bad error, please don't come back... */
      return(-1);
    }

  /* We have the results, process them */
  if(wrq.u.data.length)
    {
      struct meshw_event		iwe;
      struct meshwu_stream_descr	stream;
      meshwu_scan *	wscan = NULL;
      int			ret;
#if DEBUG
      /* Debugging code. In theory useless, because it's debugged ;-) */
      int	i;
      printf("Scan result [%02X", buffer[0]);
      for(i = 1; i < wrq.u.data.length; i++)
	printf(":%02X", buffer[i]);
      printf("]\n");
#endif

      /* Init */
      meshwu_init_event_stream(&stream, (char *) buffer, wrq.u.data.length);
      /* This is dangerous, we may leak user data... */
      context->result = NULL;

      /* Look every token */
      do
	{
	  /* Extract an event and print it */
	  ret = meshwu_extract_event_stream(&stream, &iwe, we_version);
	  if(ret > 0)
	    {
	      /* Convert to wireless_scan struct */
	      wscan = iw_process_scanning_token(&iwe, wscan);
	      /* Check problems */
	      if(wscan == NULL)
		{
		  free(buffer);
		  errno = ENOMEM;
		  return(-1);
		}
	      /* Save head of list */
	      if(context->result == NULL)
		context->result = wscan;
	    }
	}
      while(ret > 0);
    }

  /* Done with this interface - return success */
  free(buffer);
  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Perform a wireless scan on the specified interface.
 * This is a blocking procedure and it will when the scan is completed
 * or when an error occur.
 *
 * The scan results are given in a linked list of wireless_scan objects.
 * The caller *must* free the result himself (by walking the list).
 * If there is an error, -1 is returned and the error code is available
 * in errno.
 *
 * The parameter we_version can be extracted from the range structure
 * (range.we_version_compiled - see meshwu_get_range_info()), or using
 * iw_get_kernel_we_version(). For performance reason, you should
 * cache this parameter when possible rather than querying it every time.
 *
 * Return -1 for error and 0 for success.
 */
int
iw_scan(int			devFd,
	char *			ifname,
	int			we_version,
	meshwu_scan_head *	context)
{
  int		delay;		/* in ms */

  /* Clean up context. Potential memory leak if(context.result != NULL) */
  context->result = NULL;
  context->retry = 0;

  /* Wait until we get results or error */
  while(1)
    {
      /* Try to get scan results */
      delay = iw_process_scan(devFd, ifname, we_version, context);

      /* Check termination */
      if(delay <= 0)
	break;

      /* Wait a bit */
      usleep(delay * 1000);
    }

  /* End - return -1 or 0 */
  return(delay);
}

