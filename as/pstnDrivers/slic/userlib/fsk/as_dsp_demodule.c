/*
 * $Author: lizhijie $
 * $Log: as_dsp_demodule.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1  2004/12/20 03:25:37  lizhijie
 * add DSP engine code into CVS
 *
 * $Revision: 1.1.1.1 $
*/
#include "assist_lib.h"
#include "as_lib_dsp.h"

extern float filtroM(as_fsk_decode_engine *engine,float in);
extern float filtroS(as_fsk_decode_engine *engine, float in);
extern float filtroL(as_fsk_decode_engine *engine,float in);

extern int as_dsp_get_audio_sample(as_dsp_gen_engine *dsp, int fd );

int __get_audio_sample_from_config(as_dsp_gen_engine *dsp, int fd)
{
//	int position;
	int res;
	int len = 0, left = 0;
	as_fsk_decode_engine *engine = &(dsp->fsk_decoder);
	as_tone_data *data = &(dsp->config.voice_data);

#if 1//AS_FSK_DEVICE_OUTPUT
	if(data->bufp < dsp->config.buf_size)
	{
		len = dsp->config.buf_size -data->bufp;
		if(len>AS_DTMF_BLOCKSIZE)
			len = AS_DTMF_BLOCKSIZE;
		memcpy(engine->audio_buf, data->buf + data->bufp, len);
		data->bufp += len;
	}
	
	if(len < AS_DTMF_BLOCKSIZE)
	{
		left = AS_DTMF_BLOCKSIZE - len;
		res = as_dsp_get_audio_sample(dsp, fd);
		if(res)
			return len;
		
		memcpy(engine->audio_buf +len, data->buf, left);
		data->bufp += left;
	}
	return AS_DTMF_BLOCKSIZE;
#else
	len = read(fd, engine->audio_buf, AS_DTMF_BLOCKSIZE);
	return len;
#endif
}


static int as_fsk_get_audio_sample(as_dsp_gen_engine *dsp, int fd, float *retval)
{
	as_fsk_decode_engine *engine = &(dsp->fsk_decoder);

	if (engine->bp==engine->ns) 
	{
		engine->ns=__get_audio_sample_from_config( dsp,  fd);
		engine->bp=0;
		if (engine->ns != AS_DTMF_BLOCKSIZE)
		{
			engine->ns = 0;
			return(-1);
		}
	}
	*retval = (float)(FULLXLAW(engine->audio_buf[engine->bp++], dsp->config.law_type ) / 256);
	return(0);
}

static int as_fsk_demodulator(as_dsp_gen_engine *dsp, int fd, float *retval)
{
	float x,xS,xM;
	as_fsk_decode_engine *engine = &(dsp->fsk_decoder);

	if (as_fsk_get_audio_sample(dsp, fd, &x)) 
		return(-1);
	engine->fskd.cola_in[engine->fskd.pcola]=x;
	
	xS=filtroS(engine,x);
	xM=filtroM(engine,x);

	engine->fskd.cola_filtro[engine->fskd.pcola]=xM-xS;

	x=filtroL(engine, xM*xM - xS*xS);
	
	engine->fskd.cola_demod[engine->fskd.pcola++]=x;
	engine->fskd.pcola &= (NCOLA-1);

	*retval = x;
	return(0);
}


static int get_bit_raw(as_dsp_gen_engine *dsp , int fd)
{
	/* Esta funcion implementa un DPLL para sincronizarse con los bits */
	float x,spb,spb2,ds;
	int f;
	as_fsk_decode_engine *engine = &(dsp->fsk_decoder);
	
	spb=engine->fskd.spb; 
	if (engine->fskd.spb == 7) 
		spb = 8000.0 / 1200.0;
	ds=spb/32.;
	spb2=spb/2.;

	for (f=0;;)
	{
		if (as_fsk_demodulator(dsp, fd, &x)) 
			return(-1);
		if ((x*engine->x0)<0) 
		{	/* Transicion */
			if (!f) 
			{
				if (engine->cont<(spb2)) 
					engine->cont+=ds; 
				else 
					engine->cont-=ds;
				f=1;
			}
		}
		engine->x0=x;
		engine->cont+=1.;
		if (engine->cont>spb) 
		{
			engine->cont-=spb;
			break;
		}
	}
	
	f=(x>0)?0x80:0;
	return(f);
}


int as_fsk_serie(as_dsp_gen_engine *dsp, int fd)
{
	int a;
	float x1,x2;
	int i,j,n1,r;
	as_fsk_decode_engine *engine = &(dsp->fsk_decoder);	
	
	/* Esperamos bit de start	*/
	do 
	{
/* this was jesus's nice, reasonable, working (at least with RTTY) code
to look for the beginning of the start bit. Unfortunately, since TTY/TDD's
just start sending a start bit with nothing preceding it at the beginning
of a transmission (what a LOSING design), we cant do it this elegantly */
/*
		if (demodulador(zap,&x1)) return(-1);
		for(;;) {
			if (demodulador(zap,&x2)) return(-1);
			if (x1>0 && x2<0) break;
			x1=x2;
		}
*/
/* this is now the imprecise, losing, but functional code to detect the
beginning of a start bit in the TDD sceanario. It just looks for sufficient
level to maybe, perhaps, guess, maybe that its maybe the beginning of
a start bit, perhaps. This whole thing stinks! */

		if (as_fsk_demodulator(dsp, fd, &x1)) 
			return(-1);
		for(;;)
		{
			if (as_fsk_demodulator(dsp, fd, &x2)) 
				return(-1);
			if (x2 < -0.5) 
				break; 
		}
		/* Esperamos 0.5 bits antes de usar DPLL */
		i=engine->fskd.spb/2;
		for(;i;i--) 
			if (as_fsk_demodulator(dsp, fd, &x1)) 
				return(-1);	

		/* x1 debe ser negativo (confirmación del bit de start) */

	}while (x1>0);
	
	/* Leemos ahora los bits de datos */
	j=engine->fskd.nbit;
	for (a=n1=0;j;j--) 
	{
		i=get_bit_raw(dsp, fd);
		if (i == -1) 
			return(-1);
		if (i) 
			n1++;
		a>>=1; a|=i;
	}
	
	j=8 - engine->fskd.nbit;
	a>>=j;

	/* Leemos bit de paridad (si existe) y la comprobamos */
	if (engine->fskd.paridad) 
	{
		i=get_bit_raw(dsp, fd); 
		if (i == -1) 
			return(-1);
		if (i) 
			n1++;
		if (engine->fskd.paridad==1) 
		{	/* paridad=1 (par) */
			if (n1&1) 
				a|=0x100;		/* error */
		} 
		else 
		{			/* paridad=2 (impar) */
			if (!(n1&1)) 
				a|=0x100;	/* error */
		}
	}
	
	/* Leemos bits de STOP. Todos deben ser 1 */
	
	for (j=engine->fskd.nstop;j;j--) 
	{
		r = get_bit_raw(dsp, fd);
		if (r == -1) 
			return(-1);
		if (!r) 
			a|=0x200;
	}

	/* Por fin retornamos  */
	/* Bit 8 : Error de paridad */
	/* Bit 9 : Error de Framming */
	
	return a;
}

