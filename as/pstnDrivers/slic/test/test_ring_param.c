/*
 * $Author: lizhijie $
 * $Log: test_ring_param.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/04/26 06:06:10  lizhijie
 * *** empty log message ***
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/11/29 08:25:04  eagle
 * 2229 by chenchaoxin
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
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

#include "as_ring_param.h"
int as_ring_on_hook( int  fd );
int as_ring_param_reset( struct as_si_ring_para *param);
void as_ring(int fd);
char *as_chan_name_get_by_id(int chan_no);


struct as_si_ring_para params[3]=
{
	{1, 89, 0, 1, 0, 90},
	{1, 49, 0, 1, 0, 300},
	{1, 93, 0, 1, 0, 900}
};

	
int main(int argc, char *argv[])
{
	int i,fd;
	char *devname;
	for(i=0;i<3;i++)
	{
		as_ring_param_reset( &params[i]);
		sleep(2);
		devname = as_chan_name_get_by_id(params[i].channel_no);
		fd = open(devname, O_RDWR);
		if (fd < 0) 	
		{
			fprintf(stderr, "Unable to open %s: %s\n", devname, strerror(errno));
			return -1;
		}
		as_ring(fd);
		close(fd);		
	}	
	return 0;
}

