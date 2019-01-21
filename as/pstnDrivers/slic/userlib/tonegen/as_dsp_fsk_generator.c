/*
 * $Author: lizhijie $
 * $Log: as_dsp_fsk_generator.c,v $
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
#include <stdlib.h>
#include "as_dsp_private.h"

static float __as_get_carrier(as_dsp_gen_engine *dsp, fsk_carrier_type type)
{
	float t;
	fsk_gen *gen = &(dsp->fsk);
	as_tone_calculator *cal = &(dsp->cal);
	
	t = cal->cr*gen->dr[type] - cal->ci*gen->di[type];
	cal->ci= cal->cr*gen->di[type]+cal->ci*gen->dr[type];
	cal->cr=t;
	
	t=2.0-(cal->cr*cal->cr + cal->ci*cal->ci);
	cal->cr*=t;
	cal->ci*=t;
	
	return cal->cr;
};

/* send out a millisecond of CLID mark tone */
static int __as_fsk_put_a_ms_mark(as_dsp_gen_engine *dsp , int fd)
{
	int	i,r;
	float f;
	

	  /* do 8 samples of it */
	for(i = 0; i < 8; i++)
	{
		f = __as_get_carrier(dsp, AS_FSK_MARK);
		if ((r = as_dsp_put_audio_sample(dsp, fd, f, 0))) 
			return(r);
	}	
	return(0);
}

/* put out a single baud (bit) of FSK data (for CLID) , which is AS_FSK_MARK|SPACE */
static int __as_fsk_put_baud_bit(as_dsp_gen_engine *dsp, int fd, int which)
{
	int r;
	float clidsb;
	float f;

	clidsb = 8000.0 / 1200.0;  /* fractional baud time */
	for(;;)  /* loop until no more samples to send */
	{
		  /* output the sample */
		f = __as_get_carrier(dsp, which);
		if ((r = as_dsp_put_audio_sample(dsp, fd ,f, 0 ))) 
			return(r);
#if 1
		dsp->fsk.scont += 1.0;  /* bump by 1 sample time */
		if (dsp->fsk.scont > clidsb)  /* if greater then 1 baud in real time */
		{
			dsp->fsk.scont -= clidsb;  /* leave remainder around for next one */
			break;
		}
#endif		
	}
	return(0);
}

/* Output a character ( first 8 bit in int 'by'of CLID FSK data */
static int __as_fsk_put_a_char(as_dsp_gen_engine *dsp, int fd,int by)
{
	int j,k,r;
	  /* send start bit */
	if ((r = __as_fsk_put_baud_bit(dsp, fd, AS_FSK_SPACE))) 
		return(r);

	/* send byte */
	for (j=0;j<8;j++) 
	{
		k=by&1;
		by>>=1;
		if ((r = __as_fsk_put_baud_bit(dsp, fd, k))) 
			return(r);
	}

	/* send stop bit */
	if ((r = __as_fsk_put_baud_bit(dsp, fd, AS_FSK_MARK))) 
		return(r);
	return 0;
}


