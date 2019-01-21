#include "as_global.h"

as_peer_t  local =
{
	call_port		:	CALLING_AGENT_PORT,
	data_port		:	CALLING_AGENT_PORT+1,
	ip			:	"127.0.0.1"
};

as_call_packet_t packet ; 

as_thread_t calling_sendout_agent = 
{
	name 	:	"Calling Audio thread",
	handler	:	as_thread_data_sendout,
	log		:	printf,

	private	:	NULL
};

	as_net_agent_t calling_udp;
	audio_agent_t audio_agent;


int as_calling_retrieve_call_number( as_device_t *dev )
{
	int count, res, res2, pollres=0;
	char buf[1024];
	struct pollfd *pfds=NULL;
	int duration = -1000;
	int signal;

	memset(dev->calling_plan->signals, 0, AS_DIALING_MAX_NUM);
	dev->calling_plan->position = 0;
	
	for(;;) 
	{
		if (pfds)
			free(pfds);

		pfds = (struct pollfd *)malloc( sizeof(struct pollfd));
		if (!pfds) 
		{
			printf( "Critical memory error.  Zap dies.\n");
			exit(1);
		}
		memset(pfds, 0 ,  sizeof(struct pollfd));
		
		memset(buf, 0, 1024);
		count = 0;

		pfds[count].fd = dev->fd;
		pfds[count].events = POLLPRI|POLLIN;

		count = 1;

//		printf("polling..\r\n");
		res = poll(pfds, count,  dev->calling_plan->timeout *1000 );
		if (res < 0) 
		{
			if ((errno != EAGAIN) && (errno != EINTR))
				printf( "poll return %d: %s\n", res, strerror(errno));
			continue;
		}
		if ( res == 0)
		{
			printf("polling  is timeout (%d seconds) in calling thread\r\n" ,dev->timeout );
			as_tone_play_stop( dev);
			as_tone_play_congestion(dev);
			return AS_FAIL;
		}
		
		pollres = pfds[0].revents;
		if (pollres & POLLPRI) 
		{
			res = as_event_get( dev->fd);
			if (res <0 ) 
			{
				printf( "Get event failed : %s!\n", strerror(errno));
			}
			if( res==AS_EVENT_DTMFDIGIT)
			{
#if AS_EVENT_DEBUG			
				printf("Detect DTMF signal\r\n");
#endif	
				if (ioctl(dev->fd, AS_CTL_GET_DTMF_DETECT, &signal) == -1) 
				{
					printf("IOCTL error\r\n");
				}
				else
				{
#if AS_EVENT_DEBUG			
					printf("DTMF signal is '%c' \r\n", (unsigned char)signal);
#endif
					dev->calling_plan->signals[dev->calling_plan->position] = (unsigned char)signal;
					dev->calling_plan->position ++;
					if(dev->calling_plan->position == dev->calling_plan->count)
					{
#if AS_EVENT_DEBUG			
						printf("receive calling number is '%s'\r\n", dev->calling_plan->signals);
#endif

						return AS_OK;
						//as_tone_play
#if 0
						if (pollres & POLLIN) 
							return AS_OK;
						else
						{
							printf("device %s is not in IO state now\r\n" , dev->name);
							return AS_FAIL;
						}
#endif						
					}	
				}
				
			}
			else if(res==AS_EVENT_ONHOOK)
			{
#if AS_EVENT_DEBUG			
				printf("Receive ONHOOK event in calling thread\r\n");
#endif
				return AS_FAIL;
			}
			else
#if AS_EVENT_DEBUG			
				printf("Detect signal %d\r\n", res);
#endif
			
		}
		else
		{
			char buf[160];
			res = read(dev->fd, buf, sizeof(buf));
			if (res > 0) 
			{
//				printf("POLLIN read \r\n" );
				/* We read some number of bytes.  Write an equal amount of data */
//				res2 = write(dev->fd, /*i->cidspill + i->cidpos*/buf , res);
//				if (res2 != res ) 
//				{
//					printf( "Write failed: %s\n", strerror(errno));
//				}
			} 
		}
	}
	return AS_FAIL;
}

