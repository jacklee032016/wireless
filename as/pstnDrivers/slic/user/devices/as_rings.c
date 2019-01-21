#include "as_global.h"


/* called by callee thread and monitor the off hook event with timeout */
void as_ring_with_dtmf_caller_id(as_device_t *dev , unsigned char *call_id)
{
	int res;
	int y;
	AS_DIAL_OPERATION dtmf_dial_str ;
	
	y = AS_RING;
	res = ioctl( dev->fd, AS_CTL_HOOK, &y);

	dtmf_dial_str.op = AS_DIAL_OP_REPLACE;
	sprintf(dtmf_dial_str.dialstr, "%s" , call_id );

//	sleep(3);

//	res = ioctl(dev->fd,  AS_CTL_SET_DTMF_STR , &dtmf_dial_str);
}

void as_ring_on_hook(as_device_t *dev)
{
	int count = 0;
	int x;
	int res =0;
		
	if(dev->fd <0)
	{
		as_error_msg("Uninit the dev %s device\n" ,dev->name );
		return;
	}	
	
	x = AS_ONHOOK;
	res = ioctl(dev->fd,  AS_CTL_HOOK, &x);

	do 
	{
		x = AS_RING;
		res = ioctl(dev->fd,  AS_CTL_HOOK, &x);

		count =3;
		if (res) 
		{
			switch(errno) 
			{
				case EBUSY:
				case EINTR:
				/* Wait just in case */
					printf( "ring the phone BUSY\n" );
					usleep(10000);
					continue;
				case EINPROGRESS:
					printf( "ring the phone IN PROGRESS\n" );
					res = 0;
					break;
				default:
					printf( "Couldn't ring the phone: %s\n", strerror(errno));
					res = 0;
					break;
			}
		}
		else
		{
			fprintf(stderr, "Phone is ringing... count=%d\n", count);
			
//			sleep(2);
//			x = ZT_RINGOFF;
//			res = ioctl(dev->fd,  ZT_HOOK, &x);
		}
				
	} while (count <2);

	return;
}

