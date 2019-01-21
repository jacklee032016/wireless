/*
 * $Author: lizhijie $
 * $Log: dsp_dtmf_gen.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/04/20 03:12:29  lizhijie
 * no message
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1  2004/12/20 03:23:50  lizhijie
 * add test for DSP engine for test into CVS
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

int as_dsp_lib_ring_with_dtmf_caller_id(as_dsp_gen_engine *dsp,  int fd , char *digitstring ,int length );

int main(int argc, char *argv[])
{
	int fd;
	int res;
//	int law = AS_LAW_DEFAULT;
//	int hook;
	as_dsp_gen_engine *dsp;
	
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
	dsp = as_dsp_init_dsp(fd, U_LAW_CODE);
	res = as_dsp_lib_ring_with_dtmf_caller_id(dsp, fd, "3123456", 40);

	printf("DTMF DSP caller ID ended, result is %d\r\n", res);
	close(fd);
	return 0;
}