int calling_in_progress(as_thread_t *thread)
{
	as_device_t  *dev = (as_device_t *)thread->private;

	unsigned char buf[4096];
	int res;
	int count = 160;
	unsigned char  from[20];
	
	memset(buf, 0 ,160);
	sprintf(buf, "%s", "INVITE");
	int law =AS_LAW_DEFAULT;
	ioctl(dev->fd, AS_CTL_SETLAW, &law);

	calling_udp.private = (void *)thread;

	/* search calling path in system's calling map */

	/* create a callee thread, monitor callee and local device */
	as_net_init(&calling_udp, CALLING_AGENT_PORT, CALLING_AGENT_PORT+1 );

	memset(&packet, 0 ,sizeof(as_call_packet_t ));
	packet.type = AS_CALL_TYPE_INVITE;
	memcpy(&(packet.peer), &local, sizeof(as_peer_t));
	sprintf( packet.msg, "%s", dev->calling_plan->signals);
	packet.length = dev->calling_plan->count;

//	as_net_debug_packet(&packet);
	as_net_sendto_ip( &packet, sizeof(as_call_packet_t ), "127.0.0.1", CALLEE_MONITOR_PORT);

	trace
	while(1)
	{	
		trace
		res = as_net_check_message(&calling_udp) ;
		if(res == AS_CALL_AVAILABLE)
		{
			as_call_packet_t *packet;
			trace
			memset(buf, 0, 4096);
			as_net_read_call_from(&calling_udp, (void *)buf, from);

#if  AS_NET_DEBUG			
			printf("Thread %s received repsonse from peer %s:\r\n", thread->name, from );
#endif

			packet = (as_call_packet_t *)buf;
			if(packet->type == AS_CALL_TYPE_ACK)
			{
				as_tone_play_stop(dev);
//				as_gsm_file_play("sound/auth-thankyou.gsm",dev->fd);
				as_net_debug_packet( packet);
				as_net_init_socket_addr(&calling_udp, &packet->peer.ip, packet->peer.call_port, &packet->peer.ip, packet->peer.data_port);
//				as_calling_to_remote_conversation(dev, &calling_udp);
				audio_agent.dev = dev;
				audio_agent.udp_agent = &calling_udp;
				calling_sendout_agent.private = &audio_agent;


//				sendout_audio(dev, &calling_udp);

				as_thread_create(&calling_sendout_agent);
				
			}
			if(packet->type == AS_CALL_TYPE_END)
			{/* end this thread and data sendout thread related to this thread */
//				as_gsm_file_play("sound/auth-thankyou.gsm",dev->fd);
			}
		}
		else if(res == AS_DATA_AVAILABLE )
		{
			trace
			res = as_net_read_data_from(&calling_udp, buf, from);
			res = write(dev->fd, buf, res );
			if(res <= 0)
			{
				printf("Calling wrote fail, because of %s\r\n" , strerror(errno));
			}
#if AS_NET_DEBUG
			printf("Calling wrote length is %d to device %s\r\n", res, dev->name );
#endif		
			
		}
		else
		{
			trace
		}
		trace
	}

	return 0;
}

void *as_thread_calling_agent(void *data)
{
	int res;
	as_thread_t *thread = (as_thread_t *)data;
	as_device_t *dev = (as_device_t *)thread->private;

	if( !dev )
	{
		(thread->log)("No device is provided for %s thread\r\n" , thread->name );
		return NULL;
	}	
	/*  */
	(dev->lock)(dev);
	
	(thread->log)("Thread '%s' is using device '%s'...\r\n", thread->name ,dev->name );

	as_tone_play_dial(dev);
	trace
	res = as_calling_retrieve_call_number( dev);
	if(res == AS_FAIL)
	{
		dev->state = AS_DEVICE_STATE_AVAILABLE;
	}
	else
	{
		as_tone_play_stop(dev);
		printf("Receive thread enter into calling process\r\n");
		as_tone_play_ring( dev);
		calling_in_progress( thread);
	}	

	dev->state = AS_DEVICE_STATE_AVAILABLE;
	(dev->unlock)(dev);
	printf("Calling thread exit now\r\n");
	pthread_exit(NULL);
	return 0;
}


