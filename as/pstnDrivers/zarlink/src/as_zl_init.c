/* 
 * $Log: as_zl_init.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.3  2005/08/05 04:19:50  lizhijie
 * add compatability for 80234/80235 chipset
 *
 * Revision 1.2  2005/06/07 09:15:05  lizhijie
 * add module reference count
 *
 * Revision 1.1  2005/05/26 05:10:04  lizhijie
 * add zarlink 5023x driver into CVS
 *
 * $Id: as_zl_init.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
 *driver for ZARLINK 50235 echo canceller chip 
 * as_zl_init.c  : initial interface of zarlnk driver module
 * Li Zhijie, 2005.05.20
*/
#include  "as_zarlink.h"
#include <linux/proc_fs.h>
#include "zl_version.h"

static AS_ZARLINK  zarlink = 
{
#if ZARLINK_CHIPSET_50235
	name 			:	 "ZarLink 50235 Voice Echo Canceller",

	group_num		:	8,
	chan_num		:	16,
#else
	name 			:	 "ZarLink 50234 Voice Echo Canceller",

	group_num		:	4,
	chan_num		:	8,
#endif

	resource			: 	0,
	map_private		: 	0
};

//static struct proc_dir_entry *as_zl_proc_root_entry;
static struct proc_dir_entry *as_zl_proc_entry; 

#define  AS_ZL_PROC_DIR_NAME	"zlec"		/* ZarLink Echo Canceller */

char *as_zarlink_echo_canceller_channel_info(AS_ZARLINK *zr, unsigned int chan_no)
{
	char *buf = NULL;
	int len = 0;
		
	buf = (char *)kmalloc(4096, GFP_KERNEL);
	if(!buf)
	{
		return NULL;
	}

	len += sprintf(buf + len, "Echo Canceler Channel %d : \r\n", chan_no);

	len += sprintf(buf + len, "\tCR1 : 0X%x \tCR2 : 0X%x \tCR3 : 0X%x \tCR4 : 0X%x\r\n", 
			as_zarlink_ch_read(zr, chan_no, ZARLINK_CH_CR1) , as_zarlink_ch_read(zr, chan_no, ZARLINK_CH_CR2) ,
			as_zarlink_ch_read(zr, chan_no, ZARLINK_CH_CR3) , as_zarlink_ch_read(zr, chan_no, ZARLINK_CH_CR4) );

	len += sprintf(buf + len, "\tFlatDelay : %d \tDecayStepNumber : %d \t\tDecayStepSize : %d\r\n", 
			as_zarlink_ch_read(zr, chan_no, ZARLINK_CH_FLATDELAY_R) , 
			as_zarlink_ch_read(zr, chan_no, ZARLINK_CH_DECAY_STEP_NUMBER_R) , 
			as_zarlink_ch_read(zr, chan_no, ZARLINK_CH_DECAY_STEP_SIZE_R) );
		
	len += sprintf(buf + len, "\tNoiseScaling  : 0X%x \t\tNoiseControl : 0X%x\r\n", 
			as_zarlink_ch_read(zr, chan_no, ZARLINK_CH_NOISE_SCALING_R) , 
			as_zarlink_ch_read(zr, chan_no, ZARLINK_CH_NOISE_CONTROL_R) );
		
	len += sprintf(buf + len, "\tRinPeak : 0X%x \tSinPeak : 0X%x \tErrorPeak : 0X%x\r\n", 
			as_zarlink_ch_read_16(zr, chan_no, ZARLINK_CH_RIN_PEAK_R) , 
			as_zarlink_ch_read_16(zr, chan_no, ZARLINK_CH_SIN_PEAK_R) ,
			as_zarlink_ch_read_16(zr, chan_no, ZARLINK_CH_ERROR_PEAK_R) );

	len += sprintf(buf + len, "\tDTDT : 0X%x \tNLPTH : 0X%x \tMU : 0X%x \tGains : 0X%x\r\n", 
			as_zarlink_ch_read_16(zr, chan_no, ZARLINK_CH_DTDT_R) , 
			as_zarlink_ch_read_16(zr, chan_no, ZARLINK_CH_NLPTH_R) ,
			as_zarlink_ch_read_16(zr, chan_no, ZARLINK_CH_MU_STEPSIZE_R) ,
			as_zarlink_ch_read_16(zr, chan_no, ZARLINK_CH_GAINS_R) );

	return buf;
}

