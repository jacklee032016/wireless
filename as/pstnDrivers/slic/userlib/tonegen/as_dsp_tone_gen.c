/*
 * $Author: lizhijie $
 * $Log: as_dsp_tone_gen.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1  2004/12/20 03:27:44  lizhijie
 * add DSP engine and gain code into CVS
 *
 * $Revision: 1.1.1.1 $
*/
#include "assist_lib.h"
#include "as_lib_dsp.h"
#include "as_dsp_private.h"

int as_dsp_play_tone_dial(as_dsp_gen_engine *dsp,int fd)
{
	int res;
	int event;

	while(1)
	{
		res = as_dsp_arbtones(dsp, fd, 350.0, 440.0, 100, 1);
		event  = as_lib_event_get(fd);
		if(event==AS_EVENT_DTMFDIGIT)
		{
			return(as_dsp_finish_audio(dsp, fd,0) );  /* finish writing audio */
		}
	}
	return(as_dsp_finish_audio(dsp, fd,0) );  /* finish writing audio */
}

int as_dsp_play_tone_busy(as_dsp_gen_engine *dsp,int fd)
{
	int res;
	int i;
	int event;

	while(1)
	{
		for(i=0; i<5; i++)
		{
			res = as_dsp_arbtones(dsp, fd, 480.0,620.0, 100, 1);
			event  = as_lib_event_get(fd);
			if(event==AS_EVENT_ONHOOK)
			{
				return(as_dsp_finish_audio(dsp, fd,0) );  /* finish writing audio */
			}
		}	
		for(i=0; i<5; i++)
		{
			res = as_dsp_silence(dsp, fd,  100);
			event  = as_lib_event_get(fd);
			if(event==AS_EVENT_ONHOOK)
			{
				return(as_dsp_finish_audio(dsp, fd,0) );  /* finish writing audio */
			}
		}	

	}

	return(as_dsp_finish_audio(dsp, fd,0) );  /* finish writing audio */
}

int as_dsp_play_tone_ringback(as_dsp_gen_engine *dsp,int fd)
{
	int res;

	while(1)
	{
		res = as_dsp_arbtones(dsp, fd, 440.0,480.0, 2000, 1);
		res = as_dsp_silence(dsp, fd,  4000);

	}
	printf("finished\r\n");
	return(as_dsp_finish_audio(dsp, fd,0) );  /* finish writing audio */

}

