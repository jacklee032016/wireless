/*
 * $Author: lizhijie $
 * $Log: test_gsmplay.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/04/26 06:06:10  lizhijie
 * *** empty log message ***
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.4  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.3  2004/12/14 12:48:50  lizhijie
 * support building header files in the architecture platform
 *
 * Revision 1.2  2004/12/11 05:43:23  lizhijie
 * some comile warning
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

#include "as_gsm.h"
#include "asstel.h"

int __event_get(int fd)
{
	/* Avoid the silly zt_getevent which ignores a bunch of events */
	int j =-1;
	
	if (ioctl(fd, AS_CTL_GETEVENT, &j) == -1) 
		return -1;
	return j;
}

void wait_off_hook(int fd)
{
	int res;
	
	do
	{
		res = __event_get( fd);
	}while(res != AS_EVENT_RINGOFFHOOK );
	printf("RING OFFHOOK : %d\r\n" , res);
}


int main(int argc, char *argv[])
{
	int fd;

	if (argc < 3) 
	{
		fprintf(stderr, "Usage: gsmplay <assist device> <gsm file>\n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	wait_off_hook( fd);
	printf("play...\r\n");
	as_gsm_file_play(argv[2], fd);

	
	printf("Play audio over\r\n");

	close(fd);
	return 0;
}

