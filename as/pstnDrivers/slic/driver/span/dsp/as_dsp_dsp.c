/*
 * $Author: lizhijie $
 * $Log: as_dsp_dsp.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.3  2004/12/11 05:37:54  lizhijie
 * debug some for corss-compiler
 *
 * Revision 1.2  2004/11/29 01:55:36  lizhijie
 * update proc file output
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"

#define DEFAULT_DTMF_LENGTH	100 * 8
#define DEFAULT_MFV1_LENGTH	60 * 8
#define PAUSE_LENGTH			500 * 8


#if AS_PROSLIC_DSP
#else
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
#endif

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
#if AS_DEBUG		
			printk("DTMF into channel tone_state\r\n");
#endif
#if AS_PROSLIC_DSP
#else
			__as_init_tone_state(&chan->ts, chan->curtone);
#endif
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

/* This function is called by AS_CTL_SENDTONE ioctl to send out tone in a channel 
 * lock of channel is grabbed by ioctl
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
#if AS_DEBUG	
	printk("ready to init tone state res %d\n",res);
#endif

#if AS_PROSLIC_DSP
#else
	if (chan->curtone)
		__as_init_tone_state(&chan->ts, chan->curtone);
#endif	
	return res;
}

static char *__as_dsp_get_desc(struct as_dsp_dev *dsp)
{
	return dsp->name;
}

/* get the tone list info of tones with same tone_id */
static char *__as_dsp_get_info_tone_list(struct as_tone *tone)
{
	struct as_tone *p;
	char *buf = NULL;
	int len = 0;

	p = tone;
	
	buf = (char *)kmalloc(4096, GFP_KERNEL);
	if(!buf)
	{
		return NULL;
	}
	memset(buf, 0 , 4096);
	
	if(!p)
	{
		len += sprintf(buf + len, "\tThe tones provided is null");
		return buf;
	}		
	else
	{/* tone is loop list, so only the first one is display to check */
#if 0	
		len += sprintf(buf + len, "\tFac-1 : %d,  \tFac-2 : %d, \tsample : %d\r\n" , 
			p->fac1, p->fac2, p->tonesamples);
#endif			
		len += sprintf(buf + len, "\tFreq-1 : %d,  \tFreq-2 : %d, \tTime : %d" , 
			p->raw_tone.freq1, p->raw_tone.freq2, p->raw_tone.time );
//		p = p->next;
	}

	return buf;
}

static int __as_dsp_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	int i, total = 0;
	char *info;
	struct as_dsp_dev  *dsp;
	struct as_zone_tones_data *zone;

	if (off > 0)
		return 0;
		
	dsp = (struct as_dsp_dev *)data;
	if (!dsp)
		return 0;

	if ( dsp->name ) 
		len += sprintf(page + len, "Assist DSP engine : %s\r\n\r\n", dsp->name );
	if( !dsp->zone->data )
		return len;

	len += sprintf(page + len, "Current Tone Info :\r\n" );
	zone = dsp->zone->data;
	if ( zone->name )
		len += sprintf(page + len, "\tCountry Name \"%s\"\r\n", zone->name );
	else
		len += sprintf(page + len, "\"\"\r\n");

	len += sprintf(page + len, "\tRing Cadencs : ");
	for( i = 0; i< AS_MAX_CADENCE ; i++)
	{
		if(zone->ringcadence[i])
			len += sprintf(page + len, " %d ", zone->ringcadence[i]);
	}
	len += sprintf(page + len, " \r\n");
	
	for ( i=0 ; i<AS_TONE_MAX;i++ ) 
	{	
		if ( zone->tones[i] ) 
		{
			total++;
			len += sprintf(page + len, "\tTone ID : %d, ", i);

			info = __as_dsp_get_info_tone_list( zone->tones[i] );
	
			if(!info)
				len += sprintf(page + len, "No memory available\r\n");
			else	
			{
				len += sprintf(page + len, "%s\r\n", info);
				kfree(info);
			}
	
		}
	}
	len += sprintf(page + len, "\r\ntotal %d tones defined\r\n", total);

	return len;
}


static int __as_dsp_replace_tones(struct as_dsp_dev *dsp, struct as_zone_tones_data *tones)
{
	return (dsp->zone->replace_tones)(dsp->zone, tones);
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

struct as_dsp_dev assist_tone_dsp = 
{
#if AS_PROSLIC_DSP
	name		:	"Hardware DSP for Assist tone engine",
	play_dtmf	:	__as_dsp_play_dtmf,
	play_fsk		:	__as_dsp_play_dtmf,
#else
	init_tone_state:	__as_dsp_init_tone_state,
	name		:	"Software DSP for Assist tone engine",
#endif
	init			:	as_dsp_init,
	destory		:	as_dsp_destory,
	replace_tones :	__as_dsp_replace_tones,

	play_tone	:	__as_dsp_play_tone,
	play_caller_id	:	__as_dsp_play_dtmf,

	get_desc		:	__as_dsp_get_desc,
	proc_read	:	__as_dsp_proc_read
};

