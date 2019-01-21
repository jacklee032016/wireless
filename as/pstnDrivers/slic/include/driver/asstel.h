/*
 * Revision 1.2.1.0  2005/05/27 17:31  wangwei
 * add Linefeed Control setting[register 64]
 *
 * $Author: lizhijie $
 * $Log: asstel.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.3  2005/05/27 09:39:35  wangwei
 * add Linefeed Control setting[register 64]
 *
 * Revision 1.2  2005/04/20 02:34:12  lizhijie
 * no message
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.11  2005/02/05 05:34:39  lizhijie
 * add conditional compile for echo-cancel headers
 *
 * Revision 1.10  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.9  2004/12/15 07:33:05  lizhijie
 * recommit
 *
 * Revision 1.8  2004/12/14 12:48:50  lizhijie
 * support building header files in the architecture platform
 *
 * Revision 1.7  2004/12/11 06:12:19  lizhijie
 * merge into CVS
 *
 * Revision 1.6  2004/12/11 05:05:30  eagle
 * add test_gain.c
 *
 * Revision 1.5  2004/12/02 07:14:25  lizhijie
 * tuning the buffer parameters
 *
 * Revision 1.4  2004/11/29 01:59:56  lizhijie
 * some bug about echo features
 *
 * Revision 1.3  2004/11/26 12:34:15  lizhijie
 * add multi-card support
 *
 * Revision 1.2  2004/11/22 01:54:04  lizhijie
 * add some user module into CVS
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#ifndef __AS_DEV_TEL_H__
#define __AS_DEV_TEL_H__


#if  __KERNEL__
#include <linux/config.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/slab.h>
#endif

/* These 2 macro defination is moved to the Makefile */
#if 0
#define AS_SOFTWARE_DSP		0
#define AS_PROSLIC_DSP			1
#endif

/* Define the max # of outgoing DTMF or FSK digits to queue in-kernel */
#define AS_MAX_DTMF_BUF 256

#define AS_DEBUG	1

#define AS_MAX_CADENCE		16

#include "as_tel_dtmf.h"

//#include "as_dev_tones.h"

#include "as_dsp.h"

#include "as_dev_ctl.h"

#if __KERNEL__
#include "ecdis.h"
#if 0
#include "mec.h"
#endif
#include "mec2.h"
#endif

struct as_tone;
struct as_tone_state;

#ifndef AS_ELAST
#define AS_ELAST 500
#endif

/* Signalling types */
#define __AS_SIG_FXO	 (1 << 12)	/* Never use directly */
#define __AS_SIG_FXS	 (1 << 13)	/* Never use directly */

#define AS_SIG_NONE		(0)			/* Channel not configured */
#define AS_SIG_FXSLS	((1 << 0) | __AS_SIG_FXS)	/* FXS, Loopstart */
#define AS_SIG_FXSGS	((1 << 1) | __AS_SIG_FXS)	/* FXS, Groundstart */
#define AS_SIG_FXSKS	((1 << 2) | __AS_SIG_FXS)	/* FXS, Kewlstart */

#define AS_SIG_FXOLS	((1 << 3) | __AS_SIG_FXO)	/* FXO, Loopstart */
#define AS_SIG_FXOGS	((1 << 4) | __AS_SIG_FXO)	/* FXO, Groupstart */
#define AS_SIG_FXOKS	((1 << 5) | __AS_SIG_FXO)	/* FXO, Kewlstart */

/*canceled by wangwei in 2005/05/27  
add Linefeed Control setting[register 64]*/
#define AS_LINEFEED_OPEN    					0
#define AS_LINEFEED_FORWARD_ACTIVE		1
#define AS_LINEFEED_FORWARD_ONHOOK		2
#define AS_LINEFEED_TIP_OPEN				3
#define AS_LINEFEED_RINGING				4
#define AS_LINEFEED_REVERSE_ACTIVE   		5
#define AS_LINEFEED_REVERSE_ON_HOOK		6
#define AS_LINEFEED_RING_OPEN				7
#define AS_ABIT			8
#define AS_BBIT			4
#define AS_CBIT			2
#define AS_DBIT			1

