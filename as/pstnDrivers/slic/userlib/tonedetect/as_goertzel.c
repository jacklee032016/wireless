/* 
 *   DTMF decode module, part of:
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <memory.h>
#include <sys/ioctl.h>
#include <errno.h>

#define AS_CALLER_ID_WITH_MF		1

#define	LINEAR2XLAW(s,law) ((law == 1) ? linear2ulaw(s) : linear2alaw(s)) 
#define	XLAW(c,law) ((law == 1) ? mulaw[c] : alaw[c])
#define	FULLXLAW(c,law) ((law == 1) ? fullmulaw[c] : fullalaw[c])


#define	PI 3.1414926535

#include "as_goertzel.h"

int mulaw[256];
int fullmulaw[256];
int alaw[256];
int fullalaw[256];
int cos2pik[NCOEFF];      /* 2 * cos(2 * PI * k / N), precalculated */
int mcos2pik[MNCOEFF];  /* same for MF */

#define AMI_MASK        0x55 

static inline short int alaw_to_linear (uint8_t alaw)
{
	int i;
	int seg;

	alaw ^= AMI_MASK;
	i = ((alaw & 0x0F) << 4);
	seg = (((int) alaw & 0x70) >> 4);
	if (seg)
		i = (i + 0x100) << (seg - 1);

	return (short int) ((alaw & 0x80)  ?  i  :  -i);
}
/*- End of function --------------------------------------------------------*/

/*--------------------------------------------------------------------*/
void zap_goertzel_init(ZAP *zap)
{
	int i, kp, k;
   /**  Set up conversion table    */
	for(i=0;i<256;i++)
	{
		short mu,e,f,y;
		short etab[]={0,132,396,924,1980,4092,8316,16764};

		mu=255-i;
		e=(mu&0x70)/16;
		f=mu&0x0f;
		y=f*(1<<(e+3));

		y=etab[e]+y;
		if(mu&0x80)
			y=-y;

		fullmulaw[i] = y;
		mulaw[i]= (y >> (15 - AMP_BITS));
	}
	
	for(i = 0; i < 256; i++)
	{
		fullalaw[i] = alaw_to_linear(i);
		alaw[i] = fullalaw[i] >> (15 - AMP_BITS);
	}
	
   /* precalculate 2 * cos (2 PI k / N) */
	kp = 0;
	for(i = 0; i < 6; i++) 
	{
		k = (int)rint((double)mf_tones[i].freq * NPOINTS / SAMPLERATE);
		mcos2pik[kp] = 2 * (32768 * cos(2 * PI * k / NPOINTS));
		mf_tones[i].k = kp++;

		k = (int)rint((double)mf_tones[i].freq * 2 * NPOINTS / SAMPLERATE);
		mcos2pik[kp] = 2 * (32768 * cos(2 * PI * k / NPOINTS));
		mf_tones[i].k2 = kp++;
	}
   
	as_dtmf_detect_init(&zap->udtmf);
}

/*--------------------------------------------------------------------*/
int as_get_buffer(int fd, void *readbuffer, int buffer[], int samples, int blocking, int law, int linear) 
{
	int nread, i,j;
	int *ptr;
	unsigned char *ptr2;
	short		*ptr3;
	
	if (!blocking) 
	{
		j = ZT_IOMUX_READ | ZT_IOMUX_SIGEVENT;
		i = ioctl(fd, ZT_IOMUX, &j);
		if (i < 0) 
			return (-1);
		if (i & ZT_IOMUX_SIGEVENT) 
		{ 
			errno = ELAST; 
			return (-1); 
		}
	}
	nread = read(fd, readbuffer, linear ? samples * 2 : samples);
	
	if (nread < samples) 
		return(-1);

	ptr = buffer;
	ptr2 = readbuffer;
	ptr3 = readbuffer;

	if (linear) 
	{
		for(i = 0; i < samples; i++)
			*(ptr++) = ((*(ptr3++)) / 3) * 2;
	} 
	else 
	{
		for(i = 0; i < samples; i++)
			*(ptr++) = (XLAW(*(ptr2++),law) / 3) * 2;
	}

	return nread;
}


