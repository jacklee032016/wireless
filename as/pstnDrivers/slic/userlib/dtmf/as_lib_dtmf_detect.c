/* 
 *   DTMF Receiver module
 *   
 */

#include "as_lib_dtmf_detect.h"
#include "assist_lib.h"


/* Basic DTMF specs:
 *
 * Minimum tone on = 40ms
 * Minimum tone off = 50ms
 * Maximum digit rate = 10 per second
 * Normal twist <= 8dB accepted
 * Reverse twist <= 4dB accepted
 * S/N >= 15dB will detect OK
 * Attenuation <= 26dB will detect OK
 * Frequency tolerance +- 1.5% will detect, +-3.5% will reject
 */

#define SAMPLE_RATE					8000.0

#define DTMF_THRESHOLD				8.0e7
#define FAX_THRESHOLD				8.0e7
#define FAX_2ND_HARMONIC       		2.0     /* 4dB */
#define DTMF_NORMAL_TWIST			6.3     /* 8dB */
#define DTMF_REVERSE_TWIST		((isradio) ? 4.0 : 2.5)     /* 4dB normal */
#define DTMF_RELATIVE_PEAK_ROW	6.3     /* 8dB */
#define DTMF_RELATIVE_PEAK_COL	6.3     /* 8dB */
#define DTMF_2ND_HARMONIC_ROW	((isradio) ? 1.7 : 2.5)     /* 4dB normal */
#define DTMF_2ND_HARMONIC_COL	63.1    /* 18dB */

static as_tone_detection_descriptor_t dtmf_detect_row[4];
static as_tone_detection_descriptor_t dtmf_detect_col[4];
static as_tone_detection_descriptor_t dtmf_detect_row_2nd[4];
static as_tone_detection_descriptor_t dtmf_detect_col_2nd[4];
static as_tone_detection_descriptor_t fax_detect;
static as_tone_detection_descriptor_t fax_detect_2nd;


static float dtmf_row[] =
{
	697.0,  770.0,  852.0,  941.0
};

static float dtmf_col[] =
{
	1209.0, 1336.0, 1477.0, 1633.0
};

static float fax_freq = 1100.0;

static char dtmf_positions[] = "123A" "456B" "789C" "*0#D";


static as_dtmf_detect_state_t __dtmf_detector;
static int __dtmf_detector_initialized = 0;

static void __as_goertzel_init(as_goertzel_state_t *s, as_tone_detection_descriptor_t *t)
{
	s->v2 =
	s->v3 = 0.0;
	s->fac = t->fac;
}

static void __as_goertzel_update(as_goertzel_state_t *s, int16_t x[], int samples)
{
	int i;
	float v1;
    
	for (i = 0;  i < samples;  i++)
	{
		v1 = s->v2;
		s->v2 = s->v3;
		s->v3 = s->fac*s->v2 - v1 + x[i];
	}
}

static float __as_goertzel_result (as_goertzel_state_t *s)
{
	return s->v3*s->v3 + s->v2*s->v2 - s->v2*s->v3*s->fac;
}

static void __as_dtmf_detect_init (as_dtmf_detect_state_t *s)
{
	int i;
	float theta;

	s->hit1 = 
	s->hit2 = 0;

	for (i = 0;  i < 4;  i++)
	{
		theta = 2.0*M_PI*(dtmf_row[i]/SAMPLE_RATE);
		dtmf_detect_row[i].fac = 2.0*cos(theta);

		theta = 2.0*M_PI*(dtmf_col[i]/SAMPLE_RATE);
		dtmf_detect_col[i].fac = 2.0*cos(theta);

		theta = 2.0*M_PI*(dtmf_row[i]*2.0/SAMPLE_RATE);
		dtmf_detect_row_2nd[i].fac = 2.0*cos(theta);

		theta = 2.0*M_PI*(dtmf_col[i]*2.0/SAMPLE_RATE);
		dtmf_detect_col_2nd[i].fac = 2.0*cos(theta);
    
		__as_goertzel_init (&s->row_out[i], &dtmf_detect_row[i]);
		__as_goertzel_init (&s->col_out[i], &dtmf_detect_col[i]);
		__as_goertzel_init (&s->row_out2nd[i], &dtmf_detect_row_2nd[i]);
		__as_goertzel_init (&s->col_out2nd[i], &dtmf_detect_col_2nd[i]);

		s->energy = 0.0;
	}

	/* Same for the fax dector */
	theta = 2.0*M_PI*(fax_freq/SAMPLE_RATE);
	fax_detect.fac = 2.0 * cos(theta);
	__as_goertzel_init (&s->fax_tone, &fax_detect);

	/* Same for the fax dector 2nd harmonic */
	theta = 2.0*M_PI*(fax_freq * 2.0/SAMPLE_RATE);
	fax_detect_2nd.fac = 2.0 * cos(theta);
	__as_goertzel_init (&s->fax_tone2nd, &fax_detect_2nd);

	s->current_sample = 0;
	s->detected_digits = 0;
	s->lost_digits = 0;
	s->digits[0] = '\0';
	s->mhit = 0;

	__dtmf_detector_initialized = 1;
}

