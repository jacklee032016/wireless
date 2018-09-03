/*
 * $Id: meshw_ioctl_handler.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 * This file define the new driver API for Wireless Extensions
 * Cloned from linux/wireless.h, lizhijie
 */

#ifndef	__MESH_WIRELESS_HANDLER_H__
#define	__MESH_WIRELESS_HANDLER_H__

#include "meshw_ioctl.h"

#ifdef	__KERNEL__
/**************************** CONSTANTS ****************************/

/* Enable enhanced spy support. Disable to reduce footprint */
#define MESHW_WIRELESS_SPY
#define MESHW_WIRELESS_THRSPY

/* Special error message for the driver to indicate that we
 * should do a commit after return from the meshw_handler */
#define EIWCOMMIT	EINPROGRESS

/* Flags available in struct meshw_request_info */
#define MESHW_REQUEST_FLAG_NONE	0x0000	/* No flag so far */

/* Type of headers we know about (basically union meshw_req_data) */
#define MESHW_HEADER_TYPE_NULL		0	/* Not available */
#define MESHW_HEADER_TYPE_CHAR		2	/* char [MESHNAMSIZ] */
#define MESHW_HEADER_TYPE_UINT		4	/* __u32 */
#define MESHW_HEADER_TYPE_FREQ		5	/* struct meshw_freq */
#define MESHW_HEADER_TYPE_ADDR		6	/* struct sockaddr */
#define MESHW_HEADER_TYPE_POINT		8	/* struct meshw_point */
#define MESHW_HEADER_TYPE_PARAM		9	/* struct meshw_param */
#define MESHW_HEADER_TYPE_QUAL		10	/* struct meshw_quality */

/* Handling flags */
/* Most are not implemented. I just use them as a reminder of some
 * cool features we might need one day ;-) */
#define MESHW_DESCR_FLAG_NONE		0x0000	/* Obvious */
/* Wrapper level flags */
#define MESHW_DESCR_FLAG_DUMP		0x0001	/* Not part of the dump command */
#define MESHW_DESCR_FLAG_EVENT		0x0002	/* Generate an event on SET */
#define MESHW_DESCR_FLAG_RESTRICT	0x0004	/* GET : request is ROOT only */
				/* SET : Omit payload from generated iwevent */
/* Driver level flags */
#define MESHW_DESCR_FLAG_WAIT		0x0100	/* Wait for driver event */


struct meshw_request_info
{
	__u16		cmd;		/* Wireless Extension command */
	__u16		flags;		/* More to come ;-) */
};

/*
 * This is how a function handling a Wireless Extension should look
 * like (both get and set, standard and private).
 */
typedef int (*meshw_handler)(MESH_DEVICE *dev, struct meshw_request_info *info, union meshw_req_data *wrqu, char *extra);

/*
 * This define all the handler that the driver export.
 * As you need only one per driver type, please use a static const
 * shared by all driver instances... Same for the members...
 */
struct meshw_handler_def
{
	/* Number of handlers defined (more precisely, index of the last defined handler + 1) */
	__u16					num_standard;
	__u16					num_private;
	/* Number of private arg description */
	__u16					num_private_args;

	/* Array of handlers for standard ioctls
	 * We will call dev->wireless_handlers->standard[ioctl - SIOCSIWNAME]
	 */
	meshw_handler *			standard;

	/* Array of handlers for private ioctls
	 * Will call dev->wireless_handlers->private[ioctl - MESHW_IO_FIRST_PRIV]
	 */
	meshw_handler *			private;

	/* Arguments of private handler. This one is just a list, so you
	 * can put it in any order you want and should not leave holes...
	 * We will automatically export that to user space... */
	struct meshw_priv_args *	private_args;

	/* Driver enhanced spy support */
	long						spy_offset;	/* Spy data offset */

	/* In the long term, get_wireless_stats will move from
	 * 'struct net_device' to here, to minimise bloat. */
};


/* Describe how a standard IOCTL looks like. */
struct meshw_ioctl_description
{
	__u8	header_type;		/* NULL, meshw_point or other */
	__u8	token_type;		/* Future */
	__u16	token_size;		/* Granularity of payload */
	__u16	min_tokens;		/* Min acceptable token number */
	__u16	max_tokens;		/* Max acceptable token number */
	__u32	flags;			/* Special handling of the request */
};

/* Need to think of short header translation table. Later. */

/* --------------------- ENHANCED SPY SUPPORT --------------------- */
/*
 * In the old days, the driver was handling spy support all by itself.
 * Now, the driver can delegate this task to Wireless Extensions.
 * It needs to include this struct in its private part and use the
 * standard spy meshw_handler.
 */

