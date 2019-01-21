#include "assist_lib.h"

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

typedef struct 
{
	unsigned char buf[AS_DTMF_BLOCKSIZE]; /* fsk write audio buffer */
	int bufp;
}AS_DIAL_DATA;

typedef struct 
{
	float	myci;		/* ci for local (non-fsk) tone generation */
	float	mycr;		/* cr for local (non-fsk) tone generation */
	float	myci1;		/* ci1 for local (non-fsk) tone generation */
	float	mycr1;		/* cr1 for local (non-fsk) tone generation */
}AS_SINE_WAVE_CREATER;

static AS_DIAL_DATA __dtmf_data;


static AS_SINE_WAVE_CREATER __sine ;
#if 0
=
{
	mycr 	:	1.0,	/* initialize cr value for local tone genration */
	myci 	:	0.0,	/* initialize ci value for local tone generation */
	mycr1 	:	1.0,	/* initialize cr value for local tone genration */
	myci1 	:	0.0	/* initialize ci value for local tone generation */
};
#endif

static int __dtmf_generator_initialized  = 0 ;


/* get sine sample value with only 1 frequency */
static float __as_get_localtone( float ddr,float ddi)
{

	float t;
	
	t =__sine.mycr*ddr-__sine.myci*ddi;
	__sine.myci = __sine.mycr*ddi + __sine.myci*ddr;
	__sine.mycr=t;
	
	t=2.0-(__sine.mycr*__sine.mycr+__sine.myci*__sine.myci);
	__sine.mycr*=t;
	__sine.myci*=t;
	
	return __sine.mycr;
};

/* get sine sample value with 2 frequencies */
static float __as_get_localtone_df( float ddr,float ddi,float ddr1, float ddi1)
{

	float t;
	
	t =__sine.mycr*ddr-__sine.myci*ddi;
	__sine.myci=__sine.mycr*ddi+__sine.myci*ddr;
	__sine.mycr=t;
	
	t=2.0-(__sine.mycr*__sine.mycr+__sine.myci*__sine.myci);
	__sine.mycr*=t;
	__sine.myci*=t;
	
	t =__sine.mycr1*ddr1-__sine.myci1*ddi1;
	__sine.myci1=__sine.mycr1*ddi1+__sine.myci1*ddr1;
	__sine.mycr1=t;
	
	t=2.0-(__sine.mycr1*__sine.mycr1+__sine.myci1*__sine.myci1);
	__sine.mycr1*=t;
	__sine.myci1*=t;
	
	return __sine.mycr + __sine.mycr1; 
};


int __as_put_audio_sample(int fd,float y,int flag)
{
#if 0
	int index = (short)rint(8192.0 * y);
	printf(" %04x", index + 32768); 
#endif
	__dtmf_data.buf[__dtmf_data.bufp]=LINEAR2XLAW((short)rint(8192.0 * y), U_LAW_CODE);
	__dtmf_data.bufp++;
	
	if (__dtmf_data.bufp== AS_DTMF_BLOCKSIZE) 
	{
		__dtmf_data.bufp=0;
		if (write(fd, __dtmf_data.buf, AS_DTMF_BLOCKSIZE) != AS_DTMF_BLOCKSIZE)
		{
			if (errno == AS_ELAST) 
				return(-2);
			return(-1);
		}
	}

	/* if not in DTMF mode, ignore */
	if (!flag) 
		return(0);

#if 0	
	  /* get the DTMF */
	i = zap_goertzel_link(zap,zap->fd,readbuffer,  zap->digit_mode & ZAP_MF,&zap->goertzel_last,NULL,1);
	if (i < 0) /* if was an error */
	{   /* if error, return as such */
		if (errno == AS_ELAST) 
			return(-2); 
		else 
			return(-1);
	}
	else if (i) /* if DTMF heard */
	{ /* save the DTMF info */
		zap->dtmf_heard = i;
		return(1);  /* if DTMF hit */
	}
#endif	
	return(0);
}

static void __as_dtmf_coeff_reset()
{
	__sine.mycr = 1.0;	/* initialize cr value for local tone genration */
	__sine.myci = 0.0;	/* initialize ci value for local tone generation */
	__sine.mycr1 = 1.0;	/* initialize cr value for local tone genration */
	__sine.myci1 = 0.0;	/* initialize ci value for local tone generation */
}

