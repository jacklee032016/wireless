/*
 * $Author: lizhijie $
 * $Log: as_dsp_fsk_detect.c,v $
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

extern int as_fsk_serie(as_dsp_gen_engine *dsp, int fd);

/*    Baudot Receive translation module  */
static int baudot(as_fsk_decode_engine *engine,unsigned char data)	/* Decodifica el código BAUDOT	*/
{
	static char letras[32]={'<','E','\n','A',' ','S','I','U',
				'\n','D','R','J','N','F','C','K',
				'T','Z','L','W','H','Y','P','Q',
				'O','B','G','^','M','X','V','^'};
	static char simbol[32]={'<','3','\n','-',' ',',','8','7',
				'\n','$','4','\'',',','·',':','(',
				'5','+',')','2','·','6','0','1',
				'9','7','·','^','.','/','=','^'};
	int d;
	d=0;			/* Retorna 0 si el código no es un caracter	*/
	switch (data) 
	{
		case 0x1f :
			engine->modo=0; 
			break;
		case 0x1b : 
			engine->modo=1; 
			break;
		default:	
			if (engine->modo==0) 
				d=letras[data]; 
			else 
				d=simbol[data]; 
			break;
	}
	return d;
}


/*
Get an ASCII character from a TTY/TDD device
    Upon Successful completion, returns character received in ASCII.
    Returns -1 if error, with error in errno, or -2 if user disconnected.
*/
static int __as_fsk_decode_gettdd(as_dsp_gen_engine *dsp, int fd)
{
	int	b,c;
	as_fsk_decode_engine *engine = &(dsp->fsk_decoder);

	for(;;)
	{
		/* get baudot char */
		b = as_fsk_serie(dsp,  fd);
		if (b == -1)
		{
			if (errno == ELAST) 
				return(-2);
			return(-1);
		}
		  /* if null, do it again */
		if (!b) 
			continue;
		  /* if in Caller*ID mode, return with Ascii */
		if (engine->fskd.nbit == 8) 
			return(b);
		  /* if error, do it again */
		if (b > 0x7f) 
			continue;
		c = baudot(engine,b); /* translate to ascii */
		  /* if a valid char */
		if ((c > 0) && (c < 127)) 
			break;
	}
	
	return(c);
}

/* get a Caller*ID string */
/* pointer to channel structure 
 pointer to buffer in which to return caller's number 
 pointer to buffer in which to return caller's name */
