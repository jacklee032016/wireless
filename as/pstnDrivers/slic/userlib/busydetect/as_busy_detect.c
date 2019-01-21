
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include "assist_lib.h"
#include "as_busy_detect.h"

static int __as_dsp_silence(as_tone_detector *dsp, short *s, int len, int *totalsilence)
{
	int accum;
	int x;
	int res = 0;

	if (!len)
		return 0;
	
	accum = 0;
	for (x=0;x<len; x++) 
		accum += abs(s[x]);
	accum /= len;
	printf("accum=%d\n",accum);
	if (accum < dsp->threshold) 
	{
		dsp->totalsilence += len/8;
		if (dsp->totalnoise) 
		{
			/* Move and save history */
			memmove(dsp->historicnoise + DSP_HISTORY - dsp->busycount, dsp->historicnoise + DSP_HISTORY - dsp->busycount +1, dsp->busycount*sizeof(dsp->historicnoise[0]));
			dsp->historicnoise[DSP_HISTORY - 1] = dsp->totalnoise;
/* we don't want to check for busydetect that frequently */

			dsp->busymaybe = 1;
			
		}
		dsp->totalnoise = 0;
		res = 1;
	} 
	else 
	{
		dsp->totalnoise += len/8;
		if (dsp->totalsilence) 
		{
			int silence1 = dsp->historicsilence[DSP_HISTORY - 1];
			int silence2 = dsp->historicsilence[DSP_HISTORY - 2];
			/* Move and save history */
			memmove(dsp->historicsilence + DSP_HISTORY - dsp->busycount, dsp->historicsilence + DSP_HISTORY - dsp->busycount + 1, dsp->busycount*sizeof(dsp->historicsilence[0]));
			dsp->historicsilence[DSP_HISTORY - 1] = dsp->totalsilence;
			/* check if the previous sample differs only by BUSY_PERCENT from the one before it */
			if (silence1 < silence2) 
			{
				if (silence1 + silence1/BUSY_PERCENT >= silence2)
					dsp->busymaybe = 1;
				else 
					dsp->busymaybe = 0;
			} 
			else 
			{
				if (silence1 - silence1/BUSY_PERCENT <= silence2)
					dsp->busymaybe = 1;
				else 
					dsp->busymaybe = 0;
			}
					
		}
		dsp->totalsilence = 0;
	}
	if (totalsilence)
		*totalsilence = dsp->totalsilence;
	return res;
}

static int _as_dsp_busydetect(as_tone_detector *dsp)
{
	int res = 0, x;
	int avgsilence = 0, hitsilence = 0;
	int avgtone = 0, hittone = 0;
         printf("busymay=%d\n",dsp->busymaybe);
	if (!dsp->busymaybe)
		return res;
  
	for (x=DSP_HISTORY - dsp->busycount;x<DSP_HISTORY;x++) 
	{
		avgsilence += dsp->historicsilence[x];
		avgtone += dsp->historicnoise[x];
	}
	avgsilence /= dsp->busycount;
	avgtone /= dsp->busycount;

	for (x=DSP_HISTORY - dsp->busycount;x<DSP_HISTORY;x++) 
	{
		if (avgsilence > dsp->historicsilence[x]) 
		{
			if (avgsilence - (avgsilence / BUSY_PERCENT) <= dsp->historicsilence[x])
				hitsilence++;
		} 
		else 
		{
			if (avgsilence + (avgsilence / BUSY_PERCENT) >= dsp->historicsilence[x])
				hitsilence++;
		}

		if (avgtone > dsp->historicnoise[x]) 
		{
			if (avgtone - (avgtone / BUSY_PERCENT) <= dsp->historicsilence[x])
				hittone++;
		}
		else 
		{
			if (avgtone + (avgtone / BUSY_PERCENT) >= dsp->historicsilence[x])
				hittone++;
		}
	}

	if ((hittone >= dsp->busycount - 1) && (hitsilence >= dsp->busycount - 1) 
		&& (avgtone >= BUSY_MIN && avgtone <= BUSY_MAX) 
		&& (avgsilence >= BUSY_MIN && avgsilence <= BUSY_MAX) ) 
	{


		if (avgtone > avgsilence) 
		{
			if (avgtone - avgtone/(BUSY_PERCENT*2) <= avgsilence)
				res = 1;
		} 
		else 
		{
			if (avgtone + avgtone/(BUSY_PERCENT*2) >= avgsilence)
				res = 1;
		}

	}

	if (res)
		printf( "detected busy, avgtone: %d, avgsilence %d\n", avgtone, avgsilence);
	return res;
}
int as_dsp_busydetect(as_tone_detector *dsp,unsigned char *buff,int length)
{
	int silence;
//	int res;
//	int digit;
	int x;
	int len;
	unsigned short *shortdata;
	unsigned char *odata;
//	int writeback = 0;
	odata = buff;
	len=length;
	printf("law=%d\n",dsp->law);
	switch(dsp->law) 
	{
		case FORMAT_ULAW:
			shortdata = alloca(length * 2);
			if (!shortdata) 
			{
				return -1;
			}
			for (x=0;x<len;x++) 
				shortdata[x] = FULLXLAW(odata[x],1);
			break;
		case FORMAT_ALAW:
			shortdata = alloca(length * 2);
			if (!shortdata) 
			{
				printf( "Unable to allocate stack space for data: %s\n", strerror(errno));
				return  -1;
			}
			for (x=0;x<len;x++) 
				shortdata[x] = FULLXLAW(odata[x],0);
			break;
		/* following code is not test, just for cancel compile warning */	
		default:	
			shortdata = alloca(length);
			if (!shortdata) 
			{
				printf( "Unable to allocate stack space for data: %s\n", strerror(errno));
				return  -1;
			}
			for (x=0;x<len;x++) 
				shortdata[x] = (unsigned short)odata[x];
			break;
	}

	silence = __as_dsp_silence(dsp, shortdata, len, NULL);
	printf("silence =%d\n",silence);
	if(_as_dsp_busydetect(dsp))
		return 1;
		
    return 0;
}

/*init busy tone detect */
void as_dsp_reset(as_tone_detector *dsp)
{
//	int x;
	dsp->totalsilence = 0;

	memset(dsp->historicsilence, 0, sizeof(dsp->historicsilence));
	memset(dsp->historicnoise, 0, sizeof(dsp->historicnoise));
}
as_tone_detector *as_dsp_new(void)
{
	as_tone_detector *dsp;
	dsp = malloc(sizeof(as_tone_detector));
	if (dsp) 
	{
		memset(dsp, 0, sizeof(as_tone_detector));
		dsp->threshold = DEFAULT_THRESHOLD;
		dsp->features = DSP_FEATURE_SILENCE_SUPPRESS;
		dsp->busycount = DSP_HISTORY;
		dsp->law=0;//mulaw:default
		as_dsp_reset(dsp);
	}
	return dsp;
}

void as_dsp_free(as_tone_detector *dsp)
{
	free(dsp);
}


int set_law(as_tone_detector * dsp,int law)
{
	
	if(dsp==NULL)
		return -1;
	dsp->law=law;
	
	return 0;
}
