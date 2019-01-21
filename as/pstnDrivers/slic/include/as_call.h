/*
 * $Author: lizhijie $
 * $Log: as_call.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/04/26 06:06:10  lizhijie
 * *** empty log message ***
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:46  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:04  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:51  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:09  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#ifndef  __AS_CALL_H__

#define __AS_CALL_H__

#include "pthread.h"

#include "as_fifo.h"

#define AS_AUDIO_DATA_LENGTH		1024	/*20 mill second*/

#define AS_OK						0
#define AS_FAIL						-1

#define AS_YES						1
#define AS_NO						0

#define AS_TIMEOUT					-2

#define AS_CALL_NAME_LENGTH		128
#define AS_MAX_NAME_LENGTH		64

#define AS_DEVICE_MAX				3

#define AS_DIALING_MAX_NUM		15

#define TIMEOUT_FOREVER			-1 /* for system call : poll */
#define TIMEOUT_DEVICE_MONITOR	1	/* second. for every device's state change event */

typedef enum 
{
	AS_DEVICE_STATE_INVALIDATE = -1, /* file descriptor is not initted */
	AS_DEVICE_STATE_AVAILABLE,		/* available for device monitor thread */
	AS_DEVICE_STATE_USED				/* used by calling or callee thread */
}AS_DEVICE_STATE;

typedef enum
{
	AS_CHANNEL_ON_HOOK,
	AS_CHANNEL_OFF_HOOK,
	AS_CHANNEL_RETRIEVE_NUMBER,
	AS_CHANNEL_CALLING,
	AS_CHANNEL_CONVERSATION,
	AS_CHANNEL_ENDING
}AS_CHANNEL_STATE;


typedef enum 
{
	AS_SIGNAL_TYPE_FXS,
	AS_SIGNAL_TYPE_FXO	
}AS_SIGNAL_TYPE;

#if 0
typedef enum
{
	AS_TONE_STOP	= -1,
	AS_TONE_DIALTONE,
	AS_TONE_BUSY,
	AS_TONE_RINGTONE,
	AS_TONE_CONGESTION,
	AS_TONE_CALLWAIT,
	AS_TONE_DIALRECALL,
	AS_TONE_RECORDTONE,
	AS_TONE_INFO,
	AS_TONE_CUST1,
	AS_TONE_CUST2	,
	AS_TONE_STUTTER,
}AS_TONES_TYPE;
#endif

#define AS_TONE_MAX			16

#define AS_MAX_CADENCE			16

#define	AS_MAX_DTMF_DIGITS 	128



typedef struct 
{
	int timeout;	/*second, for DTMF signal */
	int count;	/* code needed */
	int position;	/* currently position */
	unsigned char  signals[AS_DIALING_MAX_NUM];
}AS_CALLING_PLAN;

struct AS_SPAN;

struct AS_DEVICE;

struct AS_DEVICE
{
	char 				name[AS_CALL_NAME_LENGTH];
	int 					device_id;
	AS_SIGNAL_TYPE		signal_type;
	AS_CALLING_PLAN		*calling_plan;
	int 					timeout; 		/* seconds */
	
	int 					fd;
	AS_DEVICE_STATE	state;

	pthread_mutex_t		mutex;
	pthread_cond_t 		cond;

	int (* lock)(struct AS_DEVICE *dev);
	int (* lock_unblock)(struct AS_DEVICE *dev);
	int (* unlock)(struct AS_DEVICE *dev);
	int (* wakeup)(struct AS_DEVICE *dev);
	int (* wait)(struct AS_DEVICE *dev);
	
	struct AS_SPAN		*span;
	void 				*private;
};

typedef struct AS_DEVICE as_device_t;

typedef struct 
{
	char name[AS_MAX_NAME_LENGTH];
	AS_CHANNEL_STATE  state;
	
	as_device_t  *dev;

	void *private;
}AS_CHANNEL;

typedef enum
{
	AS_CALLING_LOCAL,
	AS_CALLING_REMOTE	
}AS_CALLING_TYPE;


struct AS_CALLING_MAP
{
	AS_CALLING_TYPE 		type;
	unsigned char 			callee_id[AS_DIALING_MAX_NUM];
	as_device_t				*dev;
	struct AS_CALLING_MAP	*next;
};

typedef struct AS_CALLING_MAP  as_calling_map_t;

struct AS_SPAN
{
	char name[AS_MAX_NAME_LENGTH];

	int timeout; /* in ms */
	unsigned char chanmap[AS_DEVICE_MAX]; /*channel status byte map */
	
	as_device_t	*devs;
	as_fifo_t		*event_queue; 		/* event queue between device monitor and callee monitor */
	void *private;
	
};

typedef struct AS_SPAN as_span_t;

struct AS_SPAN_EVENT
{
	int dev_id;
	int type;
	int length;
	unsigned char data[4096];
} __attribute__ ((packed));

typedef struct AS_SPAN_EVENT as_span_event_t;

#define CALLEE_MONITOR_PORT	5060

#define CALLING_AGENT_PORT		5090


#include "as_gsm.h"

#endif

