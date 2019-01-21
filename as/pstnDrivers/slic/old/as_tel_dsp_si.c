#include "asstel.h"
#if 0
#include "as_zone_tone_digits.h"
#include "as_zonetone_harddata.h"
#endif

#define DEFAULT_DTMF_LENGTH	100 * 8
#define DEFAULT_MFV1_LENGTH	60 * 8
#define PAUSE_LENGTH			500 * 8

/* No bigger than 32k for everything per tone zone */
#define MAX_SIZE 32768
/* No more than 128 subtones */
#define MAX_TONES 128


static void __as_hardware_dsp_init_tone_state(struct as_dsp_dev *dsp, struct as_dev_chan *chan)
{
#if 0
	struct as_tone_state *ts;
	struct as_tone *tone;

	if( (!dsp) ||( !chan) )
		return;
	ts = &chan->ts;
	tone = chan->curtone;

	__as_init_tone_state( ts,  tone);
#endif	
}

static int __as_hardware_dsp_play_fsk( struct as_dsp_dev *dsp, struct as_dev_chan *chan)
{
	char c;
	unsigned char res;
	char data;
       int i;
	int card=chan->chanpos-1;
       struct wcfxs *wc = chan->private;
	/* Called with chan->lock held */

	if (!dsp->zone) 
	{
		return -ENODATA;
	}

#if 0
	//res=as_fxs_getreg_nolock(wc,card,32);
	//as_fxs_setreg_nolock(wc,card,32,res|0x20);
       res=as_fxs_getreg_nolock(wc,card,108);
	as_fxs_setreg_nolock(wc,card,108,res|0x40);
	/*for 1200 band;*/
	as_fxs_setreg_nolock(wc,card,36,0x13);
	/*D37 should be set to 0*/
	as_fxs_setreg_nolock(wc,card,37,0);
	/*D52 should be set to logic 1 twice;*/
	as_fxs_setreg_nolock(wc,card,52,0x01);
	as_fxs_setreg_nolock(wc,card,52,0x01);
	/*set the indirect register for fsk generation*/
	/*following is North American mode*/
	__wcfxs_proslic_setreg_indirect(wc,  card, 99,0x01b4);
	__wcfxs_proslic_setreg_indirect(wc,  card, 100,0x6b60);
	__wcfxs_proslic_setreg_indirect(wc,  card, 101,0x00e9);
	__wcfxs_proslic_setreg_indirect(wc,  card, 102,0x79c0);
	__wcfxs_proslic_setreg_indirect(wc,  card, 103,0x1110);
	__wcfxs_proslic_setreg_indirect(wc,  card, 104,0x3c00);
	/*enable the oscillator 1 active timer interrupt*/
	res=as_fxs_getreg_nolock(wc,card,21);
	as_fxs_setreg_nolock(wc,card,21,res|0x01);
	as_fxs_setreg_nolock(wc,card,32,0x56);
	/*start transmit data*/
	for(i=0;i<120;i++)
		{
		      as_fxs_setreg_nolock(wc,card,52,0);
			//printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01))
			//printk("wait transmit\n");
			/*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
		      as_fxs_setreg_nolock(wc,card,52,1);
		      //printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
		      //printk("wait transmit\n");
		      /*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
		  }
	for(i=0;i<150;i++)
		{
		      as_fxs_setreg_nolock(wc,card,52,1);
			//printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			/*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
		}
	as_fxs_setreg_nolock(wc,card,52,0);
			//printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			/*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
	for(i=0;i<8;i++)
		{
		 as_fxs_setreg_nolock(wc,card,52,0x04>>i);
			//printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			/*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
		}
	as_fxs_setreg_nolock(wc,card,52,1);
			//printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			/*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
					as_fxs_setreg_nolock(wc,card,52,0);
			//printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			/*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
	for(i=0;i<8;i++)
		{
		 as_fxs_setreg_nolock(wc,card,52,0x10>>i);
			//printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			/*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
		}
	as_fxs_setreg_nolock(wc,card,52,1);
			//printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			/*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
					int sum=0x04+0x10;
	
	while (strlen(chan->txdialbuf)) 
	{
		c = chan->txdialbuf[0];
		/* Shift the channel->txdialbuf , so we guess the command in this buffer  */
		memmove(chan->txdialbuf, chan->txdialbuf + 1, sizeof(chan->txdialbuf) - 1);

//		chan->digitmode = DIGIT_MODE_DTMF;
              /*add by ccx*/ 
              chan->dialno=c;
	       
	      
			  // if (*calliddate==NULL)
	//{
		//return 0;
	//}
	#if 0
	 //res=as_fxs_getreg_nolock(wc,card,32);
	//as_fxs_setreg_nolock(wc,card,32,res|0x20);
       res=as_fxs_getreg_nolock(wc,card,108);
	as_fxs_setreg_nolock(wc,card,108,res|0x40);
	/*for 1200 band;*/
	as_fxs_setreg_nolock(wc,card,36,0x13);
	/*D37 should be set to 0*/
	as_fxs_setreg_nolock(wc,card,37,0);
	/*D52 should be set to logic 1 twice;*/
	as_fxs_setreg_nolock(wc,card,52,0x01);
	as_fxs_setreg_nolock(wc,card,52,0x01);
	/*set the indirect register for fsk generation*/
	/*following is North American mode*/
	__wcfxs_proslic_setreg_indirect(wc,  card, 99,0x01b4);
	__wcfxs_proslic_setreg_indirect(wc,  card, 100,0x6b60);
	__wcfxs_proslic_setreg_indirect(wc,  card, 101,0x00e9);
	__wcfxs_proslic_setreg_indirect(wc,  card, 102,0x79c0);
	__wcfxs_proslic_setreg_indirect(wc,  card, 103,0x1110);
	__wcfxs_proslic_setreg_indirect(wc,  card, 104,0x3c00);
	/*enable the oscillator 1 active timer interrupt*/
	res=as_fxs_getreg_nolock(wc,card,21);
	as_fxs_setreg_nolock(wc,card,21,res|0x01);
	as_fxs_setreg_nolock(wc,card,32,0x56);
	/*start transmit data*/
	#endif
	      data=chan->dialno;
	sum+=data;
	//while(data)
	//{
	             as_fxs_setreg_nolock(wc,card,52,0);
	             //printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
		      /*clear the pending interrupt*/
		     res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
	
	     for(i=0;i<8;i++)
	     {
		      as_fxs_setreg_nolock(wc,card,52,(data>>i)&0x01);
			 // printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			  /*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);		
	     }
		 as_fxs_setreg_nolock(wc,card,52,1);
		  //printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			  	//printk("wait transmit\n");
			  /*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                  as_fxs_setreg_nolock(wc,card,18,0x01);
		// data=*(calliddata+1);
	//}
	
             #if 0
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
		#endif
	}
	sum=256 - (sum & 255);
	as_fxs_setreg_nolock(wc,card,52,0);
	             //printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
		      /*clear the pending interrupt*/
		     res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
	
	     for(i=0;i<8;i++)
	     {
		      as_fxs_setreg_nolock(wc,card,52,(sum>>i)&0x01);
			 // printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			  /*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);		
	     }
		 as_fxs_setreg_nolock(wc,card,52,1);
		  //printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			  	//printk("wait transmit\n");
			  /*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                  as_fxs_setreg_nolock(wc,card,18,0x01);
				  for(i=0;i<50;i++)
		{
		      as_fxs_setreg_nolock(wc,card,52,1);
			//printk("i numer is%d\n",i);
		      while(!(as_fxs_getreg_nolock(wc,card,18)&0x01));
			//printk("wait transmit\n");
			/*clear the pending interrupt*/
		      res=as_fxs_getreg_nolock(wc,card,18);
                    as_fxs_setreg_nolock(wc,card,18,0x01);
		}
	/* data over,stop transmit*/
	as_fxs_setreg_nolock(wc,card,37,0x50);
	as_fxs_setreg_nolock(wc,card,32,0x00);
	
	/* Notify userspace process if there is nothing left */
	chan->dialing = 0;
	
	as_channel_qevent_nolock(chan, AS_EVENT_DIALCOMPLETE);

#endif

	return 0;
}

