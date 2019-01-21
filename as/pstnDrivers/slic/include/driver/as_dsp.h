/*
 * $Author: lizhijie $
 * $Log: as_dsp.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:46  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:04  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:51  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:09  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
/*
 * all tone, zone and DSP data and handlers are defined in this file
 *  Li Zhijie 2004.11.12
*/
#ifndef  __AS_DSP_H__
#define __AS_DSP_H__

#define AS_TONE_ZONE_MAX		128
/* No bigger than 32k for everything per tone zone */
#define MAX_SIZE 32768
/* No more than 128 subtones */
#define MAX_TONES 128

struct as_dev_chan ;

#ifdef __KERNEL__
#include "asstel.h"
#endif


typedef enum
{
//	AS_TONE_ZONE_DEFAULT 	=-1,	/* To restore default */
	AS_TONE_STOP			= 	-1,
	AS_TONE_DIALTONE		=	0,
	AS_TONE_BUSY			=	1,
	AS_TONE_RINGTONE		=	2,
	AS_TONE_CONGESTION	=	3,
	AS_TONE_CALLWAIT		=	4,
	AS_TONE_DIALRECALL	=	5,
	AS_TONE_RECORDTONE	=	6,
	AS_TONE_INFO			=	7,
	AS_TONE_CUST1			=	8,
	AS_TONE_CUST2			=	9,
	AS_TONE_STUTTER		=	10
}as_tone_type_t;

#define AS_TONE_MAX		16

#define AS_MAX_CADENCE		16

/* store the raw define of tone for the purpose of AudioCode DSP module or debug  */
struct as_raw_tone
{
	int freq1;
	int freq2;
	int time;
};

struct as_tone_def_header 
{
	int zone_id;
	char name[40];
	int count;		/* How many samples follow */
	int ringcadence[AS_MAX_CADENCE];
	/* Immediately follow the as_tone_def_header by as_tone_def's */
};


struct as_tone_def 
{/* Structure for zone programming */
	int tone_id;		/* See AS_TONE_* */
	int next;		/* What the next position in the cadence is
				   (They're numbered by the order the appear here) */
	int samples;		/* How many samples to play for this cadence */
				/* Now come the constants we need to make tones */
	int shift;		/* How much to scale down the volume (2 is nice) */

	/* 
		Calculate the next 6 factors using the following equations:
		l = <level in dbm>, f1 = <freq1>, f2 = <freq2>
		gain = pow(10.0, (l - 3.14) / 20.0) * 65536.0 / 2.0;

		// Frequency factor 1 
		fac_1 = 2.0 * cos(2.0 * M_PI * (f1/8000.0)) * 32768.0;
		// Last previous two samples 
		init_v2_1 = sin(-4.0 * M_PI * (f1/8000.0)) * gain;
		init_v3_1 = sin(-2.0 * M_PI * (f1/8000.0)) * gain;

		// Frequency factor 2 
		fac_2 = 2.0 * cos(2.0 * M_PI * (f2/8000.0)) * 32768.0;
		// Last previous two samples 
		init_v2_2 = sin(-4.0 * M_PI * (f2/8000.0)) * gain;
		init_v3_2 = sin(-2.0 * M_PI * (f2/8000.0)) * gain;
	*/
	int fac1;				/* as freq 1 when hardware tone */
	int init_v2_1;		/* as amplitude when hardware tone */
	int init_v3_1;		/* as init phase when hardware tone */
	int fac2;		
	int init_v2_2;		
	int init_v3_2;
	
	int modulate;		/* no used when hardware tone */

	struct as_raw_tone raw_tone;
};


#if __KERNEL__

/* tone data structure used both in software and hardware tone implementation */
struct as_tone 
{
	int fac1;			/* as frequency 1 when hardware tone */
	int init_v2_1;	/* as amplitude 1 when hardware tone */
	int init_v3_1;	/* as init phase 1 when hardware tone */

	int fac2;			/* as frequency 2 when hardware tone */
	int init_v2_2;	/* as amplitude 2 when hardware tone */
	int init_v3_2;	/* as init phase 2 when hardware tone */

/* How long to play this tone before going to the next (in samples) */
	int tonesamples;		

	struct as_tone *next;		/* Next tone in this sequence */

	int modulate;
	
	struct as_raw_tone raw_tone;
};


#if AS_PROSLIC_DSP
#else
struct as_tone_state 
{
	int v1_1;
	int v2_1;
	int v3_1;
	int v1_2;
	int v2_2;
	int v3_2;
	int modulate;
};


