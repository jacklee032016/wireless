/*
 * $Author: lizhijie $
 * $Log: as_lib_tones.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/12/11 05:41:16  lizhijie
 * modify some header file and compile warning
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_dev_ctl.h"
#include "as_dsp.h"
#include "assist_lib.h"

static int __as_play_tone(int fd, int tone)
{
	int res;

	if( fd <0)
	{
		as_error_msg("Uninit the dev device for dev\n" );
		return -1;
	}	
	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone);
	if (res)
		as_error_msg( "Unable to play tone DIAL tone with ID of '%d'\n" , tone);
	return res;
}

int as_tone_play_stop( int fd ) 
{
	return __as_play_tone( fd ,  AS_TONE_STOP);
}

int as_tone_play_dial( int fd ) 
{
	return __as_play_tone( fd ,  AS_TONE_DIALTONE);
}

int as_tone_play_busy( int  fd ) 
{
	return __as_play_tone( fd ,  AS_TONE_BUSY);
}

int as_tone_play_ring( int  fd ) 
{
	return __as_play_tone( fd,  AS_TONE_RINGTONE);
}

int as_tone_play_congestion( int  fd ) 
{
	return __as_play_tone( fd ,  AS_TONE_CONGESTION);
}

int as_tone_play_callwait( int  fd ) 
{
	return __as_play_tone( fd,  AS_TONE_CALLWAIT );
}

int as_tone_play_dialrecall( int  fd ) 
{
	return __as_play_tone( fd,  AS_TONE_DIALRECALL);
}

int as_tone_play_record(  int  fd  ) 
{
	return __as_play_tone( fd,  AS_TONE_RECORDTONE);
}

int as_tone_play_info(  int  fd  ) 
{
	return __as_play_tone(fd,  AS_TONE_INFO);
}

int as_tone_play_custom_1(  int  fd ) 
{
	return __as_play_tone( fd,  AS_TONE_CUST1);
}

int as_tone_play_custom_2( int  fd ) 
{
	return __as_play_tone(fd,  AS_TONE_CUST2);
}

int as_tone_play_stutter(  int  fd ) 
{
	return __as_play_tone(fd,  AS_TONE_STUTTER);
}


