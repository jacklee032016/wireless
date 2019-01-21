/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_fxs_dtmf.c,v $
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
#include "as_fxs_common.h"
#include "as_dev_ctl.h"

extern void as_channel_qevent_lock(struct as_dev_chan *chan, int event);


#define WC_PROSLIC_INT3_STATUS_REG				20
#define WC_PROSLIC_INT3_MASK_REG					23
#define WC_PROSLIC_DTMF_REG						24


/* Register 20 */
#define WC_PROSLIC_DTMF_INT	    ( 1 )	/*clear pending DTMF Interrupt */
/* Register 23 */
#define WC_PROSLIC_DTMF_INT_EN	( 1 ) /* DTMF Interrupt enabled */
/* Register 24 */
#define WC_PROSLIC_DTMF_DIGIT	    	( 0x0F )
#define WC_PROSLIC_DTMF_VALID		(1<<4 )


char dtmf_define[16]={"D1234567890*#ABC"}; /* refer to Si3021 page 74 */

unsigned char as_fxs_get_dtmf(struct wcfxs *wc, int card )
{
	unsigned long flags;
	unsigned char byVal;
	unsigned char dtmf_status;

	spin_lock_irqsave(&wc->lock, flags);
	dtmf_status = as_fxs_getreg_nolock(wc, card, WC_PROSLIC_INT3_STATUS_REG);
	if( !(dtmf_status&0x01) )
	{
//		printk("No DTMF interrupt happened\r\n");
		spin_unlock_irqrestore(&wc->lock, flags);
		return -1;
	}

	byVal = as_fxs_getreg_nolock(wc, card, WC_PROSLIC_DTMF_REG);
	as_fxs_setreg_nolock(wc, card, WC_PROSLIC_INT3_STATUS_REG, 1);/* clear pending DTMF interrupt */

//	printk("DTMF signal integer: %d (%d) received\r\n", byVal & WC_PROSLIC_DTMF_VALID, byVal);
	
	if (byVal & WC_PROSLIC_DTMF_VALID) 
	{
		wc->chans[card]->dtmf_detect.dtmf_incoming = 1;
		wc->chans[card]->dtmf_detect.dtmf_current  = dtmf_define[byVal & WC_PROSLIC_DTMF_DIGIT];
		
		printk("DTMF signal integer:'%c'   received\r\n", wc->chans[card]->dtmf_detect.dtmf_current );

		as_channel_qevent_lock( wc->chans[card] , AS_EVENT_DTMFDIGIT);

		spin_unlock_irqrestore(&wc->lock, flags);
		return wc->chans[card]->dtmf_detect.dtmf_current;
	} 

	spin_unlock_irqrestore(&wc->lock, flags);
	return -1;
}

void as_fxs_enable_dtmf_detect(struct wcfxs *wc, int card)
{
	as_fxs_setreg(wc, card, WC_PROSLIC_INT3_MASK_REG, 1);
	as_fxs_setreg(wc, card, WC_PROSLIC_INT3_STATUS_REG, 1);
}

