#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_dev_ctl.h"

char *as_dev_event_debug(int fd, int event)
{
	char *event_str;
	int signal;
	
	event_str = (char *)malloc(64);
	if(!event_str)
	{
		printf("Memory failed\r\n");
		exit(1);
	}
	memset(event_str, 0, 64);
	
	switch(event)
	{
		case AS_EVENT_NONE:
			sprintf(event_str, "None");
			break;
		case AS_EVENT_ONHOOK:
			sprintf(event_str, "On Hook");
			break;
		case AS_EVENT_RINGOFFHOOK:
			sprintf(event_str, "Ring OFF-Hook");
			break;
		case AS_EVENT_WINKFLASH:
			sprintf(event_str, "WinkFlash");
			break;
		case AS_EVENT_ALARM:
			sprintf(event_str, "Alarm");
			break;
		case AS_EVENT_NOALARM:
			sprintf(event_str, "No Alarm");
			break;
		case AS_EVENT_BADFCS:
			sprintf(event_str, "Bad FCS");
			break;
		case AS_EVENT_DIALCOMPLETE:
			sprintf(event_str, "Dial Complete");
			break;
		case AS_EVENT_RINGERON:
			sprintf(event_str, "Ringer ON");
			break;
		case AS_EVENT_RINGEROFF:
			sprintf(event_str, "Ringer OFF");
			break;
		case AS_EVENT_HOOKCOMPLETE:
			sprintf(event_str, "Hook Complete");
			break;
		case AS_EVENT_BITSCHANGED:
			sprintf(event_str, "BITS Changed");
			break;
		case AS_EVENT_PULSE_START:
			sprintf(event_str, "Pulse Start");
			break;
		case AS_EVENT_TIMER_EXPIRED:
			sprintf(event_str, "Timer Expired");
			break;
		case AS_EVENT_TIMER_PING:
			sprintf(event_str, "Timer Ping");
			break;
		case AS_EVENT_PULSEDIGIT:
			sprintf(event_str, "PULSE Digit");
			break;
		case AS_EVENT_DTMFDIGIT:
			if (ioctl(fd, AS_CTL_GET_DTMF_DETECT, &signal) == -1) 
			{
				sprintf(event_str, "IOCTL error when get DTMF digit\r\n");
			}
			else
			{
				sprintf(event_str, "DTMF Digit : '%c'" , (unsigned char)signal);
			}
			break;
		default:
			sprintf(event_str, "Unsupport event : %d(0X%x)", event, event);
			break;
	}
	
	return event_str;
}

int __event_get(int fd)
{
	/* Avoid the silly zt_getevent which ignores a bunch of events */
	int j =-1;
	
	if (ioctl(fd, AS_CTL_GETEVENT, &j) == -1) 
		return -1;
	return j;
}

int main(int argc, char *argv[])
{
	int fd;
	int res;
	
	if (argc < 2) 
	{
		fprintf(stderr, "Usage: io_test <assist device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	do
	{
		res = __event_get( fd);
		if(res!=0)
		{
			char *event = as_dev_event_debug(fd, res) ;
			printf("event =%s\r\n", event);
			free(event);
		}	
	}while(1 );

	close(fd);
	return 0;
}