#define AS_DEV_MAJOR		197		/* 196 */

#if __ARM__
#define AS_CHUNKSIZE		 		8*4
#else
#define AS_CHUNKSIZE		 		8
#endif
#define AS_MIN_CHUNKSIZE	 		AS_CHUNKSIZE
#define AS_DEFAULT_CHUNKSIZE	 	AS_CHUNKSIZE
#if __ARM__
#define AS_MAX_CHUNKSIZE 	 		AS_CHUNKSIZE*2
#else
#define AS_MAX_CHUNKSIZE 	 		AS_CHUNKSIZE
#endif

#define AS_DEFAULT_BLOCKSIZE 		160
#define AS_MAX_BLOCKSIZE 	 		8192
#define AS_DEFAULT_NUM_BUFS	 	2
#define AS_MAX_NUM_BUFS		 	32
#define AS_MAX_BUF_SPACE         		32768


#define AS_POLICY_IMMEDIATE	 	0		/* Start play/record immediately */
#define AS_POLICY_WHEN_FULL  		1		/* Start play/record when buffer is full */

#define	AS_RING_DEBOUNCE_TIME	2000	/* 2000 ms ring debounce time */

#define AS_LAW_DEFAULT	0	/* Default law for span */
#define AS_LAW_MULAW	1	/* Mu-law */
#define AS_LAW_ALAW	2	/* A-law */

/* Define the maximum block size */
#define	AS_MAX_BLOCKSIZE	8192

#define AS_MAX_CHANNELS		16		

#include <linux/types.h>
#include <linux/poll.h>


#define	AS_MAX_EVENTSIZE	64	/* 64 events max in buffer */

#define 	AS_RINGTRAILER (50 * 8)	/* Don't consider a ring "over" until it's been gone at least this
									   much time */

#define	AS_LOOPCODE_TIME 10000		/* send loop codes for 10 secs */
#define	AS_ALARMSETTLE_TIME	5000	/* allow alarms to settle for 5 secs */
#define	AS_AFTERSTART_TIME 500		/* 500ms after start */

#define 	AS_RINGOFFTIME 4000		/* Turn off ringer for 4000 ms */
#define	AS_KEWLTIME 500		/* 500ms for kewl pulse */
#define	AS_AFTERKEWLTIME 300    /* 300ms after kewl pulse */

/* Span flags */
#define 	AS_CHAN_FLAG_REGISTERED		(1 << 0)
#define 	AS_CHAN_FLAG_RUNNING			(1 << 1)
#define 	AS_FLAG_RBS						(1 << 12)	/* Span uses RBS signalling */

/* Channel flags */
#define AS_CHAN_FLAG_DTMFDECODE		(1 << 2)	/* Channel supports native DTMF decode */
#define AS_CHAN_FLAG_MFDECODE		(1 << 3)	/* Channel supports native MFr2 decode */
#define AS_CHAN_FLAG_ECHOCANCEL		(1 << 4)	/* Channel supports native echo cancellation */

#define AS_CHAN_FLAG_PSEUDO			(1 << 7)	/* Pseudo channel */
#define AS_CHAN_FLAG_CLEAR			(1 << 8)	/* Clear channel */
#define AS_CHAN_FLAG_AUDIO			(1 << 9)	/* Audio mode channel */
#define AS_CHAN_FLAG_OPEN				(1 << 10)	/* Channel is open */

#define AS_FLAG_FCS						(1 << 11)	/* Calculate FCS */
#define AS_CHAN_FLAG_LINEAR			(1 << 13)	/* Talk to user space in linear */


#if __KERNEL__
/* defines for transmit signalling */
typedef enum
{
	AS_TXSIG_ONHOOK,			/* On hook */
	AS_TXSIG_OFFHOOK,			/* Off hook */
	AS_TXSIG_START,				/* Start / Ring */
	AS_TXSIG_KEWL				/* Drop battery if possible */
} as_txsig_t;

