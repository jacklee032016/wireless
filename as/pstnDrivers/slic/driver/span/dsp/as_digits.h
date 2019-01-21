/*
 * $Author: lizhijie $
 * $Log: as_digits.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.3  2004/12/15 07:33:05  lizhijie
 * recommit
 *
 * Revision 1.2  2004/12/14 12:48:50  lizhijie
 * support building header files in the architecture platform
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
/*
 * Used for DTMF Caller ID both by Proslic Hardware and Software DSP
 * Li zhijie  2004.11.11
 */
#ifndef __AS_DIGITS_H__
#define __AS_DIGITS_H__

#define 	DEFAULT_DTMF_LENGTH	100 * 8	/* 100 ms every dtmf tone */
#define 	DEFAULT_MFV1_LENGTH	60 * 8
#define	PAUSE_LENGTH			500 * 8	/* 500 ms after dialing tone for caller ID */

/* At the end of silence, the tone stops */
static struct as_tone as_dtmf_silence =
	{ 0, 0, 0, 0, 0, 0, DEFAULT_DTMF_LENGTH, NULL, 0, {0, 0, DEFAULT_DTMF_LENGTH}};

/* At the end of silence, the tone stops */
static struct as_tone as_mfv1_silence =
	{ 0, 0, 0, 0, 0, 0, DEFAULT_MFV1_LENGTH, NULL, 0, {0, 0, DEFAULT_MFV1_LENGTH} };

/* A pause in the dialing */
static struct as_tone as_tone_pause =
	{ 0, 0, 0, 0, 0, 0, PAUSE_LENGTH, NULL, 0, {0, 0, PAUSE_LENGTH}  };

#ifdef  __ARM__
	#include "as_dtmf_tones_arm.h"
	#include "as_zone_tones_arm.h"
#else
	#include "as_dtmf_tones_i686.h"
	#include "as_zone_tones_i686.h"
#endif

#endif 
