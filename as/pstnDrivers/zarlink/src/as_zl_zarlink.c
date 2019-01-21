/* 
 * $Log: as_zl_zarlink.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/05/26 05:10:04  lizhijie
 * add zarlink 5023x driver into CVS
 *
 * $Id: as_zl_zarlink.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
 * driver for ZARLINK 50235 echo canceller chip 
 * as_zl_zarlink.c : Hardware manipulate operation
 * Li Zhijie, 2005.05.20
*/
#include  "as_zarlink.h"

inline unsigned char as_zarlink_read(AS_ZARLINK *zr, unsigned long offset )
{
	return *((volatile unsigned char *)(zr->map_private + offset) );
}

inline void as_zarlink_write(AS_ZARLINK *zr, unsigned long offset, unsigned char value)
{
	*(volatile unsigned char *)(zr->map_private + offset ) = value;
}


char as_zarlink_ch_read(AS_ZARLINK *zr, unsigned int channel_no, int reg)
{
	unsigned long offset;

	if(channel_no >= zr->chan_num || channel_no < 0 )
	{
		printk(KERN_ERR "Channel No. %d is not validate (ought to be [0~%d])\r\n", channel_no, zr->chan_num);
		return -ENXIO;
	}
	offset =  0x20*channel_no + reg;

	return as_zarlink_read(zr, offset);
}

void as_zarlink_ch_write(AS_ZARLINK *zr, unsigned int channel_no, int reg, unsigned char value)
{
	unsigned long offset;

	if((reg==ZARLINK_CH_CR1 ) && (channel_no%2) )
	{
		value = value | (1<<1);
	}

	if(channel_no >= zr->chan_num || channel_no < 0 )
	{
		printk(KERN_ERR "Channel No. %d is not validate (ought to be [0~%d])\r\n", channel_no, zr->chan_num);
	}
	offset = 0x20*channel_no + reg;
	as_zarlink_write( zr, offset, value);
	/* Control Register 1 and 2 of ECB must write twice between 350 ns and 20 us */
	if((reg==ZARLINK_CH_CR1 || reg==ZARLINK_CH_CR2) && (channel_no%2) )
	{
		udelay(1);
		as_zarlink_write( zr, offset, value);
	}

}

unsigned short as_zarlink_ch_read_16(AS_ZARLINK *zr, unsigned int chan_no, int reg)
{
	unsigned short value = 0;

	value |= as_zarlink_ch_read(zr, chan_no, reg);
	value |= ( as_zarlink_ch_read(zr, chan_no, reg+1)<<8);

	return value;
}

void as_zarlink_ch_write_16(AS_ZARLINK *zr, unsigned int chan_no, int reg, unsigned short value)
{
	unsigned char byte_value;

	byte_value = value&0xFF;
	as_zarlink_ch_write( zr, chan_no,  reg, byte_value);
	byte_value = (value>>8)&0xFF;
	as_zarlink_ch_write(zr, chan_no, reg+1, byte_value);
}

#if 0
void set_time_cycle(unsigned char t1, unsigned char t2, unsigned char t3, unsigned char t4, unsigned char t5)
{
	unsigned long value = (t1<< 28)|(t2<<26)|(t3<<22)|(t4<<20)|(t5<<16);

	*ZARLINK_CS_REGISTER |= value;

	printk(KERN_ERR "CS1 flags =0x%x\r\n",  *ZARLINK_CS_REGISTER );
}
#endif

static int __init as_zarlink_check_device_insane(AS_ZARLINK *zr )
{
	char value;
	int res = 0;
	int i;

#if 0

	ZL_DEBUG("Check ZARLINK chip");
	for(i=0; i<zr->chan_num; i++)
	{
	as_zarlink_ch_write(zr, i, ZARLINK_CH_CR2, 0x77);
	printk(KERN_ERR "Channel(%d)  0X%x\r\n", i, as_zarlink_ch_read(zr, i, ZARLINK_CH_CR2) );
	}
#endif	
	for(i=0; i< zr->chan_num; i++)
	{
		value = as_zarlink_ch_read(zr, i, ZARLINK_CH_CR1);
		if(value != 0x00 && value != 0x02 )
		{
			printk(KERN_ERR "Channel Register 3 ( Channel %d ) is not validate : 0X%x is not 0X00 or 0X02\r\n", i, value);
			res =  -ENODEV;
		}
	
		value = as_zarlink_ch_read(zr, i, ZARLINK_CH_CR3);
		if(value != 0xFB)
		{
			printk(KERN_ERR "Channel Register 3 ( Channel %d ) is not validate : 0X%x is not 0XFB\r\n", i, value);
			res =  -ENODEV;
		}

		value = as_zarlink_ch_read(zr, i, ZARLINK_CH_CR4);
		if(value != 0x54)
		{
			printk(KERN_ERR "Channel Register 4 ( Channel %d ) is not validate : 0X%x is not 0X54\r\n", i, value);
			res = -ENODEV;
		}
	}
	return res;
}

/* refer to datasheet page 17 */
static int  __init as_zarlink_device_software_reset(AS_ZARLINK *zr )
{
	int i;

	/* power-on for at least 250 us, refer to ZL datasheet page 17 */
	for(i=0; i< zr->group_num; i++)
	{
		as_zarlink_write(zr, ZARLINK_MAIN_CR_BASE+i, 0x01);
	}
	udelay(2*PCM_FRAME_DURATION);

	/* power-off for at least 500 us to PLL relock , refer to ZL datasheet page 17 */
	for(i=0; i< zr->group_num; i++)
	{
		as_zarlink_write(zr, ZARLINK_MAIN_CR_BASE+i, 0x00);
	}
	udelay(4* PCM_FRAME_DURATION);

	/* in order to operting the chip , must wait re-power on */
	for(i=0; i< zr->group_num; i++)
	{
		as_zarlink_write(zr, ZARLINK_MAIN_CR_BASE+i, 0x01);
	}
	/* poweron process must keep 2 frame, refer to page 31 in datasheet */
	udelay(2*PCM_FRAME_DURATION); 

	return 0;
}

static int  __init as_zarlink_hardware_init(AS_ZARLINK *zr)
{
	int i;

	/* step 1 : init the Main Controller Register */
	as_zarlink_write( zr, ZARLINK_MAIN_CR_BASE, ZL_MCR_0_VALUE|0x01 );

	for(i=1; i<zr->group_num; i++)
		as_zarlink_write( zr, ZARLINK_MAIN_CR_BASE+ i, ZL_MCR_N_VALUE|0x01);

	/* Step 2 : init the Control Register 1 and 2 */
	for(i=0; i<zr->chan_num; i++)
	{
		as_zarlink_ch_write( zr, i, ZARLINK_CH_CR1 , ZL_CR1_INIT_VALUE);
		as_zarlink_ch_write( zr, i, ZARLINK_CH_CR2 , ZL_CR2_INIT_VALUE);
	}

	return 0;
}

int __init as_zarlink_hardware_check_config(AS_ZARLINK *zr)
{
	int res;

	as_zarlink_device_software_reset( zr );

	res = as_zarlink_check_device_insane( zr);
	if( res <0)
	{
		printk(KERN_ERR "ZARLINK : Device not found\r\n");
		return res;
	}

	res = as_zarlink_hardware_init( zr);
	return res;
}

