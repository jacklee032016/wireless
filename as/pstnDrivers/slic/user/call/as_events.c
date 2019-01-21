#include "as_global.h"


#define AS_DEBUG_EVENT			1


/* this struct is defined as following for description.  It sould be optimized when released */

int as_event_get(int fd)
{
	/* Avoid the silly zt_getevent which ignores a bunch of events */
	int j =-1;
	
	if (ioctl(fd, AS_CTL_GETEVENT, &j) == -1) 
		return -1;
	return j;
}

int as_set_hook(as_device_t *dev, AS_DEVICE_STATE hook_stat)
{
	int x, res;
	
	if(hook_stat == AS_DEVICE_STATE_AVAILABLE)
		x = AS_ONHOOK;
	else
		x = AS_OFFHOOK;
	
	res = ioctl(dev->fd, AS_CTL_HOOK, &x);
	if (res < 0) 
	{
		if (errno == EINPROGRESS) 
		{
			printf("IN PROGRESS in SET HOOK\r\n");
			return 0;
		}	
		printf( "as hook failed: %s\n", strerror(errno));
	}
	
	return res;
}


int as_handle_init_event(as_device_t *dev, int event)
{
	int res;

	switch(event) 
	{
		case AS_EVENT_NONE:
		case AS_EVENT_BITSCHANGED:
//			printf("EVENT NONE\r\n");
			break;
		case AS_EVENT_WINKFLASH:
			printf("device %s : WINKFLASH event in main monitor thread\r\n" ,dev->name );
			res = as_off_hook_event_handler( dev);
			break;
		case AS_EVENT_RINGOFFHOOK:
		/* Got a ring/answer.  What kind of dev are we? */
			printf("device %s : RINGOFFHOOK event in main monitor thread\r\n", dev->name );
			res = as_off_hook_event_handler( dev);
			break;
		case AS_EVENT_NOALARM:
			printf( "device %s : Alarm cleared on dev  in main monitor thread\n" , dev->name );
			break;
		case AS_EVENT_ALARM:
//		res = get_alarms(i);
			printf( "Detected alarm on dev %s in main monitor thread\n", dev->name );
		/* fall thru intentionally */
		case AS_EVENT_ONHOOK:
		/* Back on hook.  Hang up. */
			printf("Detected ONHOOK on dev %s in main monitor thread\n" , dev->name);
//			as_set_hook(dev, ZT_OFFHOOK);
//				usleep(1);
//			res = tone_zone_play_tone( dev->fd, -1);
//			as_set_hook( dev , ZT_ONHOOK);
//			dev->state = AS_CALL_STATE_ONHOOK;
			res = as_on_hook_event_handler( dev );
			break;
		case AS_EVENT_DTMFDIGIT:
			printf("Detected DTMF digit on dev %s in main monitor thread\n" , dev->name);
			break;
		default:
			printf("event is %d on device %s in main monitor thread\r\n", event, dev->name );
			break;
		}
	
	return 0;
}


void as_call_timeout_handler(as_device_t *dev )
{
	switch( dev->state)
	{
		case AS_DEVICE_STATE_INVALIDATE:
			as_error_msg("Phone in ON_HOOK status can not be timeout\r\n");
			break;
			
		case AS_DEVICE_STATE_USED:
			as_error_msg("Phone in OFF_HOOK status timeout\r\n");
			dev->state = AS_DEVICE_STATE_AVAILABLE;
			
			break;
		default:
			as_error_msg("Other timeout event for dev %s\r\n", dev->name );
			break;
	}
}


int  as_event_wait_hook_event(as_device_t *dev, int event_type)
{
	int count = 0, res, pollres=0;
	struct pollfd *pfds=NULL;
	int seconds = 1000;
	
	pfds = (struct pollfd *)malloc( sizeof(struct pollfd));
	if (!pfds) 
	{
		printf( "Critical memory error.  Zap dies.\n");
		exit(1);
	}
	memset(pfds, 0 ,  sizeof(struct pollfd));

	pfds[count].fd = dev->fd;
	pfds[count].events = POLLPRI;

	if(event_type == AS_EVENT_RINGOFFHOOK)
		printf("Device %s wait event OFF_HOOK\r\n", dev->name);
	if(event_type == AS_EVENT_ONHOOK)
		printf("Device %s wait event ON_HOOK\r\n", dev->name);

	trace
	while(1)
	{
	res = poll(pfds, 1,  10*seconds );
	if (res < 0) 
	{
		trace
		if ((errno != EAGAIN) && (errno != EINTR))
			printf( "poll return %d: %s\n", res, strerror(errno));
		return AS_FAIL;
	}
	if ( res == 0)
	{
		printf("Dvice %s polling  is timeout\r\n" , dev->name );
		return AS_TIMEOUT;
	}

		trace		
	pollres = pfds[0].revents;
	if (pollres & POLLPRI) 
	{
		trace
		res = as_event_get( dev->fd);
		if (res <0 ) 
		{
			printf( "Get event failed : %s!\n", strerror(errno));
		}
		if(res == event_type)
			return AS_OK;
		else
			printf("Unexpected event %d received \r\n", res);
		}
	}
	return AS_FAIL;
}


