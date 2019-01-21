/*
 * $Revision: 1.1.1.1 $
 * $Log: as_tel_gain.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/06/27 06:02:20  lizhijie
 * no message
 *
 * $Author: lizhijie $
 * $Id: as_tel_gain.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/
#include "asstel.h"

#if 0
static u_char as_defgain[256];

void  __init as_default_gain_init(void)
{
	int i;

	for(i = 0;i < 256;i++)
	{
		as_defgain[i] = i;
	}
}
#endif
void as_channel_set_default_gain(struct as_dev_chan  *chan)
{
	if(chan->gainalloc)
	{
		kfree(chan->rxgain);
		chan->rxgain = NULL;
		chan->gainalloc = 0;
	}
#if 0
	chan->rxgain = as_defgain;
	chan->txgain = as_defgain;
#endif	
}

#if 0
/* compare the gain of a channel with the default gain. if the two are equal, then free the channel' gain and use the default */
static void  as_channel_compare_gain(struct as_dev_chan *chan)
{
	if (!memcmp(chan->rxgain, as_defgain, 256) && 
		    !memcmp(chan->txgain, as_defgain, 256)) 
		    
	{/* This is really just a normal gain, so deallocate the memory and go back to defaults */
		if (chan->gainalloc)
		{
			kfree(chan->rxgain);
			chan->gainalloc = 0;
		}
		
		as_channel_set_default_gain( chan);
	}
}
#endif

int as_channel_set_gain(struct as_dev_chan *chan, struct as_gains *gain)
{
	int j;
	
	if (!chan->gainalloc ) 
	{
		chan->rxgain = kmalloc(512, GFP_KERNEL);
			
		if (!chan->rxgain) 
		{
//			as_channel_set_default_gain( chan);
			return -ENOMEM;
		} 
		else 
		{
			chan->gainalloc = 1;
			chan->txgain = chan->rxgain + 256;
		}
	}

	for (j=0;j<256;j++) 
	{
		chan->rxgain[j] = gain->rxgain[j];
		chan->txgain[j] = gain->txgain[j];
	}
		
//	gain->chan = chan->channo ; /* put the span # in here */
	return 0;
}

