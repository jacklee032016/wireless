/*
 * $Author: lizhijie $
 * $Log: as_global.h,v $
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
#ifndef  __AS_GLOBAL_H__
#define __AS_GLOBAL_H__
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "asstel.h"
#include "as_fxs.h"

//#include <poll.h>

#include "as_call.h"
#include "as_thread.h"
#include "as_net_agent.h"

struct AS_AUDIO_OUT_AGENT
{
	as_device_t 		*dev;
	as_net_agent_t 	*udp_agent;
};

typedef struct AS_AUDIO_OUT_AGENT  audio_agent_t;


#define AS_NET_DEBUG			1

#define AS_THREAD_DEBUG		1

#define AS_EVENT_DEBUG			1


extern as_span_t  master_span;


int as_event_get(int fd);


void *as_thread_device_monitor(void *data);
void *as_thread_callee_monitor(void *data);
void *as_thread_callee_agent(void *data);
void *as_thread_calling_agent(void *data);

void *as_thread_data_sendout(void *data);

#endif

