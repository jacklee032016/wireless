#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_dev_ctl.h"
#include "as_dsp.h"


#define LENGTH   1024
int main(int argc, char *argv[])
{
	int fd;
	int res;
	int tone_id ;
	
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
	sleep(2);


	close(fd);
	return 0;
}

