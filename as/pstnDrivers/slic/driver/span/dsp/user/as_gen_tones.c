/*
 * $Author: lizhijie $
 * $Log: as_gen_tones.c,v $
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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "as_tonezone.h"

#define DEBUG_TONES	1

#define CLIP 32635
#define BIAS 0x84

#define LEVEL -10

char *tone_zone_tone_name(int id)
{
	static char tmp[80];
	switch(id) 
	{
		case AS_TONE_DIALTONE:
			return "Dialtone";
		case AS_TONE_BUSY:
			return "Busy";
		case AS_TONE_RINGTONE:
			return "Ringtone";
		case AS_TONE_CONGESTION:
			return "Congestion";
		case AS_TONE_CALLWAIT:
			return "Call Waiting";
		case AS_TONE_DIALRECALL:
			return "Dial Recall";
		case AS_TONE_RECORDTONE:
			return "Record Tone";
		case AS_TONE_CUST1:
			return "Custom 1";
		case AS_TONE_CUST2:
			return "Custom 2";
		case AS_TONE_INFO:
			return "Special Information";
		case AS_TONE_STUTTER:
			return "Stutter Dialtone";
	default:
		snprintf(tmp, sizeof(tmp), "Unknown tone %d", id);
		return tmp;
	}
}

static int build_tone(FILE  *f, struct tone_zone_sound *t, int *count)
{
	char *dup, *s;
	struct as_tone_def tone_def[32];
	int firstnobang = -1;
	int freq1, freq2, time =0 ;
	int modulate = 0;
#if AS_PROSLIC_DSP
	float coeff1,coeff2;
#else
	float gain;
#endif
	int total = 0;
	int i;
	
	dup = strdup(t->data);
	s = strtok(dup, ",");

	while(s && strlen(s)) 
	{
		/* Handle optional ! which signifies don't start here*/
		if (s[0] == '!') 
			s++;
		else if (firstnobang < 0) 
		{
			firstnobang = *count;
#if DEBUG_TONES
			printf("First no bang: %s(%d)\n", s, firstnobang );
#endif			
		}

		if (sscanf(s, "%d+%d/%d", &freq1, &freq2, &time) == 3) 
		{/* f1+f2/time format */
#if DEBUG_TONES
			printf("f1+f2/time format: %d, %d, %d\n", freq1, freq2, time);
#endif			
		} 
		else if (sscanf(s, "%d*%d/%d", &freq1, &freq2, &time) == 3) 
		{/* f1*f2/time format */
			modulate = 1;
#if DEBUG_TONES
			printf("f1+f2/time format: %d, %d, %d\n", freq1, freq2, time);
#endif			
		} 
		else if (sscanf(s, "%d+%d", &freq1, &freq2) == 2) 
		{
#if DEBUG_TONES
			printf("f1+f2 format: %d, %d\n", freq1, freq2);
#endif			
			time = 0;
		} 
		else if (sscanf(s, "%d*%d", &freq1, &freq2) == 2) 
		{
			modulate = 1;
#if DEBUG_TONES
			printf("f1+f2 format: %d, %d\n", freq1, freq2);
#endif			
			time = 0;
		} 
		else if (sscanf(s, "%d/%d", &freq1, &time) == 2) 
		{
#if DEBUG_TONES
			printf("f1/time format: %d, %d\n", freq1, time);
#endif			
			freq2 = 0;
		} 
		else if (sscanf(s, "%d", &freq1) == 1) 
		{
#if DEBUG_TONES		
			printf("f1 format: %d\n", freq1);
#endif		
			if(firstnobang<0)
				firstnobang = *count;
			freq2 = 0;
			time = 0;
		} 
		else 
		{
			fprintf(stderr, "tone component '%s' of '%s' is a syntax error\n", s,t->data);
			return -1;
		}
#if DEBUG_TONES
		printf("Using  samples for %d and %d\n",  freq1, freq2);
#endif

#if AS_PROSLIC_DSP
		coeff1=cos(2*M_PI*(freq1 / 8000.0));
		coeff2=cos(2*M_PI*(freq2 / 8000.0));
		
		tone_def[total].fac1 = coeff1 *  32768.0;
		tone_def[total].init_v2_1= 0.25*sqrt((1-coeff1)/(1+coeff1))*32767.0*0.5;
		tone_def[total].init_v3_1 =0;
		
		tone_def[total].fac2 = coeff2 *  32768.0;
		tone_def[total].init_v2_2 = 0.25*sqrt((1-coeff2)/(1+coeff2))*32767.0*0.5;
		tone_def[total].init_v3_2 = 0;