int as_fsk_clid( as_dsp_gen_engine *dsp, int fd, char *number, char *name )
{
	int	c,cs,m,n,i,j,rv;
	unsigned char msg[256];
	as_fsk_decode_engine *engine = &(dsp->fsk_decoder);

	if (!engine) /* if NULL arg */
	{
		errno= EINVAL;  /* set to invalid arg */
		return(-1);  /* return with error */
	}
	
//	as_dsp_set_gain(zap->fd, 0, zap->txgain, zap->rxgain + 9.0, zap->law);

#if AS_FSK_DEVICE_OUTPUT	
	do   /* wait for a ring */
	{
		j = as_lib_event_get(fd); /* wait for ring */
		if (j < 0) return(j);  /* return if error */
	} while (j != AS_EVENT_RINGOFFHOOK); /* until ring */
#endif

	/* Caller*ID read values setup */
	/* Valores por defecto */
	engine->fskd.spb=7;	/* 1200 baudios */
	engine->fskd.hdlc=0;		/* Modo asíncrono */
	engine->fskd.nbit=8;		/* Ascii */
	engine->fskd.nstop=1;	/* Bits de Stop */
	engine->fskd.paridad=0;	/* Paridad ninguna */
	engine->fskd.bw=1;		/* Filtro 800 Hz */
	engine->fskd.f_mark_idx=2;	/* f_MARK = 1200 Hz */
	engine->fskd.f_space_idx=3;	/* f_SPACE = 2200 Hz */
	engine->fskd.pcola=0;		/* Colas vacías */
	i = 0; /* reset flag-found flag */
	engine->cont = 0; /* reset DPLL thing */
	rv = 0; /* start with happy return value */
	errno = 0;

	for(;;)
	{
		  /* clear output buffers, if specified */
		if (name) 
			*name = 0;
		if (number) 
			*number = 0;

		m = __as_fsk_decode_gettdd(dsp, fd);  /* get first byte */
		if (m < 0) /* if error, return as such */
		{
			rv = -1;
			break;
		}
		if (m == 'U')  /* if a flag byte, set the flag */
		{
			i = 1; /* got flag */
			continue;
		}
		if (!i) 
			continue; /* if no flag, go on */
		  /* if not message lead-in, go on */
		if ((m != 4) && (m != 0x80)) 
			continue;

		n = __as_fsk_decode_gettdd(dsp, fd); /* get message size */
		if (n < 0)  /* if error, return as such */
		{
			rv = -1;
			break;
		}
		j = m + n; /* initialize checksum with first 2 bytes */
		  /* collect the entire message */
		for(i = 0; i < n; i++)
		{
			c = __as_fsk_decode_gettdd(dsp, fd);  /* get next byte */
			if (c < 0)  /* if error, stop */
			{
				rv = -1;
				break;
			}
			msg[i] = c; /* save it */
			j += c;  /* add it to checksum */
		}
		if (rv == -1) 
			break; /* if error, return as such */
		msg[i] = 0; /* terminate the string */
		
		cs = __as_fsk_decode_gettdd(dsp, fd); /* get checksum byte from message */
		if (cs < 0) /* if error, return as such */
		{
			rv = -1;
			break;
		}
		  /* if bad checksum, try it again */
		if (cs != 256 - (j & 255))
		{
			i = 0; /* reset flag-found flag */
			engine->cont = 0; /* reset DPLL thing */
			continue;  /* do it again */
		}

		if (m == 0x80) /* MDMF format */
		{
			printf("MDMF\r\n");
			for(i = 0; i < n;)  /* go thru whole message */
			{
				switch(msg[i++]) /* message type */
				{
					case 1: /* date */
						break;
					case 2: /* number */
					case 4: /* number */
						if (number) /* if number buffer specified */
						{ /* copy it */
							strncpy(number,msg + i + 1,msg[i]);
							/* terminate it */
							number[msg[i]] = 0;
						}
						break;
				    case 7: /* name */
				    case 8: /* name */
						if (name) /* if name buffer specified */
						{ /* copy it */
							strncpy(name,msg + i + 1,msg[i]);
							/* terminate it */
							name[msg[i]] = 0;
						}
						break;
				}
				/* advance index to next message element */
				i += msg[i] + 1; 
			}
		}
		else /* must be SDMF then */
		{ /* if number buffer specified */
			printf("SDMF\r\n");
			if (number) 
				strcpy(number, msg + 8); /* copy number */
		}
		rv = 0; /* good, happy return */
		break;
	}
	
	/* TDD read values setup */
	/* Valores por defecto */
	engine->fskd.spb=176;	/* 45.5 baudios */
	engine->fskd.hdlc=0;		/* Modo asíncrono */
	engine->fskd.nbit=5;		/* Baudot */
	engine->fskd.nstop=1.5;	/* Bits de Stop */
	engine->fskd.paridad=0;	/* Paridad ninguna */
	engine->fskd.bw=0;		/* Filtro 75 Hz */
	engine->fskd.f_mark_idx=0;	/* f_MARK = 1400 Hz */
	engine->fskd.f_space_idx=1;	/* f_SPACE = 1800 Hz */
	engine->fskd.pcola=0;		/* Colas vacías */
	  /* put original gains back */
//	as_dsp_set_gain(zap->fd, 0, zap->txgain, zap->rxgain, zap->law);
	  /* if hung up */
	if ((rv == -1) && (errno == ELAST)) 
		return(-2);
	
	return(rv); /* return the return value */
}