static void __as_dtmf_generator_init(void )
{
	__sine.mycr = 1.0;	/* initialize cr value for local tone genration */
	__sine.myci = 0.0;	/* initialize ci value for local tone generation */
	__sine.mycr1 = 1.0;	/* initialize cr value for local tone genration */
	__sine.myci1 = 0.0;	/* initialize ci value for local tone generation */

	__dtmf_data.bufp = 0;
	__dtmf_generator_initialized = 1;
}


/* finish writing out the audio buffer (if any). Returns 0 if not interrupted 
by DTMF, 1 if interrupted by DTMF, or -1 if error, or -2 if hung up . Also
sets zap->nxfer with total number of bytes padded out */
static int __as_finish_audio(int fd,int flag)
{

	if (__dtmf_data.bufp)  /* if something to write */
	{
		while(__dtmf_data.bufp < AS_DTMF_BLOCKSIZE)   /* pad with silence */
		{		
			__dtmf_data.buf[__dtmf_data.bufp] = LINEAR2XLAW(0, U_LAW_CODE); 
			__dtmf_data.bufp++;
		}
		__dtmf_data.bufp = 0;
		  /* write the block */
		if (write(fd, __dtmf_data.buf, AS_DTMF_BLOCKSIZE) != AS_DTMF_BLOCKSIZE)
		{
			if (errno == AS_ELAST) 
				return(-2); /* hangup */
			return(-1);
		}
	}
	
	/* if not in DTMF mode, ignore */
	if (!flag) 
		return(0);
#if 0	
	i = zap_goertzel_link(zap,zap->fd,readbuffer, zap->digit_mode & ZAP_MF,&zap->goertzel_last,NULL,0);
	if (i < 0) /* if was an error */
	{  /* if error, return as such */
		if (errno == AS_ELAST) 
			return(-2); 
		else 
			return(-1);
	}
	else if (i) /* if DTMF heard */
	{ /* save the DTMF info */
		zap->dtmf_heard = i;
		return(1);  /* if DTMF hit */
	}
#endif	
	return(0);
}


/* send arbitrary tone pair */
/* f0, f1 : tone 1/2 freq in hz, len:  length in ms */
/* flag: ZAP_DTMFINT */
static int __as_arbtones(int fd , float f0 , float f1 , int len ,int flag )
{
	float	ddr,ddi,ddr1,ddi1;
	int	i,r;

	ddr = cos(f0*2.0*M_PI/8000.0);
	ddi = sin(f0*2.0*M_PI/8000.0);
	ddr1 = cos(f1*2.0*M_PI/8000.0);
	ddi1 = sin(f1*2.0*M_PI/8000.0);

	__as_dtmf_coeff_reset();
	
	for(i = 0; i < (8 * len); i++) 
	{
		if ((r = __as_put_audio_sample(fd,__as_get_localtone_df(ddr,ddi,ddr1,ddi1) / 2,flag))) 
		{
			return(r);
		}
	}	
	return 0;
}



/* send silence 
 * len :  length in ms 
*/
int _as_lib_silence(int fd ,int len )
{
	int	i,r;
	int flag = 0;
	
	if(!__dtmf_generator_initialized )
		__as_dtmf_generator_init();
	
	for(i = 0; i < (len * 8); i++) 
		if ((r = __as_put_audio_sample(fd,0,flag))) 
			return(r);
	return 0;
}

/* dial a digit string 
 digitstring : digit string to dial in ascii 
 length : length of every tone(char) in ms,default is 100 ms
 */
int as_lib_dial(int fd , char *digitstring ,int length )
{
	int	i, j, len, r;
	AS_DIAL *d;

	if(!__dtmf_generator_initialized )
		__as_dtmf_generator_init();
	
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
		if ((r = __as_arbtones(fd, d[j].f1, d[j].f2, len,0))) 
			return(r); 
		  /* silence too */
		if ((r = _as_lib_silence(fd, len ))) 
			return(r);
	}

	return(__as_finish_audio(fd,0) );  /* finish writing audio */
}

