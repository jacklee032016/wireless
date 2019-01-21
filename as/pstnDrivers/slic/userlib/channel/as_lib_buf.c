/*
 * $Author: lizhijie $
 * $Log: as_lib_buf.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.5  2004/12/11 06:22:08  lizhijie
 * add local MACRO
 *
 * Revision 1.4  2004/12/11 06:01:22  lizhijie
 * add CVS keywaord
 *
 * $Revision: 1.1.1.1 $
*/
#include "assist_lib.h"
#include <stdlib.h>

#define LENGTH	1024


int as_buf_get_info(  int  fd,struct as_bufferinfo *info )
{
	int res;
	res = ioctl( fd, AS_CTL_GET_BUFINFO, info);
	if(res<0)
	{
		printf("error in get bufinfo : %s\r\n", strerror(errno) );
	}
	else
	{
		debug_buf_info( info);
	}
	return res;
}
/*********************************************************
**************** the para limits:*************************
	  16<=bufsize<=AS_MAX_BLOCKSIZE(8192)
	  bufsize * numbufs <AS_MAX_BUF_SPACE(32768)
*********************************************************/

int as_buf_para_set(  int  fd ,struct as_bufferinfo *info)
{
	int res;
	res = ioctl( fd, AS_CTL_SET_BUFINFO, info);
	if(res<0)
	{
		printf("error in set bufinfo : %s\r\n", strerror(errno) );
	}
	return res;
}



int as_get_block_size(int fd,int *block_size)
{
	int res;
	res = ioctl( fd, AS_CTL_GET_BLOCKSIZE, block_size);
	if(res<0)
	{
		printf("error in get bufinfo : %s\r\n", strerror(errno) );
	}
	return res;
}
/**********************************************************
********************** the para limits:********************
		16<=block_size<=AS_MAX_BLOCKSIZE
**********************************************************/
int as_set_block_size(int fd,int block_size)
{
	int res;
	res = ioctl( fd, AS_CTL_SET_BLOCKSIZE, &block_size);
	if(res<0)
	{
		printf("error in set blocksize : %s\r\n", strerror(errno) );
	}
	return res;
}


int as_get_event(int fd,int event)
{
	int res;
	char *eventchar;
	res = ioctl( fd, AS_CTL_GETEVENT, &event);
	if(res<0)
	{
		printf("error in get event on queue : %s\r\n", strerror(errno) );
	}
	else
	{
		eventchar = as_dev_event_debug(fd, res) ;
		printf("event =%s\r\n", eventchar);
		free(eventchar);
	}
	return res;
}


/**************************************************************
	AS_LAW_DEFAULT	0	 Default law for span 
	AS_LAW_MULAW	1	 Mu-law 
 	AS_LAW_ALAW	2	 A-law 
**************************************************************/
int as_set_law(int fd,int law)
{
	int res;
	res = ioctl( fd, AS_CTL_SETLAW, &law);
	if(res<0)
	{
		printf("error in set law : %s\r\n", strerror(errno) );
	}
	return res;
	
}
/***************************************************************
**********************linear>0 set linear***********************
**********************linear<=0 unset linear*********************
***************************************************************/
int as_set_linear(int fd,int linear)
{
	int res;
	res = ioctl( fd, AS_CTL_SETLINEAR,&linear);
	if(res<0)
	{
		printf("error in set linear : %s\r\n", strerror(errno) );
	}
	return res;
	
}
 void debug_buf_info( struct as_bufferinfo *info)
{
	printf("buf size : %d\r\n", info->bufsize);
	printf("buf number    : %d\r\n", info->numbufs );
	printf("read full bufs  : %d\r\n", info->readbufs );
	printf("write full bufs : %d\r\n", info->writebufs);
	printf("rx buf policy   : %d\r\n", info->rxbufpolicy);
	printf("tx buf policy   : %d\r\n", info->txbufpolicy);
}












int as_get_chan_para(int fd,struct as_params *para)
{
	int res;
	res = ioctl( fd, AS_CTL_GET_PARAMS, para);
	if(res<0)
	{
		printf("error in get channel parameters: %s\r\n", strerror(errno) );
	}
	return res;	
}

