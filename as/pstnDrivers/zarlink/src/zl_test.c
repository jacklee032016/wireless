/* 
 * $Log: zl_test.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/05/26 05:10:04  lizhijie
 * add zarlink 5023x driver into CVS
 *
 * $Id: zl_test.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
 * Test program driver for ZARLINK 50235 echo canceller chip 
 * zl_test.c : interface for dynamic operation for zarlink hardware device
 * Li Zhijie, 2005.05.20
*/

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
	char str[128];
	int res = 0;

	if (argc < 2) 
	{
		fprintf(stderr, "Usage: zl_test <astel device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	while(1)
	{
		res = scanf("%s", str);
		if(res)
			break;
	};

	
	close(fd);
	return 0;
}


