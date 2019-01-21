/*
 * $Author: lizhijie $
 * $Log: as_lib_tonezone.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:47  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:05  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:52  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:11  fengshikui
 * ÐÞ¸Ä°æ
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

#include "asstel.h"
#include "as_tonezone.h"

#define AS_SPAN_DEV		"/dev/asstel/span"

static struct tone_zone *__as_lib_tone_zone_find(char *country)
{
	struct tone_zone *zone;
	zone = builtin_zones;
	
	while(zone->zone_id > -1) 
	{
		if (!strcasecmp(country, zone->country))
			return zone;
		zone++;
	}
	return NULL;
}

/* fill a group of as_tone_def structure into data from tone_zone_sound */
static int __as_lib_tone_build_tone(char *data, int size, struct tone_zone_sound *t, int *count)
{
	char *dup, *s;
	struct as_tone_def *tone_def=NULL;
#if 0
	struct as_tone_def_header *header  = NULL;
#endif
	int firstnobang = -1;
	int freq1, freq2, time;
	int used = 0;
	int modulate = 0;
	int total = 0;
#if AS_PROSLIC_DSP
	float coeff1,coeff2;
#else
	float gain;
#endif

#if 0
	header = (struct as_tone_def_header *)(data -sizeof(struct as_tone_def_header)- (*count)*sizeof(struct as_tone_def)  ) ;
	printf("Name=%s\r\n", header->name);
	sprintf(header->name, "asd");
#endif	
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

		if (size < sizeof(struct as_tone_def)) 
		{
			fprintf(stderr, "Not enough space for samples\n");
			return -1;
		}		
		tone_def = (struct as_tone_def *)data;
		
#if AS_PROSLIC_DSP
		coeff1=cos(2*M_PI*(freq1 / 8000.0));
		coeff2=cos(2*M_PI*(freq2 / 8000.0));
		
		tone_def->fac1 = coeff1 *  32768.0;
		tone_def->init_v2_1= 0.25*sqrt((1-coeff1)/(1+coeff1))*32767.0*0.5;
		tone_def->init_v3_1 =0;
		
		tone_def->fac2 = coeff2 *  32768.0;
		tone_def->init_v2_2 = 0.25*sqrt((1-coeff2)/(1+coeff2))*32767.0*0.5;
		tone_def->init_v3_2 = 0;

#else
		/* Bring it down -8 dbm */
		gain = pow(10.0, (LEVEL - 3.14) / 20.0) * 65536.0 / 2.0;

		tone_def->fac1 = 2.0 * cos(2.0 * M_PI * (freq1 / 8000.0)) * 32768.0;
		tone_def->init_v2_1 = sin(-4.0 * M_PI * (freq1 / 8000.0)) * gain;
		tone_def->init_v3_1 = sin(-2.0 * M_PI * (freq1 / 8000.0)) * gain;
		
		tone_def->fac2 = 2.0 * cos(2.0 * M_PI * (freq2 / 8000.0)) * 32768.0;
		tone_def->init_v2_2 = sin(-4.0 * M_PI * (freq2 / 8000.0)) * gain;
		tone_def->init_v3_2 = sin(-2.0 * M_PI * (freq2 / 8000.0)) * gain;

#endif
		tone_def->modulate = modulate;
		tone_def->tone_id = t->toneid;
		if (time) 		
		{/* We should move to the next tone */			
			tone_def->next = *count + 1;			
			tone_def->samples = time * 8;		
		} 		
		else 		
		{/* Stay with us */			
			tone_def->next = *count;			
			tone_def->samples = 8000;		
		}		
		
		tone_def->raw_tone.freq1 = freq1 ;
		tone_def->raw_tone.freq2 = freq2 ;
		tone_def->raw_tone.time = tone_def->samples;

		
		data += (sizeof(struct as_tone_def));
		used += (sizeof(struct as_tone_def));
		size -= (sizeof(struct as_tone_def));
		
		(*count)++;
		total++;
		s = strtok(NULL, ",");
#if 0
		printf("tone address=%ld, freq1=%d freq2=%d time=%d\r\n", (long)tone_def, tone_def->raw_tone.freq1,tone_def->raw_tone.freq2, tone_def->raw_tone.time);
#endif
	}

	if ( firstnobang>= 0 )
	{		/* If we don't end on a solid tone, return */		
		tone_def->next = firstnobang;	
	}	
//	printf("used =%d\r\n" ,used);
	return used;
}


static int __as_lib_tone_register_zone( struct tone_zone *zone)
{
	char buf[MAX_SIZE];
	int res;
	int count=0;
	int x;
	int used = 0;
	int space = MAX_SIZE;
	int fd;
	struct as_tone_def_header *h;
	char *ptr = buf;
	memset(buf, 0, MAX_SIZE);
	
	fd = open( AS_SPAN_DEV, O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open '%s' and fd not provided\n", AS_SPAN_DEV );
		return -1;
	}
	
	h = (struct as_tone_def_header *)ptr;
	ptr += sizeof(struct as_tone_def_header);
	space -= sizeof(struct as_tone_def_header);
	used += sizeof(struct as_tone_def_header);

	for (x=0;x<AS_MAX_CADENCE;x++) 
		h->ringcadence[x] = zone->ringcadence[x];
	h->zone_id = zone->zone_id;
	strncpy(h->name, zone->description, sizeof(h->name));

	/* Put in an appropriate method for a kernel ioctl */
	for (x=0;x<AS_TONE_MAX;x++) 
	{
		if (strlen(zone->tones[x].data)) 
		{
			/* It's a real tone */
#if 0
			printf("Tone: %d, string: %s\n", z->tones[x].toneid, z->tones[x].data);
#endif			
			res = __as_lib_tone_build_tone(ptr, space, &zone->tones[x], &count);
			if (res < 0) 
			{
				fprintf(stderr, "Tone not built.\n");
				close(fd);
				return -1;
			}
			ptr += res;
			used += res;
			space -= res;
		}
	}
	
	h->count = count;
#if 1
	{
		int j;
		struct as_tone_def *tone ;
		printf("count=%d name =%s\r\n", count,h->name );
		for(j=0; j<count; j++)
		{
			tone = (struct as_tone_def *)(buf+sizeof(struct as_tone_def_header) +j*sizeof(struct as_tone_def)  );
			printf("index=%d tone id=%d\r\n", j, tone->tone_id );
			printf("fac1=%d fac2=%d time=%d ",  tone->fac1, tone->fac2, tone->samples);
			printf("freq1=%d freq2=%d time=%d\r\n",  tone->raw_tone.freq1, tone->raw_tone.freq2, tone->raw_tone.time);
			
		}
	}
#endif
	res = ioctl(fd, AS_CTL_LOADZONE, h);
	if (res) 
		fprintf(stderr, "ioctl(AS_CTL_LOADZONE) failed: %s\n", strerror(errno) );

	close(fd);
	return res;
}

char *as_lib_tone_name(int id)
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

/* refer to tonezone_data file for the key words of country*/
int as_lib_set_zone( char *country)
{
	int res=-1;
	struct tone_zone *zone;
	
	zone = __as_lib_tone_zone_find(country);
	if (zone)
	{
		res = __as_lib_tone_register_zone( zone );
	}
	else
	{
		fprintf(stderr, "No zone tone data with name of '%s' is found\r\n" , country);
	}	
	
	return res;
}

int as_lib_set_zone_japan()
{
	return as_lib_set_zone("jp");
}

