/*
 * $Author: lizhijie $
 * $Log: dsp_fsk_test_detect.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1  2004/12/20 03:21:47  lizhijie
 * add test for FSK generate and detect for test into CVS
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


int main(int argc, char *argv[])
{
	int fd, fd2;
	int res;
	char *file = {"fsk_4123456.data"};
	char phonenumber[32];
	char name[32];
	as_dsp_gen_engine *dsp;
	
	if (argc < 2) 
	{
		fprintf(stderr, "Usage: dsp_fsk_test_detect <asstel device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}
	fd2 = open(file, O_RDWR);
	if (fd2 < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", file, strerror(errno));
		exit(1);
	}
	dsp = as_dsp_init_dsp(fd, U_LAW_CODE);

	printf("FSK detect\r\n");
	res = as_fsk_clid(dsp,  fd2, phonenumber,  name);
	close(fd);
	close(fd2);
	as_dsp_detsory_dsp(dsp);
	printf("End of output FSK data!res =%d phonenumber ='%s' name = '%s'\r\n", 
		res, phonenumber, name );
	return 0;
}

