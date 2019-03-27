/*
 * $Log: as_isdn_ctrl.h,v $
 * Revision 1.1.1.1  2006/11/29 08:55:13  lizhijie
 * AS600 Kernel
 *
 * Revision 1.2  2005/12/29 03:02:20  lizhijie
 * *** empty log message ***
 *
 * Revision 1.1  2005/12/26 08:40:39  lizhijie
 * no message
 *
 * Revision 1.1  2005/12/22 10:08:15  lizhijie
 * no message
 *
 * $Id: as_isdn_ctrl.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
*/

#ifndef  __AS_ISDN_CTL_H__
#define  __AS_ISDN_CTL_H__

#define AS_MAX_DTMF_BUF 			256

#define AS_MAX_CADENCE  			16
/* Flush and stop the read (input) process */
#define	AS_FLUSH_READ				1
/* Flush and stop the write (output) process */
#define	AS_FLUSH_WRITE			2
/* Flush and stop both (input and output) processes */
#define	AS_FLUSH_BOTH				(AS_FLUSH_READ | AS_FLUSH_WRITE)
/* Flush the event queue */
#define	AS_FLUSH_EVENT			4
/* Flush everything */
#define	AS_FLUSH_ALL				(AS_FLUSH_READ | AS_FLUSH_WRITE | AS_FLUSH_EVENT)


#define AS_DIAL_OP_APPEND			1
#define AS_DIAL_OP_REPLACE			2
#define AS_DIAL_OP_CANCEL			3


/* Value for AS_CTL_HOOK command */
#define	AS_ONHOOK		0
#define	AS_OFFHOOK	1
#define	AS_WINK		2
#define	AS_FLASH		3
#define	AS_START		4
#define	AS_RING		5
#define 	AS_RINGOFF  	6


/* Ret. Value for GET/WAIT Event, no event */
#define	AS_EVENT_NONE					0

/* Ret. Value for GET/WAIT Event, Went Onhook */
#define	AS_EVENT_ONHOOK 				1

/* Ret. Value for GET/WAIT Event, Went Offhook or got Ring */
#define	AS_EVENT_RINGOFFHOOK 		2

/* Ret. Value for GET/WAIT Event, Got Wink or Flash */
#define	AS_EVENT_WINKFLASH 			3

/* Ret. Value for GET/WAIT Event, Got Alarm */
#define	AS_EVENT_ALARM				4

/* Ret. Value for GET/WAIT Event, Got No Alarm (after alarm) */
#define	AS_EVENT_NOALARM 				5

/* Ret. Value for GET/WAIT Event, HDLC Abort frame */
#define AS_EVENT_ABORT 				6

/* Ret. Value for GET/WAIT Event, HDLC Frame overrun */
#define AS_EVENT_OVERRUN 				7

/* Ret. Value for GET/WAIT Event, Bad FCS */
#define AS_EVENT_BADFCS 				8

/* Ret. Value for dial complete */
#define AS_EVENT_DIALCOMPLETE			9

/* Ret Value for ringer going on */
#define AS_EVENT_RINGERON 				10

/* Ret Value for ringer going off */
#define AS_EVENT_RINGEROFF 			11

/* Ret Value for hook change complete */
#define AS_EVENT_HOOKCOMPLETE 		12

/* Ret Value for bits changing on a CAS / User channel */
#define AS_EVENT_BITSCHANGED 			13

/* Ret value for the beginning of a pulse coming on its way */
#define AS_EVENT_PULSE_START 			14

/* Timer event -- timer expired */
#define AS_EVENT_TIMER_EXPIRED		15

/* Timer event -- ping ready */
#define AS_EVENT_TIMER_PING			16

#define AS_EVENT_PULSEDIGIT 			(1 << 16)	/* This is OR'd with the digit received */
#define AS_EVENT_DTMFDIGIT  			(1 << 17)	/* Ditto for DTMF */


#if 0
struct as_ring_cadence 
{
	int ringcadence [AS_MAX_CADENCE];
};


typedef struct as_dialoperation
{
	int op;
	char dialstr[AS_MAX_DTMF_BUF];
} AS_DIAL_OPERATION;


typedef struct as_params
{
	int channo;		/* Channel number */
	int spanno;		/* Span itself */
	int chanpos;	/* Channel number in span */
	int	sigtype;  /* read-only */
	int sigcap;	 /* read-only */
	int rxisoffhook; /* read-only */
	int rxbits;	/* read-only */
	int txbits;	/* read-only */
	int txhooksig;	/* read-only */
	int rxhooksig;	/* read-only */
	int curlaw;	/* read-only  -- one of ZT_LAW_MULAW or ZT_LAW_ALAW */
	int idlebits;	/* read-only  -- What is considered the idle state */
	char name[40];	/* Name of channel */
	int	prewinktime;
	int	preflashtime;
	int	winktime;
	int	flashtime;
	int	starttime;
	int	rxwinktime;
	int	rxflashtime;
	int	debouncetime;
} AS_PARAMS;