typedef enum 
{
	AS_RXSIG_ONHOOK,
	AS_RXSIG_OFFHOOK,
	AS_RXSIG_START,
	AS_RXSIG_RING,
	AS_RXSIG_INITIAL
} as_rxsig_t;
	

typedef enum
{
	AS_CHANNEL_TYPE_FXS,			/* FXS device channel */
	AS_CHANNEL_TYPE_FXO			/* FXO device channel */
} as_channel_type_t;


struct as_dev_span;
struct as_dev_chan;

struct as_dev_chan 
{
	spinlock_t lock;
	char name[40];		/* Name */

	/* Specified by zaptel */
	int channo;			/* Channel number */
	int chanpos;			/* start from 1 */
	int flags;
	long rxp1;
	long rxp2;
	long rxp3;
	int txtone;
	int tx_v2;
	int tx_v3;
	int v1_1;
	int v2_1;
	int v3_1;
	int toneflags;

	u_char 	*writechunk;						/* Actual place to write to */
	u_char 	swritechunk[ AS_MAX_CHUNKSIZE];	/* Buffer to be written */
	u_char 	*readchunk;						/* Actual place to read from */
	u_char 	sreadchunk[ AS_MAX_CHUNKSIZE];	/* Preallocated static area */
	
	/* Pointer to tx and rx gain tables */
	u_char *rxgain; /* length os 256 for every direction */
	u_char *txgain;
	
	/* Whether or not we have allocated gains or are using the default : booleab type */
	int gainalloc;

	/* Specified by driver, readable by Assist driver */
	void 		*private;			/* Private channel data : it point to as_fxs struct*/
	struct file 	*file;				/* File structure , passed by system in file_operation */
	
	
	struct as_dev_span *span;		/* Span we're a member of */
	struct as_dev_chan  *next;
	int sig;			/* Signalling */
	int sigcap;			/* Capability for signalling */
	

	/* Used only by zaptel -- NO DRIVER SERVICEABLE PARTS BELOW */
	/* Buffer declarations */
	u_char				*readbuf[AS_MAX_NUM_BUFS];	/* read buffer */
	int					inreadbuf;
	int					outreadbuf;
	wait_queue_head_t 	readbufq; /* read wait queue */

	u_char				*writebuf[AS_MAX_NUM_BUFS]; /* write buffers */
	int					inwritebuf;
	int					outwritebuf;
	wait_queue_head_t 	writebufq; /* write wait queue */
	
	int		blocksize;	/* Block size */

	int			eventinidx;  /* out index in event buf (circular) */
	int			eventoutidx;  /* in index in event buf (circular) */
	unsigned int	eventbuf[AS_MAX_EVENTSIZE];  /* event circ. buffer */
	wait_queue_head_t eventbufq; /* event wait queue */
	
	wait_queue_head_t txstateq;	/* waiting on the tx state to change */
	
	int		readn[AS_MAX_NUM_BUFS];  /* # of bytes ready in read buf */
	int		readidx[AS_MAX_NUM_BUFS];  /* current read pointer */
	int		writen[AS_MAX_NUM_BUFS];  /* # of bytes ready in write buf */
	int		writeidx[AS_MAX_NUM_BUFS];  /* current write pointer */
	
	int		numbufs;			/* How many buffers in channel */
	int		txbufpolicy;			/* Buffer policy */
	int		rxbufpolicy;			/* Buffer policy */
	int		txdisable;				/* Disable transmitter */
	int 		rxdisable;				/* Disable receiver */
	
	/* Ring cadence */	
	int ringcadence[AS_MAX_CADENCE];

	/* Digit string dialing stuff ,this buffer is used to send DTMF format CallerID
	to the phone */
	int		digitmode;		/* What kind of tones are we sending? DIGIT_MODE_DTMF or  DIGIT_MODE_FSK*/
	char		txdialbuf[AS_MAX_DTMF_BUF];
	int 		dialing;

