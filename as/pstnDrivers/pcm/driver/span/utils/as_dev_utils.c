/*
 * $Revision: 1.1.1.1 $
 * $Log: as_dev_utils.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/06/27 06:02:38  lizhijie
 * no message
 *
 * $Author: lizhijie $
 * $Id: as_dev_utils.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

#include "asstel.h"

#include <linux/slab.h>

/* sleep in user space until woken up. Equivilant of tsleep() in BSD */
int  as_schedule_delay(wait_queue_head_t *q)
{
	/* declare a wait_queue_t variable as 'wait', and add 'current' task into it */
	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue(q, &wait);
	current->state = TASK_INTERRUPTIBLE;

	if (!signal_pending(current)) 
		schedule();
	
	/* here, this task is waken up, and going to continue */
	current->state = TASK_RUNNING;
	remove_wait_queue(q, &wait);

	if (signal_pending(current)) 
		return -ERESTARTSYS;

	return(0);
}

