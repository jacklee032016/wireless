//#include "as_dev_tel.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

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

//	res = ioctl(fd,  ZT_SETLAW, &law );
	
	close(fd);
	return 0;
}


