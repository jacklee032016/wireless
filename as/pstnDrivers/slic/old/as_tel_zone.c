#include "asstel.h"

#include "as_digits.h"

#include "as_zone_tone_digits.h"

/* No bigger than 32k for everything per tone zone */
#define MAX_SIZE 32768
/* No more than 128 subtones */
#define MAX_TONES 128


/* get a as_tone for a DTMF digit */
/* this is a symbol exported by TONE driver and can be used by other drier */
static struct as_tone *__as_zone_get_dtmf_tone( struct as_zone *zone, unsigned char digit )
{
	struct as_tone *tone = zone->dtmf_tones;

	switch(digit) 
	{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			printk("DTMF digital '%c'\r\n" ,digit);
			return tone + (int)(digit - '0');
		case '*':
			return tone + 10;
		case '#':
			return tone + 11;
		case 'A':
		case 'B':
		case 'C':
			return tone + (digit + 12 - 'A');
		case 'D':
			return tone + ( digit + 12 - 'A');
		case 'a':
		case 'b':
		case 'c':
			return tone + (digit + 12 - 'a');
		case 'd':
			return tone + ( digit + 12 - 'a');
		case 'W':
		case 'w':
		default:	
			return zone->pause_tone;
	}
	return NULL;
}

static struct as_raw_tone *__as_zone_get_dtmf_raw_tone( struct as_zone *zone, unsigned char digit )
{
	struct as_tone *tmp = NULL;

	tmp = __as_zone_get_dtmf_tone( zone,  digit);
	if(tmp)
		return &(tmp->raw_tone);
	return NULL;
}

static struct as_tone *__as_zone_get_tone( struct as_zone *zone, as_tone_type_t type)
{
	if(type >AS_MAX_CADENCE || type<AS_TONE_STOP)
	{
		printk("Tone ID is out of range in %s\r\n", __FUNCTION__);
		return NULL;
	}	

	return zone->tones[type];
}

static struct as_raw_tone *__as_zone_get_raw_tone( struct as_zone *zone, as_tone_type_t type)
{
	struct as_tone *tone;
	tone = __as_zone_get_tone( zone, type);
	if(tone)
		return &(tone->raw_tone);
	return NULL;
}


struct as_zone __init *as_zone_init(void )
{
	struct as_tone *samples[MAX_TONES];
	int next[MAX_TONES];
	struct as_tone *tone;
	struct as_zone *zone;
	void *slab, *ptr;
	long size;
	int total = 0;
	int x;

	total = sizeof( tone_defs)/sizeof(struct as_tone_def);
	printk("Tone Def number is %d\r\n", total);
	size = total*sizeof(struct as_tone) +sizeof(struct as_zone);	
	ptr = slab = (char *)kmalloc(size , GFP_KERNEL);
	if (!slab)
		return NULL;
	memset(slab, 0,  size );

	zone = (struct as_zone *)ptr;
	ptr += sizeof(struct as_zone);
	strncpy(zone->name,  zone_name, sizeof(zone_name) - 1);
	
	for (x=0;x<AS_MAX_CADENCE;x++)
		zone->ringcadence[x] = ringcadence.ringcadence[x];

	for (x=0;x<total ;x++) 
	{
		/* Index the current sample */
		samples[x] = tone = (struct as_tone *)ptr;
		ptr += sizeof(struct as_tone);
		/* Remember which sample is next */
		next[x] = tone_defs[x].next;

		if ((next[x] >= total ) || (next[x] < 0)) 
		{
			printk("Invalid 'next' pointer\n");
			kfree(slab);
			return NULL;
		}
		
		if (tone_defs[x].tone >= AS_TONE_MAX) 
		{
			printk("Too many tones defined\n");
			kfree(slab);
			return NULL;
		}
		
		
		/* Fill in tonedata, datalen, and tonesamples fields */
		tone->tonesamples = tone_defs[x].samples;
		tone->fac1 = tone_defs[x].fac1;
		tone->init_v2_1 = tone_defs[x].init_v2_1;
		tone->init_v3_1 = tone_defs[x].init_v3_1;
		tone->fac2 = tone_defs[x].fac2;
		tone->init_v2_2 = tone_defs[x].init_v2_2;
		tone->init_v3_2 = tone_defs[x].init_v3_2;
		tone->modulate = tone_defs[x].modulate;
		
		tone->raw_tone.freq1 = tone_defs[x].raw_tone.freq1;
		tone->raw_tone.freq2 = tone_defs[x].raw_tone.freq2;
		tone->raw_tone.time = tone_defs[x].raw_tone.time;
		
		tone->next = NULL;					/* XXX Unnecessary XXX */
		if (!zone->tones[tone_defs[x].tone])
			zone->tones[tone_defs[x].tone] = tone;
	}
	
	for (x=0;x<total ;x++) 
		/* Set "next" pointers */
		samples[x]->next = samples[next[x]];

	zone->dtmf_tones = as_dtmf_tones;
	zone->pause_tone = &as_tone_pause;

	zone->get_dtmf_tone = __as_zone_get_dtmf_tone;
	zone->get_dtmf_raw_tone = __as_zone_get_dtmf_raw_tone;
	zone->get_tone = __as_zone_get_tone;
	zone->get_raw_tone = __as_zone_get_raw_tone;
	
	return zone;
}

void __exit as_zone_destory(struct as_zone *zone)
{
	kfree(zone);
}

