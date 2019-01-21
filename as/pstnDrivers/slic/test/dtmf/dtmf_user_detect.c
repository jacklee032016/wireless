/*
 * $Author: lizhijie $
 * $Log: dtmf_user_detect.c,v $
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

#include "as_dev_ctl.h"
#include "assist_lib.h"

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
	int res;
	int i;
	char buf[LENGTH];
	short dtmf_buf[LENGTH];
	char dial_str[32];
	
	if (argc < 2) 
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

	wait_off_hook( fd );

/* for the second received number from FXO device,off_hook must   */
	i = AS_OFFHOOK;
	res = ioctl(fd, AS_CTL_HOOK,&i) ;

	printf("Off Hook in FXO\r\n");

	while(1)
	{
		memset(dial_str, 0, 32);
		res = read(fd, buf, LENGTH);
		if(res != LENGTH)
				printf("read Length is not correct : %d\r\n" , res);
//		printf("read Length is  : %d\r\n" , res);

		for(i=0; i< LENGTH; i++)
		{
			dtmf_buf[i] = FULLXLAW(buf[i], U_LAW_CODE);
		}
		
		res = as_lib_dtmf_detect(dtmf_buf, LENGTH, dial_str, 32);
		if(res )
			printf("Dial str : %s\r\n" , dial_str);
	};

	close(fd);
	return 0;
}

