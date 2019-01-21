/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_thread.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/12/11 05:40:32  lizhijie
 * modify some header file
 *
 * Revision 1.1  2004/11/25 07:24:15  lizhijie
 * move these files from user to userlib
 *
*/

#include "as_thread.h"

int as_thread_create(as_thread_t *thread)
{
	int res;
	
//	pthread_t threadid;
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	(thread->log)("Thread %s created\r\n", thread->name);
	
	res = pthread_create(&thread->id, &attr, thread->handler, thread);
	if( res)
	{
		printf("Thread %s create failed\r\n" , thread->name );
		return AS_FAIL;
	}

	
	return AS_OK;	
}

