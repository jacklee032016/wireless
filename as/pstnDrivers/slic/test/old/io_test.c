#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_dev_ctl.h"

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


#define LENGTH   1024
int main(int argc, char *argv[])
{
	int fd;
	int fd2;
	int res;
	int x = 1000000, y;
	int law ;
	int i;
	int linear ;
	char buf[LENGTH];
	int connected = 0;
	
	if (argc < 3) 
	{
		fprintf(stderr, "Usage: io_test <assist device1> <assist device2> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	fd2 = open(argv[2], O_RDWR);
	if (fd2 < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[2], strerror(errno));
		exit(1);
	}

#if 0
	x = AS_OFFHOOK;	
	res = ioctl(fd, AS_CTL_HOOK, &x);

	x = AS_OFFHOOK;	
	res = ioctl(fd2, AS_CTL_HOOK, &x);
#endif

	wait_off_hook( fd );
//	law = ZT_LAW_ALAW;
//	ioctl( fd, ZT_SETLAW, &law);

		if(! connected)
		{
			connected = 1;
			wait_off_hook( fd2 );
		}
		

	while(1)
	{
		res = read(fd, buf, LENGTH);
		if(res != LENGTH)
				printf("read Length is not correct : %d\r\n" , res);
		printf("read Length is  : %d\r\n" , res);

		res = write( fd2, buf , res);
		if(res != LENGTH)
			printf("write Length is not correct : %d\r\n" , res);
	};

	close(fd);
	close(fd2);
	return 0;
}

