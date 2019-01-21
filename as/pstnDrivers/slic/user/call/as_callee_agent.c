#include "as_global.h"

#define AS_CALLEE_AGENT_PORT_START		5100

/* callee thread currently, it is used to determine the port number */
static int _callee_count = 0; 

as_thread_t callee_sendout_agent = 
{
	name 	:	"Callee Audio thread",
	handler	:	as_thread_data_sendout,
	log		:	printf,

	private	:	NULL
};

	as_net_agent_t callee_udp;
	audio_agent_t callee_audio_agent;


static as_net_agent_t *__as_callee_init_net_agent( as_peer_t *peer)
{
	as_net_agent_t *agent;
	int res;
	
	int call_port ;
	_callee_count++;
	call_port = AS_CALLEE_AGENT_PORT_START + _callee_count * 2;

	agent = (as_net_agent_t *)malloc(sizeof(as_net_agent_t ));
	if(agent == NULL)
	{
		printf("No memory available now, program exit\r\n");
		exit(1);
	}

#if AS_NET_DEBUG
	printf("callee thread call port %d\r\n", call_port);
#endif

	res = as_net_init(agent,  call_port, call_port+1 );
	if(res==AS_FAIL )
		return NULL;
	res = as_net_init_socket_addr(agent, peer->ip, peer->call_port, peer->ip, peer->data_port);
	if(res==AS_FAIL )
		return NULL;
	memcpy(&agent->peer , peer, sizeof(as_peer_t) );
	return agent;
}


static void __as_callee_terminate(as_device_t *dev, as_net_agent_t *agent)
{
	(dev->unlock)(dev);
	dev->state = AS_DEVICE_STATE_AVAILABLE;
	as_net_close( agent);
	free(agent);
	_callee_count--;
}

static int __as_callee_response( as_device_t *dev, as_net_agent_t *agent)
{
	int law = AS_LAW_ALAW;
	int res;
	int count;
	unsigned char audio_buf[AS_AUDIO_DATA_LENGTH];
	unsigned char call_buf[4096];
	unsigned char from[20];
	int linear = 0;
	
	as_call_packet_t packet;
	packet.type = AS_CALL_TYPE_ACK;
	packet.peer.call_port = AS_CALLEE_AGENT_PORT_START + _callee_count * 2;
	packet.peer.data_port = AS_CALLEE_AGENT_PORT_START + _callee_count * 2 +1;
	sprintf(packet.peer.ip, "%s", "127.0.0.1");
	packet.length = 0;

	trace
	res = as_net_sendout_call( agent, (unsigned char *)&packet, sizeof(as_call_packet_t) );
	if(res < 0)
	{
		printf("ERROR : send out ACK failed\r\n");
		return AS_FAIL;
	}


	law = AS_OFFHOOK;
	ioctl(dev->fd, AS_CTL_HOOK, &law);

	
#if 0
	ioctl( dev->fd, AS_CTL_SETLINEAR, &linear);
	sleep(1);
	ioctl(dev->fd, AS_CTL_SETLAW, &law);
	sleep(1);
#endif

//				as_gsm_file_play("sound/agent-loginok.gsm",dev->fd);
//				as_gsm_file_play("sound/demo-echotest.gsm",dev->fd);
//				as_gsm_file_play("sound/demo-echotest.gsm",dev->fd);
	printf("Callee enter into conversation\r\n");
	while(1)
	{
trace
		res = as_net_check_message(agent) ;

		if(res == AS_CALL_AVAILABLE)
		{
			as_call_packet_t *_packet;
			memset(call_buf, 0, 4096);
			as_net_read_call_from(agent, (void *)call_buf, from);
			_packet = (as_call_packet_t *)call_buf;
			if( _packet->type == AS_CALL_TYPE_END)
			{
//				as_gsm_file_play("sound/agent-loginok.gsm",dev->fd);
				as_tone_play_congestion( dev);
			}
		}
		else if(res == AS_DATA_AVAILABLE )
		{
			trace
			res = as_net_read_data_from(agent , audio_buf, from);
#if AS_NET_DEBUG
			printf("Callee received length is %d, is writing to device %s\r\n", res, dev->name );
#endif

			res = write(dev->fd, audio_buf, res );
			if(res <= 0)
			{
				printf("Callee wrote fail, because of %s\r\n" , strerror(errno));
			}
#if AS_NET_DEBUG
			printf("Callee wrote length is %d to device %s\r\n", res, dev->name );
#endif		

		}
		else
		{
			trace
		}
		trace
	
	}
}

void *as_thread_callee_agent(void *data)
{
	as_thread_t *thread = (as_thread_t *)data;
	as_device_t *dev =(as_device_t *)thread->private;
	as_span_event_t *event = dev->private;
	as_call_packet_t *packet = (as_call_packet_t *)event->data;

	as_net_agent_t *agent ;
	int res, x;

	if( !dev || !event || !packet )
		return NULL;
	(dev->lock)(dev);
	agent = __as_callee_init_net_agent( &packet->peer);
	agent->private = (void *) thread;

#if AS_THREAD_DEBUG
	(thread->log)("Thread '%s' is trying use the device '%s'...\r\n", thread->name ,dev->name );
#endif

	as_ring_with_dtmf_caller_id(dev , packet->msg);

#if AS_THREAD_DEBUG
	printf("%s is running\r\n" , thread->name );
#endif

	res = as_event_wait_hook_event( dev, AS_EVENT_RINGOFFHOOK);
	trace

#if 0	
	x = AS_OFFHOOK;
	
	res = ioctl(dev->fd, AS_CTL_HOOK, &x);
	if (res < 0) 
	{
		if (errno == EINPROGRESS) 
		{
			printf("IN PROGRESS in SET HOOK\r\n");
			return 0;
		}	
		printf( "as hook failed: %s\n", strerror(errno));
	}
#endif	

	callee_audio_agent.dev = dev;
	callee_audio_agent.udp_agent = agent;

	callee_sendout_agent.private = &callee_audio_agent;

	as_thread_create(&callee_sendout_agent);
//	if(res == AS_OK)
	{
		__as_callee_response(dev,  agent);
	}
	trace
	while(1)
		;
//	if(res == AS_NO)
	__as_callee_terminate(dev, agent);
	return NULL;
}

