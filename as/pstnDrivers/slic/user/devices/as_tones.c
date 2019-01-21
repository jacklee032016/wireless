#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_global.h"

static int __as_play_tone(as_device_t *dev, int tone)
{
	int res;

	if( dev->fd <0)
	{
		as_error_msg("Uninit the dev device for dev %s\n", dev->name );
		return -1;
	}	
	
	res = ioctl( dev->fd, AS_CTL_SENDTONE, &tone);
//	sleep(2);
	if (res)
		as_error_msg( "Unable to play tone DIAL tone\n" );
	return res;
}

int as_tone_play_stop( as_device_t *dev ) 
{
	return __as_play_tone(dev,  AS_TONE_STOP);
}

int as_tone_play_dial( as_device_t *dev ) 
{
	return __as_play_tone(dev ,  AS_TONE_DIALTONE);
}

int as_tone_play_busy( as_device_t *dev ) 
{
	return __as_play_tone( dev ,  AS_TONE_BUSY);
}

int as_tone_play_ring( as_device_t *dev ) 
{
	return __as_play_tone( dev,  AS_TONE_RINGTONE);
}

int as_tone_play_congestion( as_device_t *dev ) 
{
	return __as_play_tone( dev ,  AS_TONE_CONGESTION);
}

int as_tone_play_callwait( as_device_t *dev ) 
{
	return __as_play_tone( dev,  AS_TONE_CALLWAIT );
}

int as_tone_play_dialrecall( as_device_t *dev ) 
{
	return __as_play_tone( dev,  AS_TONE_DIALRECALL);
}

int as_tone_play_record( as_device_t *dev ) 
{
	return __as_play_tone( dev,  AS_TONE_RECORDTONE);
}

int as_tone_play_info( as_device_t *dev ) 
{
	return __as_play_tone(dev,  AS_TONE_INFO);
}

int as_tone_play_custom_1( as_device_t *dev ) 
{
	return __as_play_tone(dev,  AS_TONE_CUST1);
}

int as_tone_play_custom_2( as_device_t *dev ) 
{
	return __as_play_tone(dev,  AS_TONE_CUST2);
}

int as_tone_play_stutter( as_device_t *dev ) 
{
	return __as_play_tone(dev,  AS_TONE_STUTTER);
}


