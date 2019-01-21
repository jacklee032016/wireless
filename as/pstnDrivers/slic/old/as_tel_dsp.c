#include "asstel.h"

#define DEFAULT_DTMF_LENGTH	100 * 8
#define DEFAULT_MFV1_LENGTH	60 * 8
#define PAUSE_LENGTH			500 * 8

/* No bigger than 32k for everything per tone zone */
#define MAX_SIZE 32768
/* No more than 128 subtones */
#define MAX_TONES 128

static void  __as_init_tone_state(struct as_tone_state *ts, struct as_tone *tone)
{
	ts->v1_1 = 0;
	ts->v2_1 = tone->init_v2_1;
	ts->v3_1 = tone->init_v3_1;
	ts->v1_2 = 0;
	ts->v2_2 = tone->init_v2_2;
	ts->v3_2 = tone->init_v3_2;
	ts->modulate = tone->modulate;
}

static void __as_dsp_init_tone_state(struct as_dsp_dev *dsp, struct as_dev_chan *chan)
{
	struct as_tone_state *ts;
	struct as_tone *tone;

	if( (!dsp) ||( !chan) )
		return;
	ts = &chan->ts;
	tone = chan->curtone;

	__as_init_tone_state( ts,  tone);
}

static int __as_dsp_play_dtmf( struct as_dsp_dev *dsp, struct as_dev_chan *chan)
{
	char c;
	/* Called with chan->lock held */

	if (!dsp->zone) 
	{
		return -ENODATA;
	}
	
	while (strlen(chan->txdialbuf)) 
	{
		c = chan->txdialbuf[0];
		/* Shift the channel->txdialbuf , so we guess the command in this buffer  */
		memmove(chan->txdialbuf, chan->txdialbuf + 1, sizeof(chan->txdialbuf) - 1);

//		chan->digitmode = DIGIT_MODE_DTMF;
		chan->curtone = (dsp->zone->get_dtmf_tone)(dsp->zone,  c ); 
		chan->tonep = 0;
			/* All done */
		if (chan->curtone) 
		{
			printk("DTMF into channel tone_state\r\n");
			__as_init_tone_state(&chan->ts, chan->curtone);
			return 0;
		}
		else
			printk("DTMF is NULL for channel \r\n");
	}
	
	/* Notify userspace process if there is nothing left */
	chan->dialing = 0;
	
	as_channel_qevent_nolock(chan, AS_EVENT_DIALCOMPLETE);

	return 0;
}
void print_list(struct as_tone *tone)
{
	struct as_tone *p;
	p = tone;
	printk("start show tone list\n");
	while(p!= NULL)
	{
		printk("fac %d fac2 %d\n",p->fac1,p->fac2);
		p = p->next;
	}
}

/* This function is called by AS_CTL_SENDTONE ioctl to send out tone in a channel 
*/
static int __as_dsp_play_tone(struct as_dsp_dev *dsp, struct as_dev_chan *chan, int tone_id)
{
	struct as_tone *tone;
	int res = -EINVAL;
	
	/* Stop the current tone, no matter what */
	chan->tonep = 0;
	chan->curtone = NULL;
	chan->txdialbuf[0] = '\0';
	chan->dialing =  0;

#if AS_DEBUG
	printk("tone id %d\n",tone_id);
#endif

	if ((tone_id >= AS_TONE_MAX) || (tone_id < -1)) 
		return -EINVAL;
	
/* Just wanted to stop the tone anyway */
	if (tone_id < 0)
		return 0;
	
	if (dsp->zone) 
	{
		/* Have a tone zone */
		tone = (dsp->zone->get_tone)(dsp->zone, tone_id);
		if ( tone ) 
		{
			//print_list(tone);
			chan->curtone = tone;
			res = 0;
		} 
		else	/* Indicate that zone is loaded but no such tone exists */
			res = -ENOSYS;
	} 
	else	/* Note that no tone zone exists at the moment */
		res = -ENODATA;
	printk("ready to init tone state res %d\n",res);
	if (chan->curtone)
		__as_init_tone_state(&chan->ts, chan->curtone);
	return res;
}

static char *__as_dsp_get_desc(struct as_dsp_dev *dsp)
{
	return dsp->name;
}

int __init as_dsp_init(struct as_dsp_dev *dsp, struct as_dev_span *span)
{
	struct as_zone *zone;

	zone = as_zone_init();
	if(!zone)
	{
		return EFAULT;
	}
	
	dsp->zone = zone;
	span->zone = zone;

	span->dsp = dsp;
	
	return 0;
}

void __exit as_dsp_destory(struct as_dsp_dev *dsp)
{
	as_zone_destory(dsp->zone);
	
}

struct as_dsp_dev assist_software_tone_dsp = 
{
	name		:	"Software DSP for Assist tone engine",
		
	init			:	as_dsp_init,
	destory		:	as_dsp_destory,

	init_tone_state:	__as_dsp_init_tone_state,
	play_tone	:	__as_dsp_play_tone,
	play_dtmf	:	__as_dsp_play_dtmf,

	get_desc		:	__as_dsp_get_desc	
};

