#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "assist_lib.h"

int __event_get(int fd)
{
	// Avoid the silly zt_getevent which ignores a bunch of events 
	int j =-1;
	
	if (ioctl(fd, AS_CTL_GETEVENT, &j) == -1) 
		return -1;
	return j;
}

#define LENGTH   1024

int main(int argc, char *argv[])
{
	int fd;
	int res;
	unsigned char buf[LENGTH];
	short dtmf_buf[LENGTH];
	char phonenum[64];
	int i;

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

	i = AS_ONHOOK;/* AS_OFFHOOK;*/
	res = ioctl(fd,AS_CTL_HOOK,&i) ;

	while(1)
	{
		res =  __event_get(fd);
		if (res)
		{
			printf("get event ='%d'\n",res);                               //check event
			break;
		}	
	}

	i = AS_OFFHOOK;
	res = ioctl(fd,AS_CTL_HOOK,&i) ;


	while(1)
	{
		memset(phonenum, 0, 64);
		res = read(fd, buf, LENGTH);
		if(res != LENGTH)
				printf("read Length is not correct : %d\r\n" , res);
//		printf("read Length is  : %d\r\n" , res);
		for(i=0; i<LENGTH; i++)
		{
			dtmf_buf[i] = buf[i];//FULLXLAW(, U_LAW_CODE);
		}
		res = as_lib_dtmf_detect(dtmf_buf, LENGTH, phonenum, 64);
		if(res>0)
			printf("received number = '%s'\r", phonenum);
#if 1			
		res = write( fd, buf , LENGTH);
		if(res != LENGTH)
			printf("write Length is not correct : %d\r\n" , res);
#endif

	};
	
#if 0
	do
	{
		if(res!=0)
		{
			//event = as_dev_event_debug(fd, res) ;
			//printf("event =%s\r\n", event);
		//	free(event);
		}
            sleep(1);	
	}while(res!=ZAP_EVENT_RINGANSWER);

/*
	do
	{
		 res =  zap_getevent(pChanzap);
                 printf("getevent%d\n",res);                               //check event
		if(res!=0)
		{
			//event = as_dev_event_debug(fd, res) ;
			//printf("event =%s\r\n", event);
		//	free(event);
		}
            sleep(1);	
	}while(res!=ZAP_EVENT_RINGANSWER);
*/
	if(res == ZAP_EVENT_RINGANSWER)//tone is ringing!
	{
		printf("\n");
               printf("EVENT OK\n");
              cmd = ZAP_OFFHOOK;
	        res = zap_sethook(pChanzap,cmd);
              //  res=1;
		if(res==-1)
		{
			printf("\nfailed to offhook");
		}
		else
		{                       
                                        
                                       printf("OFFHOOK OK\n");
 				 zap_digitmode(pChanzap,ZAP_MUTECONF);
				printf("mode ok\n");
				 zap_clrdtmf(pChanzap);
				 printf("GETDT");

                 printf("getevent%d\n",res);        
                      //  fd= zap_fd(pChanzap);   
                         
                        //   read(fd,buf,320);
                    // for(i=0;i<320;i++)
                          //   printf("buf%s\n",buf);   
                                                   // check event
				// zap_clrdtmf(pChanzap);
  	        	       res= zap_getdtmf(pChanzap, 4, buf,0, 0, 0, ZAP_HOOKEXIT);
                                printf("num=%d\n",res);
                                j= zap_dtmfbuf(pChanzap);
  				printf("the number is :%s\n",j);
                                zap_dial(pChanzap,"T8887777777",0);
                                 sleep(3);
                                zap_dial(pChanzap,"T8887777777",0);
                                sleep(3);
                                zap_dial(pChanzap,"T8887777777",0);
                               sleep(3);
			        zap_sethook(pChanzap,ZAP_ONHOOK);
		}

	}
               
             sleep(1);
   }
       zap_close(pChanzap);
#endif

	return 0;
}