	int 		firstDialingTimer;		/* used for the called ID after first ringing, lizhijie 2004.11.16 */

	int		afterdialingtimer;
	int		cadencepos;				/* Where in the cadence we are */

	/* I/O Mask */	
	int					iomask;  /* I/O Mux signal mask */
	wait_queue_head_t 	sel;	/* thingy for select stuff */
	
	short	getlin[AS_MAX_CHUNKSIZE];			/* Last transmitted samples */
	unsigned char getraw[AS_MAX_CHUNKSIZE];		/* Last received raw data */
	short	getlin_lastchunk[AS_MAX_CHUNKSIZE];	/* Last transmitted samples from last chunk */
	short	putlin[AS_MAX_CHUNKSIZE];			/* Last received samples */
	unsigned char putraw[AS_MAX_CHUNKSIZE];		/* Last received raw data */

	/* Is echo cancellation enabled or disabled */
	int		echocancel;
	echo_can_state_t	*ec;
	echo_can_disable_detector_state_t txecdis;
	echo_can_disable_detector_state_t rxecdis;
	
	int 	echostate;		/* State of echo canceller */
	int		echolastupdate;	/* Last echo can update pos */
	int		echotimer;		/* Timer for echo update */
	
	/* RBS timings  */
	int		prewinktime;  /* pre-wink time (ms) */
	int		preflashtime;	/* pre-flash time (ms) */
	int		winktime;  /* wink time (ms) */
	int		flashtime;  /* flash time (ms) */
	int		starttime;  /* start time (ms) */
	int		rxwinktime;  /* rx wink time (ms) */
	int		rxflashtime; /* rx flash time (ms) */
	int		debouncetime;  /* FXS GS sig debounce time (ms) */

	/* RING debounce timer */
	int	ringdebtimer;
	
	/* RING trailing detector to make sure a RING is really over */
	int ringtrailer;

	/* PULSE digit receiver stuff */
	int	pulsecount;
	int	pulsetimer;

	/* RBS timers */
	int 	itimerset;		/* what the itimer was set to last */
	int 	itimer;
	int 	otimer;
	
	/* RBS state */
	int gotgs;
	int txstate;
	int rxsig;
	int txsig;
	int rxsigstate;

	/* non-RBS rx state */
	int rxhooksig;
	int txhooksig;
	int kewlonhook;

	int deflaw;		/* 1 = mulaw, 2=alaw, 0=undefined */
	short *xlaw;       	/* xlaw to line table, x is decided by the 'deflaw' field */
	unsigned char *lin2x;	/*line to xlaw change table, x can chosen from A or MU */

	struct as_dtmf_input dtmf_detect;
	int (*hooksig)(struct as_dev_chan *chan, as_txsig_t hookstate);
	int (*open)(struct as_dev_chan *chan);

	int (*close)(struct as_dev_chan *chan);
	
	int (*ioctl)(struct as_dev_chan *chan, unsigned int cmd, unsigned long data);
	
	int (*watchdog)(struct as_dev_chan *chan, int cause);

/* check whether the channel is on-hook, it is used when read/write in this channel */
	int (*is_onhook)(struct as_dev_chan *chan);

	struct as_dsp_dev *dsp;

#if AS_PROSLIC_DSP
	int (*slic_play_tone)(struct as_dev_chan *chan);
	int (*slic_play_dtmf)(struct as_dev_chan *chan);
	int (*slic_ring)(struct as_dev_chan *chan);
#else
	struct as_tone_state ts;		/* Tone state */
#endif
	/* Tone zone stuff */
	struct as_tone *curtone;		/* Current tone we're playing (if any) */
	int		tonep;					/* Current position in tone */

};


