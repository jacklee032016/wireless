/*
 * $Author: lizhijie $
 * $Log: test_rw_2.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.1  2004/11/25 07:36:12  lizhijie
 * create the sub-directories for different test
 *
 * $Revision: 1.1.1.1 $
 * communicate between 2 channels to test echo directly 
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_dev_ctl.h"
#include "as_thread.h"
#include "assist_lib.h"

int fda;
int fdb;

#define LENGTH   1024

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

void *a2b_thread_handler(void *data)
{
	char buffer[LENGTH];
	int length;

	printf("Thread running...\r\n");
	while(1)
	{
		length = read(fda, buffer, LENGTH);
		if(length != LENGTH)
			printf("Read length from A device is %d\r\n", length );

		printf("Child thread running...   ");
		length = write(fdb, buffer, length);
		if(length != LENGTH)
			printf("write length to B device is %d\r\n", length );
	}		

	return NULL;
}

as_thread_t  a2b_thread =
{
	name	:	"FXO 2 FXS",
	handler	:	a2b_thread_handler,
	log		: 	printf,

	private	:	NULL
};


int main(int argc, char *argv[])
{
	int res;
	char buf[LENGTH];
	
	if (argc < 3) 
	{
		fprintf(stderr, "Usage: io_test <assist device1> <assist device2> \n");
		exit(1);
	}

	fda = open(argv[1], O_RDWR);
	if (fda < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	fdb = open(argv[2], O_RDWR);
	if (fdb < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[2], strerror(errno));
		exit(1);
	}

//	wait_off_hook( fda );
	sleep(2);
	printf("Please speak.....\r\n");
//	as_thread_create(&a2b_thread);

	while(1)
	{
		res = read(fdb, buf, LENGTH);
		if(res != LENGTH)
				printf("read Length is not correct : %d\r\n" , res);

		printf("Main thread running...   ");
		res = write( fda, buf , res);
		if(res != LENGTH)
			printf("write Length is not correct : %d\r\n" , res);
	};

	close(fda);
	close(fdb);
	return 0;
}

