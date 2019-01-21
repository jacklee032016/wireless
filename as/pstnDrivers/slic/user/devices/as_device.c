#include "as_global.h"


#define __USE_GNU
#include <pthread.h>

static int as_device_lock(as_device_t *dev);
static int as_device_lock_unblock(as_device_t *dev);
static int as_device_unlock(as_device_t *dev);
static int as_device_wakeup(as_device_t *dev);
static int as_device_wait(as_device_t *dev);

#define MUTEX_INIT_OP	PTHREAD_MUTEX_INITIALIZER//PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP

AS_CALLING_PLAN default_plan = 
{
	timeout		:	20,
	count		:	5, /*MAX phone numbers */
	position		:	0
};

static as_fifo_t	_span_event_queue;

as_span_t  master_span;

static as_device_t as_devices[AS_DEVICE_MAX]=
{
	{
		name 		:	"Device-1(FXS)",
		device_id 	: 	1,
		signal_type	:	AS_SIGNAL_TYPE_FXO,
		calling_plan	:	&default_plan,
		fd 			: 	-1,
		state 		: 	AS_DEVICE_STATE_INVALIDATE,
		
		mutex		:	MUTEX_INIT_OP,
		cond			:	PTHREAD_COND_INITIALIZER,
		
		lock			:	as_device_lock,
		lock_unblock	:	as_device_lock_unblock,
		unlock		:	as_device_unlock,
		wakeup		:	as_device_wakeup,
		wait			:	as_device_wait,
		
		span			:	&master_span,
	},
	{
		name 		:	"Device-2(FXS)",
		device_id 	: 	2,
		signal_type	:	AS_SIGNAL_TYPE_FXO,
		calling_plan	:	&default_plan,
		fd 			: 	-1,
		state 		: 	AS_DEVICE_STATE_INVALIDATE,
		
		mutex		:	MUTEX_INIT_OP,
		cond			:	PTHREAD_COND_INITIALIZER,
		
		lock			:	as_device_lock,
		lock_unblock	:	as_device_lock_unblock,
		unlock		:	as_device_unlock,
		wakeup		:	as_device_wakeup,
		wait			:	as_device_wait,
		
		span			:	&master_span,
	},
	{
		name 		:	"Device-3(FXS)",
		device_id 	: 	3,
		signal_type	:	AS_SIGNAL_TYPE_FXO,
		calling_plan	:	&default_plan,
		fd 			: 	-1,
		state 		: 	AS_DEVICE_STATE_INVALIDATE,
		
		mutex		:	MUTEX_INIT_OP,
		cond			:	PTHREAD_COND_INITIALIZER,
		
		lock			:	as_device_lock,
		lock_unblock	:	as_device_lock_unblock,
		unlock		:	as_device_unlock,
		wakeup		:	as_device_wakeup,
		wait			:	as_device_wait,
		
		span			:	&master_span,
	}
#if 0		
,
	{
		name 		:	"Device-4(FXO)",
		device_id 	: 	4,
		signal_type	:	AS_SIGNAL_TYPE_FXS,
		calling_plan	:	&default_plan,
		fd 			: 	-1,
		state 		: 	AS_DEVICE_STATE_INVALIDATE,
		
		mutex		:	MUTEX_INIT_OP,
		cond			:	PTHREAD_COND_INITIALIZER,
		
		lock			:	as_device_lock,
		lock_unblock	:	as_device_lock_unblock,
		unlock		:	as_device_unlock,
		wakeup		:	as_device_wakeup,
		wait			:	as_device_wait,
		
		span			:	&master_span,
	},
#endif	
};

as_span_t  master_span =
{
	name		:	"Digium card agent",
	timeout		:	 TIMEOUT_FOREVER,
	event_queue	:	&_span_event_queue,
	devs			:	as_devices
};

static int as_device_lock(as_device_t *dev)
{
	int res;

	trace

	res = pthread_mutex_lock(&dev->mutex);
	if(res == EDEADLK )
	{
		printf("device %s is in the state of deadlock\r\n", dev->name );
		return AS_NO;
	}
	
	return AS_YES;
}

static int as_device_lock_unblock(as_device_t *dev)
{
	int res ;
	res = pthread_mutex_trylock(&dev->mutex);
	if(res==EBUSY)
	{
		printf("device %s is used by\r\n", dev->name );
		return AS_NO;
	}
	printf("device %s is not used by\r\n", dev->name );
	return AS_YES;
}

static int as_device_unlock(as_device_t *dev)
{
//	trace

	pthread_mutex_unlock(&dev->mutex);
	return AS_YES;
}

static int as_device_wakeup(as_device_t *dev)
{
	trace

//	pthread_mutex_lock( &dev->mutex );

	trace
	pthread_cond_signal(&dev->cond);
//	pthread_mutex_unlock(&dev->mutex);
	return AS_YES;
}

static int as_device_wait(as_device_t *dev)
{
	int res;
	struct timeval  now;
	struct timespec delay ;

	if(gettimeofday(&now, NULL) )
	{
		printf("get current time failed, '%s'\r\n" ,strerror(errno));
		return NULL;
	}
	
	delay.tv_sec = now.tv_sec + 1;
	delay.tv_nsec = now.tv_usec * 1000; 

	trace
		
	pthread_mutex_lock(&dev->mutex);
	trace
//	res = pthread_cond_timedwait(&dev->cond, &dev->mutex, &delay);
	res = pthread_cond_wait(&dev->cond, &dev->mutex );
	if(res == ETIMEDOUT )
	{				/* no elements... */
		trace
		pthread_mutex_unlock(&dev->mutex);
		printf("Timeout in device '%s' wait\r\n" , dev->name );
		return AS_NO;
	}

trace
	pthread_mutex_unlock(&dev->mutex);
	return AS_YES;
}

as_device_t *as_device_get(int dev_id)
{
	int x;
	for (x=0;x<(sizeof(as_devices) / sizeof(as_devices[0])); x++) 
	{
		if ( as_devices[x].device_id==dev_id )
			return &as_devices[x];
	}
	printf("WARNNING : Device with ID is %d is not found\r\n", dev_id );
	return NULL;	
}

int as_span_queue_event(as_span_t *span, void *data, int length)
{
	as_span_event_t *event;
	
	event = (as_span_event_t *)malloc(sizeof(as_span_event_t));
	if(event==NULL)
	{
		printf("No memory alvailable \r\n");
		exit(1);
	}
	memset(event, 0, sizeof(as_span_event_t) );
	event->dev_id = 1;
	event->length = length;
	memcpy(event->data, data, length );
	
	if(as_fifo_add(span->event_queue,  (void *) event)==-1)
	{
		printf("FAIL : enqueue event in the span's event queue failed\r\n");
		return AS_FAIL;
	}	
	return AS_OK;
}

/* return device_id or 0 */
as_span_event_t *as_span_dequeue_event(as_span_t *span)
{
	as_span_event_t *event;
	
	event = (as_span_event_t *)as_fifo_tryget(span->event_queue);
	if(event==NULL)
	{
		return 0;
	}

	return event;
}

