/*
 * $Author: lizhijie $
 * $Log: as_dsp_dtmf_generator.c,v $
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
#include "as_dsp_private.h"

/* Dial frequency tables */
typedef struct
{
	char	chr;	/* character representation */
	float	f1;	/* first freq */
	float	f2;	/* second freq */
}AS_DIAL;

AS_DIAL as_dtmf_dial[] = 
{
	{ '1',697.0,1209.0 },
	{ '4',770.0,1209.0 },
	{ '7',852.0,1209.0 },
	{ '*',941.0,1209.0 },
	{ '2',697.0,1336.0 },
	{ '5',770.0,1336.0 },
	{ '8',852.0,1336.0 },
	{ '0',941.0,1336.0 },
	{ '3',697.0,1477.0 },
	{ '6',770.0,1477.0 },
	{ '9',852.0,1477.0 },
	{ '#',941.0,1477.0 },
	{ 'A',697.0,1633.0 },
	{ 'B',770.0,1633.0 },
	{ 'C',852.0,1633.0 },
	{ 'D',941.0,1633.0 },
	{ 0,0,0 }
} ;
#if 0
AS_DIAL as_mf_dial[] = 
{
	{ '1',700.0,900.0 },
	{ '2',700.0,1100.0 },
	{ '3',900.0,1100.0 },
	{ '4',700.0,1300.0 },
	{ '5',900.0,1300.0 },
	{ '6',1100.0,1300.0 },
	{ '7',700.0,1500.0 },
	{ '8',900.0,1500.0 },
	{ '9',1100.0,1500.0 },
	{ '0',1300.0,1500.0 },
	{ '*',1100.0,1700.0 }, /* KP */
	{ '#',1500.0,1700.0 }, /* ST */
	{ 'A',900.0,1700.0 }, /* ST' */
	{ 'B',1300.0,1700.0}, /* ST'' */
	{ 'C',700.0,1700.0}, /* ST''' */
	{ 0,0,0 }
} ;
#endif

/* dial a digit string 
 digitstring : digit string to dial in ascii 
 length : length of every tone(char) in ms,default is 100 ms
 */
int as_dsp_lib_ring_with_dtmf_caller_id(as_dsp_gen_engine *dsp,  int fd , char *digitstring ,int length )
{
	int	i, j, len, r;
	int res;
	AS_DIAL *d;
	
	if (fd < 0) /* if NULL arg */
	{
		errno= EINVAL;  /* set to invalid arg */
		return(-1);  /* return with error */
	}
	
	if (!digitstring) 
		return(0);
	  /* if not specified, make it default */
	if ( (!length) || (length <0) ) 
		len = AS_DEFAULT_DTMFLENGTH ;
	else 
		len = length;


	/* one ringy-dingy */
	i = AS_RINGOFF;  /* make sure ringer is off */
	ioctl( fd, AS_CTL_HOOK, &i);
	i = AS_RING;  /* ring phone once */
	  /* if error, return as such */
	if(ioctl( fd, AS_CTL_HOOK, &i) == -1) 
		return(-1);

	do /* loop until we get a ringer off or an answer */
	{
		j = as_lib_event_get(fd ); /* wait for an event */
		if (j < 0) /* if error, return as such */
		{
			i = AS_RINGOFF;  /* make sure ringer is off */
			  /* do the ioctl to turn ringer off. if its already off it will ret. error. its okay. */
			ioctl( fd, AS_CTL_HOOK, &i );
			return(-1);
		}			
	}while ((j != AS_EVENT_RINGEROFF) && (j != AS_EVENT_RINGOFFHOOK ));

#if 0
	/* following code only used in FXO device */
	i = AS_RINGOFF; 
	  /* if error, return as such */
	if(ioctl(fd, AS_CTL_HOOK,&i) == -1) 
		return(-1);
	  /* if they interrupted the ring, return here */
	if (j == AS_EVENT_RINGOFFHOOK) 
		return(2);   
#endif

	usleep(500000);  /* sleep at least 1/2 sec */

	for(i = 0; digitstring[i]; i++)
	{
				
		d =  as_dtmf_dial;
		
		for(j = 0; d[j].chr; j++)
		{
			if(d[j].chr == digitstring[i]) 
				break;
		}

		 /* if not found, just skip */
		if (!d[j].chr) 
			continue;

		  /* send a tone */
		if ((r = as_dsp_arbtones(dsp, fd, d[j].f1, d[j].f2, len,0))) 
			return(r); 

		if ( as_lib_event_get(fd) == AS_EVENT_RINGOFFHOOK) 
			return(2);

		  /* silence too */
		if ((r = as_dsp_silence(dsp, fd, len ))) 
			return(r);
		if ( as_lib_event_get(fd) == AS_EVENT_RINGOFFHOOK) 
			return(2);
	}

	res = as_dsp_finish_audio(dsp, fd,0) ;  /* finish writing audio */

	i =  strlen(digitstring)*len*2; /* ms */
	len = RING_OFF_LENGTH - i -500 ;	
	while(len > 0)  /* do rest of ring silence period */
	{
		len --;  /* 125 usec per sample at 8000 */
		  /* output 1 sample of silence */
		i = as_dsp_put_audio_sample(dsp, fd, 0, 0);
		if (i == -1) 
			return(-1); /* return if error */
		if (i != -2) 
			continue;  /* if no event, continue */
		i = as_lib_event_get(fd); /* get the event */
		  /* if answered, get outa here and report as such */
		if (i == AS_EVENT_RINGOFFHOOK) 
			return(1);
	}

	i = AS_START;
	res = ioctl(fd, AS_CTL_HOOK, &i);

	return res;
}

