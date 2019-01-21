/*
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

#define MAX_SIZE 16384
#define CLIP 32635
#define BIAS 0x84

#define LEVEL -10


char *hard_zone_tone_name(int id)
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


static int build_hardtone(FILE  *f, struct tone_zone_sound *t, int *count,int numidtone)
{
	char *dup=NULL;
	char *s=NULL;
	struct as_hard_tone hardtone_def[48];
	int firstnobang = -1;
	int freq1, freq2, time =0 ;
	float coeff1,coeff2;
	int total = 0;
	int i;
	
	dup = strdup(t->data);
	s = strtok(dup, ",");

	while(s && strlen(s)) 
	{
		/* Handle optional ! which signifies don't start here*/
		if (s[0] == '!') 
			{
			    s++;
			}
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

		coeff1=cos(2*M_PI*(freq1 / 8000.0));
		coeff2=cos(2*M_PI*(freq2 / 8000.0));
		
		hardtone_def[total].frequency1 = coeff1 *  32768.0;
		hardtone_def[total].amplitude1= 0.25*sqrt((1-coeff1)/(1+coeff1))*32767.0*0.5;
		hardtone_def[total].initphase1 =0;
		
		hardtone_def[total].frequency2 = coeff2 *  32768.0;
		hardtone_def[total].amplitude2 = 0.25*sqrt((1-coeff2)/(1+coeff2))*32767.0*0.5;
		hardtone_def[total].initphase2 = 0;
                
		if (time) 		
		{/* We should move to the next tone */			
			hardtone_def[total].next = *count + 1;			
			hardtone_def[total].time= time * 8;		
		} 		
		else 		
		{/* Stay with us */			
			hardtone_def[total].next  = *count;			
			hardtone_def[total].time = 8000;		
		}		
		
		(*count)++;
		total++;
		s = strtok(NULL, ",");
	}

	if ( firstnobang>= 0 )
	{		/* If we don't end on a solid tone, return */		
		hardtone_def[total-1].next = firstnobang;	
	}	

	for(i=0; i< total; i++)
	{
		fprintf(f, "\t /* %s */\t{%d, %d, %d, %d, %d, %d, %d, %d, %d } ,\n", 
			hard_zone_tone_name(t->toneid) , 
			hardtone_def[i].frequency1, hardtone_def[i].amplitude1, hardtone_def[i].initphase1,
			hardtone_def[i].frequency2, hardtone_def[i].amplitude2, hardtone_def[i].initphase2,
			hardtone_def[i].next, hardtone_def[i].time,t->toneid);
		/*, tone_def[i].next, tone_def[i].samples,
			tone_def[i].fac1, tone_def[i].init_v2_1, tone_def[i].init_v3_1, 
			tone_def[i].fac2, tone_def[i].init_v2_2, tone_def[i].init_v3_2,
			 tone_def[i].modulate, fre1[i], fre2[i], tone_def[i].samples);
		*/
			 
	}
	return i;
}

int hard_tone_write_zone(FILE *f, struct tone_zone *z)
{
	int res = 0 ;
	int count=0;
	int x;
//	struct zt_tone_def_header h;

	/*
	 * Fill in ring cadence 
	 */
	fprintf(f, "static struct as_ring_cadence hard_ringcadence=\r\n{\r\n"); 
	fprintf(f, "\t\t{%d" ,z->ringcadence[0]);
	for (x=1;x<AS_MAX_CADENCE;x++) 
	{
		fprintf(f, ", %d", z->ringcadence[x]);
	}
	fprintf(f, "}\r\n};/* end of struct as_ringcadence */\r\n\r\n"); 
	 

	fprintf(f, "static struct as_hard_tone hardtonedata[] =\r\n{\r\n");
	for (x=0;x<AS_TONE_MAX;x++) 
	{
		if (strlen(z->tones[x].data)) 
		{/* It's a real tone */
#if DEBUG_TONES
			printf("Tone: %s(%d), string: %s\n", 
				hard_zone_tone_name(z->tones[x].toneid), z->tones[x].toneid, z->tones[x].data);
#endif			
			res = build_hardtone(f, &z->tones[x], &count,x);
			if (res < 0) 
			{
				fprintf(stderr, "Tone not built.\n");
				return -1;
			}
		}
		else 
		{
			fprintf(f,"};");
			break;
		}
	}
	//fprintf(f, "\r\n};/* end of struct as_tone_def */\r\n\r\n"); 

	return res;
}

int main(int argc, char *argv[])
{
	FILE *f=NULL;
	int i,j;
	struct tone_zone *zone=NULL;
	struct tone_zone *zonenext=NULL;

	zone = builtin_zones;
	
	if ((f = fopen("as_zonetone_harddata.h", "w") )!=NULL ) 
	{
		fprintf(f, "/* Tones used by the Assist Telephone Driver, in static tables.\n"
				   "   Generated automatically from program.  Do not edit by hand.  */\n"); 
               for(i=USED_ZONE_INDEX; i<USED_ZONE_INDEX+1;i++)
		{
		       j=i+1;
			zone = &builtin_zones[i];
			zonenext=&builtin_zones[j];
			if(zone->zone!= -1 && i==0)
			{
				printf("***********Zone %s*************\r\n", zone->country);

				fprintf(f, "\r\nstatic char hard_zone_name[]=\r\n{\r\n\t\"%s\"\r\n};\r\n\r\n", zone->description ); 
				hard_tone_write_zone(f, zone);
				fprintf(f, "\r\n" );
				fclose(f);
				
			}	
			else
			{
				if(zone->zone!= -1 && i!=0 &&zonenext->zone!= -1)
				{
					   fprintf(f,"\"%s\",",zone->description);
				}
				else if(zone->zone!= -1 && i!=0 &&zonenext->zone== -1)
				{
					   fprintf(f,"\"%s\"\n}\n",zone->description);
				}
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

