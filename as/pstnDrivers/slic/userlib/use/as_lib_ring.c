/*
 * $Author: lizhijie $
 * $Log: as_lib_ring.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.4  2004/12/11 06:19:10  lizhijie
 * remove some warnning
 *
 * Revision 1.3  2004/12/11 05:41:16  lizhijie
 * modify some header file and compile warning
 *
 * Revision 1.2  2004/11/25 07:18:55  lizhijie
 * add some new function in this file
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "as_dev_ctl.h"
#include "assist_lib.h"

#define AS_HOOK_DEBUG		1


/* called by callee thread and monitor the off hook event with timeout */
int as_ring_with_dtmf_caller_id( int  fd  , unsigned char *call_id)
{
	int res;
	int y;
	AS_DIAL_OPERATION dtmf_dial_str ;
	
	y = AS_RING;
	res = ioctl( fd, AS_CTL_HOOK, &y);

	dtmf_dial_str.op = AS_DIAL_OP_REPLACE;
	sprintf(dtmf_dial_str.dialstr, "%s" , call_id );

	res = ioctl( fd,  AS_CTL_SET_DTMF_STR , &dtmf_dial_str);
	
	return res;
}

int as_ring_on_hook( int  fd )
{
//	int count = 0;
	int x;
	int res =0;
		
	if( fd <0)
	{
		as_error_msg("Uninit the dev device in %s\n" ,__FUNCTION__ );
		return -1;
	}	
#if AS_DEBUG	
	x = AS_ONHOOK;
	res = ioctl( fd,  AS_CTL_HOOK, &x);
#endif

	x = AS_RING;
	res = ioctl(fd,  AS_CTL_HOOK, &x);
#if AS_DEBUG
	if (res) 
	{
		switch(errno) 
		{
			case EBUSY:
			case EINTR:
				/* Wait just in case */
				printf( "ring the phone BUSY\n" );
				usleep(10000);
				break;
			case EINPROGRESS:
				printf( "ring the phone IN PROGRESS\n" );
				res = 0;
				break;
			default:
				printf( "Couldn't ring the phone: %s\n", strerror(errno));
				break;
		}
	}
#endif	
	return res;
}

/* set device to ONHOOK status, eg. reset the device */
int as_lib_onhook(int fd)
{
	int res;
	int i;
	
	i = AS_ONHOOK;
	res = ioctl(fd,AS_CTL_HOOK,&i) ;
	return res;
}

/* set the device to OFFHOOK status, eg. begin to communicate */
int as_lib_offhook(int fd)
{
	int res;
	int i;
	
	i = AS_OFFHOOK;
	res = ioctl(fd,AS_CTL_HOOK,&i) ;
	return res;
}

/* get an event of the device */
int as_lib_event_get(int fd)
{
	int j =-1;
	
	if (ioctl(fd, AS_CTL_GETEVENT, &j) == -1) 
		return -1;
	return j;
}


/* wait RINGOFF event, 
 * when it used on a FXO device, this is first RINGEROFF
 * when is used on a FXS device, this is OFFHOOK
*/
void as_lib_wait_offhook(int fd)
{
	int res;
	
	do
	{
		res = as_lib_event_get( fd);
#if AS_HOOK_DEBUG
		if(res )
			as_error_msg( "Event '%d' received\r\n", res );
#endif		
	}while(res != AS_EVENT_RINGOFFHOOK );
#if AS_HOOK_DEBUG	
	as_error_msg("RING OFFHOOK \r\n" );
#endif
}

void as_lib_wait_onhook(int fd)
{
	int res;
	
	do
	{
		res = as_lib_event_get( fd);
	}while(res != AS_EVENT_ONHOOK );
}



