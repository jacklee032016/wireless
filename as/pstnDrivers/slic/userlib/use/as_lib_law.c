#define LAW_AMP_BITS  		9		/* bits in amplitude -- can't be 16, */

#define A_LAW_AMI_MASK 0x55  /* A law constant is 86 */

static short __mu2lin_table[256];
static short __full_mu2lin_table[256];
static short __a2lin_table[256];
static short __full_a2lin_table[256];

#if 0
/* line code has the length of 14 bit with a bit of signed */
static unsigned char __lin2mu_table[16384]; /* 2**14 = 16 K */
static unsigned char __lin2a_table[16384];  
#endif

static int __initialized = 0;

static short int __as_lib_law_alaw2linear (unsigned char alaw)
{
	int i;
	int seg;

	alaw ^= A_LAW_AMI_MASK;
	i = ((alaw & 0x0F) << 4);
 	seg = (((int) alaw & 0x70) >> 4);
	
	if (seg)
		i = (i + 0x100) << (seg - 1);
	return (short int) ((alaw & 0x80)  ?  i  :  -i);
}

/*
** This routine converts from linear to ulaw
** Input: Signed 16 bit linear sample
** Output: 8 bit ulaw sample
*/
#define MU_LAW_ZEROTRAP    /* turn on the trap as per the MIL-STD */
#define MU_LAW_BIAS 0x84   /* define the add-in bias for 16 bit samples */
#define MU_LAW_CLIP 32635/* 8159*4 , 8159 is the value of last segment , less than 2**15=32768  */


/* initialize  ALAW/MULAW and default gain table of rc/tx conversion function tabel */
void  __as_lib_law_init(void)
{
	int i;

	/* Set up mu/a-law to linear conversion table */
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
	        __full_mu2lin_table[i] = y;
		__mu2lin_table[i] = y >> (15 - LAW_AMP_BITS);
		__full_a2lin_table[i] = __as_lib_law_alaw2linear(i);
		__a2lin_table[i] = __full_a2lin_table[i]>> (15 - LAW_AMP_BITS);
	}
#if 0	
	  /* set up the reverse (mu-law) conversion table */
	for(i = -32768; i < 32768; i += 4)
	{
		__lin2mu_table[((unsigned short)(short)i) >> 2] = __as_lib_law_lineartoulaw(i);
		__lin2a_table[((unsigned short)(short)i) >> 2] = __as_lib_law_lineartoalaw(i);
	}
#endif	
	__initialized = 1;
}

/* this function is used to calclate the table of lin2a */
unsigned char   as_lib_law_linear2alaw (short linear)
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
	{/* Sign (7th) bit = 1 , passtive */
		mask = A_LAW_AMI_MASK | 0x80;
	}
	else
	{/* Sign bit = 0 , negative */
		mask = A_LAW_AMI_MASK;
		pcm_val = -pcm_val;
	}

    /* Convert the scaled magnitude to segment number. */
	for (seg = 0;  seg < 8;  seg++)
	{/* which segement it belone to */
		if (pcm_val <= seg_end[seg])
			break;
	}
    /* Combine the sign, segment, and quantization bits. */
	return  ((seg << 4) | ((pcm_val >> ((seg)  ?  (seg + 3)  :  4)) & 0x0F)) ^ mask;
}

/* change a short integer into a char */
unsigned char  as_lib_law_linear2ulaw(short sample)
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
	if (sample > MU_LAW_CLIP) 
		sample = MU_LAW_CLIP;             /* clip the magnitude */

  /* Convert from 16 bit linear to ulaw. */
	sample = sample + MU_LAW_BIAS;
	exponent = exp_lut[(sample >> 7) & 0xFF];
	mantissa = (sample >> (exponent + 3)) & 0x0F;
	ulawbyte = ~(sign | (exponent << 4) | mantissa);
#ifdef MU_LAW_ZEROTRAP
	if (ulawbyte == 0) 
		ulawbyte = 0x02;   /* optional CCITT trap */
#endif
	if (ulawbyte == 0xff) 
		ulawbyte = 0x7f;   /* never return 0xff */
	
	return(ulawbyte);
}

short as_lib_law_ulaw2linear(unsigned char sample)
{
	if(!__initialized )
		__as_lib_law_init();
	return __mu2lin_table[sample];
}

short as_lib_law_full_ulaw2linear(unsigned char sample)
{
	if(!__initialized )
		__as_lib_law_init();
	return __full_mu2lin_table[sample];
}

short as_lib_law_alaw2linear(unsigned char sample)
{
	if(!__initialized )
		__as_lib_law_init();
	return __a2lin_table[sample];
}

short as_lib_law_full_alaw2linear(unsigned char sample)
{
	if(!__initialized )
		__as_lib_law_init();
	return __full_a2lin_table[sample];
}



