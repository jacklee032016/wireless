/*
 * $Author: lizhijie $
 * $Log: dtmf_generator.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1  2004/11/25 07:36:12  lizhijie
 * create the sub-directories for different test
 *
 * Revision 1.1  2004/11/22 01:54:04  lizhijie
 * add some user module into CVS
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

#include "assist_lib.h"

char str[9]="5432198";


int main(int argc, char *argv[])
{
	int fd;
	
	if (argc < 2) 
	{
		fprintf(stderr, "Usage: dtmf_gen <astel device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	as_lib_dial(fd, str, 100 );
	
	close(fd);
	return 0;
}