int as_goertzel(int N, int lohi, int *sample, int *cos2pik, int *result) 
{
	int sk, sk1, sk2;
	int k, n;

	if (lohi < 2)
	{
		for(k = 8 * lohi; k < 8 * (lohi + 1); k++) 
		{
			sk = sk1 = sk2 = 0.0;
			for(n = 0; n < N; n++) 
			{
				sk = sample[n] + (((int)(cos2pik[k] * sk1)) >> 15) - sk2;
				sk2 = sk1;
				sk1 = sk;
			}

			result[k] =  ((sk * sk) >> AMP_BITS) 
				- (((((int)(cos2pik[k] * sk)) >> 15) * sk2) >> AMP_BITS) 
				+      ((sk2 * sk2) >> AMP_BITS);
/*    if (result[k] < 0) result[k] = -result[k]; */
		}
	}
	else
	{
		for(k = 0; k < MNCOEFF; k++) 
		{
			sk = sk1 = sk2 = 0.0;
			for(n = 0; n < N; n++) 
			{
				sk = sample[n] + ((cos2pik[k] * sk1) >> 15) - sk2;
				sk2 = sk1;
				sk1 = sk;
			}

			result[k] =  ((sk * sk) >> AMP_BITS) 
				- ((((cos2pik[k] * sk) >> 15) * sk2) >> AMP_BITS) 
				+  ((sk2 * sk2) >> AMP_BITS);
			if (result[k] < 0) 
				result[k] = -result[k];
		}
	}
	return 0;
}

