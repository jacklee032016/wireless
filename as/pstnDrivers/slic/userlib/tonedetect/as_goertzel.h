/*
 * Header file for:
 */

#define NPOINTS 				204		/* samples/run */
#define SAMPLERATE 			8000
#define NCOEFF 				16		/* nr of frequencies to be analyzed */
#define MNCOEFF 			12		/* nr of frequencies to be analyzed (MF) */

#define DTMF_TRESH 			700		/* min. volume for DTMF */
#define SILENCE_TRESH 		500		/* max. volume for silence */
#define H2_TRESH   			20000	/* 2nd harmonic */
#define MF_TRESH 			700		/* min. volume for DTMF */

#define AMP_BITS  			9		/* bits in amplitude -- can't be 16,
				   since we'd get integer overflows */

#define SILENCE_LEVEL		16		/* max level for channel silence */


#define LOGRP 			0
#define HIGRP 			1

typedef struct 
{
	int freq;			/* frequency */
	int grp;			/* low/high group */
	int k;			/* k */
	int k2;			/* k for 1st harmonic */
} dtmf_t;

typedef struct 
{
	int freq;			/* frequency */
	int dur;			/* duration */
	int k;			/* k */
	int k2;			/* k for 1st harmonic */
} mf_t;

static mf_t mf_tones[6] = 
{
	{ 700, 0, 0 },
	{ 900, 0, 0 },
	{ 1100, 0, 0 },
	{ 1300,  0, 0 },
	{ 1500,  0, 0 },
	{ 1700,  0, 0 }
};

static char *mfs = 
"   1 23  45 6    78 9   0        CA *   B       #                ";

/* mfs[3] = '1'; mfs[5] = '2'; mfs[6] = '3'; mfs[9] = '4'; mfs[10] = '5';
mfs[12] = '6'; mfs[17] = '7'; mfs[18] = '8'; mfs[20] = '9'; mfs[24] = '0';
mfs[36] = '*'; mfs[48] = '#'; mfs[34] = 'A'; mfs[40] = 'B'; mfs[33] = 'C'; */


