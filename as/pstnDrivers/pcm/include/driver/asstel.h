/*
 * $Author: lizhijie $
 * $Id: asstel.h,v 1.2 2007/03/21 18:11:54 lizhijie Exp $
 * Driver for Tiger-AnalogPBX
*/
#ifndef __ASSIST_TELEPHONE_H__
#define __ASSIST_TELEPHONE_H__

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

#define AS_DEBUG	1

typedef struct as_gains
{
	int	chan;		/* channel number, 0 for current */
	unsigned char rxgain[256];	/* Receive gain table */
	unsigned char txgain[256];	/* Transmit gain table */
} AS_GAINS;

/* Flush and stop the read (input) process */
#define	AS_FLUSH_READ			1
/* Flush and stop the write (output) process */
#define	AS_FLUSH_WRITE		2
/* Flush and stop both (input and output) processes */
#define	AS_FLUSH_BOTH			(AS_FLUSH_READ | AS_FLUSH_WRITE)
/* Flush the event queue */
#define	AS_FLUSH_EVENT		4
/* Flush everything */
#define	AS_FLUSH_ALL			(AS_FLUSH_READ | AS_FLUSH_WRITE | AS_FLUSH_EVENT)


#define AS_DEV_MAJOR					197		/* 196 */
#define AS_CTL_CODE						'J'

/* * Set Channel audio gains */
#define AS_CTL_SETGAINS				_IOWR (AS_CTL_CODE, 17, struct as_gains)
/* Flush Buffer(s) and stop I/O */
#define AS_CTL_FLUSH					_IOW (AS_CTL_CODE, 3, int)


#ifndef AS_ELAST
#define AS_ELAST 500
#endif

#define AS_CHUNKSIZE		 		80
#define AS_MIN_CHUNKSIZE	 		AS_CHUNKSIZE
#define AS_DEFAULT_CHUNKSIZE	 	AS_CHUNKSIZE
#define AS_MAX_CHUNKSIZE 	 		AS_CHUNKSIZE

#define AS_DEFAULT_BLOCKSIZE 		160
#define AS_MAX_BLOCKSIZE 	 		8192
#define AS_DEFAULT_NUM_BUFS	 	4
#define AS_MAX_NUM_BUFS		 	32
#define AS_MAX_BUF_SPACE         		32768

#define AS_POLICY_IMMEDIATE	 	0		/* Start play/record immediately */
#define AS_POLICY_WHEN_FULL  		1		/* Start play/record when buffer is full */

#define AS_MAX_CHANNELS		16		

#include <linux/types.h>
#include <linux/poll.h>


/* Span flags */
#define 	AS_CHAN_FLAG_REGISTERED			(1 << 0)
#define 	AS_CHAN_FLAG_RUNNING			(1 << 1)
#define 	AS_FLAG_RBS						(1 << 12)	/* Span uses RBS signalling */

/* Channel flags */
#define AS_CHAN_FLAG_CLEAR			(1 << 2)		/* Clear channel */
#define AS_CHAN_FLAG_AUDIO			(1 << 3)		/* Audio mode channel */

#define AS_CHAN_FLAG_OPEN				(1 << 5)	/* Channel is open */
#define AS_CHAN_FLAG_ALLOCAT			(1 << 6)	/* for PCM-Analog  */

#if __KERNEL__
/* defines for transmit signalling */
	
struct as_dev_span;
struct as_dev_chan;

struct as_dev_chan 
{
	spinlock_t lock;
	char name[40];		/* Name */

	int channo;			/* Channel number, start from 0 */
	int chanpos;			/* start from 0 */
	int flags;

	u_char 	*writechunk;						/* Actual place to write to */
	u_char 	swritechunk[ AS_MAX_CHUNKSIZE];		/* Buffer to be written */
	u_char 	*readchunk;							/* Actual place to read from */
	u_char 	sreadchunk[ AS_MAX_CHUNKSIZE];		/* Preallocated static area */
	
	/* Whether or not we have allocated gains or are using the default : booleab type */
	int gainalloc;
	/* Pointer to tx and rx gain tables */
	u_char *rxgain; 
	u_char *txgain;
	

	/* Specified by driver, readable by Assist driver */
	void 		*private;			/* Private channel data : it point to as_fxs struct*/
	struct file 	*file;				/* File structure , passed by system in file_operation */
	
	
	struct as_dev_span *span;		/* Span we're a member of */
	struct as_dev_chan  *next;
	
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

	int		readn[AS_MAX_NUM_BUFS];  /* # of bytes ready in read buf */
	int		readidx[AS_MAX_NUM_BUFS];  /* current read pointer */
	int		writen[AS_MAX_NUM_BUFS];  /* # of bytes ready in write buf */
	int		writeidx[AS_MAX_NUM_BUFS];  /* current write pointer */
	
	int		numbufs;			/* How many buffers in channel */
	
#if AS_POLICY_ENBALE	
	int		txbufpolicy;			/* Buffer policy */
	int		rxbufpolicy;			/* Buffer policy */
	int		txdisable;				/* Disable transmitter */
	int 		rxdisable;				/* Disable receiver */
#endif

	wait_queue_head_t 	sel;	/* thingy for select stuff */

	int (*open)(struct as_dev_chan *chan);

	int (*close)(struct as_dev_chan *chan);
	
	int (*ioctl)(struct as_dev_chan *chan, unsigned int cmd, unsigned long data);
	
	int (*watchdog)(struct as_dev_chan *chan, int cause);
};


struct as_dev_span 
{
	spinlock_t lock;
	char name[40];			/* Span name */
	char desc[80];			/* Span description */
	
	int flags;
	int irq;			/* IRQ for this span's hardware */

	int channels;			/* Number of channels in span */

	struct as_dev_chan *chans;		/* Member channel structures */

#if 0
	/* If the watchdog detects no received data, it will call the watchdog routine */
	int (*watchdog)(struct as_dev_span *span, int cause);
	int watchcounter;
	int watchstate;
#endif
	void *private;			/* Private stuff, point to wcfxs*/
	int count;				/* number of channels */
};

/* macro-oni for determining a unit (channel) number */
#define	UNIT(file) 			MINOR(file->f_dentry->d_inode->i_rdev)


int as_io_span_in_receive(void *fxs);
int as_io_span_out_transmit(void *fxs);

void as_close_channel(struct as_dev_chan *chan);

/* from as_tel_init.c */
struct as_dev_chan  *as_span_get_channel(int channo);


int  as_schedule_delay(wait_queue_head_t *q);


int  as_channel_reallocbufs(struct as_dev_chan *ss, int size, int numbufs);
char *as_channel_debug_info(struct as_dev_chan *chan);

void as_channel_set_default_gain(struct as_dev_chan  *chan);
int as_channel_set_gain(struct as_dev_chan *chan, AS_GAINS *gain);


/* from as_tel_init.c */
int as_channel_register(struct as_dev_chan *chan);
void as_channel_unregister(struct as_dev_chan *chan);


char *as_cards_debug_info(void *data);

#if AS_DEBUG_TIGER
char *as_cards_debug_status_info(void *data);
#endif

#endif


#ifdef __KERNEL__
//	#define trace			printk("%s[%d]\r\n", __FUNCTION__, __LINE__);
	#define trace			
#else
	#define trace		printf("%s[%d]\r\n", __FUNCTION__, __LINE__);
#endif

#include "as_version.h"

#endif 
