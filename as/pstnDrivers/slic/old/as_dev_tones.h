#ifndef  __AS_TONES_H__
#define __AS_TONES_H__

struct as_dev_chan ;

#ifdef __KERNEL__
#include "asstel.h"
#endif

#define AS_TONE_ZONE_MAX		128

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

/* store the raw define of tone for the purpose of AudioCode DSP module */
#if	AS_PROSLIC_DSP
/* used to initialize the as_tonearray instance in kernel for every channel */
struct as_hard_tone
{
	int frequency1;
	int amplitude1;
	int initphase1;
	int frequency2;
	int amplitude2;
	int initphase2;
	int next;
	int time;		
	int toneid;
};

#else
struct as_raw_tone
{
	int freq1;
	int freq2;
	int time;
};

struct as_tone_def 
{/* Structure for zone programming */
	int tone;		/* See ZT_TONE_* */
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
	int fac1;		
	int init_v2_1;		
	int init_v3_1;		
	int fac2;		
	int init_v2_2;		
	int init_v3_2;
	int modulate;

	struct as_raw_tone raw_tone;
};
#endif

#if __KERNEL__

#if AS_PROSLIC_DSP
struct as_tone_array
{
	int  frequency1;
	int  amplitude1;
	int  initphase1;
	int  frequency2;
	int  amplitude2;
	int  initphase2;
	struct as_tone_array *next;
	int  time;
};

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

/* Used privately by zaptel.  Avoid touching directly */
struct as_tone 
{
	int fac1;
	int init_v2_1;
	int init_v3_1;

	int fac2;
	int init_v2_2;
	int init_v3_2;

	int tonesamples;		/* How long to play this tone before 
					   going to the next (in samples) */
	int modulate;

	struct as_tone *next;		/* Next tone in this sequence */

	struct as_raw_tone raw_tone;
};

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

struct as_zone 
{
	char name[40];	/* Informational, only */
	int ringcadence[AS_MAX_CADENCE];

#if AS_PROSLIC_DSP
	struct as_tone_array  *tones[AS_TONE_MAX];
	struct as_tone_array  *dtmf_tones;
	struct as_tone_array  *pause_tone;
	
	struct as_tone_array *(*get_dtmf_tone)(struct as_zone *zone, unsigned char digit);
	struct as_tone_array *(*get_tone)(struct as_zone *zone, as_tone_type_t type);
#else
	struct as_tone *tones[AS_TONE_MAX]; 

	struct as_tone *pause_tone; /* point to as_pause_tone defined in as_digit.h */
	
	/* Each of these is a circular list
	   of zt_tones to generate what we
	   want.  Use NULL if the tone is
	   unavailable */
	struct as_tone  *dtmf_tones;

	struct as_tone *(*get_dtmf_tone)(struct as_zone *zone, unsigned char digit);

	struct as_raw_tone *(*get_dtmf_raw_tone)(struct as_zone *zone, unsigned char digit);

	struct as_tone *(*get_tone)(struct as_zone *zone, as_tone_type_t type);

	struct as_raw_tone *(*get_raw_tone)(struct as_zone *zone, as_tone_type_t type);
#endif
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

struct as_dsp_dev
{
	char name[128];
	struct as_zone *zone;

/* init this dsp device : init the dsp and zone data member of span */
	int (*init)(struct as_dsp_dev *dsp, struct as_dev_span *span);
	void (*destory)(struct as_dsp_dev *dsp);

#if AS_PROSLIC_DSP

#else
	void (*init_tone_state)(struct as_dsp_dev *dsp, struct as_dev_chan *chan);
#endif

	int (*play_tone)(struct as_dsp_dev *dsp, struct as_dev_chan *chan, as_tone_type_t  type);
//	int (*play_dtmf)(struct as_dsp_dev *dsp, struct as_dev_chan *chan, unsigned char digit);		
/* play all the dialing string stored in channel */
	int (*play_dtmf)(struct as_dsp_dev *dsp, struct as_dev_chan *chan );		
	char * (*get_desc)(struct as_dsp_dev *dsp);
};

#endif

#endif

