#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "assist_lib.h"


int __event_get(int fd)
{
	// Avoid the silly zt_getevent which ignores a bunch of events 
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



/* set hook switch state */
int as_lib_sethook(int fd , int hookstate )
{
	int	i;

	if (fd<0) /* if NULL arg */
	{
		errno= EINVAL;  /* set to invalid arg */
		return(-1);  /* return with error */
	}
	i = AS_ONHOOK;
	if (hookstate) 
		i = AS_OFFHOOK;
	return(ioctl(fd,AS_CTL_HOOK,&i) );
}


int fxo_try_dial(int fd)
{
	int res =0;
	char * digitstring = "818";	
	int hookstate = AS_OFFHOOK;	

#if 0	
	i = AS_START;
	res = ioctl(fd, AS_CTL_HOOK, &i) ;
#endif
	printf("\nafter ring...\n");
#if 0		
	while(1)
	{
		res =  __event_get(fd);
		if (res)
		{
			printf("get event ='%d'\n",res);                               //check event
			break;
		}
	}
#endif		

	res = as_lib_sethook(fd, hookstate);
	if(res==-1)	   
	{
		printf("\nzap_offhook failed");
		return -1;
	}

	printf("OFFHOOK OK\n");  
	sleep(2);     
//	res= zap_getevent(pzap);    
//		printf("event =%d\n",res);      
		// zap_flushevent(pzap);;	
	res = 0;	
	res = as_lib_dial(fd,  digitstring, 100);
	printf("res=%d\n",res);	  
	if(res==-1)	  
	{
	      		printf("\nzap_dial fail\n");
		//	return -1;	
	}
	      
	else
		printf("dial ok\n");

//	data= serie(pzap);
//	printf("\n get data %x",data);
//	sleep(50);	
	return 0;
}

int main(int argc, char *argv[])
{
	int fd;
	int res;

	if (argc < 2) 
	{
		fprintf(stderr, "Usage: file_test <astel device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

//	wait_off_hook( fd );

		res = fxo_try_dial(fd);
	while(1)
	{
		sleep(2);
	}


	return 0;
}
		  
