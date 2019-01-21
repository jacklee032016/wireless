/*
 * Test Case 1 : FXO device switch in status of RINGON/RINGOFF 
 * Test Case 2 : FXO get caller ID between the first ring and second ring
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "as_thread.h"
#include "assist_lib.h"

#define TEST_CASE_1			0

int fdo;

//static int ring_count = 0;

#define LENGTH   1024


void *monitor_thread_handler(void *data)
{
	int res;

	printf("Thread running...\r\n");

	res = as_lib_event_get(fdo );
	if(res == AS_EVENT_RINGERON)
	{
		printf("Second RINGON detected, exit now\r\n");
		as_lib_onhook(fdo);
		exit(1);
	}

	return NULL;
}

as_thread_t  monitor_thread =
{
	name	:	"Monitor second RING",
	handler	:	monitor_thread_handler,
	log		: 	printf,

	private	:	NULL
};



int main(int argc, char *argv[])
{
	int res;
	unsigned char buf[LENGTH];
	short dtmf_buf[LENGTH];
	char phonenum[32];
	int i;

	if (argc < 2) 
	{
		fprintf(stderr, "Usage: fxo_call <astel FXO device>\n");
		exit(1);
	}

	fdo = open(argv[1], O_RDWR);
	if (fdo < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	as_lib_onhook(fdo);

#if TEST_CASE_1
	while(1)
#endif		
	{
		as_lib_wait_offhook(fdo);
#if TEST_CASE_1
		ring_count++;
		printf("%d times RINGEROFF\r\n", ring_count);
#endif		
	}	

	as_thread_create(&monitor_thread);
	
#if 0
	printf("FXO device OFF_HOOK \r\n" );
	i = AS_OFFHOOK;
	res = ioctl(fdo,AS_CTL_HOOK,&i) ;
	//sleep(1);
#endif

	{
		printf("Must set the register to enable ONHOOK caller ID\r\n");
		struct wcfxs_regop regop;
		regop.indirect = 0;
		regop.reg = 5;
		regop.val =(0x08)&0xff;
	
		res = ioctl(fdo, WCFXS_SET_REG, &regop);
	}

	while(1)
	{
		memset(phonenum, 0, 32);
		res = read(fdo,  buf,  LENGTH);
		if(res != LENGTH)
				printf("read Length is not correct : %d\r\n" , res);

		for(i=0; i<LENGTH; i++)
		{
			dtmf_buf[i] = FULLXLAW(buf[i], U_LAW_CODE);
		}
		res = as_lib_dtmf_detect(dtmf_buf, LENGTH, phonenum, 32);
		if(res>0)
			printf("received number = %s\r\n", phonenum);
			
#if  0			
		res = write( fd, buf , LENGTH);
		if(res != LENGTH)
			printf("write Length is not correct : %d\r\n" , res);
#endif
	};
	
	return 0;
}

