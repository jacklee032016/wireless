#include "as_global.h"

#include "as_thread.h"

#define CHANNEL_DEVICE		"/dev/asstel/"

as_thread_t  device_monitor =
{
	name	:	"DEVICES_MONITOR",
	handler	:	as_thread_device_monitor,
	log		: 	printf,

	private	:	&master_span
};

as_thread_t  callee_monitor =
{
	name	:	"CALLEES_MONITOR",
	handler	:	as_thread_callee_monitor,
	log		: 	printf,
	
	private	:	&master_span
};

as_thread_t  calling_thread =
{
	name	:	"CALLING_THREAD",
	handler	:	as_thread_calling_agent,
	log		: 	printf,
	
	private	:	NULL
};


int as_init_device(as_span_t *span)
{
	char device_name[40];
	as_device_t *dev;
	AS_CHANNEL_STATE ch_state;
	int x = 0;
	int i;

	as_fifo_init( span->event_queue );
		
	for( i=0; i<AS_DEVICE_MAX; i++ )
	{
		dev = span->devs +i ;
		memset(device_name, 0,  40);
		sprintf(device_name, "%s%d", CHANNEL_DEVICE, dev->device_id);

		//printf("device_name %s\n",device_name);
		dev->fd = open( device_name, O_RDWR);
		if( dev->fd < 0)
		{
			as_error_msg("Unable to open '%s' on channel %s: %s\n", device_name, dev->name, strerror(errno));
			exit(1);
		}	

		dev->state = AS_DEVICE_STATE_AVAILABLE;
#if	AS_DEBUG_DEVICE
		printf("Device %s for channel %s open successfully!\r\n", device_name, dev->name );	
#endif

		if(as_check_channel_state( dev)==AS_DEVICE_STATE_AVAILABLE)
		{
			span->chanmap[x] = 1;
			dev->state = AS_DEVICE_STATE_AVAILABLE;
		}
		else
		{
			span->chanmap[x] = 0;
			dev->state = AS_DEVICE_STATE_USED;
		}	

		x++;
	}

	return 0;
}
	

int main_test(int argc, char *argv[])
{
	printf("in main_test\n");
	as_init_device( &master_span);
	
//	as_ring_on_hook(channel);
//	as_tone_play_dial(channel );
	
	as_thread_create( &device_monitor);

	callee_monitor.private = &master_span;
	as_thread_create( &callee_monitor);

	while(1)
		{
		}
	
	return 0;
}