struct as_dev_span 
{
	spinlock_t lock;
	char name[40];			/* Span name */
	char desc[80];			/* Span description */
	int deflaw;			/* Default law (ZT_MULAW or ZT_ALAW) */
	int alarms;			/* Pending alarms on span */
	int flags;
	int irq;			/* IRQ for this span's hardware */
	int lbo;			/* Span Line-Buildout */
	int lineconfig;			/* Span line configuration */
	int linecompat;			/* Span line compatibility */
	int channels;			/* Number of channels in span */
	int txlevel;			/* Tx level */
	int rxlevel;			/* Rx level */
	int syncsrc;			/* current sync src (gets copied here) */
	unsigned int bpvcount;		/* BPV counter */
	unsigned int crc4count;	        /* CRC4 error counter */
	unsigned int ebitcount;		/* current E-bit error count */
	unsigned int fascount;		/* current FAS error count */

	int maintstat;			/* Maintenance state */
	wait_queue_head_t maintq;	/* Maintenance queue */
	int mainttimer;			/* Maintenance timer */
	
	int irqmisses;			/* Interrupt misses */

	struct as_dev_chan *chans;		/* Member channel structures */

	/*   ==== Span Callback Operations ====   */
	/* Req: Set the requested chunk size.  This is the unit in which you must
	   report results for conferencing, etc */
	int (*setchunksize)(struct as_dev_span *span, int chunksize);

	/* Opt: Configure the span (if appropriate) */
//	int (*spanconfig)(struct as_dev_span *span, struct zt_lineconfig *lc);
	
	/* Opt: Start the span */
	int (*startup)(struct as_dev_span *span);
	
	/* Opt: Shutdown the span */
	int (*shutdown)(struct as_dev_span *span);
	
	/* Opt: Enable maintenance modes */
	int (*maint)(struct as_dev_span *span, int mode);

	/* ====  Channel Callback Operations ==== */
	/* Opt: Set signalling type (if appropriate) */
	int (*chanconfig)(struct as_dev_chan *chan, int sigtype);

	/* Opt: Prepare a channel for I/O */
	int (*open)(struct as_dev_chan *chan);

	/* Opt: Close channel for I/O */
	int (*close)(struct as_dev_chan *chan);
	
	/* Opt: IOCTL */
	int (*ioctl)(struct as_dev_chan *chan, unsigned int cmd, unsigned long data);
	
	
	int (*rbsbits)(struct as_dev_chan *chan, int bits);
	/* Option 2: If you don't know about sig bits, but do have their
	   equivalents (i.e. you can disconnect battery, detect off hook,
	   generate ring, etc directly) then you can just specify a
	   sethook function, and we'll call you with appropriate hook states
	   to set.  Still set the ZT_FLAG_RBS in this case as well */
	int (*hooksig)(struct as_dev_chan *chan, as_txsig_t hookstate);
	
	/* Option 3: If you can't use sig bits, you can write a function
	   which handles the individual hook states  */
	int (*sethook)(struct as_dev_chan *chan, int hookstate);

	/* Used by zaptel only -- no user servicable parts inside */
	int spanno;			/* Span number for zaptel */
	int offset;			/* Offset within a given card */
	int lastalarms;		/* Previous alarms */

	/* If the watchdog detects no received data, it will call the
	   watchdog routine */
	int (*watchdog)(struct as_dev_span *span, int cause);
	int watchcounter;
	int watchstate;

	void *private;			/* Private stuff, pointer to wcfxs  */
	int count;				/* number of channels */
	struct as_zone *zone;		/* Zone for selecting tones */
//	int 	tonezone;				/* Tone zone for this channel */

	struct as_dsp_dev *dsp;
	
};


/* macro-oni for determining a unit (channel) number */
#define	UNIT(file) 	MINOR(file->f_dentry->d_inode->i_rdev)

#define AS_XLAW(a,c) (c->xlaw[(a)])
#define AS_LIN2X(a,c) ((c)->lin2x[((unsigned short)(a)) >> 2])

