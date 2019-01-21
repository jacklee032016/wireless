#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "as_mf_digits.h"

int main(int argc, char *argv[])
{
	FILE *f=NULL;
	float coeff1,coeff2;
	int frequency1,frequency2;
	int amplitude1,amplitude2;
	int initphase1 = 0,initphase2 = 0;
	int time = DEFAULT_DTMF_LENGTH;
	int i=0;

	if ((f = fopen("as_dtmf_hard_tones.h", "w") )!=NULL )
	{
		fprintf(f, "/* dtmf digits used by the Assist Telephone Driver, in static tables.\n"
				   "   Generated automatically from program.  Do not edit by hand.  */\n");
		fprintf(f,"static struct as_tone_array as_si_dtmf_tones[]=\r\n{\r\n");
		
		while( as_dtmf_dial[i].chr !=0)
		{
			coeff1=cos(2*M_PI*(as_dtmf_dial[i].f1/ 8000.0));
        		coeff2=cos(2*M_PI*(as_dtmf_dial[i].f2/ 8000.0));
		
			frequency1 = coeff1 *  32768.0;
			amplitude1= 0.25*sqrt((1-coeff1)/(1+coeff1))*32767.0*0.5;
		
			frequency2 = coeff2 *  32768.0;
			amplitude2 = 0.25*sqrt((1-coeff2)/(1+coeff2))*32767.0*0.5;

			fprintf(f,"	\t/* '%c' */{ %d, %d, %d, %d, %d, %d ,&as_si_dtmf_silence, DEFAULT_DTMF_LENGTH } ,\n",
					as_dtmf_dial[i].chr,
					frequency1, amplitude1, initphase1,
					frequency2, amplitude2, initphase2	);
			i++;
		}
		fprintf(f,"};");
		fclose(f);		
	}
	else 
	{
		fprintf(stderr, "Unable to open as_dtmf_digits.h for writing\n");
		return 1;
	}
	
	return 0;
}
