//#include "as_global.h"
#include "assist_lib.h"

#include <sys/poll.h>
#include <stdlib.h>
/*
 * receive the DTMF caller ID with hardware in Si3210, 
 * fd : must be a FXS device
 * phone : buffer return the caller ID, it must be allocate in advanced
 * max : max phone number, less than the buffer size of 'phone'
 * timeout : timeout between every char in ms
 * return : -1 : timeout, -2: ONHOOK, >0: total of phone number 
*/
int as_lib_hard_dmtf_caller_id( int fd, unsigned char *phone, int max , int timeout)
{
	int count, res,  pollres=0;  //res2,
	struct pollfd *pfds=NULL;
	int signal;
	int position = 0;
	int timeout_times = 0;

	memset(phone, 0, max);
	
	for(;;) 
	{
		if (pfds)
			free(pfds);

		pfds = (struct pollfd *)malloc( sizeof(struct pollfd));
		if (!pfds) 
		{
			printf( "Critical memory error.  Program dies.\n");
			exit(1);
		}
		memset(pfds, 0 ,  sizeof(struct pollfd));
		
		count = 0;

		pfds[count].fd = fd;
		pfds[count].events = POLLPRI;//|POLLIN;

		count = 1;

		res = poll(pfds, count,  timeout  );
		if (res < 0) 
		{
			if ((errno != EAGAIN) && (errno != EINTR))
				printf( "poll return %d: %s\n", res, strerror(errno));
			continue;
		}
		if ( res == 0)
		{
			printf("polling is timeout (%d seconds) in calling thread\r\n" , timeout );
			timeout_times++;
			if(timeout_times>=max)
				return AS_ERROR_TIMEOUT;
			else
				continue;
		}
		
		pollres = pfds[0].revents;
		if (pollres & POLLPRI) 
		{
			res = as_lib_event_get( fd);
			if (res <0 ) 
			{
				printf( "Get event failed : %s!\n", strerror(errno));
			}
			if( res==AS_EVENT_DTMFDIGIT)
			{
#if AS_EVENT_DEBUG			
				printf("Detect DTMF signal\r\n");
#endif	
				if (ioctl(fd, AS_CTL_GET_DTMF_DETECT, &signal) == -1) 
				{
					printf("IOCTL error\r\n");
				}
				else
				{
#if AS_EVENT_DEBUG			
					printf("DTMF signal is '%c' \r\n", (unsigned char)signal);
#endif
					phone[position] = (unsigned char)signal;
					position ++;
					if((position+timeout_times) >= max)
					{
#if AS_EVENT_DEBUG			
						printf("receive calling number is '%s'\r\n", phone);
#endif
						return position;
					}	
				}
				
			}
			else if(res==AS_EVENT_ONHOOK)
			{
#if AS_EVENT_DEBUG			
				printf("Receive ONHOOK event in calling thread\r\n");
#endif
				return AS_ERROR_ONHOOK; /* ONHOOK when receive caller ID */
			}
			else
				printf("Detect signal %d\r\n", res);
#if AS_EVENT_DEBUG			
				printf("Detect signal %d\r\n", res);
#endif
			
		}
#if 0
/* no data is needed */
		else
		{
		}
#endif		
	}
	return AS_ERROR_TIMEOUT;
}