int as_set_chan_para(int fd,struct as_params *para)
{
	int res;
	res = ioctl( fd, AS_CTL_SET_PARAMS, para);
	if(res<0)
	{
		printf("error in set channel parameters: %s\r\n", strerror(errno) );
	}
	return res;	
}




int as_get_gains(int fd,struct as_gains *gain)
{
	int res;
	res = ioctl( fd, AS_CTL_GETGAINS, gain);
	if(res<0)
	{
		printf("error in get gains: %s\r\n", strerror(errno));
	}		
	return res;	
}
int as_set_gains(int fd,struct as_gains *gain)
{
	int res;
	res = ioctl( fd, AS_CTL_SETGAINS, gain);
	if(res<0)
	{
		printf("error in set gains: %s\r\n", strerror(errno));
	}
	return res;	
}
int as_print_gains(struct as_gains *gain)
{
	int i;
	printf("channel no is :%d\n",gain->chan);
	printf("rxgain is as follows:\n");
	for(i=0;i<256;i++)
	{
		printf("%3d:%3d",i,gain->rxgain[i]);
		if(((i+1)%5)==0)
			
			printf("\n");
	}
	printf("\n");
	printf("rxgain is as follows:\n");
	for(i=0;i<256;i++)
	{
		printf("%3d:%3d",i,gain->txgain[i]);
		if(((i+1)%5)==0)
			printf("\n");
	}
	printf("\n");
	return 0;
}



int as_get_wcfxs_stats(int fd,struct wcfxs_stats *stats)
{
	
	int res;
	res = ioctl( fd, WCFXS_GET_STATS, stats);
	if(res<0)
	{
		printf("error in get wcfxs stats: %s\r\n", strerror(errno));
	}
	else 
	{
		printf("TIP: %7.4f Volts\n", (float)stats->tipvolt / 1000.0);
		printf("RING: %7.4f Volts\n", (float)stats->ringvolt / 1000.0);
		printf("VBAT: %7.4f Volts\n", (float)stats->batvolt / 1000.0);
	}
	return res;	
}


int as_get_wcfxs_regs(int fd,struct wcfxs_regs *regs)
{
	
	int res,x;
	res = ioctl( fd, WCFXS_GET_REGS, regs);
	if(res<0)
	{
		printf("error in get wcfxs regs: %s\r\n", strerror(errno));
	}
	else
	{
		printf("Direct registers: \n");
		for(x=0;x<NUM_REGS;x++)
		{
			printf("%3d. %02x  ", x, regs->direct[x]);
			if ((x % 8) == 7)
			  printf("\n");
		}
		printf("\n\nIndirect registers: \n");
		for (x=0;x<NUM_INDIRECT_REGS;x++) 
		{
			printf("%3d. %04x  ", x, regs->indirect[x]);
			if ((x % 6) == 5)
			  printf("\n");
		}
	}
	return res;
}

int as_set_wcfxs_regs(int fd,struct wcfxs_regop *regop)
{
	int res;
	res = ioctl( fd, WCFXS_SET_REG, regop);
	if(res<0)
	{
		printf("error in set wcfxs reg: %s\r\n", strerror(errno));
	}
	return res;
}



