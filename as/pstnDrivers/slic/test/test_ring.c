#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_dev_ctl.h"

void test_ring(int fd)
{
	int res;
	int hook = AS_ONHOOK;

#if 0
	res = ioctl(fd, AS_CTL_HOOK, &hook);
	if(res<0)
	{
		printf("error in HOOK IOCTL : %s\r\n", strerror(errno) );
		exit(1);
	}
	sleep(10);
#endif	
	
	hook = AS_RING;
	printf("fd is %d\n",fd);
	res = ioctl(fd, AS_CTL_HOOK, &hook);
	if(res<0)
	{
		printf("error in HOOK IOCTL : %s\r\n", strerror(errno) );
		exit(1);
	}

	for(res=0;res<0xffffff00;res++);
	for(res=0;res<0xffffff00;res++);
	
	hook = AS_RINGOFF;
	res = ioctl(fd, AS_CTL_HOOK, &hook);
	if(res<0)
	{
		printf("error in HOOK IOCTL : %s\r\n", strerror(errno) );
		exit(1);
	}

#if 0
	sleep(10);
	printf("stop ring\n");
	hook = AS_RINGOFF;
	res = ioctl(fd, AS_CTL_HOOK, &hook);
#endif

//	sleep(10);
}

int main(int argc, char *argv[])
{
	int fd;

	if (argc < 2) 
	{
		fprintf(stderr, "Usage: test_ring <astel device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	test_ring(fd);
	close(fd);
	return 0;
}

