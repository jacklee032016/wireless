/*
 * Used for DTMF Caller ID by Proslic Hardware 
 * Li zhijie  2004.11.11
 */
#ifndef __AS_DIGITS_SI_H__
#define __AS_DIGITS_SI_H__

#define 	DEFAULT_DTMF_LENGTH	100 * 8	/* 100 ms every dtmf tone */
#define 	DEFAULT_MFV1_LENGTH	60 * 8
#define	PAUSE_LENGTH			500 * 8	/* 500 ms after dialing tone for caller ID */

/* At the end of silence, the tone stops */
static struct as_tone_array as_si_dtmf_silence =
	{ 0, 0, 0, 0, 0, 0, NULL, DEFAULT_DTMF_LENGTH };

/* At the end of silence, the tone stops */
#if 0
static struct as_tone_array as_si_mfv1_silence =
	{ 0, 0, 0, 0, 0, 0, NULL, DEFAULT_MFV1_LENGTH };
#endif

/* A pause after the first dialing */
static struct as_tone_array as_si_tone_pause =
	{ 0, 0, 0, 0, 0, 0, NULL, PAUSE_LENGTH };

#include "as_dtmf_hard_tones.h"

#endif 
