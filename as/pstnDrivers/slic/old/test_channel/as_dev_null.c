#include "asstel.h"

#define NULL_CH_COUNT		3

static struct as_dev_chan __null_chans[NULL_CH_COUNT] ;

static void __init  __as_null_ch_setup(void )
{
	int i;
	memset(__null_chans, 0 , sizeof(struct as_dev_chan)*NULL_CH_COUNT );
	for(i =0 ; i< NULL_CH_COUNT; i++)
	{
		sprintf(__null_chans[i].name, "FXS(NULL)-%d", i+1 );
//		__null_chans[i].flags = AS_CHAN_FLAG_RUNNING;
		__null_chans[i].sig = AS_SIG_FXOKS;
	}
	
};

static int __init __as_null_ch_init(void) 
{
	int res;

	__as_null_ch_setup();
	
	res = as_channel_register(& __null_chans[0]);
	if(res < 0)
		return res;
	res = as_channel_register(& __null_chans[1]);
	if(res < 0)
		return res;
	res = as_channel_register(& __null_chans[2]);
	
	return res;
}

static void __exit __as_null_ch_cleanup(void) 
{
	as_channel_unregister(& __null_chans[1]);
	as_channel_unregister(& __null_chans[2]);
	as_channel_unregister(& __null_chans[0]);
}


MODULE_AUTHOR("Chengdu R&D <support@assistcn.com>");
MODULE_DESCRIPTION("Assist Null Channel Device for Test");
MODULE_LICENSE("GPL");


module_init( __as_null_ch_init);
module_exit( __as_null_ch_cleanup );


