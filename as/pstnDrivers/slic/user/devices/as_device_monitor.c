#include "as_global.h"

typedef struct poll_table
{
	struct pollfd   	*pfds;
	int				dev_ids[AS_DEVICE_MAX];
}poll_table_t;


static as_thread_t  calling_thread =
{
	name	:	"CALLING_THREAD",
	handler	:	as_thread_calling_agent,
	log		: 	printf,
	
	private	:	NULL
};

static as_thread_t  callee_thread =
{
	name	:	"CALLEE_THREAD",
	handler	:	as_thread_callee_agent,
	log		: 	printf,
	
	private	:	NULL
};

void *as_monitor_by_poll(as_span_t  *span)
{
	as_device_t *dev = span->devs;
	as_device_t  *_dev;
	as_span_event_t *event;
	as_call_packet_t *packet;
	
	poll_table_t  table=
	{
		pfds:NULL
	};
	
	int res, res2, pollres=0;
	char buf[1024];
	int duration = TIMEOUT_DEVICE_MONITOR * 1000 ; /* ms */
	int count =0;
	int total = 0 , last =0;
	int i;
	int required_dev_id = -1;
	int timeout;

	timeout = TIMEOUT_FOREVER;
	
		
	for(;;) 
	{
		count =0;
		total = 0;
		required_dev_id = -1;
		memset(table.dev_ids, -1 ,  AS_DEVICE_MAX);
//		trace
		event = as_span_dequeue_event( span);
		if(event)
		{
			int length = event->length;
			packet = (as_call_packet_t *) event->data;
			as_net_debug_packet(packet);

			trace
				required_dev_id = event->dev_id;
				printf("Callee thread is wait device with id %d, require type is %d\r\n" , required_dev_id, packet->type);
//			if(packet->type == AS_CALL_TYPE_INVITE)
			{
				free(event);
				_dev = as_device_get(required_dev_id);
//				(_dev->unlock)(dev);
//				(_dev->lock)(dev);
				if(_dev->state != AS_DEVICE_STATE_USED)
				{
					callee_thread.private = _dev;
					_dev->private = event;
		
					_dev->state = AS_DEVICE_STATE_USED;
					as_thread_create( &callee_thread);
				}	
			}
		}

		for(i=0; i< AS_DEVICE_MAX; i++)
		{
			_dev = dev+i;
			if(_dev->state != AS_DEVICE_STATE_USED)
			{
				res =  ( _dev->lock_unblock)(_dev );
				if(res == AS_YES )
				{
					table.dev_ids[count] = _dev->device_id;
					total++;
					count++;
				}	
			}	
			else
				printf("dev %d state is not used\n",i);
		}

		if(last != total)
		{
			printf("Monitor %d device in main thread\r\n", total);
			last = total;
		}
			for(i=0;i<total;i++)
				printf("%d ", table.dev_ids[i]);
			printf(", total is %d\r\n", total);
		

		if (table.pfds)
			free( table.pfds);
		table.pfds = (struct pollfd *)malloc( total *sizeof(struct pollfd));
		if (!table.pfds) 
		{
			printf( "Critical memory error.  Zap dies.\n");
			return NULL;
		}
		memset(table.pfds, 0 ,  total * sizeof(struct pollfd));
		
		memset(buf, 0, 1024);

		count =0;
//		for(i=0; i< AS_DEVICE_MAX; i++)
		for(i=0; i<total; i++)
		{
//			printf("index=%d\r\n", table.dev_ids[i]);
			_dev = dev+ table.dev_ids[i]-1;
			//if(agent->chanmap[i])
//			if(  ((dev+i)->lock_unblock)(dev+i) == AS_YES )
			{
				table.pfds[count].fd = _dev->fd;
				table.pfds[count].events = POLLPRI;
				
				count++;
//		if(dev->state!= AS_CALL_STATE_ONHOOK)
//			pfds[count].events |= POLLIN;
			}
		}

		res = poll(table.pfds,  total,  duration );
		if (res < 0) 
		{
			if ((errno != EAGAIN) && (errno != EINTR))
				printf( "poll return %d: %s in main monitor thread\n", res, strerror(errno));
			continue;
		}
		if ( res == 0)
		{/* timeout */
//			as_call_timeout_handler(dev);
			for(i=0; i<total; i++)
			{
				_dev = dev+ table.dev_ids[i]-1;
				(_dev->unlock)(_dev);
			}
			continue;
		}
			trace

		for(i=0; i<total; i++)
		{
			pollres = table.pfds[i].revents;
			_dev = dev+ table.dev_ids[i]-1;
			(_dev->unlock)(_dev);

			if (pollres & POLLPRI) 
			{
			trace
				
				res = as_event_get( _dev->fd);
				if (res <0 ) 
				{
					printf( "Get event failed : %s!\n", strerror(errno));
				}
				if(res == AS_EVENT_RINGOFFHOOK|| res ==AS_EVENT_WINKFLASH)
				{
					trace
					calling_thread.private = _dev;
					_dev->state = AS_DEVICE_STATE_USED;
//					(_dev->lock)(_dev);
					as_thread_create(&calling_thread);
				}
				else
				{
					as_handle_init_event(_dev, res);
				}	
			}
			else
			{
			}
		}
	}
	
	return NULL;
}

void *as_thread_device_monitor(void *data)
{
	as_thread_t *thread = (as_thread_t *)data;

	as_span_t *span = (as_span_t *)thread->private;
	
	(thread->log)("Thread '%s' is running...\r\n", thread->name );
	
	as_monitor_by_poll( span );
	return NULL;
}