typedef struct as_maintinfo
{
	int	spanno;		/* span number 1-2 */
	int	command;	/* command */
} AS_MAINTINFO;

/*how to use this structure ?*/
typedef struct as_gains
{
	int	chan;		/* channel number, 0 for current */
	unsigned char rxgain[256];	/* Receive gain table */
	unsigned char txgain[256];	/* Transmit gain table */
} AS_GAINS;


typedef struct as_bufferinfo
{
	int txbufpolicy;	/* Policy for handling receive buffers */
	int rxbufpolicy;	/* Policy for handling receive buffers */
	int numbufs;		/* How many buffers to use */
	int bufsize;		/* How big each buffer is */
	int readbufs;		/* How many read buffers are full (read-only) */
	int writebufs;		/* How many write buffers are full (read-only) */
} AS_BUFFERINFO;

typedef struct as_dialparams
{
	int mfv1_tonelen;		/* MF tone length (KP = this * 5/3) */
	int dtmf_tonelen;			/* DTMF tone length */
	int reserved[4];			/* Reserved for future expansion -- always set to 0 */
} AS_DIAL_PARAMS;


struct as_global_para
{
	char *country;

};
struct as_channel_para
{
	int law;
	int callerid;//0 is dtmf;1 is fsk;
	int buffer_size;
	int buffer_count;
	int block_size;
};
struct as_channel_state
{
	int channel_no;
	int type;
	int available;//0 is available; -1 is the para over.
};
#endif


#define AS_ISDN_CTL_CODE						'K'

/* * Get Transfer Block Size. */
#define AS_ISDN_CTL_ACTIVATE			_IOR (AS_ISDN_CTL_CODE, 1, int)

/* * Set Transfer Block Size. */
#define AS_ISDN_CTL_DEACTIVATE		_IOW (AS_ISDN_CTL_CODE, 2, int)

#if 0
/* Flush Buffer(s) and stop I/O */
#define AS_CTL_FLUSH					_IOW (AS_ISDN_CTL_CODE, 3, int)

/*  Wait for Write to Finish : e.g. low layer device consume the data and user space can write 
 */
#define	AS_CTL_SYNC					_IOW (AS_ISDN_CTL_CODE, 4, int)

/* * Get channel parameters */
#define AS_CTL_GET_PARAMS				_IOW (AS_ISDN_CTL_CODE, 5, struct as_params)

#define AS_CTL_SET_PARAMS				_IOW (AS_ISDN_CTL_CODE, 6, struct as_params)

/* * Set Hookswitch Status */
#define AS_CTL_HOOK						_IOW ( AS_ISDN_CTL_CODE, 7, int)

/* * Get Signalling Event */
#define AS_CTL_GETEVENT				_IOR ( AS_ISDN_CTL_CODE, 8, int)

/* * Set Maintenance Mode */
#define AS_CTL_MAINT					_IOW (AS_ISDN_CTL_CODE, 11, struct zt_maintinfo)

/* * Get Channel audio gains */
#define AS_CTL_GETGAINS				_IOWR (AS_ISDN_CTL_CODE, 16, struct as_gains)

/* * Set Channel audio gains */
#define AS_CTL_SETGAINS				_IOWR (AS_ISDN_CTL_CODE, 17, struct as_gains)

/* * Set Line (T1) Configurations and start system */
#define	AS_CTL_SPANCONFIG				_IOW (AS_ISDN_CTL_CODE, 18, struct as_lineconfig)

/* * Send a particular tone (see ZT_TONE_*) */
#define	AS_CTL_SENDTONE				_IOW (AS_ISDN_CTL_CODE, 21, int)

/*
 * Set your region for tones (see ZT_TONE_ZONE_*)
 */
#define	AS_CTL_SETTONEZONE			_IOW (AS_ISDN_CTL_CODE, 22, int)

/*
 * Retrieve current region for tones (see ZT_TONE_ZONE_*)
 */
#define	AS_CTL_GETTONEZONE			_IOR (AS_ISDN_CTL_CODE, 23, int)

/*
 * Master unit only -- set default zone (see ZT_TONE_ZONE_*)
 */
#define	AS_CTL_DEFAULTZONE			_IOW (AS_ISDN_CTL_CODE, 24, int)

/*
 * Load a tone zone from a as_tone_def_header, see below...
 */
#define AS_CTL_LOADZONE				_IOW (AS_ISDN_CTL_CODE, 25, struct as_tone_def_header)

/*
 * Set buffer policy 
 */
