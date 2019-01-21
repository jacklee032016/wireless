/*
 * $Author: lizhijie $
 * $Log: as_dsp_filtros.c,v $
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

/* Coeficientes para filtros de entrada					*/
/* Tabla de coeficientes, generada a partir del programa "mkfilter"	*/
/* Formato: coef[IDX_FREC][IDX_BW][IDX_COEF]				*/
/* IDX_COEF=0	=>	1/GAIN						*/
/* IDX_COEF=1-6	=>	Coeficientes y[n]				*/

double coef_in[NF][NBW][8]=
{
#include "coef_in.h"
};

/* Coeficientes para filtro de salida					*/
/* Tabla de coeficientes, generada a partir del programa "mkfilter"	*/
/* Formato: coef[IDX_BW][IDX_COEF]					*/
/* IDX_COEF=0	=>	1/GAIN						*/
/* IDX_COEF=1-6	=>	Coeficientes y[n]				*/

double coef_out[NBW][8]=
{
#include "coef_out.h"
};


/* Filtro pasa-banda para frecuencia de MARCA */
float filtroM(as_fsk_decode_engine *engine,float in)
{
	int i,j;
	double s;
	double *pc;
	
	pc=&coef_in[engine->fskd.f_mark_idx][engine->fskd.bw][0];
	engine->fmxv[(engine->fmp+6)&7]=in*(*pc++);
	
	s=(engine->fmxv[(engine->fmp+6)&7] - engine->fmxv[engine->fmp]) + 3 * (engine->fmxv[(engine->fmp+2)&7] - engine->fmxv[(engine->fmp+4)&7]);
	for (i=0,j=engine->fmp;i<6;i++,j++) 
		s+=engine->fmyv[j&7]*(*pc++);
	engine->fmyv[j&7]=s;
	engine->fmp++; 
	engine->fmp&=7;
	return s;
}

/* Filtro pasa-banda para frecuencia de ESPACIO */
float filtroS(as_fsk_decode_engine *engine, float in)
{
	int i,j;
	double s;
	double *pc;
	
	pc=&coef_in[engine->fskd.f_space_idx][engine->fskd.bw][0];
	engine->fsxv[(engine->fsp+6)&7]=in*(*pc++);
	
	s=(engine->fsxv[(engine->fsp+6)&7] - engine->fsxv[engine->fsp]) + 3 * (engine->fsxv[(engine->fsp+2)&7] - engine->fsxv[(engine->fsp+4)&7]);
	for (i=0,j=engine->fsp;i<6;i++,j++) 
		s+=engine->fsyv[j&7]*(*pc++);
	engine->fsyv[j&7]=s;
	engine->fsp++; 
	engine->fsp&=7;
	return s;
}

/* Filtro pasa-bajos para datos demodulados */
float filtroL(as_fsk_decode_engine *engine,float in)
{
	int i,j;
	double s;
	double *pc;
	
	pc=&coef_out[engine->fskd.bw][0];
	engine->flxv[(engine->flp + 6) & 7]=in * (*pc++); 
	
	s=     (engine->flxv[engine->flp]       + engine->flxv[(engine->flp+6)&7]) +
	  6  * (engine->flxv[(engine->flp+1)&7] + engine->flxv[(engine->flp+5)&7]) +
	  15 * (engine->flxv[(engine->flp+2)&7] + engine->flxv[(engine->flp+4)&7]) +
	  20 *  engine->flxv[(engine->flp+3)&7]; 
	
	for (i=0,j=engine->flp;i<6;i++,j++) 
		s+=engine->flyv[j&7]*(*pc++);
	
	engine->flyv[j&7]=s;
	engine->flp++; 
	engine->flp&=7;
	return s;
}


