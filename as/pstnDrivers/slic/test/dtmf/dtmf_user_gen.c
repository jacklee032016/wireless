/*
 * $Author: lizhijie $
 * $Log: dtmf_user_gen.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.3  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.2  2004/12/11 05:43:38  lizhijie
 * some comile warning
 *
 * Revision 1.1  2004/11/25 07:36:12  lizhijie
 * create the sub-directories for different test
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_dev_ctl.h"
#include "as_dsp.h"

extern int as_lib_dial(int fd , char *digitstring ,int length );

AS_DIAL_OPERATION dtmf_dial_str ;

char str[9]="5432198";

int __event_get(int fd)
{
	/* Avoid the silly zt_getevent which ignores a bunch of events */
	int j =-1;
	
	if (ioctl(fd, AS_CTL_GETEVENT, &j) == -1) 
		return -1;
	return j;
}

void wait_dial_complete(int fd)
{
	int res;
	
	do
	{
		res = __event_get( fd);
		if(res)
			printf("event=%d\r\n", res );
	}while(res != AS_EVENT_DIALCOMPLETE );
	printf("Dial complete : %d\r\n" , res);
}


int main(int argc, char *argv[])
{
	int fd;
	int res;
//	int law = AS_LAW_DEFAULT;
	int hook;
	
	if (argc < 2) 
	{
		fprintf(stderr, "Usage: test_dtmf <astel device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	hook = AS_START;
	res = ioctl(fd, AS_CTL_HOOK, &hook);


	sleep(3);
	as_lib_dial( fd, "82381234" , 100 );
	
	sleep(3);
	close(fd);
	return 0;
}

