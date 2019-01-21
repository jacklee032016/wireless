#include <stdio.h>
#include <sys/ioctl.h>

#include "as_global.h"

static char *as_device_stats[]=
{
	"Invalidate status",
	"Available",
	"Used"	
};

AS_DEVICE_STATE  as_check_channel_state(as_device_t *dev)
{
	int res;
	struct wcfxs_stats stats;
	
	res = ioctl(dev->fd, WCFXS_GET_STATS, &stats);
	if (res) 
	{
		as_error_msg("Unable to get stats on channel %s\n", dev->name );
		return AS_DEVICE_STATE_INVALIDATE;
	} 
	else 
	{
#if 0	
		printf("TIP: %7.4f Volts\n", (float)stats.tipvolt / 1000.0);
		printf("RING: %7.4f Volts\n", (float)stats.ringvolt / 1000.0);
		printf("VBAT: %7.4f Volts\n", (float)stats.batvolt / 1000.0);
		printf("Value of Register is %d\r\n", stats.ringvolt);
#endif
		if( stats.ringvolt < (-40000) )
			return AS_DEVICE_STATE_AVAILABLE;
		else
			return AS_DEVICE_STATE_USED;
	}

	return AS_DEVICE_STATE_INVALIDATE;
}


void as_print_device_state(as_device_t *dev)
{
	printf("Device %s status is %s\r\n", dev->name, as_device_stats[as_check_channel_state(dev) ] );
}

int as_set_linear(as_device_t  *dev, int linear)
{
	int res;
	res = ioctl( dev->fd, AS_CTL_SETLINEAR, &linear);
	if (res)
		return res;
	return 0;
}


int as_set_law(as_device_t  *dev, int law)
{
	int res;
	
	res = ioctl( dev->fd, AS_CTL_SETLAW, &law);
	if (res)
		return res;
	return 0;
}

