/*
 * $Author: lizhijie $
 * $Log: as_tel_law.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.4  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.3  2004/12/11 05:05:30  eagle
 * add test_gain.c
 *
 * Revision 1.2  2004/11/29 01:58:27  lizhijie
 * some little bug about channel gain
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"

#define AMI_MASK 0x55  /* A law constant is 86 */

static u_char as_defgain[256];

short __as_mu2lin_table[256];
short __as_a2lin_table[256];

/* line code has the length of 14 bit with a bit of signed */
u_char __as_lin2mu_table[16384]; /* 2**14 = 16 K */
u_char __as_lin2a_table[16384];  

static inline short int __init __as_law_alaw2linear (uint8_t alaw)
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

/*
** This routine converts from linear to ulaw
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) "A New Digital Technique for Implementation of Any
**     Continuous PCM Companding Law," Villeret, Michel,
**     et al. 1973 IEEE Int. Conf. on Communications, Vol 1,
**     1973, pg. 11.12-11.17
** 3) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: Signed 16 bit linear sample
** Output: 8 bit ulaw sample
*/

#define ZEROTRAP    /* turn on the trap as per the MIL-STD */
#define BIAS 0x84   /* define the add-in bias for 16 bit samples */
#define CLIP 32635

/* change a short integer into a char
 * This function is used to calculate the zt_lin2mu table
*/
static unsigned char  __init __as_law_lineartoulaw(short sample)
{
	static int exp_lut[256] = {0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,
                             4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
                             5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
                             5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                             7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
	int sign, exponent, mantissa;
	unsigned char ulawbyte;

  /* Get the sample into sign-magnitude. */
	sign = (sample >> 8) & 0x80;          /* set aside the sign */
	if (sign != 0) 
		sample = -sample;              /* get magnitude */
	if (sample > CLIP) 
		sample = CLIP;             /* clip the magnitude */

  /* Convert from 16 bit linear to ulaw. */
	sample = sample + BIAS;
	exponent = exp_lut[(sample >> 7) & 0xFF];
	mantissa = (sample >> (exponent + 3)) & 0x0F;
	ulawbyte = ~(sign | (exponent << 4) | mantissa);
#ifdef ZEROTRAP
	if (ulawbyte == 0) 
		ulawbyte = 0x02;   /* optional CCITT trap */
#endif
	if (ulawbyte == 0xff) 
		ulawbyte = 0x7f;   /* never return 0xff */
	
	return(ulawbyte);
}

/* this function is used to calclate the table of lin2a */
static inline unsigned char __init  __as_law_lineartoalaw (short linear)
{
    int mask;
    int seg;
    int pcm_val;
    static int seg_end[8] =
    {
         0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF
    };
    
    pcm_val = linear;
    if (pcm_val >= 0)
    {
        /* Sign (7th) bit = 1 */
        mask = AMI_MASK | 0x80;
    }
    else
    {
        /* Sign bit = 0 */
        mask = AMI_MASK;
        pcm_val = -pcm_val;
    }

    /* Convert the scaled magnitude to segment number. */
    for (seg = 0;  seg < 8;  seg++)
    {
        if (pcm_val <= seg_end[seg])
	    break;
    }
    /* Combine the sign, segment, and quantization bits. */
    return  ((seg << 4) | ((pcm_val >> ((seg)  ?  (seg + 3)  :  4)) & 0x0F)) ^ mask;
}


/* initialize  ALAW/MULAW and default gain table of rc/tx conversion function tabel */
void  __init as_law_conv_tables_init(void)
{
	int i;

	/* 
	 *  Set up mu/a-law to linear conversion table
	 */
	for(i = 0;i < 256;i++)
	{
		short mu,e,f,y;
		static short etab[]={0,132,396,924,1980,4092,8316,16764};

		mu = 255-i;
		e = (mu & 0x70)/16;
		f = mu & 0x0f;
		y = f * (1 << (e + 3));
		y += etab[e];
		if (mu & 0x80) y = -y;
	        __as_mu2lin_table[i] = y;
		__as_a2lin_table[i] = __as_law_alaw2linear(i);
		/* Default (0.0 db) gain table */
		as_defgain[i] = i;
	}
	
	  /* set up the reverse (mu-law) conversion table */
	for(i = -32768; i < 32768; i += 4)
	{
		__as_lin2mu_table[((unsigned short)(short)i) >> 2] = __as_law_lineartoulaw(i);
		__as_lin2a_table[((unsigned short)(short)i) >> 2] = __as_law_lineartoalaw(i);
	}
}



/* set the two law tables(lin2xlaw and xlaw2lin ) for this channel  */
void as_channel_set_law(struct as_dev_chan *chan, int law)
{
/* when no law is defined ,eg. defined with ZT_LAW_DEFAULT
* then,we get the law from channel, and then from span's law
*/
	if (!law) 
	{
		if (chan->deflaw)
			law = chan->deflaw;
		else
			if (chan->span) 
				law = chan->span->deflaw;
			else 
				law = AS_LAW_MULAW;
	}
	
	if (law == AS_LAW_ALAW) 
	{
		chan->xlaw = __as_a2lin_table;
		chan->lin2x = __as_lin2a_table;
	} 
	else 
	{
		chan->xlaw = __as_mu2lin_table;
		chan->lin2x = __as_lin2mu_table;
	}
}

void as_channel_set_default_gain(struct as_dev_chan  *chan)
{
	chan->rxgain = as_defgain;
	chan->txgain = as_defgain;
	chan->gainalloc = 0;
}

/* compare the gain of a channel with the default gain. if the two are equal, then free the channel' gain and use the default */
void  as_channel_compare_gain(struct as_dev_chan *chan)
{
	if (!memcmp(chan->rxgain, as_defgain, 256) && 
		    !memcmp(chan->txgain, as_defgain, 256)) 
	{
			/* This is really just a normal gain, so 
			   deallocate the memory and go back to defaults */
			if (chan->gainalloc)
				kfree(chan->rxgain);
			as_channel_set_default_gain( chan);
	}
}

/* check the law of this channel */
int as_channel_check_law(struct as_dev_chan *chan)
{
	if( chan->xlaw == __as_a2lin_table )
		return AS_LAW_ALAW;
	
	return AS_LAW_MULAW;
}