char *as_dev_event_debug(int fd, int event)
{
	char *event_str;
	int signal;
	
	event_str = (char *)malloc(64);
	if(!event_str)
	{
		printf("Memory failed\r\n");
		exit(1);
	}
	memset(event_str, 0, 64);
	
	switch(event)
	{
		case AS_EVENT_NONE:
			sprintf(event_str, "None");
			break;
		case AS_EVENT_ONHOOK:
			sprintf(event_str, "On Hook");
			break;
		case AS_EVENT_RINGOFFHOOK:
			sprintf(event_str, "Ring OFF-Hook");
			break;
		case AS_EVENT_WINKFLASH:
			sprintf(event_str, "WinkFlash");
			break;
		case AS_EVENT_ALARM:
			sprintf(event_str, "Alarm");
			break;
		case AS_EVENT_NOALARM:
			sprintf(event_str, "No Alarm");
			break;
		case AS_EVENT_BADFCS:
			sprintf(event_str, "Bad FCS");
			break;
		case AS_EVENT_DIALCOMPLETE:
			sprintf(event_str, "Dial Complete");
			break;
		case AS_EVENT_RINGERON:
			sprintf(event_str, "Ringer ON");
			break;
		case AS_EVENT_RINGEROFF:
			sprintf(event_str, "Ringer OFF");
			break;
		case AS_EVENT_HOOKCOMPLETE:
			sprintf(event_str, "Hook Complete");
			break;
		case AS_EVENT_BITSCHANGED:
			sprintf(event_str, "BITS Changed");
			break;
		case AS_EVENT_PULSE_START:
			sprintf(event_str, "Pulse Start");
			break;
		case AS_EVENT_TIMER_EXPIRED:
			sprintf(event_str, "Timer Expired");
			break;
		case AS_EVENT_TIMER_PING:
			sprintf(event_str, "Timer Ping");
			break;
		case AS_EVENT_PULSEDIGIT:
			sprintf(event_str, "PULSE Digit");
			break;
		case AS_EVENT_DTMFDIGIT:
			if (ioctl(fd, AS_CTL_GET_DTMF_DETECT, &signal) == -1) 
			{
				sprintf(event_str, "IOCTL error when get DTMF digit\r\n");
			}
			else
			{
				sprintf(event_str, "DTMF Digit : '%c'" , (unsigned char)signal);
			}
			break;
		default:
			sprintf(event_str, "Unsupport event : %d(0X%x)", event, event);
			break;
	}
	
	return event_str;
}




int as_set_candence(int fd,struct as_ring_cadence *candence)
{
	int res;
	res = ioctl( fd, AS_CTL_SETCADENCE, candence);
	if(res<0)
	{
		printf("error in set candence: %s\r\n", strerror(errno));
	}
	return res;
}



int as_set_global_para(struct as_global_para *global_para)
{
	as_lib_set_zone(global_para->country);
	return 0;
}
int as_get_channel_state(struct as_channel_state **channel_states)
{
	int fd,res,chan_num;
	
	struct as_channel_state chan_state;
	int i=1;
	fd=open("/dev/asstel/span",O_RDWR);
	if (fd<0)
	{
		printf("unable to open the span:%s\n ",strerror(errno));
		return -1;
	}
	res=ioctl(fd,AS_GET_CHANNEL_NUMBER,&chan_num);
	while(chan_num>0)		
	{	
		chan_state.channel_no=i;
		//printf("chan_state.channel_no:%d\n",i);
		res=ioctl(fd,AS_GET_CHANNELS_STATES,&chan_state);
		if(res<0)
		{
			printf("error in get the channels' states: %s\r\n", strerror(errno));
		}
		else
		{
			//printf("adress:%X",*channel_states);
			(*channel_states)->type=chan_state.type;
			//printf("chan_state.type:%d\n",chan_state.type);
			(*channel_states)->channel_no=chan_state.channel_no;
			//printf("chan_state.channel_no:%d\n",chan_state.channel_no);
			(*channel_states)->available=chan_state.available;
			//printf("chan_state.available:%d\n",chan_state.available);
			(*channel_states)++;
		}
		i++;
		chan_num--;
	}
	(*channel_states)->type=0;
	(*channel_states)->channel_no=0;
	(*channel_states)->available=-1;
	(*channel_states)-=(i-1);
	close(fd);
	return res;
}




