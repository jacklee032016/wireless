
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_dev_ctl.h"

void debug_buf_info( struct as_bufferinfo *info)
{
	printf("buf size : %d\r\n", info->bufsize);
	printf("buf number    : %d\r\n", info->numbufs );
	printf("read full bufs  : %d\r\n", info->readbufs );
	printf("write full bufs : %d\r\n", info->writebufs);
	printf("rx buf policy   : %d\r\n", info->rxbufpolicy);
	printf("tx buf policy   : %d\r\n", info->txbufpolicy);
}

void test_blocksize(int fd)
{
	int blocksize;
	struct as_bufferinfo info;
	int res;

	res = ioctl(fd, AS_CTL_GET_BLOCKSIZE, &blocksize);
	if(res<0)
	{
		printf("error in get blocksize : %s\r\n", strerror(errno) );
		exit(1);
	}

	printf("default block size is %d\r\n", blocksize);
	res = ioctl( fd, AS_CTL_GET_BUFINFO, &info);
	if(res<0)
	{
		printf("error in get blocksize : %s\r\n", strerror(errno) );
		exit(1);
	}
	debug_buf_info( &info);
	
	blocksize = 32;
	res = ioctl(fd, AS_CTL_SET_BLOCKSIZE, &blocksize);
	if(res<0)
	{
		printf("error in get blocksize : %s\r\n", strerror(errno) );
		exit(1);
	}

	res = ioctl(fd, AS_CTL_GET_BLOCKSIZE, &blocksize);
	if(res<0)
	{
		printf("error in get blocksize : %s\r\n", strerror(errno) );
		exit(1);
	}
	printf("After setting, block size is %d\r\n", blocksize);
	res = ioctl( fd, AS_CTL_GET_BUFINFO, &info);
	if(res<0)
	{
		printf("error in get blocksize : %s\r\n", strerror(errno) );
		exit(1);
	}
	debug_buf_info( &info);
}


void test_hook(int fd)
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
	sleep(10);
	printf("stop ring\n");
	hook = AS_RINGOFF;
	res = ioctl(fd, AS_CTL_HOOK, &hook);
	
//	sleep(10);
}

int main(int argc, char *argv[])
{
	int fd;
	int res;
	char buf[320];
	int linear ;

	if (argc < 2) 
	{
		fprintf(stderr, "Usage: file_test <astel device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

//	test_blocksize( fd);
//	res = ioctl(fd,  ZT_SETLAW, &law );

	test_hook(fd);
	close(fd);
	return 0;
}



