#include "as_dev_ctl.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
extern int as_get_channel_num(int *chan_num);
extern int as_get_channel_state(struct as_channel_state **channel_states);
int main(int argc, char *argv[])
{
	struct as_channel_state *chan_states;
	int chan_num;
	as_get_channel_num(&chan_num);
	printf("chan:%d\n",chan_num);
	chan_num++;
	sleep(3);
	int i=0;
	chan_states=(struct as_channel_state *)malloc(chan_num*sizeof(struct as_channel_state));
	//printf("address:%X",chan_states);
	as_get_channel_state(&chan_states);
	//printf("address:%X",chan_states);
	while((chan_states->available !=-1) && (i<(chan_num-1)))
	{
		printf("channo:%d\n",chan_states->channel_no);
		if(chan_states->type==1)
			printf("the type is fxs\n");
		else
			printf("the type is fxo\n");	
		if(chan_states->available==0)
			printf("the channel is available\n");
		else
			printf("the channel is busy\n");
		i++;
		chan_states++;
	}
	chan_states-=i;
	free(chan_states);
	printf("over\n");
	return 0;
}

