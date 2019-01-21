#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "as_thread.h"
#include "assist_lib.h"

/*  
 * test logic of FXO as callee  
*/
int fds;
int fdo;

#define MAX_PHONE_NUM		3

#define LENGTH   1024

void *fxo2fxs_thread_handler(void *data)
{
	char buffer[LENGTH];
	int length;

	printf("Thread running...\r\n");
	while(1)
	{
		length = read(fdo, buffer, LENGTH);
		if(length != LENGTH)
			printf("Read length from FXO device is %d\r\n", length );

		length = write(fds, buffer, length);
			printf("write length to FXS device is %d\r\n", length );
	}		

	return NULL;
}

as_thread_t  fxo2fxs_thread =
{
	name	:	"FXO 2 FXS",
	handler	:	fxo2fxs_thread_handler,
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
	unsigned char phone[32];

	memset(phone, 0, 32);
	if (argc < 3) 
	{
		fprintf(stderr, "Usage: fxo_call <astel FXS device> <astel FXO device>\n");
		exit(1);
	}

	fds = open(argv[1], O_RDWR);
	if (fds < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	fdo = open(argv[2], O_RDWR);
	if (fdo < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[2], strerror(errno));
		exit(1);
	}

	printf("FXO device as callee\r\nSTEP 1: reset FXO device\r\n");
	as_lib_onhook(fdo);

	printf("STEP 2: Waiting First RINGOFF event.....\r\n");
	as_lib_wait_offhook(fdo);
	printf("\tFXO device OFF_HOOK \r\n" );

	printf("STEP 3: set the FXO device into OFFHOOK status\r\n");
	as_lib_offhook( fdo);
	//sleep(1);

	printf("STEP 4:receiving %d phone number from data channel\r\n", MAX_PHONE_NUM); 
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
		{
			printf("received number = %s\r\n", phonenum);
			strcat(phone, phonenum);
			if(strlen(phone)>=MAX_PHONE_NUM)
				break;
		}	
	};
	
	printf("STEP 5:Ring the phone now.....");
	as_ring_with_dtmf_caller_id(fds, phone);
	
	printf("STEP 6:Waiting phone offhook.....");
	as_lib_wait_offhook(fds);
	
	printf("STEP 7:communicating now.....");
	as_thread_create(&fxo2fxs_thread);
	as_tone_play_stop(fds);
	while(1)
	{
		memset(phonenum, 0, 32);
		res = read(fds,  buf,  LENGTH);
		if(res != LENGTH)
				printf("read Length is not correct : %d\r\n" , res);
			
		res = write( fdo, buf , LENGTH);
		if(res != LENGTH)
			printf("write Length is not correct : %d\r\n" , res);
	};

	close(fdo);
	close(fds);
	return 0;
}