#define AS_CTL_SET_BUFINFO				_IOW (AS_ISDN_CTL_CODE, 27, struct as_bufferinfo)

/*
 * Get current buffer info
 */
#define AS_CTL_GET_BUFINFO				_IOR (AS_ISDN_CTL_CODE, 28, struct as_bufferinfo)

/*
 * Get dialing parameters
 */
#define AS_CTL_GET_DIALPARAMS			_IOR (AS_ISDN_CTL_CODE, 29, struct as_dialparams)

/*
 * Set dialing parameters
 */
#define AS_CTL_SET_DIALPARAMS			_IOW (AS_ISDN_CTL_CODE, 30, struct as_dialparams)

/*
 * Append, replace, or cancel a dial string
 */
#define AS_CTL_SET_DTMF_STR			_IOW (AS_ISDN_CTL_CODE, 31, struct as_dialoperation)

/*
 * Set a clear channel into audio mode
 */
#define AS_CTL_AUDIOMODE				_IOW (AS_ISDN_CTL_CODE, 32, int)

/*
 * Enable or disable echo cancellation on a channel 
 * The number is zero to disable echo cancellation and non-zero
 * to enable echo cancellation.  If the number is between 32
 * and 256, it will also set the number of taps in the echo canceller
 */
#define AS_CTL_ECHOCANCEL				_IOW (AS_ISDN_CTL_CODE, 33, int)

/*
 * Return a channel's channel number (useful for the /dev/zap/pseudo type interfaces 
 */
#define AS_CTL_CHANNO					_IOR (AS_ISDN_CTL_CODE, 34, int)


/*
 * Return a flag indicating whether channel is currently dialing
 * return the status of a channel, this command is send to pseudo device
 */
#define AS_CTL_DIALING					_IOR (AS_ISDN_CTL_CODE, 35, int)

/* Numbers 60 to 90 are reserved for private use of low level hardware
   drivers */

/*
 * Temporarily set the law on a channel to 
 * AS_LAW_DEFAULT, AS_LAW_ALAW, or AS_LAW_MULAW.  Is reset on close.  
 */
#define AS_CTL_SETLAW					_IOW (AS_ISDN_CTL_CODE, 39, int)

/*
 * Temporarily set the channel to operate in linear mode when non-zero
 * or default law if 0
 */
#define AS_CTL_SETLINEAR				_IOW (AS_ISDN_CTL_CODE, 40, int)


/*
 * Set the ring cadence for FXS interfaces
 */
#define AS_CTL_SETCADENCE				_IOW (AS_ISDN_CTL_CODE, 42, struct as_ring_cadence)

/*
 * Display Channel Diagnostic Information on Console
 */
#define AS_CTL_CHAN_DIAG				_IOR (AS_ISDN_CTL_CODE, 44, int) 


/*
 * Set timer expiration (in samples)
 */
#define AS_CTL_TIMERCONFIG				_IOW (AS_ISDN_CTL_CODE, 47, int)


/*
 * Request echo training in some number of ms (with muting in the mean time)
 */
#define	AS_CTL_ECHOTRAIN				_IOW (AS_ISDN_CTL_CODE, 50, int)

/*
 * Set on hook transfer for n number of ms -- implemnted by low level driver
 */
#define	AS_CTL_ONHOOKTRANSFER		_IOW (AS_ISDN_CTL_CODE, 51, int)

/*
 * Queue Ping
 */
#define AS_CTL_TIMERPING 				_IOW (AS_ISDN_CTL_CODE, 42, int) /* Should be 52, but works */

/*
 * Acknowledge ping
 */
#define AS_CTL_TIMERPONG 				_IOW (AS_ISDN_CTL_CODE, 53, int)

/*
 * Set/get signalling freeze
 */
#define AS_CTL_SIGFREEZE 				_IOW (AS_ISDN_CTL_CODE, 54, int)
#define AS_CTL_GETSIGFREEZE 			_IOR (AS_ISDN_CTL_CODE, 55, int)

/*
 *  60-80 are reserved for private drivers
 *  80-85 are reserved for dynamic span stuff
 */

#define AS_CTL_SPAN_STARTUP			_IOW (AS_ISDN_CTL_CODE, 99, int)
#define AS_CTL_SPAN_SHUTDOWN			_IOW (AS_ISDN_CTL_CODE, 100, int)


#define AS_CTL_GET_DTMF_DETECT		_IOW (AS_ISDN_CTL_CODE, 150, int )
#define AS_GET_CHANNEL_NUMBER		_IOW (AS_ISDN_CTL_CODE, 151, int )
#define AS_GET_CHANNELS_STATES		_IOW (AS_ISDN_CTL_CODE, 152, struct as_channel_state)

#endif

#endif