#if 0
void hardware_print_list(struct as_tone *tone)
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
#endif


/* This function is called by IOCTL system call in a channel 
*/
static int __as_hardware_dsp_play_dtmf(struct as_dsp_dev *dsp, struct as_dev_chan *chan )
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
//		chan->tonep = 0;
		/* All done */
		if (chan->curtone) 
		{
			printk("DTMF into channel tone_state\r\n");
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
*/
static int __as_hardware_dsp_play_tone(struct as_dsp_dev *dsp, struct as_dev_chan *chan, int tone_id)
{
       /*add by ccx ,for playing tones via the hardware*/      
	struct  as_tone_array *tone;
	int start,i,j,next;
	int res;
	
	/* Stop the current tone, no matter what */
	chan->curtone = NULL;
	chan->txdialbuf[0] = '\0';
	chan->dialing =  0;

	if ((tone_id >= AS_TONE_MAX) || (tone_id < -1)) 
		return -EINVAL;

	/* Just wanted to stop the tone anyway */
	if (tone_id < 0 )
		return 0;
	
	/*i will point to the tone start address in the array*/
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
#if 0
	chan->tonetoplay = ;
#endif
	chan->start=0;
	chan->hardring=1;
	chan->hardtime=0;

	return res;
}

static char *__as_hardware_dsp_get_desc(struct as_dsp_dev *dsp)
{
	return dsp->name;
}

int __init as_hardware_dsp_init(struct as_dsp_dev *dsp, struct as_dev_span *span)
{
	struct as_zone *zone;

	zone = as_proslic_zone_init();
	if(!zone)
	{
		return EFAULT;
	}
	
	dsp->zone = zone;
	span->zone = zone;

	span->dsp = dsp;
	
	return 0;
}

void __exit as_hardware_dsp_destory(struct as_dsp_dev *dsp)
{
	as_proslic_zone_destory(dsp->zone);
	
}

struct as_dsp_dev assist_hardware_tone_dsp = 
{
	name		:	"Hardware DSP for Assist tone engine",
		
	init			:	as_hardware_dsp_init,
	destory		:	as_hardware_dsp_destory,

	play_tone	:	__as_hardware_dsp_play_tone,
	play_dtmf	:	__as_hardware_dsp_play_fsk,

	get_desc		:	__as_hardware_dsp_get_desc	
};