#define AS_DIGIT_MODE_DTMF 		0
#define AS_DIGIT_MODE_MFV1		1


/* states for transmit signalling */
typedef enum 
{
	AS_TXSTATE_ONHOOK,
	AS_TXSTATE_OFFHOOK,
	AS_TXSTATE_START,
	AS_TXSTATE_PREWINK,
	AS_TXSTATE_WINK,
	AS_TXSTATE_PREFLASH,
	AS_TXSTATE_FLASH,
	AS_TXSTATE_DEBOUNCE,
	AS_TXSTATE_AFTERSTART,
	AS_TXSTATE_RINGON,
	AS_TXSTATE_RINGOFF,
	AS_TXSTATE_KEWL,
	AS_TXSTATE_AFTERKEWL
} AS_TXSTATE_t;

#if AS_PROSLIC_DSP
#else
static inline short as_txtone_nextsample(struct as_dev_chan *ss)
{
	/* follow the curves, return the sum */
	ss->v1_1 = ss->v2_1;
	ss->v2_1 = ss->v3_1;
	ss->v3_1 = (ss->txtone * ss->v2_1 >> 15) - ss->v1_1;
	return ss->v3_1;
}

#endif

#if 0
int as_io_span_in_receive(struct as_dev_span *span);
int as_io_span_out_transmit(struct as_dev_span *span);
#endif
int as_io_span_in_receive(void *fxs);
int as_io_span_out_transmit(void *fxs);
void as_io_channel_hooksig(struct as_dev_chan *chan, as_rxsig_t rxsig);


void as_close_channel(struct as_dev_chan *chan);

/* from as_tel_init.c */
struct as_dev_chan  *as_span_get_channel(int channo);
struct as_dsp_dev  *as_span_get_dsp(void );


int  as_schedule_delay(wait_queue_head_t *q);


int  as_channel_reallocbufs(struct as_dev_chan *ss, int size, int numbufs);
void as_channel_rbs_sethook(struct as_dev_chan *chan, int txsig, int txstate, int timeout);
int as_channel_hangup(struct as_dev_chan *chan);
char *as_channel_debug_info(struct as_dev_chan *chan);


int as_channel_check_law(struct as_dev_chan *chan);
void  as_channel_compare_gain(struct as_dev_chan *chan);
void as_channel_set_default_gain(struct as_dev_chan  *chan);
void as_channel_set_law(struct as_dev_chan *chan, int law);
void  __init as_law_conv_tables_init(void);


int as_chanandpseudo_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data, int unit);

void as_channel_qevent_nolock(struct as_dev_chan *chan, int event);
void as_channel_qevent_lock(struct as_dev_chan *chan, int event);
int as_channel_deqevent_lock(struct as_dev_chan *chan );
#endif

#ifdef __KERNEL__
	#define trace		printk("%s[%d]\r\n", __FUNCTION__, __LINE__);
#else
	#define trace		printf("%s[%d]\r\n", __FUNCTION__, __LINE__);
#endif

#ifdef __KERNEL__
struct as_zone __init *as_zone_init(void );
void __exit as_zone_destory(struct as_zone *zone);

int  as_span_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data);
unsigned int as_span_poll(struct file *file, struct poll_table_struct *wait_table);
int as_span_open(struct inode *inode, struct file *file);
int as_span_release(struct inode *inode, struct file *file);
ssize_t as_span_read(struct file *file, char *usrbuf, size_t count, loff_t *ppos);
ssize_t as_span_write(struct file *file, const char *usrbuf, size_t count, loff_t *ppos);

/* from as_tel_init.c */
int as_channel_register(struct as_dev_chan *chan);
void as_channel_unregister(struct as_dev_chan *chan);

void as_ec_chunk(struct as_dev_chan *ss, unsigned char *rxchunk, const unsigned char *txchunk);


char *as_cards_debug_info(void *data);
char *as_cards_debug_status_info(void *data);


#endif

#include "as_version.h"


#endif 