static int as_zarlink_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	int i;
	char *info;
	static int half = 0;
	int begin;
	AS_ZARLINK *zr;

	if (off > 0)
		return 0;
		
	zr = (AS_ZARLINK *)data;
	if (!zr)
		return 0;

	MOD_INC_USE_COUNT;

	if ( zr->name ) 
	{
		len += sprintf(page + len, "%s : ", zr->name);
		len += sprintf(page + len,  AS_VERSION_INFO("ZarLink") );
	}

	len += sprintf(page + len, "\r\n\r\n");
	len += sprintf(page + len, "Chip Info : \r\n\tGroup : %d \t\t\t\tChannel : %d\r\n", zr->group_num, zr->chan_num );

	for(i=0; i< zr->group_num; i++)
	{
		len += sprintf(page + len, "\tMainControl Register %d : 0X%x \t", i, as_zarlink_read(zr, ZARLINK_MAIN_CR_BASE + i ) );
		if(i%2)
			len += sprintf(page + len, "\r\n" );
	}

	len += sprintf(page + len, "\r\n\r\nChannels Info : \r\n" );

	begin = zr->chan_num/2;
	for(i= begin*half; i< begin*half+begin; i++)
	{
		info = as_zarlink_echo_canceller_channel_info(zr, i);
		if(!info)
			len += sprintf(page + len, "No memory available\r\n");
		else	
		{
			len += sprintf(page + len, "%s\r\n", info);
			kfree(info);
			info = NULL;
		}
	}
	half = (half+1)%2;

	MOD_DEC_USE_COUNT;

	return len;
}

static void __exit as_zarlink_cleanup(void) 
{
	if (zarlink.map_private)
		iounmap((void *)zarlink.map_private);
	
	if (zarlink.resource)
		release_mem_region(ZARLINK_BASE_ADDRESS, ZARLINK_WINDOW_SIZE);
    
	*ZARLINK_CS_REGISTER = 0;//|= ~(ZARLINK_CONTROL_STATUS);

	remove_proc_entry(AS_ZL_PROC_DIR_NAME,  NULL);
	
	printk(KERN_INFO "Assist Driver for ZARLINK 50234 Unloaded\r\n" );
}

static int __init as_zarlink_init(void) 
{
	int res = 0;

	*ZARLINK_CS_REGISTER |= ZARLINK_CONTROL_STATUS;
#if 0	
	printk(KERN_ERR "CS0 flags =0x%x CS1 flags =0x%x\r\n", *IXP425_EXP_CS0,  *ZARLINK_CS_REGISTER );
	set_time_cycle(3, 3, 3, 1, 2);
	printk(KERN_ERR "EXP Controler 0 =0x%x\r\n",  *IXP425_EXP_CFG0 );
#endif
	
	zarlink.resource = request_mem_region(ZARLINK_BASE_ADDRESS, ZARLINK_WINDOW_SIZE, "zarlink50235");
	if(!zarlink.resource) 
	{
		printk(KERN_ERR "ZARLINK : Could not request mem region.\n" );
		res = -ENOMEM;
		goto Error;
	}
	
	zarlink.map_private = (unsigned long)ioremap(ZARLINK_BASE_ADDRESS, ZARLINK_WINDOW_SIZE);
	if (!zarlink.map_private) 
	{
		printk(KERN_ERR "ZARLINK : Failed to map IO region. (ioremap)\n");
		res = -EIO;
		goto Error;
	}

#if 0
	printk(KERN_ERR"map is  %lx\r\n", zarlink.map_private);
#endif

	res = as_zarlink_hardware_check_config(&zarlink);
	if( res <0)
	{
		printk(KERN_ERR "ZARLINK : Device failed when initialized\n");
		goto Error;
	}

	as_zl_proc_entry = create_proc_read_entry(AS_ZL_PROC_DIR_NAME, 0444, NULL , as_zarlink_proc_read, (int *)(long)&zarlink );
	as_zl_proc_entry->owner = THIS_MODULE;
		
	printk(KERN_INFO "Assist Driver for ZARLINK 50234 loaded\r\n" );
	printk(KERN_INFO  AS_VERSION_INFO("ZarLink") );
	return res;

Error:
	as_zarlink_cleanup();
	return res;
}


MODULE_AUTHOR("Chengdu R&D <support@assistcn.com>");
MODULE_DESCRIPTION("Assist Telephony Interface");
MODULE_LICENSE("GPL");


module_init( as_zarlink_init);
module_exit( as_zarlink_cleanup );

