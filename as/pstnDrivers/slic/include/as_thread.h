/*
 * $Author: lizhijie $
 * $Log: as_thread.h,v $
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
 * Revision 1.2  2004/12/11 05:39:12  lizhijie
 * modify the function declaration
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#ifndef __AS_THREAD_H__
#define __AS_THREAD_H__
#include "as_global.h"

#include "pthread.h"

//void *(run)(void *data);

//typedef void*(*run)(void *data)  as_thread_handler;

struct AS_THREAD
{
	pthread_t  id;
	char name[AS_MAX_NAME_LENGTH];
	void*(*handler)(void *data);
	int (*log)(const char *format, ...);
	
	void *private; 					/* private data for this thread : pointer to device which thread used */
};

typedef struct AS_THREAD as_thread_t;

int as_thread_create(as_thread_t *thread);

#endif