/*--------------------------------------------------------------------*/
int as_goertzel_link(ZAP *zap,int inf, void *readbuffer, int mode, char *last,int *nsamp, int tedge)
{
	int silence, flr[2];
  
	int buffer[NPOINTS];
	int result[NCOEFF];
	short *ubuffer = NULL;
	short udatabuf[NPOINTS];
	void *rp,*rbufp;
	unsigned char *urp;
	int nread,tonems;
	int i,j,retval,m,maybe;
	char what,w1,w2;
	struct zt_confinfo confinfo;

	retval = 0;
	maybe = 0;
	tonems = 0;
	
	for(;;)
	{
		if (zap->digit_mode & ZAP_MUTEMAX)
		{
			rbufp = zap->dbuf;
			memcpy(readbuffer,zap->dbuf,zap->linear ? NPOINTS * 2 : NPOINTS);  /* copy old buffer to read buffer */
		}
		else 
			rbufp = readbuffer;

		if (!mode)  /* if DTMF mode */
		{

			if (!zap->blocking) 
			{
				j = ZT_IOMUX_READ | ZT_IOMUX_SIGEVENT;
				i = ioctl(zap->fd, ZT_IOMUX, &j);
				if (i < 0) 
					return (-1);

				if (i & ZT_IOMUX_SIGEVENT) 
				{ 
					errno = ELAST; 
					return (-1); 
				}
			}

			if (zap->linear)
				nread = read(zap->fd, rbufp, NPOINTS * 2);
			else
				nread = read(zap->fd, rbufp, NPOINTS);

			if (nread < NPOINTS) 
				return(-1);

			rp = rbufp;
			if (zap->linear) 
			{
				ubuffer = rp;
				for(i = 0,j = 0; i < NPOINTS; i++)
				{
					if ((ubuffer[i] < SILENCE_LEVEL) && (ubuffer[i] > -SILENCE_LEVEL)) 
						j++; 
					else 
						j = 0;
				}
			} 
			else 
			{
				urp = (unsigned char *)rp;
				ubuffer = udatabuf;
				for(i = 0,j = 0; i < NPOINTS; i++)
				{
					ubuffer[i] = FULLXLAW(*urp++, zap->law);
					if ((ubuffer[i] < SILENCE_LEVEL) &&  (ubuffer[i] > -SILENCE_LEVEL)) 
						j++; 
					else 
						j = 0;
				}
			}
		}
		else /* else is MF */
		{
			i = as_get_buffer(inf, rbufp, buffer, NPOINTS, zap->blocking, zap->law, zap->linear);
			if (i < 0) 
				return(i);

			for(i = 0,j = 0; i < NPOINTS; i++)
			{
				if ((buffer[i] < SILENCE_LEVEL) && (buffer[i] > -SILENCE_LEVEL)) 
					j++; 
				else 
					j = 0;
			}
		}


		if (j) 
			zap->nsilence += j; 
		else 
			zap->nsilence = 0;
		
    /* if counting, add sample count */
		if (nsamp) 
			*nsamp += NPOINTS; 

		if (!mode) /* if DTMF */
		{
			what = 0;
			w1 = as_lib_dtmf_detect(&zap->udtmf,ubuffer,102,zap->radio);
			w2 = as_lib_dtmf_detect(&zap->udtmf,ubuffer + 102,102,zap->radio);
			if (w1) 
				what = w1; 
			else if (w2) 
				what = w2;
      /* if was different tone last time, and no slience, is trash */
			if ((what) && (*last) && (what != *last)) 
				what = '.'; 
/*      if ((*last != what) && (what != '.') && what) retval = what; */
			if ((what != '.') && what) 
				retval = what; 
/*	printf("@@@@@@@@ what is %d, last is %d\n, retval is %d",
		what,*last,retval); */
			*last = what;
		}

		
		else /* if MF */
		{
			as_goertzel(NPOINTS, 2, buffer, mcos2pik, result);

#ifdef	DISPLAYRESULT
			for(i = 0; i < 6; i++)
				printf("result [%d] is %d %d\n",i,result[i * 2],result[1 + (i * 2)]);
#endif

	    /* get highest */
			for(flr[0] = 0,i = 0; i < 6; i++)
			{
				if (result[mf_tones[i].k] > flr[0])
					flr[0] = result[mf_tones[i].k];
			}
			flr[0] = (flr[0] * 45) / 100;
			if (zap->mflevel) 
			{
				flr[0] = (flr[0] + zap->mflevel) / 2;
			}
		
			if (flr[0] < MF_TRESH) 
				flr[0] = MF_TRESH;
			zap->mflevel = flr[0];
			flr[1] = (flr[0] * 30) / 100;
	
			m = 0;
			silence = 0;
			for(i = 0; i < 6; i++)
			{
				if ((result[mf_tones[i].k] > /* MF_TRESH */ flr[0]) 
					&& 	(result[mf_tones[i].k2] < /* MF_H2_TRESH */ flr[1]) ) 
				{
					m |= 1 << i;
				}
				else if((result[mf_tones[i].k] < SILENCE_TRESH) 
					&&  (result[mf_tones[i].k2] < SILENCE_TRESH))
				silence++;
			}

			if (silence == 6) 
				what = ' ';  /* all is silent */
			else 
			{ /* we have something */
				if (mfs[m] != ' ') 
				{ /* if what we have is valid */
					what = mfs[m]; /* get from decode string */
					if(*last != ' ' && *last != '.')
						*last = what;		/* min. 1 period of non-DTMF between tones */
				} 
				else
					what = '.';
			}

			if((what != *last)&&(what !='.')&&(what !=' '))
			{
#ifdef	DISPLAYOUT
				printf("%c",what);
				fflush(stdout);
#endif
				retval = what;
			}
		}

		*last = what;
		if (retval && (retval == what)) /* if tone decoded */
		{
			memset(&confinfo, 0, sizeof(confinfo));
			if (ioctl(zap->fd,ZT_GETCONF,&confinfo) == -1) 
				return(-1);
			if (zap->linear) 
			{
				memset(readbuffer,0x0,NPOINTS * 2);
				memset(zap->dbuf,0x0,NPOINTS * 2);
			} 
			else 
			{
				memset(readbuffer,0x7f,NPOINTS);
				memset(zap->dbuf,0x7f,NPOINTS);
			}
	
			if ((!maybe) && (zap->digit_mode & ZAP_MUTECONF) /* && confinfo.confno */) 
			{
				i = 1; /* mute off conf */
				if (ioctl(zap->fd,ZT_CONFMUTE,&i) == -1) 
					return(-1);
				maybe = 1;
				if (zap->digit_mode & ZAP_MF) 
					*last = ' '; 
				else 
					*last = 0;
				retval = 0;
				continue;
			}
	
			tonems += (NPOINTS / 8);
			zap->dtmfms = tonems;
			if (tedge == 1) 
				continue; /* wait until end of toneage */
		}
		i = 0;  /* put back on conf */

		if (ioctl(zap->fd,ZT_CONFMUTE,&i) == -1) 
			return(-1);
		if (tedge == 2)
		{
			i = zap->lastgretval;
			zap->lastgretval = retval;
			if ((!i) || retval)
				retval = 0; 
			else 
				retval = i;
		}
		return(retval);
	}
	
}