/*
 * Instance specific spy data, i.e. addresses spied and quality for them.
 */
struct meshw_spy_data
{
#ifdef MESHW_WIRELESS_SPY
	/* --- Standard spy support --- */
	int					spy_number;
	u_char				spy_address[MESHW_MAX_SPY][ETH_ALEN];
	struct meshw_quality	spy_stat[MESHW_MAX_SPY];
#ifdef MESHW_WIRELESS_THRSPY
	/* --- Enhanced spy support (event) */
	struct meshw_quality	spy_thr_low;	/* Low threshold */
	struct meshw_quality	spy_thr_high;	/* High threshold */
	u_char				spy_thr_under[MESHW_MAX_SPY];
#endif /* MESHW_WIRELESS_THRSPY */
#endif /* MESHW_WIRELESS_SPY */
};

/**************************** PROTOTYPES ****************************/

/* Handle /proc/net/wireless */
extern int dev_get_wireless_info(char * buffer, char **start, off_t offset, int length);

int meshw_ioctl_process(MESH_DEVICE *dev, unsigned long data, unsigned int cmd);

/* Second : functions that may be called by driver modules */
/* Send a single event to user space */
extern void wireless_send_event(MESH_DEVICE *dev,unsigned int cmd,union meshw_req_data *wrqu,char *extra);

/* We may need a function to send a stream of events to user space.
 * More on that later... */

/* Standard handler for MESHW_IO_W_SPY */
extern int iw_handler_set_spy(MESH_DEVICE *dev, struct meshw_request_info *info, union meshw_req_data *wrqu, char *extra);
/* Standard handler for MESHW_IO_R_SPY */
extern int iw_handler_get_spy(MESH_DEVICE *dev,struct meshw_request_info *info, union meshw_req_data *wrqu,char *extra);
/* Standard handler for MESHW_IO_W_SPY_THR */
extern int iw_handler_set_thrspy(MESH_DEVICE *dev, struct meshw_request_info *info, union meshw_req_data *wrqu, char *extra);
/* Standard handler for MESHW_IO_R_SPY_THR */
extern int iw_handler_get_thrspy(MESH_DEVICE *	dev, struct meshw_request_info *info, union meshw_req_data *wrqu, char *extra);
/* Driver call to update spy records */
extern void wireless_spy_update(MESH_DEVICE *	dev, unsigned char *address, struct meshw_quality *wstats);

/* Wrapper to add an Wireless Event to a stream of events. */
static inline char *iwe_stream_add_event(char *	stream,		/* Stream of events */
		     char *	ends,		/* End of stream */
		     struct meshw_event *iwe,	/* Payload */
		     int	event_len)	/* Real size of payload */
{
	/* Check if it's possible */
	if((stream + event_len) < ends)
	{
		iwe->len = event_len;
		memcpy(stream, (char *) iwe, event_len);
		stream += event_len;
	}
	return stream;
}

/* Wrapper to add an short Wireless Event containing a pointer to a stream of events. */
static inline char *iwe_stream_add_point(char *	stream,		/* Stream of events */
		     char *	ends,		/* End of stream */
		     struct meshw_event *iwe,	/* Payload */
		     char *	extra)
{
	int	event_len = MESHW_EV_POINT_LEN + iwe->u.data.length;
	/* Check if it's possible */
	if((stream + event_len) < ends)
	{
		iwe->len = event_len;
		memcpy(stream, (char *) iwe, MESHW_EV_POINT_LEN);
		memcpy(stream + MESHW_EV_POINT_LEN, extra, iwe->u.data.length);
		stream += event_len;
	}
	return stream;
}

/*
 * Wrapper to add a value to a Wireless Event in a stream of events.
 * Be careful, this one is tricky to use properly :
 * At the first run, you need to have (value = event + MESHW_EV_LCP_LEN).
 */
static inline char *iwe_stream_add_value(char *	event,		/* Event in the stream */
		     char *	value,		/* Value in event */
		     char *	ends,		/* End of stream */
		     struct meshw_event *iwe,	/* Payload */
		     int	event_len)	/* Real size of payload */
{
	/* Don't duplicate LCP */
	event_len -= MESHW_EV_LCP_LEN;

	/* Check if it's possible */
	if((value + event_len) < ends)
	{
		/* Add new value */
		memcpy(value, (char *) iwe + MESHW_EV_LCP_LEN, event_len);
		value += event_len;
		/* Patch LCP */
		iwe->len = value - event;
		memcpy(event, (char *) iwe, MESHW_EV_LCP_LEN);
	}
	return value;
}

#endif /* __KERNEL__ */

#endif/* _MESH_WIRELESS_HANDLER_H */