#else
		/* Bring it down -8 dbm */
		gain = pow(10.0, (LEVEL - 3.14) / 20.0) * 65536.0 / 2.0;

		tone_def[total].fac1 = 2.0 * cos(2.0 * M_PI * (freq1 / 8000.0)) * 32768.0;
		tone_def[total].init_v2_1 = sin(-4.0 * M_PI * (freq1 / 8000.0)) * gain;
		tone_def[total].init_v3_1 = sin(-2.0 * M_PI * (freq1 / 8000.0)) * gain;
		
		tone_def[total].fac2 = 2.0 * cos(2.0 * M_PI * (freq2 / 8000.0)) * 32768.0;
		tone_def[total].init_v2_2 = sin(-4.0 * M_PI * (freq2 / 8000.0)) * gain;
		tone_def[total].init_v3_2 = sin(-2.0 * M_PI * (freq2 / 8000.0)) * gain;

#endif
		tone_def[total].modulate = modulate;

		if (time) 		
		{/* We should move to the next tone */			
			tone_def[total].next = *count + 1;			
			tone_def[total].samples = time * 8;		
		} 		
		else 		
		{/* Stay with us */			
			tone_def[total].next = *count;			
			tone_def[total].samples = 8000;		
		}		
		
		tone_def[total].raw_tone.freq1 = freq1 ;
		tone_def[total].raw_tone.freq2 = freq2 ;
		tone_def[total].raw_tone.time = tone_def[total].samples;
		
		(*count)++;
		total++;
		s = strtok(NULL, ",");
	}

	if ( firstnobang>= 0 )
	{		/* If we don't end on a solid tone, return */		
		tone_def[total-1].next = firstnobang;	
	}	

	for(i=0; i< total; i++)
	{
		fprintf(f, "\t /* %s */\t{%d, %d, %d, 2, %d, %d, %d, %d, %d, %d, %d,\r\n\t\t\t{%d, %d, %d} }, \n", 
			tone_zone_tone_name(t->toneid) , 
			t->toneid, tone_def[i].next, tone_def[i].samples,
			tone_def[i].fac1, tone_def[i].init_v2_1, tone_def[i].init_v3_1, 
			tone_def[i].fac2, tone_def[i].init_v2_2, tone_def[i].init_v3_2,
			tone_def[i].modulate, 
			tone_def[i].raw_tone.freq1, tone_def[i].raw_tone.freq2, tone_def[i].raw_tone.time);
	}
	
	return i;
}

int tone_zone_write_zone(FILE *f, struct tone_zone *z)
{
	int res = 0 ;
	int count=0;
	int x;
//	struct zt_tone_def_header h;
	

	/*
	 * Fill in ring cadence 
	 */
	fprintf(f, "static struct as_ring_cadence ringcadence=\r\n{\r\n"); 
	fprintf(f, "\t\t{%d" ,z->ringcadence[0]);
	for (x=1;x<AS_MAX_CADENCE;x++) 
	{
		fprintf(f, ", %d", z->ringcadence[x]);
	}
	fprintf(f, "}\r\n};/* end of struct as_ringcadence */\r\n\r\n"); 
	
	fprintf(f, "static struct as_tone_def tone_defs[] =\r\n{\r\n"); 
	for (x=0;x<AS_TONE_MAX;x++) 
	{
		if (strlen(z->tones[x].data)) 
		{/* It's a real tone */
#if DEBUG_TONES
			printf("Tone: %s(%d), string: %s\n", 
				tone_zone_tone_name(z->tones[x].toneid), z->tones[x].toneid, z->tones[x].data);
#endif			
			res = build_tone(f, &z->tones[x], &count);
			if (res < 0) 
			{
				fprintf(stderr, "Tone not built.\n");
				return -1;
			}
		}
	}
	fprintf(f, "\r\n};/* end of struct as_tone_def */\r\n\r\n"); 

	return res;
}

int main(int argc, char *argv[])
{
	FILE *f;
	int i;
	struct tone_zone *zone;

	zone = builtin_zones;

#ifdef  __ARM__
	if ((f = fopen("as_zone_tones_arm.h", "w"))) 
#else
	if ((f = fopen("as_zone_tones_i686.h", "w"))) 
#endif
	{
		fprintf(f, "/* Tones used by the Assist Telephone Driver, in static tables.\n"
				   "   Generated automatically from program.  Do not edit by hand.  */\n"); 
		for(i=USED_ZONE_INDEX ;i<USED_ZONE_INDEX+1;i++)
		{
			zone = &builtin_zones[i];
			if(zone->zone_id!= -1)
			{
				printf("***********Zone %s*************\r\n", zone->country);

				fprintf(f, "\r\nstatic char zone_name[]=\r\n{\r\n\t\"%s\"\r\n};\r\n\r\n", zone->description ); 
				tone_zone_write_zone(f, zone);
			}	
			else
			{
				fprintf(f, "/* END as_zone_tone_digits.h */\n");
				fclose(f);
				break;
			}
		}	
	} 
	else 
	{
		fprintf(stderr, "Unable to open as_zone_tone_digits.h for writing\n");
		return 1;
	}

	return 0;
}   

