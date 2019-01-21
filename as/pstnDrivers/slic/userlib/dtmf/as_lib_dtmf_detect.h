#ifndef  __AS_LIB_DTMF_DETECT_H__
#define __AS_LIB_DTMF_DETECT_H__
/* 
tone_detect.h - General telephony tone detection, and specific detection of DTMF.
*/

/* keep the calculate result of every Goertzel step */
typedef struct
{
	float v2;
	float v3;
	float fac;
}as_goertzel_state_t;

#define	AS_MAX_DTMF_DIGITS 	128

typedef struct
{
	int hit1;
	int hit2;
	int hit3;
	int hit4;

	int mhit; /* MF ?*/

	as_goertzel_state_t row_out[4];
	as_goertzel_state_t col_out[4];
	as_goertzel_state_t row_out2nd[4];
	as_goertzel_state_t col_out2nd[4];
	as_goertzel_state_t fax_tone;
	as_goertzel_state_t fax_tone2nd;
	float energy;
    
	int current_sample;
	char digits[AS_MAX_DTMF_DIGITS + 1];
	int current_digits;
	int detected_digits;
	int lost_digits;
	int digit_hits[16];
	int fax_hits;
}as_dtmf_detect_state_t;

/* keep the coeffiecent for every frequency element , this is not variable in algorithm */
typedef struct
{
	float fac;
} as_tone_detection_descriptor_t;

#endif