typedef struct
{
	long	x1;
	long	x2;
	long	y1;
	long	y2;
	long	e1;
	long	e2;
	int	samps;
	int	lastdetect;
} sf_detect_state_t;

static inline short as_tone_nextsample(struct as_tone_state *ts, struct as_tone *zt)
{
	/* follow the curves, return the sum */

	int p;

	ts->v1_1 = ts->v2_1;
	ts->v2_1 = ts->v3_1;
	ts->v3_1 = (zt->fac1 * ts->v2_1 >> 15) - ts->v1_1;

	ts->v1_2 = ts->v2_2;
	ts->v2_2 = ts->v3_2;
	ts->v3_2 = (zt->fac2 * ts->v2_2 >> 15) - ts->v1_2;

	/* Return top 16 bits */
	if (!ts->modulate) 
		return ts->v3_1 + ts->v3_2;
	/* we are modulating */
	p = ts->v3_2 - 32768;
	if (p < 0) 
		p = -p;
	p = ((p * 9) / 10) + 1;
	
	return (ts->v3_1 * p) >> 15;

}

#endif

struct as_dev_span;
struct as_dev_chan;

struct as_zone_tones_data 
{
	int zone_id;
	char name[40];	/* Informational, only */
	int 	ringcadence[AS_MAX_CADENCE];

/* delay between the end of first ring and begin of callerID, added by lizhijie 2004.11.16 */
	int pause;			
	
	struct as_tone *tones[AS_TONE_MAX]; 
/* immiedately followed by all tones data */	
};


struct as_zone
{
	struct as_tone *(*get_dtmf_tone)(struct as_zone *zone, unsigned char digit);

	struct as_raw_tone *(*get_dtmf_raw_tone)(struct as_zone *zone, unsigned char digit);

	struct as_tone *(*get_tone)(struct as_zone *zone, as_tone_type_t type);

	struct as_raw_tone *(*get_raw_tone)(struct as_zone *zone, as_tone_type_t type);

	int (*replace_tones)(struct as_zone *zone, struct as_zone_tones_data *tones);

	struct as_tone *pause_tone; /* point to as_pause_tone defined in as_digit.h */
	/* Each of these is a circular list  of as_tones to generate what we want.  
	Use NULL if the tone is unavailable */
	struct as_tone  *dtmf_tones;
/* upper fields can not be changed when running */

	struct as_zone_tones_data  *data;
};

#define __ECHO_STATE_MUTE				(1 << 8)
#define ECHO_STATE_IDLE				(0)
#define ECHO_STATE_PRETRAINING		(1 | (__ECHO_STATE_MUTE))
#define ECHO_STATE_STARTTRAINING		(2 | (__ECHO_STATE_MUTE))
#define ECHO_STATE_AWAITINGECHO		(3 | (__ECHO_STATE_MUTE))
#define ECHO_STATE_TRAINING			(4 | (__ECHO_STATE_MUTE))
#define ECHO_STATE_ACTIVE				(5)

#define AS_WATCHDOG_NOINTS			(1 << 0)
#define AS_WATCHDOG_INIT				1000
#define AS_WATCHSTATE_UNKNOWN		0
#define AS_WATCHSTATE_OK				1
#define AS_WATCHSTATE_RECOVERING		2
#define AS_WATCHSTATE_FAILED			3

/* data handler is protected by the span.lock and play handler is protected by the channel.lock */
struct as_dsp_dev
{
	char name[128];

#if AS_PROSLIC_DSP
	int (*play_dtmf)(struct as_dsp_dev *dsp, struct as_dev_chan *chan );		
	int (*play_fsk)(struct as_dsp_dev *dsp, struct as_dev_chan *chan );		
#else
	void (*init_tone_state)(struct as_dsp_dev *dsp, struct as_dev_chan *chan);
#endif

/* init this dsp device : init the dsp and zone data member of span */
	int (*init)(struct as_dsp_dev *dsp, struct as_dev_span *span);
	void (*destory)(struct as_dsp_dev *dsp);
	int (*replace_tones)(struct as_dsp_dev *dsp, struct as_zone_tones_data *tones);

	int (*play_tone)(struct as_dsp_dev *dsp, struct as_dev_chan *chan, as_tone_type_t  type);
/* play all the dialing string stored in channel */
	int (*play_caller_id)(struct as_dsp_dev *dsp, struct as_dev_chan *chan );		

	char * (*get_desc)(struct as_dsp_dev *dsp);
	
	int (*proc_read)(char *page, char **start, off_t off, int count, int *eof, void *data);

	struct as_zone *zone;

};

#endif

#endif

