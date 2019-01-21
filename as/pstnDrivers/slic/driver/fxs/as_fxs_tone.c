/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_fxs_tone.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/11/25 07:59:24  lizhijie
 * recommit all
 *
*/
#include "asstel.h"
#include "as_fxs_common.h"

int as_fxs_play_tone( struct as_dev_chan *chan)
{ 
	unsigned char flags;
	struct as_tone *tone = chan->curtone;
	struct wcfxs *wc = chan->private;

	if(!tone)
		return -1;
//	chan->curtone=chan->curtone->next;

/* disable oscillator 1 and 2 control register */
       as_fxs_setreg_nolock(wc,  chan->chanpos-1, 32, 0);
	as_fxs_setreg_nolock(wc,  chan->chanpos-1, 33, 0);

      	//as_fxs_setreg_nolock(wc,  chan->chanpos-1 ,31, 0x1);
       //as_fxs_setreg_nolock(wc,  chan->chanpos-1 ,20, 0);

	/* frequncy */
	__wcfxs_proslic_setreg_indirect(wc,  chan->chanpos-1, 13, tone->fac1 );
	__wcfxs_proslic_setreg_indirect(wc,  chan->chanpos-1, 16, tone->fac2 );
	/* amplitude */
	__wcfxs_proslic_setreg_indirect(wc,  chan->chanpos-1, 14, tone->init_v2_1 );       
	__wcfxs_proslic_setreg_indirect(wc,  chan->chanpos-1, 17, tone->init_v2_2 );
	/* init phase */
	__wcfxs_proslic_setreg_indirect(wc,  chan->chanpos-1, 15, tone->init_v3_1 );
	__wcfxs_proslic_setreg_indirect(wc,  chan->chanpos-1, 18, tone->init_v3_2 );

       /* Oscilator 1 active timer */
	as_fxs_setreg_nolock(wc,  chan->chanpos-1, 36, tone->tonesamples&0xff );	
	as_fxs_setreg_nolock(wc,  chan->chanpos-1 ,37, (tone->tonesamples>>8) &0xff);
	/* Oscilator 2 active timer */
	as_fxs_setreg_nolock(wc,  chan->chanpos-1, 40, tone->tonesamples&0xff);
	as_fxs_setreg_nolock(wc,  chan->chanpos-1, 41, (tone->tonesamples>>8) &0xff );

       /*enable the oscillator 1 active timer interrupt*/   		
	flags=as_fxs_getreg_nolock(wc,  chan->chanpos-1, 21);
	as_fxs_setreg_nolock(wc,  chan->chanpos-1, 21, flags|0xc1);

	/*start ringtone*/	
	as_fxs_setreg_nolock(wc,  chan->chanpos-1, 32, 0xb7);
	as_fxs_setreg_nolock(wc,  chan->chanpos-1, 33, 0xb7);

#if 0	
	 chan->hardtime=chan->curtone->time;
	 if(chan->dialing==1)
	 {
	 	printk("ffsdfsdf%d",starttone);
	 	if(chan->start==0)
			chan->dialing=0;
	 }
#endif

	return 0;
}
	/*add over by ccx 2004.11.3*/
