/*
 * $Author: lizhijie $
 * $Log: as_dsp_utils.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/04/20 03:05:03  lizhijie
 * no message
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
#include <stdlib.h>

float as_dsp_get_localtone(as_dsp_gen_engine *dsp, float ddr,float ddi)
{
	as_tone_calculator *cal = &(dsp->cal);
	float t;
	
	t =cal->cr*ddr-cal->ci*ddi;
	cal->ci = cal->cr*ddi + cal->ci*ddr;
	cal->cr=t;
	
	t=2.0-(cal->cr*cal->cr+cal->ci*cal->ci);
	cal->cr*=t;
	cal->ci*=t;
	
	return cal->cr;
};

/* get sine sample value with 2 frequencies */
float as_dsp_get_localtone_df(as_dsp_gen_engine *dsp, float ddr,float ddi,float ddr1, float ddi1)
{
	as_tone_calculator *cal = &(dsp->cal);
	float t;
	
	t =cal->cr*ddr-cal->ci*ddi;
	cal->ci=cal->cr*ddi+cal->ci*ddr;
	cal->cr=t;
	
	t=2.0-(cal->cr*cal->cr+cal->ci*cal->ci);
	cal->cr*=t;
	cal->ci*=t;
	
	t =cal->cr1*ddr1-cal->ci1*ddi1;
	cal->ci1=cal->cr1*ddi1+cal->ci1*ddr1;
	cal->cr1=t;
	
	t=2.0-(cal->cr1*cal->cr1+cal->ci1*cal->ci1);
	cal->cr1*=t;
	cal->ci1*=t;
	
	return cal->cr + cal->cr1; 
};


int as_dsp_put_audio_sample(as_dsp_gen_engine *dsp, int fd,float y,int flag)
{
	as_tone_data *data = &(dsp->config.tone_data);
	
	data->buf[data->bufp]=LINEAR2XLAW((short)rint(8192.0 * y), dsp->config.law_type /*U_LAW_CODE*/);
	data->bufp++;
	
	//	printf("count =%d bufp =%d\r\n" ,count, data->bufp);
	if (data->bufp== dsp->config.buf_size/*AS_DTMF_BLOCKSIZE*/) 
	{
		data->bufp=0;
		if (write(fd, data->buf, dsp->config.buf_size) != dsp->config.buf_size)
		{
			printf("write length is error\r\n");
			if (errno == AS_ELAST) 
				
				return(-2);
			return(-1);
		}
	}

	return(0);
}

int as_dsp_get_audio_sample(as_dsp_gen_engine *dsp, int fd )
{
	int len;
	as_tone_data *data = &(dsp->config.voice_data);
	
	//	printf("count =%d bufp =%d\r\n" ,count, data->bufp);
	if (data->bufp== dsp->config.buf_size/*AS_DTMF_BLOCKSIZE*/) 
	{
		data->bufp=0;
		len = read(fd, data->buf, dsp->config.buf_size);
		if ( len != dsp->config.buf_size)
		{
			printf("READ length is error, should be %d bytes, but only %d bytes\r\n", dsp->config.buf_size, len);
			if (errno == AS_ELAST) 
				return(-2);
			return(-1);
		}
	}
	else
	{
		printf("ALERT : Voice data from device is not used up, left %d bytes now\r\n", 
			dsp->config.buf_size - data->bufp);
		return -1;
	}

	return(0);
}


void as_dsp_coeff_reset(as_dsp_gen_engine *dsp)
{
	as_tone_calculator *cal = &(dsp->cal);
	cal->cr = 1.0;	/* initialize cr value for local tone genration */
	cal->ci = 0.0;	/* initialize ci value for local tone generation */
	cal->cr1 = 1.0;	/* initialize cr value for local tone genration */
	cal->ci1 = 0.0;	/* initialize ci value for local tone generation */
}


int as_dsp_arbtones(as_dsp_gen_engine *dsp, int fd , float f0 , float f1 , int len ,int flag )
{
	float	ddr,ddi,ddr1,ddi1;
	int	i,r;

	ddr = cos(f0*2.0*M_PI/8000.0);
	ddi = sin(f0*2.0*M_PI/8000.0);
	ddr1 = cos(f1*2.0*M_PI/8000.0);
	ddi1 = sin(f1*2.0*M_PI/8000.0);

	as_dsp_coeff_reset(dsp);

	for(i = 0; i < (8 * len); i++) 
	{
		if ((r = as_dsp_put_audio_sample(dsp, fd, as_dsp_get_localtone_df(dsp, ddr,ddi,ddr1,ddi1) / 2,flag))) 
		{
			return(r);
		}
	}	
	return 0;
}


/* finish writing out the audio buffer (if any). Returns 0 if not interrupted 
by DTMF, 1 if interrupted by DTMF, or -1 if error, or -2 if hung up . Also
sets zap->nxfer with total number of bytes padded out */
int as_dsp_finish_audio(as_dsp_gen_engine *dsp, int fd,int flag)
{
	as_tone_data *data = &(dsp->config.tone_data);
	
	if ( data->bufp)  /* if something to write */
	{
		while( data->bufp < dsp->config.buf_size /*AS_DTMF_BLOCKSIZE*/)   /* pad with silence */
		{		
			 data->buf[ data->bufp] = LINEAR2XLAW(0, dsp->config.law_type /*U_LAW_CODE*/); 
			 data->bufp++;
		}
		data->bufp = 0;
		  /* write the block */
		if (write(fd, data->buf, dsp->config.buf_size) != dsp->config.buf_size)
		{
			if (errno == AS_ELAST) 
				return(-2); /* hangup */
			return(-1);
		}
	}
	
	/* if not in DTMF mode, ignore */
	return(0);
}