int as_set_channel_para(int channelno,struct as_channel_para *channel_para)
{
	
	struct as_bufferinfo *bufinfo;
	struct as_channel_state *chan_info;
	char *name;
	int fd,i,res;
	i=0;
	name=(char *)malloc(18*sizeof(char));
	bufinfo=(struct as_bufferinfo *)malloc(sizeof(struct as_bufferinfo));
	
	memset(name,0,17);
	if (channelno !=-1)
	{
		sprintf(name,"/dev/asstel/%d",channelno);
		printf("%s\n",name);
		fd = open(name, O_RDWR);
		if (fd < 0) 
		{
			printf( "Unable to open %s: %s\n", name, strerror(errno));
			free(name);
			free(bufinfo);
			return -1;			
		}
		as_set_law(fd,channel_para->law);
		as_set_block_size( fd, channel_para->block_size);
		as_buf_get_info( fd, bufinfo);
		bufinfo->bufsize=channel_para->buffer_size;
		bufinfo->numbufs=channel_para->buffer_count;
		res=as_buf_para_set(fd, bufinfo);
		if(res<0)
		{
			printf("error in set bufinfo : %s\r\n", strerror(errno) );
		}
		as_buf_get_info( fd, bufinfo);
		close(fd);
	}
	else
	{
		i=as_get_channel_num(&i);
		chan_info=(struct as_channel_state *)malloc((i+1)*sizeof(struct as_channel_state));
		res= as_get_channel_state(&chan_info);
		if(res<0)	
		{
			printf("get channel states error\n");
			free(chan_info);
			return -1;
		}
		i=0;
		while(chan_info->available!=-1 )
		{
			if(chan_info->available==0 || chan_info->type==0)// 0 is AS_CHANNEL_TYPE_FXS
			{
				sprintf(name,"/dev/asstel/%d",chan_info->channel_no);
				printf("%s\n",name);
				fd = open(name, O_RDWR);
				if (fd < 0) 
				{
					printf( "Unable to open %s and set channel %d: %s\nplease reset latter\n", name, chan_info->channel_no, strerror(errno));
					i++;
					chan_info++;
					continue;			
				}
				as_set_law(fd,channel_para->law);
				as_set_block_size( fd, channel_para->block_size);
				as_buf_get_info( fd, bufinfo);
				bufinfo->bufsize=channel_para->buffer_size;
				bufinfo->numbufs=channel_para->buffer_count;
				as_buf_para_set(fd, bufinfo);
				close(fd);
				printf("channel %d is set successfully\n",chan_info->channel_no);
			}
			else
			{
				printf("channel %d is busy now or is not a fxs,please reset latter.\n",chan_info->channel_no);
			}
			i++;
			chan_info++;
		}
		chan_info-=i;
		free(chan_info);		
	}
	free(bufinfo);
	free(name);
	return 0;
}
int as_get_channel_num(int *chan_num)
{
	int fd,res;
	fd=open("/dev/asstel/span",O_RDWR);
	if (fd<0)
	{
		printf("unable to open the span:%s\n ",strerror(errno));
		return -1;
	}
	res=ioctl(fd,AS_GET_CHANNEL_NUMBER,chan_num);
	if(res<0)
	{
		printf("error in get the channels' num: %s\r\n", strerror(errno));
	}
	else
	{
		close(fd);
	}
	return res;
}


void as_ring(int fd)
{

	int res;
	int hook;	
	hook = AS_RING;
	printf("fd is %d\n",fd);
	res = ioctl(fd, AS_CTL_HOOK, &hook);
	if(res<0)
	{
		printf("error in HOOK IOCTL : %s\r\n", strerror(errno) );
		exit(1);
	}
	sleep(10);
	printf("stop ring\n");
	hook = AS_RINGOFF;
	res = ioctl(fd, AS_CTL_HOOK, &hook);
}
int __event_get(int fd)
{
	int j =-1;
	
	if (ioctl(fd, AS_CTL_GETEVENT, &j) == -1) 
		return -1;
	return j;
}
void as_test_sound(int fd)
{
	int res;
	char buf[LENGTH];
	
	do
	{
		res = __event_get( fd);
	}while(res != AS_EVENT_RINGOFFHOOK );
	
	printf("RING OFFHOOK : %d\r\n" , res);
	
	
	while(res !=AS_EVENT_ONHOOK)
	{
		res = read(fd, buf, LENGTH);
		if(res != LENGTH)
				printf("read Length is not correct : %d\r\n" , res);
		printf("read Length is  : %d\r\n" , res);

		res = write( fd, buf , res);
		if(res != LENGTH)
			printf("write Length is not correct : %d\r\n" , res);
		res = __event_get( fd);
	};

	
}

	

	