/* ring the phone and send a Caller*ID string. Returns 0 if caller did not
answer during this function, 1 if caller answered after receiving CLID, and
2 if caller answered before receiving CLID */
/* pointer to buffer in which to return caller's number */
/* pointer to buffer in which to return caller's name */
int as_dsp_lib_ring_with_fsk_callerid(as_dsp_gen_engine *dsp, int fd , char *number , char *name )
{
	int	i,j,k,r;
	char	msg[100];
	time_t	t;
	struct	tm *tm;

	if (fd <0 ) /* if NULL arg */
	{
		errno= EINVAL;  /* set to invalid arg */
		return(-1);  /* return with error */
	}
	dsp->fsk.scont = 0.0; /* initialize fractional time accumulator */

	  /* get current time */
	time(&t);
	  /* put into usable form */
	tm = localtime(&t);
	  /* put date into message */
	sprintf(msg,"\001\010%02d%02d%02d%02d",tm->tm_mon + 1,tm->tm_mday, tm->tm_hour,tm->tm_min);
	   /* if number absence */

	if ((!number) || (number[0] == 'O') || (number[0] == 'P'))
	{
		strcat(msg,"\004\001"); /* indicate number absence */
		if (number && (number[0] == 'O')) 
			strcat(msg,"O");
		else 
			strcat(msg,"P");
	}
	else /* if its there */
	{
		i = strlen(number);  /* get len of number */
		if (i > 10) 
			i = 10;  /* limit at 10 */
		sprintf(msg + strlen(msg),"\002%c", i);  /* indicate number sequence */
		j = strlen(msg);
		  /* add number to end of message */
		for(k = 0; k < i; k++) 
			msg[k + j] = number[k];
		
		msg[k + j] = 0;  /* terminate string */
	}
	   /* if name absence */
	if ((!name) || (((name[0] == 'O') || (name[0] == 'P')) && !name[1]))
	{
		strcat(msg,"\010\001"); /* indicate name absence */
		if (name && (name[0] == 'O')) 
			strcat(msg,"O");
		else 
			strcat(msg,"P");
	}
	else /* if its there */
	{
		i = strlen(name);  /* get len of name */
		if (i > 16) 
			i = 16;  /* limit at 16 */
		sprintf(msg + strlen(msg),"\007%c", i );  /* indicate name sequence */
		j = strlen(msg);
		  /* add name to end of message */
		for(k = 0; k < i; k++) 
			msg[k + j] = name[k];
		msg[k + j] = 0;  /* terminate string */
	}
	
#if  AS_FSK_DEVICE_OUTPUT
	/* one ringy-dingy */
	i = AS_RINGOFF;  /* make sure ringer is off */
	ioctl( fd, AS_CTL_HOOK, &i);
	i = AS_RING;  /* ring phone once */
	  /* if error, return as such */
	if(ioctl( fd, AS_CTL_HOOK, &i) == -1) 
		return(-1);
#endif

#if  AS_FSK_DEVICE_OUTPUT
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
#endif

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

	/* output 30 0x55's : 200ms channel used ID */
	for(i = 0; i < 30; i++) 
	{
		r = __as_fsk_put_a_char(dsp, fd, 0x55);
		if (r == -1) 
			return(-1);

#if  AS_FSK_DEVICE_OUTPUT
		if ((r == -2) && ( as_lib_event_get( fd) == AS_EVENT_RINGOFFHOOK) ) 
			return(2);
#endif
	}
	
	/* 150ms of markage */
	for(i = 0; i < 150; i++)
	{
		r = __as_fsk_put_a_ms_mark(dsp, fd);
		if (r == -1) 
			return(-1);
#if  AS_FSK_DEVICE_OUTPUT
		if ((r == -2) && ( as_lib_event_get(fd) == AS_EVENT_RINGOFFHOOK)) 
			return(2);
#endif
	}

	/* put out 0x80 indicting MDMF format : Multi Data Message Format  */
	r = __as_fsk_put_a_char(dsp, fd,128);
	if (r == -1) 
		return(-1);
#if  AS_FSK_DEVICE_OUTPUT
	if ((r == -2) && (as_lib_event_get(fd) == AS_EVENT_RINGOFFHOOK)) 
		return(2);
#endif

	/* put out length of entire message */
	r = __as_fsk_put_a_char(dsp, fd, strlen(msg));
	if (r == -1) 
		return(-1);
#if  AS_FSK_DEVICE_OUTPUT
	if ((r == -2) && ( as_lib_event_get(fd) == AS_EVENT_RINGOFFHOOK)) 
		return(2);
#endif

	/* go thru entire message */
	for(i = 0,j = 0;  msg[i];  i++)
	{
		r = __as_fsk_put_a_char(dsp, fd, msg[i]);  /* output this byte */
		if (r == -1) 
			return(-1);
#if  AS_FSK_DEVICE_OUTPUT
		if ((r == -2) && ( as_lib_event_get( fd) == AS_EVENT_RINGOFFHOOK)) 
			return(2);
#endif
		j += msg[i]; /* update checksum */
	}
	
	j += 128; /* add message type to checksum */
	j += strlen(msg);  /* add message length to checksum */
	r = __as_fsk_put_a_char(dsp, fd,  256 - (j & 255));  /* output checksum (twos-complement) */
	if (r == -1) 
		return(-1);
#if  AS_FSK_DEVICE_OUTPUT
	if ((r == -2) && (as_lib_event_get(fd) == AS_EVENT_RINGOFFHOOK)) 
		return(2);
#endif

	/* send marks for 50 ms to allow receiver to settle */
	for(i = 0; i < 50; i++)
	{
		r = __as_fsk_put_a_ms_mark(dsp, fd);  /* send mark for a millisecond */
		if (r == -1) 
			return(-1); /* return if error */
#if  AS_FSK_DEVICE_OUTPUT
		if ((r == -2) && (as_lib_event_get(fd) == AS_EVENT_RINGOFFHOOK)) 
			return(2);
#endif
	}

	/* finish writing audio */
	i = as_dsp_finish_audio(dsp, fd, 0);
	if (i == -1) 
		return(-1); /* return if error */
#if  AS_FSK_DEVICE_OUTPUT
	if ((i == -2) && ( as_lib_event_get(fd) == AS_EVENT_RINGOFFHOOK)) 
		return(2);
#endif

	/* calculate how long (in microseconds) its been since end of ringy */
	i = ((strlen(msg) + 33) * 8333) + 700000 + (102 * 125); /* half of 204 */
	k = 4000000 - i;	
	while(k > 0)  /* do rest of ring silence period */
	{
		k -= 125;  /* 125 usec per sample at 8000 */
		  /* output 1 sample of silence */
		i = as_dsp_put_audio_sample(dsp, fd, 0, 0);
		if (i == -1) 
			return(-1); /* return if error */
		if (i != -2) 
			continue;  /* if no event, continue */
#if  AS_FSK_DEVICE_OUTPUT
		i = as_lib_event_get(fd); /* get the event */
		  /* if answered, get outa here and report as such */
		if (i == AS_EVENT_RINGOFFHOOK) 
			return(1);
#endif

	}

#if  AS_FSK_DEVICE_OUTPUT
	i = AS_START;
	k = ioctl(fd, AS_CTL_HOOK, &i);
#endif

	return k;
}

