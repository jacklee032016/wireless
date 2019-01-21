/*
 * $Author: lizhijie $
 * $Log: as_gen_dtmf_tones.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/12/14 12:47:34  lizhijie
 * build the tones header in architecture platform
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
/* Generate a header file for a particular  single or double frequency */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "as_mf_digits.h"

#define CLIP 32635
#define BIAS 0x84

unsigned char linear2ulaw(short sample)
{
  static int exp_lut[256] = {0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,
                             4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
                             5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
                             5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};

	int sign, exponent, mantissa;
	unsigned char ulawbyte;
 
  /* Get the sample into sign-magnitude. */
	sign = (sample >> 8) & 0x80;          /* set aside the sign */
	if (sign != 0) sample = -sample;              /* get magnitude */
	if (sample > CLIP) sample = CLIP;             /* clip the magnitude */
 
  /* Convert from 16 bit linear to ulaw. */
	sample = sample + BIAS;
	exponent = exp_lut[(sample >> 7) & 0xFF];
	mantissa = (sample >> (exponent + 3)) & 0x0F;
	ulawbyte = ~(sign | (exponent << 4) | mantissa);
#ifdef ZEROTRAP
	if (ulawbyte == 0) 
		ulawbyte = 0x02;   /* optional CCITT trap */
#endif
 
	return(ulawbyte);
}                                                                                            

#define LEVEL -10

int process(FILE *f, char *label, AS_DIAL z[])
{
	char c;
#if AS_PROSLIC_DSP
	float coeff1,coeff2;
#else
	float gain;
#endif
	int fac1, init_v2_1, init_v3_1,
	    fac2, init_v2_2, init_v3_2;

	while(z->chr) 
	{
		c = z->chr;
		if (c == '*')
			c = 's';
		if (c == '#')
			c ='p';

#if AS_PROSLIC_DSP
		coeff1=cos(2*M_PI*(z->f1/ 8000.0));
        	coeff2=cos(2*M_PI*(z->f2/ 8000.0));
		
		fac1 = coeff1 *  32768.0;
		init_v2_1= 0.25*sqrt((1-coeff1)/(1+coeff1))*32767.0*0.5;
		init_v3_1 = 0;
		
		fac2 = coeff2 *  32768.0;
		init_v2_2 = 0.25*sqrt((1-coeff2)/(1+coeff2))*32767.0*0.5;
		init_v3_2 = 0;
#else
		/* Bring it down 6 dbm */
		gain = pow(10.0, (LEVEL - 3.14) / 20.0) * 65536.0 / 2.0;

		fac1 = 2.0 * cos(2.0 * M_PI * (z->f1 / 8000.0)) * 32768.0;
		init_v2_1 = sin(-4.0 * M_PI * (z->f1 / 8000.0)) * gain;
		init_v3_1 = sin(-2.0 * M_PI * (z->f1 / 8000.0)) * gain;
		
		fac2 = 2.0 * cos(2.0 * M_PI * (z->f2 / 8000.0)) * 32768.0;
		init_v2_2 = sin(-4.0 * M_PI * (z->f2 / 8000.0)) * gain;
		init_v3_2 = sin(-2.0 * M_PI * (z->f2 / 8000.0)) * gain;
#endif
		fprintf(f, "\t /* %s_%c */ { %d, %d, %d, %d, %d, %d, DEFAULT_DTMF_LENGTH, &%s_silence, 0 ,\r\n\t\t{%d, %d, DEFAULT_DTMF_LENGTH}}, \n", 
			label, c,
			fac1, init_v2_1, init_v3_1, 
			fac2, init_v2_2, init_v3_2,
			label, (int)z->f1, (int)z->f2 );
		
		z++;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	FILE *f;
#ifdef  __ARM__
	if ((f = fopen("as_dtmf_tones_arm.h", "w"))) 
#else
	if ((f = fopen("as_dtmf_tones_i686.h", "w"))) 
#endif
	{
		fprintf(f, "/* DTMF and MF tones used by the Assist Telephone Driver, in static tables.\n"
				   "   Generated automatically from gendigits.  Do not edit by hand.  */\n"); 
		fprintf(f, "static struct as_tone as_dtmf_tones[16] = {\n");
		process(f, "as_dtmf", as_dtmf_dial);
		fprintf(f, "};\n\n");
		fprintf(f, "static struct as_tone as_mfv1_tones[15] = {\n");
		process(f, "as_mfv1", as_mf_dial);
		fprintf(f, "};\n\n");
		fprintf(f, "/* END as_dtmf_tones.h */\n");
		fclose(f);
	} 
	else 
	{
		fprintf(stderr, "Unable to open as_dtmf_tones.h for writing\n");
		return 1;
	}

	return 0;
}

