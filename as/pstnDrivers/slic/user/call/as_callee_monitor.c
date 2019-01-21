#include "as_global.h"


static as_net_agent_t _callee_udp;

void *as_thread_callee_monitor(void *data)
{
	as_thread_t *thread = (as_thread_t *)data;
	as_span_t  *span = (as_span_t *)thread->private;
	int res;
	unsigned char buf[4096];
	unsigned char from[20];
	
#if AS_THREAD_DEBUG
	(thread->log)("Thread '%s' is running...\r\n", thread->name );
#endif

	_callee_udp.private = (void *)thread;
	/* although no data packet but call packet send to callee monotor */
	as_net_init(&_callee_udp, CALLEE_MONITOR_PORT, CALLEE_MONITOR_PORT+1);

	while(1)
	{
		if( (res = as_net_check_message(&_callee_udp)  ))
		{
			memset(buf, 0, 4096);
			res = as_net_read_call_from(&_callee_udp, (void *)buf,  from);
			if(res >0 )
			{
#if  AS_NET_DEBUG			
				printf("Thread %s receive msg , length is %d from %s\r\n", thread->name, res, from);	
//				as_net_debug_packet((as_call_packet_t *)buf );
#endif
				as_span_queue_event(span , buf,  res );
#if  AS_NET_DEBUG
				printf("Thread %s send msg to thread device monitor to require the device \r\n", thread->name );
#endif

			}
		}
		
	}

	as_udp_close(&_callee_udp);
	
	return NULL;
}


