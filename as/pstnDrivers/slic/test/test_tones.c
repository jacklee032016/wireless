/*
 * $Author: lizhijie $
 * $Log: test_tones.c,v $
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
 * Revision 1.1.1.5  2004/12/31 08:48:46  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:04  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:52  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:10  fengshikui
 * ÐÞ¸Ä°æ
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

#include "asstel.h"

#include "assist_lib.h"


int test_play_one_zone( int fd )
{
	int tone_id;
	int res;
	
	printf("Dial TONE\r\n");
	tone_id = AS_TONE_DIALTONE;	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone_id);
	sleep(2);

	printf("Busy TONE\r\n");
	tone_id = AS_TONE_BUSY;	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone_id);
	sleep(2);
	
	printf("Ring TONE\r\n");
	tone_id = AS_TONE_RINGTONE;	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone_id);
	sleep(2);

	printf("Congestion TONE\r\n");
	tone_id = AS_TONE_CONGESTION;	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone_id);
	sleep(2);

	printf("CallWait TONE\r\n");
	tone_id = AS_TONE_CALLWAIT;	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone_id);
	sleep(2);

	printf("Dial recall TONE\r\n");
	tone_id = AS_TONE_DIALRECALL;	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone_id);
	sleep(2);

	printf("Record TONE\r\n");
	tone_id = AS_TONE_RECORDTONE;	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone_id);
	sleep(2);


	printf("Stutter TONE\r\n");
	tone_id = AS_TONE_STUTTER;	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone_id);
	sleep(2);

	printf("Stop TONE\r\n");
	tone_id = AS_TONE_STOP;	
	res = ioctl(fd, AS_CTL_SENDTONE, &tone_id);

	return 0;	
}


int main(int argc, char *argv[])
{
	int fd;
	
	if (argc < 2) 
	{
		fprintf(stderr, "Usage: tones_test <assist device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	printf("Play default zone of USA\r\n");
	test_play_one_zone( fd);

	as_lib_set_zone_japan();
	printf("Play zone of Japan\r\n");
	test_play_one_zone( fd);

	close(fd);
	return 0;
}

