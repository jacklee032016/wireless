#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "as_thread.h"
#include "assist_lib.h"

/*  
 * test logic of FXO dialing : FXO device as caller 
*/
int fds;
int fdo;

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
		if(length != LENGTH)
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
	char phonenum[64];
//	int linear ;

	memset(phonenum, 0, 64);
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

	printf("FXO device as caller\r\nSTEP 1: reset FXO device\r\n");
	as_lib_onhook(fdo);

	printf("STEP 2: Waiting OFFHOOK event in phone.....\r\n");
	as_lib_wait_offhook(fds);
	printf("\tPhone is  OFF_HOOK \r\n" );
	
	printf("STEP 3: Play dial tone in phone\r\n");
	as_tone_play_dial(fds);

	printf("STEP 4: receiving the out office number '0'.....\r\n\tDial can be timeout in 4s, Exit when phone ON_HOOK\r\n");
	while(1)
	{
		res = as_lib_hard_dmtf_caller_id(fds, phonenum, 1, 4000);
		if(res == AS_ERROR_TIMEOUT )
		{
			as_tone_play_congestion(fds);
			printf("Waiting ON_HOOK to exit\r\n");
			as_lib_wait_onhook(fds);
			exit(1);
		}
		else if(res == AS_ERROR_ONHOOK)
		{
			printf("\tPhone is ON_HOOK\r\n");
			exit(1);
		}
		else if( phonenum[0]=='0' )
		{
			break;
		}	
	}
	
	printf("STEP 5: offhook the FXO device\r\n");
	as_lib_offhook(fdo);

	printf("STEP 6: stop tone in FXS device\r\n");
	as_tone_play_stop(fds);
	printf("STEP 7: connect the FXS device to the remote switch\r\n");
	as_thread_create(&fxo2fxs_thread);
	while(1)
	{
		res = read(fds, buf, LENGTH);
		if(res != LENGTH)
				printf("read Length is not correct : %d\r\n" , res);
		res = write( fdo, buf , res );
		if(res != LENGTH)
				printf("write Length is not correct : %d\r\n" , res);
	};
	
	close(fdo);
	close(fds);
	return 0;
}

