#include "as_dev_ctl.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
extern int as_buf_get_info(  int  fd,struct as_bufferinfo *info );

extern int as_set_channel_para(int channelno,struct as_channel_para *channel_para);
struct as_channel_para chan_para={1,0,2048,3,2048};
int main(int argc, char * argv [ ])
{
	struct as_bufferinfo * info;
	int fd;//,res,size;
	info=(struct as_bufferinfo *)malloc(sizeof(struct as_bufferinfo ));
	fd=open("/dev/asstel/1",O_RDWR);
	if(fd<0)
	{
		printf("open device error\n");
		free(info);
		return -1;		
	}
	as_buf_get_info( fd, info);
	//printf("buffersize:%d\n",info->bufsize);
	//printf("number of buffers:%d\n",info->numbufs);
	//as_get_block_size(fd,&size);
	//printf("blocksize:%d\n",size);
	close(fd);
	as_set_channel_para(1, &chan_para);
	
	free(info);

	return 0;
}

