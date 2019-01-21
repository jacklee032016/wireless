/*
 * $Author: lizhijie $
 * $Log: dsp_gain_test.c,v $
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
	int fd;
	int res;
	as_dsp_gen_engine *dsp;
	
	if (argc < 2) 
	{
		fprintf(stderr, "Usage: dsp_gain_test <asstel device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}
	dsp = as_dsp_init_dsp(fd, U_LAW_CODE);

	printf("Default gain for transmit path\r\n");
	res = as_dsp_play_tone_dial(dsp, fd);
	res = as_dsp_set_gain(dsp, fd, 12.0, 12.0 );
	
	printf("After gain set  to 12 dB(*4), transmit path\r\n");
	res = as_dsp_play_tone_dial(dsp, fd);

	res = as_dsp_set_gain(dsp, fd, 6.0, 6.0 );
	printf("After gain set  to 6 dB(*2), transmit path\r\n");
	res = as_dsp_play_tone_dial(dsp, fd);
	printf("DSP gain test ended, result is %d\r\n", res);
	close(fd);
	free(dsp);
	return 0;
}

