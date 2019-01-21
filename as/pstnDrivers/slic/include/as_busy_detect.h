#ifndef _ASTERISK_DSP_H
#define _ASTERISK_DSP_H

#define DSP_FEATURE_SILENCE_SUPPRESS (1 << 0)
#define DSP_FEATURE_BUSY_DETECT      (1 << 1)


#define DEFAULT_THRESHOLD 	1024

#define BUSY_PERCENT		10	/* The percentage diffrence between the two last silence periods */
#define BUSY_THRESHOLD		100	/* Max number of ms difference between max and min times in busy */
#define BUSY_MIN		75	/* Busy must be at least 80 ms in half-cadence */
#define BUSY_MAX		1100	/* Busy can't be longer than 1100 ms in half-cadence */

/* Remember last 15 units */


/* Define if you want the fax detector -- NOT RECOMMENDED IN -STABLE */
#define FAX_DETECT
#define TONE_THRESH 10.0	/* How much louder the tone should be than channel energy */
#define TONE_MIN_THRESH 1e8	/* How much tone there should be at least to attempt */
#define COUNT_THRESH  3		/* Need at least 50ms of stuff to count it */


#define FORMAT_ALAW 	1
#define FORMAT_ULAW 	0
#define DSP_HISTORY 10

typedef struct 
{
	int threshold;
	int totalsilence;
	int totalnoise;
	int features;
	int busymaybe;
	int busycount;
	int historicnoise[DSP_HISTORY];
	int historicsilence[DSP_HISTORY];
	int law;  //1 a-law 0 u-law  defalut 0 u-law
}as_tone_detector ;


#endif
