/* 
 * $Log: as_zl_if.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/05/26 05:10:04  lizhijie
 * add zarlink 5023x driver into CVS
 *
 * $Id: as_zl_if.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
 * driver for ZARLINK 50235 echo canceller chip 
 * a_zl_if.c : interface for dynamic operation for zarlink hardware device
 * Li Zhijie, 2005.05.20
*/
#include  "as_zarlink.h"

extern AS_ZARLINK zarlink;

void zl_ch_bypass_enable(unsigned int chan_no)
{
	unsigned char value ;

	value = as_zarlink_ch_read( &zarlink, chan_no -1, ZARLINK_CH_CR1 );
	value = value|(AS_ZARLINK_ENABLE<<ZL_CR1_BYPASS_BIT) ;
	printk(KERN_ERR "Channel %d Bypass Enabled : 0X%x\r\n", chan_no -1, value );

	as_zarlink_ch_write( &zarlink, chan_no-1, ZARLINK_CH_CR1, value);
}

void zl_ch_bypass_disable(unsigned int chan_no)
{
	unsigned char value ;

	value = as_zarlink_ch_read( &zarlink, chan_no-1, ZARLINK_CH_CR1 );
	value = value&(~(AS_ZARLINK_ENABLE<<ZL_CR1_BYPASS_BIT) );
	printk(KERN_ERR "Channel %d Bypass Disabled : 0X%x\r\n", chan_no-1, value);

	as_zarlink_ch_write( &zarlink, chan_no-1, ZARLINK_CH_CR1,  value );
}

void zl_ch_clear_coeffs(unsigned int chan_no)
{
	zl_ch_bypass_enable(chan_no);
	udelay(PCM_FRAME_DURATION);
	zl_ch_bypass_disable( chan_no);
}

void zl_ch_mute_receive(unsigned int chan_no)
{
	unsigned char value ;
	value =as_zarlink_ch_read(&zarlink, chan_no-1, ZARLINK_CH_CR2);
	as_zarlink_ch_write(&zarlink, chan_no-1,  ZARLINK_CH_CR2, value|(AS_ZARLINK_ENABLE<<ZL_CR2_MUTE_R_BIT));
}

void zl_ch_on_receive(unsigned int chan_no)
{
	unsigned char value ;
	value =as_zarlink_ch_read(&zarlink, chan_no-1, ZARLINK_CH_CR2);
	as_zarlink_ch_write(&zarlink, chan_no-1,  ZARLINK_CH_CR2, value&(~(AS_ZARLINK_ENABLE<<ZL_CR2_MUTE_R_BIT) ));
}

void zl_ch_mute_send(unsigned int chan_no)
{
	unsigned char value ;
	value =as_zarlink_ch_read(&zarlink, chan_no-1, ZARLINK_CH_CR2);
	as_zarlink_ch_write(&zarlink, chan_no-1,  ZARLINK_CH_CR2, value|(AS_ZARLINK_ENABLE<<ZL_CR2_MUTE_S_BIT));
}

void zl_ch_on_send(unsigned int chan_no)
{
	unsigned char value ;
	value =as_zarlink_ch_read(&zarlink, chan_no-1, ZARLINK_CH_CR2);
	as_zarlink_ch_write(&zarlink, chan_no-1,  ZARLINK_CH_CR2, value&(~(AS_ZARLINK_ENABLE<<ZL_CR2_MUTE_R_BIT) ));
}


EXPORT_SYMBOL(zl_ch_bypass_enable);
EXPORT_SYMBOL(zl_ch_bypass_disable);
EXPORT_SYMBOL(zl_ch_clear_coeffs );
EXPORT_SYMBOL(zl_ch_mute_receive);
EXPORT_SYMBOL(zl_ch_on_receive);
EXPORT_SYMBOL(zl_ch_mute_send);
EXPORT_SYMBOL(zl_ch_on_send);

