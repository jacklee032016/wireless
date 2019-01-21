#include "asstel.h"

#include "as_digits_si.h"
#include "as_zonetone_harddata.h"


/* No bigger than 32k for everything per tone zone */
#define MAX_SIZE 32768
/* No more than 128 subtones */
#define MAX_TONES 128

/* get a as_tone for a DTMF digit */
/* this is a symbol exported by TONE driver and can be used by other drier */
static struct as_tone_array *__as_proslic_zone_get_dtmf_tone( struct as_zone *zone, unsigned char digit )
{
	struct as_tone_array *tone = zone->dtmf_tones;

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

static struct as_tone_array *__as_proslic_zone_get_tone( struct as_zone *zone, as_tone_type_t type)
{
	if(type >AS_MAX_CADENCE || type<AS_TONE_STOP)
	{
		printk("Tone ID is out of range in %s\r\n", __FUNCTION__);
		return NULL;
	}	

	return zone->tones[type];
}

struct as_zone __init *as_proslic_zone_init(void )
{
	struct as_tone_array *samples[MAX_TONES];
	struct as_tone_array *tone;
	struct as_zone *zone;
	int next[MAX_TONES];
	void *slab, *ptr;
	long size;
	int total = 0;
	int x;

	total = sizeof( hardtonedata)/sizeof(struct as_hard_tone);
#if AS_DEBUG	
	printk("Tone Def number is %d\r\n", total);
#endif
	size = total*sizeof(struct as_tone_array) +sizeof(struct as_zone);	

	ptr = slab = (char *)kmalloc(size , GFP_KERNEL);
	if (!slab)
		return NULL;
	memset(slab, 0,  size );

	zone = (struct as_zone *)ptr;
	ptr += sizeof(struct as_zone);
	strncpy(zone->name,  hard_zone_name, sizeof(hard_zone_name) - 1);
	
	for (x=0;x<AS_MAX_CADENCE;x++)
		zone->ringcadence[x] = hard_ringcadence.ringcadence[x];

	for (x=0;x<total ;x++) 
	{
		/* Index the current sample */
		samples[x] = tone = (struct as_tone_array *)ptr;
		ptr += sizeof(struct as_tone_array);
		/* Remember which sample is next */
		next[x] = hardtonedata[x].next;

		if ((next[x] >= total ) || (next[x] < 0)) 
		{
			printk("Invalid 'next' pointer\n");
			kfree(slab);
			return NULL;
		}
		
		if (hardtonedata[x].toneid >= AS_TONE_MAX) 
		{
			printk("Too many tones defined\n");
			kfree(slab);
			return NULL;
		}
		
		tone->frequency1=hardtonedata[x].frequency1;
		tone->amplitude1=hardtonedata[x].amplitude1;
		tone->initphase1=hardtonedata[x].initphase1;

		tone->frequency2=hardtonedata[x].frequency2;
		tone->amplitude2=hardtonedata[x].amplitude2;
		tone->initphase2=hardtonedata[x].initphase2;

		tone->time=hardtonedata[x].time;
		tone->next = NULL;
		
		if (!zone->tones[hardtonedata[x].toneid])
			zone->tones[hardtonedata[x].toneid] = tone;
	}
	
	for (x=0;x<total ;x++) 
		/* Set "next" pointers */
		samples[x]->next = samples[next[x]];

#if 0
	zone->get_dtmf_raw_tone = __as_zone_get_dtmf_raw_tone;
	zone->get_raw_tone = __as_zone_get_raw_tone;
#endif	
	zone->dtmf_tones = as_si_dtmf_tones;
	zone->pause_tone = &as_si_tone_pause;
	zone->get_dtmf_tone = __as_proslic_zone_get_dtmf_tone;
	zone->get_tone = __as_proslic_zone_get_tone;
	return zone;
}

void __exit as_proslic_zone_destory(struct as_zone *zone)
{
	kfree(zone);
}