/* send silence 
 * len :  length in ms 
*/
int as_dsp_silence(as_dsp_gen_engine *engine, int fd ,int len )
{
	int	i,r;
	int flag = 0;
	
	for(i = 0; i < (len * 8); i++) 
		if ((r = as_dsp_put_audio_sample(engine, fd, 0, flag))) 
			return(r);
	return 0;
}

/* return 0, success, others, failed , law , 0 :a-law, 1 : mu-law */
int as_dsp_init_config(as_dsp_config *config, int fd, int law )
{
#if 0
	struct as_bufferinfo info;
	int res;
#endif
	
	memset(config, 0, sizeof(as_dsp_config) );

#if 0
	res = ioctl( fd, AS_CTL_GET_BUFINFO, &info);
	if(res<0)
	{
		printf("error in get bufinfo, this file descriptor is not a device interface : %s\r\n", strerror(errno) );
		return res;
	}
#endif

	if(law)
		config->law_type = U_LAW_CODE;
	else
		config->law_type = A_LAW_CODE;

	config->buf_size = 160;//info.bufsize;
	config->tone_data.buf = (unsigned char *)malloc(config->buf_size);
	if(!config->tone_data.buf)
	{
		printf("No critical memory %s", __FUNCTION__);
		exit(1);
	}
	config->voice_data.buf = (unsigned char *)malloc(config->buf_size);
	if(!config->voice_data.buf)
	{
		printf("No critical memory %s", __FUNCTION__);
		exit(1);
	}
	return 0;
}

/* law : A_LAW_CODE(0) or U_LAW_CODE(1) */
as_dsp_gen_engine *as_dsp_init_dsp(int fd, int law )
{
#if 0
	float	f0,f1;
/* Global variables set up for the fsk decoding / encoding */
int spb,spb1;  /* these dont change, so they are okay as globals */
#endif
	float fsk_space_f,fsk_mark_f;
	as_dsp_gen_engine *dsp;
	int res;
#if 0	
	f0=1800.0;   /* space tone for TDD generation */
	f1=1400.0;   /* mark tone for TDD generation */
	spb = 176;
	spb1 = 264;
#endif	
	dsp = (as_dsp_gen_engine *)malloc(sizeof(as_dsp_gen_engine) );
	if(!dsp)
	{
		printf("No critical memory %s", __FUNCTION__);
		exit(1);
	}
	memset(dsp, 0, sizeof(as_dsp_gen_engine) );

	res = as_dsp_init_config(&(dsp->config), fd, law);
	if(res)
	{
		free(dsp);
		return NULL;
	}
		
	fsk_space_f=2200.0;   /* space tone for CLID generation */
	fsk_mark_f=1200.0;   /* mark tone for CLID generation */
	dsp->fsk.dr[0]=cos(fsk_space_f*2.0*M_PI/8000.0);
	dsp->fsk.di[0]=sin( fsk_space_f*2.0*M_PI/8000.0);
	dsp->fsk.dr[1]=cos(fsk_mark_f*2.0*M_PI/8000.0);
	dsp->fsk.di[1]=sin(fsk_mark_f*2.0*M_PI/8000.0);

	dsp->cal.ci = 1.0;
	dsp->cal.cr = 0.0;
	dsp->cal.ci1 = 1.0;
	dsp->cal.cr1 = 0.0;

	dsp->fsk_decoder.bp = 0;		/* fsk rx buffer pointer */
	dsp->fsk_decoder.ns = 0;		/* silly fsk read buffer size */
	dsp->fsk_decoder.x0 = 0.0;		/* stuff for dpll */
	dsp->fsk_decoder.cont = 0.0;	/* stuff for dpll */
	dsp->fsk_decoder.scont = 0.0;	/* stuff for clid sending */

//	dsp->fsk_decoder.fskd
	dsp->fsk_decoder.fskd.spb=176;	/* 45.5 baudios */
	dsp->fsk_decoder.fskd.hdlc=0;		/* Modo asíncrono */
	dsp->fsk_decoder.fskd.nbit=5;		/* Baudot */
	dsp->fsk_decoder.fskd.nstop=1.5;	/* Bits de Stop */
	dsp->fsk_decoder.fskd.paridad=0;	/* Paridad ninguna */
	dsp->fsk_decoder.fskd.bw=0;		/* Filtro 75 Hz */
	dsp->fsk_decoder.fskd.f_mark_idx=0;	/* f_MARK = 1400 Hz */
	dsp->fsk_decoder.fskd.f_space_idx=1;	/* f_SPACE = 1800 Hz */
	dsp->fsk_decoder.fskd.pcola=0;		/* Colas vacías */

	return dsp;
}


void as_dsp_detsory_dsp(as_dsp_gen_engine *dsp)
{
	if(dsp->config.tone_data.buf)
	{
		free(dsp->config.tone_data.buf);
		dsp->config.tone_data.buf = NULL;
	}
	if(dsp->config.voice_data.buf)
	{
		free(dsp->config.voice_data.buf);
		dsp->config.voice_data.buf = NULL;
	}

	free(dsp);
	dsp = NULL;
}