/* return 0 when no DTMF detected; return a int represent the char's ASCII value */
static int __as_lib_dtmf_detect (as_dtmf_detect_state_t *s, int16_t amp[],  int samples, int isradio)
{
	float row_energy[4];
	float col_energy[4];
	float fax_energy;
	float fax_energy_2nd;
	float famp;
	float v1;
	int i;
	int j;
	int sample;
	int best_row;
	int best_col;
	int hit;
	int limit;

	hit = 0;
	for (sample = 0;  sample < samples;  sample = limit)
	{
        /* 102 is optimised to meet the DTMF specs. */
		if ((samples - sample) >= (102 - s->current_sample))
			limit = sample + (102 - s->current_sample);
		else
			limit = samples;

        /* The following unrolled loop takes only 35% (rough estimate) of the 
           time of a rolled loop on the machine on which it was developed */
		for (j = sample;  j < limit;  j++)
		{
			famp = amp[j];
	    
			s->energy += famp*famp;
	    
            /* With GCC 2.95, the following unrolled code seems to take about 35%
               (rough estimate) as long as a neat little 0-3 loop */
			v1 = s->row_out[0].v2;
			s->row_out[0].v2 = s->row_out[0].v3;
			s->row_out[0].v3 = s->row_out[0].fac*s->row_out[0].v2 - v1 + famp;
    
			v1 = s->col_out[0].v2;
			s->col_out[0].v2 = s->col_out[0].v3;
			s->col_out[0].v3 = s->col_out[0].fac*s->col_out[0].v2 - v1 + famp;
    
			v1 = s->row_out[1].v2;
			s->row_out[1].v2 = s->row_out[1].v3;
			s->row_out[1].v3 = s->row_out[1].fac*s->row_out[1].v2 - v1 + famp;

			v1 = s->col_out[1].v2;
			s->col_out[1].v2 = s->col_out[1].v3;
			s->col_out[1].v3 = s->col_out[1].fac*s->col_out[1].v2 - v1 + famp;

			v1 = s->row_out[2].v2;
			s->row_out[2].v2 = s->row_out[2].v3;
			s->row_out[2].v3 = s->row_out[2].fac*s->row_out[2].v2 - v1 + famp;

			v1 = s->col_out[2].v2;
			s->col_out[2].v2 = s->col_out[2].v3;
			s->col_out[2].v3 = s->col_out[2].fac*s->col_out[2].v2 - v1 + famp;

			v1 = s->row_out[3].v2;
			s->row_out[3].v2 = s->row_out[3].v3;
			s->row_out[3].v3 = s->row_out[3].fac*s->row_out[3].v2 - v1 + famp;

			v1 = s->col_out[3].v2;
			s->col_out[3].v2 = s->col_out[3].v3;
			s->col_out[3].v3 = s->col_out[3].fac*s->col_out[3].v2 - v1 + famp;

			v1 = s->col_out2nd[0].v2;
			s->col_out2nd[0].v2 = s->col_out2nd[0].v3;
			s->col_out2nd[0].v3 = s->col_out2nd[0].fac*s->col_out2nd[0].v2 - v1 + famp;

			v1 = s->row_out2nd[0].v2;
			s->row_out2nd[0].v2 = s->row_out2nd[0].v3;

			s->row_out2nd[0].v3 = s->row_out2nd[0].fac*s->row_out2nd[0].v2 - v1 + famp;
        
			v1 = s->col_out2nd[1].v2;
			s->col_out2nd[1].v2 = s->col_out2nd[1].v3;
			s->col_out2nd[1].v3 = s->col_out2nd[1].fac*s->col_out2nd[1].v2 - v1 + famp;

			v1 = s->row_out2nd[1].v2;
			s->row_out2nd[1].v2 = s->row_out2nd[1].v3;
			s->row_out2nd[1].v3 = s->row_out2nd[1].fac*s->row_out2nd[1].v2 - v1 + famp;
        
			v1 = s->col_out2nd[2].v2;
			s->col_out2nd[2].v2 = s->col_out2nd[2].v3;
			s->col_out2nd[2].v3 = s->col_out2nd[2].fac*s->col_out2nd[2].v2 - v1 + famp;
        
			v1 = s->row_out2nd[2].v2;
			s->row_out2nd[2].v2 = s->row_out2nd[2].v3;
			s->row_out2nd[2].v3 = s->row_out2nd[2].fac*s->row_out2nd[2].v2 - v1 + famp;
        
			v1 = s->col_out2nd[3].v2;
			s->col_out2nd[3].v2 = s->col_out2nd[3].v3;
			s->col_out2nd[3].v3 = s->col_out2nd[3].fac*s->col_out2nd[3].v2 - v1 + famp;
        
			v1 = s->row_out2nd[3].v2;
			s->row_out2nd[3].v2 = s->row_out2nd[3].v3;
			s->row_out2nd[3].v3 = s->row_out2nd[3].fac*s->row_out2nd[3].v2 - v1 + famp;

			/* Update fax tone */
			v1 = s->fax_tone.v2;
			s->fax_tone.v2 = s->fax_tone.v3;
			s->fax_tone.v3 = s->fax_tone.fac*s->fax_tone.v2 - v1 + famp;

			v1 = s->fax_tone.v2;
			s->fax_tone2nd.v2 = s->fax_tone2nd.v3;
			s->fax_tone2nd.v3 = s->fax_tone2nd.fac*s->fax_tone2nd.v2 - v1 + famp;
		}
		
		s->current_sample += (limit - sample);
		if (s->current_sample < 102)
			continue;

		/* Detect the fax energy, too */
		fax_energy = __as_goertzel_result(&s->fax_tone);
		
        /* We are at the end of a DTMF detection block */
        /* Find the peak row and the peak column */
		row_energy[0] = __as_goertzel_result (&s->row_out[0]);
		col_energy[0] = __as_goertzel_result (&s->col_out[0]);

		for (best_row = best_col = 0, i = 1;  i < 4;  i++)
		{
			row_energy[i] = __as_goertzel_result (&s->row_out[i]);
			if (row_energy[i] > row_energy[best_row])
				best_row = i;
			col_energy[i] = __as_goertzel_result (&s->col_out[i]);
			if (col_energy[i] > col_energy[best_col])
				best_col = i;
		}

		hit = 0;
        /* Basic signal level test and the twist test */
		if (row_energy[best_row] >= DTMF_THRESHOLD
			&&  col_energy[best_col] >= DTMF_THRESHOLD
			&&  col_energy[best_col] < row_energy[best_row]*DTMF_REVERSE_TWIST
			&&  col_energy[best_col]*DTMF_NORMAL_TWIST > row_energy[best_row])
		{
            /* Relative peak test */
			for (i = 0;  i < 4;  i++)
			{
				if ((i != best_col  &&  col_energy[i]*DTMF_RELATIVE_PEAK_COL > col_energy[best_col])
					||    (i != best_row  &&  row_energy[i]*DTMF_RELATIVE_PEAK_ROW > row_energy[best_row]))
				{
					break;
				}
			}
			
            /* ... and second harmonic test */
			if (i >= 4   
				&& (row_energy[best_row] + col_energy[best_col]) > 42.0*s->energy
                		&&  __as_goertzel_result (&s->col_out2nd[best_col])*DTMF_2ND_HARMONIC_COL < col_energy[best_col]
				&&  __as_goertzel_result (&s->row_out2nd[best_row])*DTMF_2ND_HARMONIC_ROW < row_energy[best_row])
			{
				hit = dtmf_positions[(best_row << 2) + best_col];
                /* Look for two successive similar results */
                /* The logic in the next test is:
                   We need two successive identical clean detects, with
		   something different preceeding it. This can work with
		   back to back differing digits. More importantly, it
		   can work with nasty phones that give a very wobbly start
		   to a digit. */
				if (hit == s->hit3  &&  s->hit3 != s->hit2)
				{
					s->mhit = hit;
					s->digit_hits[(best_row << 2) + best_col]++;
					s->detected_digits++;
					if (s->current_digits < AS_MAX_DTMF_DIGITS)
					{
						s->digits[s->current_digits++] = hit;
						s->digits[s->current_digits] = '\0';
					}
					else
					{
						s->lost_digits++;
					}
				}
			}
		}
		
		if (!hit && (fax_energy >= FAX_THRESHOLD) && (fax_energy > s->energy * 21.0)) 
		{
			fax_energy_2nd = __as_goertzel_result(&s->fax_tone2nd);
			if (fax_energy_2nd * FAX_2ND_HARMONIC < fax_energy) 
			{
#if 0
				printf("Fax energy/Second Harmonic: %f/%f\n", fax_energy, fax_energy_2nd);
#endif					
					/* XXX Probably need better checking than just this the energy XXX */
				hit = 'f';
				s->fax_hits++;
			} /* Don't reset fax hits counter */
		} 
		else 
		{
			if (s->fax_hits > 5) 
			{
				s->mhit = 'f';
				s->detected_digits++;
				if (s->current_digits < AS_MAX_DTMF_DIGITS)
				{
					s->digits[s->current_digits++] = hit;
					s->digits[s->current_digits] = '\0';
				}
				else
				{
					s->lost_digits++;
				}
			}
			s->fax_hits = 0;
		}
		s->hit1 = s->hit2;
		s->hit2 = s->hit3;
		s->hit3 = hit;
		
        /* Reinitialise the detector for the next block */
		for (i = 0;  i < 4;  i++)
		{
			__as_goertzel_init (&s->row_out[i], &dtmf_detect_row[i]);
			__as_goertzel_init (&s->col_out[i], &dtmf_detect_col[i]);
			__as_goertzel_init (&s->row_out2nd[i], &dtmf_detect_row_2nd[i]);
			__as_goertzel_init (&s->col_out2nd[i], &dtmf_detect_col_2nd[i]);
		}

		__as_goertzel_init (&s->fax_tone, &fax_detect);
		__as_goertzel_init (&s->fax_tone2nd, &fax_detect_2nd);
		s->energy = 0.0;
		s->current_sample = 0;
	}

	if ((!s->mhit) || (s->mhit != hit))
	{
		s->mhit = 0;
		return(0);
	}
	return (hit);
}

