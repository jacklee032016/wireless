#include "as_global.h"

void sendout_audio(as_device_t *dev, as_net_agent_t *agent)
{
	int law = AS_LAW_DEFAULT;
	int res;
	int count = AS_AUDIO_DATA_LENGTH;
	unsigned char buf[AS_AUDIO_DATA_LENGTH];
		
//	ioctl(dev->fd, ZT_SETLAW, &law);
	
	printf("%s enter into sendout audio state\r\n", dev->name );
	while(1)
	{
		res = read(dev->fd, buf, count);
		if(res != count)
		{
			printf("WARNING : read data is too short in device %s, recv=%d need=%d\r\n", dev->name, res, count );
//			return AS_FAIL;
		}	
		res = as_net_sendout_data( agent, (void *)buf, res);
		if(res != count)
		{
			printf("WARNING : sendout data is too short in device %s, recv=%d need=%d\r\n", dev->name , res, count );
//			return AS_FAIL;
		}	
		{
			/* check the flags to terminate this sendout thread */
		}
		printf("%s sendout %d byte to peer\r\n", dev->name, res );
	}	

}


void *as_thread_data_sendout(void *data)
{
	as_thread_t *thread = (as_thread_t *)data;
	audio_agent_t *audio_agent = (audio_agent_t *)thread->private;
	int res;
	
#if AS_THREAD_DEBUG
	(thread->log)("Thread '%s' is running...\r\n", thread->name );
#endif

	sendout_audio(audio_agent->dev, audio_agent->udp_agent);
	return NULL;
}


