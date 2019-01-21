#ifndef __AS_LIB_DSP_H__
#define __AS_LIB_DSP_H__

#include <stdint.h>

/* for test : output FSK data into a normal file */
#define AS_FSK_DEVICE_OUTPUT		1

#define FS	8000 

#define NBW	2
#define BWLIST	{75,800}
#define	NF	4
#define	FLIST {1400,1800,1200,2200}

#define NSBUF	1024
#define NCOLA	0x4000

#ifndef	ELAST
#define	ELAST 500
#endif

/* used internally, but needs to be here so that ZAP structure will be valid */
typedef struct 
{
	float spb;	/* Samples / Bit */
	int nbit;	/* Número de bits de datos (5,7,8) */
	float nstop;	/* Número de bits de stop 1,1.5,2  */
	int paridad;	/* Bit de paridad 0=nada 1=Par 2=Impar */
	int hdlc;	/* Modo Packet */

	int bw;		/* Ancho de Banda */

	int f_mark_idx;	/* Indice de frecuencia de marca (f_M-500)/5 */
	int f_space_idx;/* Indice de frecuencia de espacio (f_S-500)/5 */
	
	int pcola;	/* Puntero de las colas de datos */
	float cola_in[NCOLA];		/* Cola de muestras de entrada */
	float cola_filtro[NCOLA];	/* Cola de muestras tras filtros */
	float cola_demod[NCOLA];	/* Cola de muestras demoduladas */
} fsk_data;


typedef struct  
{
	fsk_data  fskd;		/* fsk data */
	int	bp;		/* fsk read buffer pointer */
	int	ns;		/* fsk read silly thingy */
	float	x0,cont;	/* stuff for dpll */
	int	mode;		/* fsk transmit mode */
	int	modo;		/* fsk receive mode */
	unsigned char audio_buf[AS_DTMF_BLOCKSIZE];  /* fsk read audio buffer */
//	int	p;		/* fsk transmit buffer pointer */
	double fmxv[8],fmyv[8];	/* filter stuff for M filter */
	int	fmp;				/* pointer for M filter */
	double fsxv[8],fsyv[8];	/* filter stuff for S filter */
	int	fsp;					/* pointer for S filter */
	double flxv[8],flyv[8];		/* filter stuff for L filter */
	int	flp;					/* pointer for L filter */
//	float lxv[13],lyv[13];	/* filter stuff for low group DTMF filter */
//	float hxv[9],hyv[9];	/* filter stuff for high group DTMF filter */

//	int	nxfer;		/* number of bytes transfered last */
//	unsigned int nsilence;	/* number of contiguous silence samples */
//	int	mflevel;	/* accumulator for MF det. level */
	float	scont;		/* stuff for CLID sending */
//	unsigned short recbuf[ZAP_BLOCKSIZE * 2];
//	int recsamps;

//	int law;

}as_fsk_decode_engine;

#define RING_OFF_LENGTH	4000	/* ms */

typedef enum 
{
	AS_FSK_SPACE = 0,	/* 2200 HZ */
	AS_FSK_MARK = 1	/* 1200 HZ */
}fsk_carrier_type;

typedef struct 
{
	unsigned char *buf; /*[AS_DTMF_BLOCKSIZE];  write audio buffer ,length is get from ioctl */
	int bufp;
}as_tone_data;

typedef struct 
{
	float	ci;		/* ci for local (non-fsk) tone generation */
	float	cr;		/* cr for local (non-fsk) tone generation */
	float	ci1;		/* ci1 for local (non-fsk) tone generation */
	float	cr1;		/* cr1 for local (non-fsk) tone generation */
}as_tone_calculator;

typedef struct 
{/* keep float for space and mark */
	float dr[2];
	float di[2];
	float scont ; /* initialize fractional time accumulator */
}fsk_gen;

typedef struct
{
}dtmf_gen;

typedef struct 
{
	int buf_size;
	int law_type;		
	as_tone_data  	tone_data;
	as_tone_data		voice_data;
}as_dsp_config;

typedef struct
{
	as_dsp_config			config;
	as_tone_calculator		cal;
	fsk_gen				fsk;
	as_fsk_decode_engine	fsk_decoder;
}as_dsp_gen_engine;

as_dsp_gen_engine *as_dsp_init_dsp(int fd, int law );
void as_dsp_detsory_dsp(as_dsp_gen_engine *dsp);

int as_fsk_clid( as_dsp_gen_engine *dsp, int fd, char *number, char *name );


#endif