/* get the detected DTMF string from detect_state structure into buf, and 
* return the length because the detected length may less than max parameter 
*/
static int __as_lib_dtmf_get (as_dtmf_detect_state_t *s, char *buf, int max)
{
	if (max > s->current_digits)
		max = s->current_digits;

	if (max > 0)
	{
		memcpy (buf, s->digits, max);
		memmove (s->digits, s->digits + max, s->current_digits - max);
		s->current_digits -= max;
	}
	buf[max] = '\0';
	return  max;
}

/* return :
	0 : no dtmd caller id detect, 
	>0, numbr of detected caller ID,
	<0, error
 Parameters :
 	length : should be the times of DTMF_DETECT_STEP_SIZE
 	callerID : keep the detected caller ID, must allocate in advanced
 	max : max length of caller ID you want get 
*/
int as_lib_dtmf_detect(int16_t samples[], int length, char *callerId, int max)
{
	int sampleP = 0;
	int res;
	
	if(!__dtmf_detector_initialized)
		__as_dtmf_detect_init(& __dtmf_detector);
	while( (sampleP+DTMF_DETECT_STEP_SIZE) < length)
	{
		__as_lib_dtmf_detect(&__dtmf_detector, samples+sampleP, DTMF_DETECT_STEP_SIZE, 0);
		sampleP += DTMF_DETECT_STEP_SIZE;
	}

	res = __as_lib_dtmf_get(&__dtmf_detector, callerId, max);

	return res;
}

