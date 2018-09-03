/*
* $Id: _meshw_ioctl.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef	___MESHW_IOCTL_H__
#define	___MESHW_IOCTL_H__

#include 		"mesh.h"


#define WE_STRICT_WRITE		/* Check write buffer size */
/* I'll probably drop both the define and kernel message in the next version */

/* Debuging stuff */
#undef MESHW_IOCTL_DEBUG		/* Debug IOCTL API */
#define	MESHW_IOCTL_DEBUG		/* Debug IOCTL API */ 
#undef WE_EVENT_DEBUG		/* Debug Event dispatcher */
#undef WE_SPY_DEBUG		/* Debug enhanced spy support */

/* Options */
//#define WE_EVENT_NETLINK	/* Propagate events using rtnetlink */
#define WE_SET_EVENT		/* Generate an event on some set commands */


extern	struct meshw_ioctl_description	standard_ioctl[];
extern	int standard_ioctl_num;

/*
 * Return the driver handler associated with a specific Wireless Extension.
 * Called from various place, so make sure it remains efficient.
 */
static inline meshw_handler get_handler(MESH_DEVICE *dev, unsigned int cmd)
{
	/* Don't "optimise" the following variable, it will crash */
	unsigned int	index;		/* *MUST* be unsigned */

	if(dev->wireless_handlers == NULL)
		return NULL;

	/* Try as a standard command */
	index = cmd - MESHW_IO_FIRST;
	if(index < dev->wireless_handlers->num_standard)
		return dev->wireless_handlers->standard[index];

	/* Try as a private command */
	index = cmd - MESHW_IO_FIRST_PRIV;
	if(index < dev->wireless_handlers->num_private)
		return dev->wireless_handlers->private[index];

	/* Not found */
	return NULL;
}

/*
 * Call the commit handler in the driver
 * (if exist and if conditions are right)
 *
 * Note : our current commit strategy is currently pretty dumb,
 * but we will be able to improve on that...
 * The goal is to try to agreagate as many changes as possible
 * before doing the commit. Drivers that will define a commit handler
 * are usually those that need a reset after changing parameters, so
 * we want to minimise the number of reset.
 * A cool idea is to use a timer : at each "set" command, we re-set the
 * timer, when the timer eventually fires, we call the driver.
 * Hopefully, more on that later.
 *
 * Also, I'm waiting to see how many people will complain about the
 * netif_running(dev) test. I'm open on that one...
 * Hopefully, the driver will remember to do a commit in "open()" ;-)
 */
static inline int call_commit_handler(MESH_DEVICE *dev)
{
	if((meshif_running(dev)) && (dev->wireless_handlers->standard[0] != NULL))
	{/* Call the commit handler on the driver */
		return dev->wireless_handlers->standard[0](dev, NULL,NULL, NULL);
	}
	else
		return 0;		/* Command completed successfully */
}

static inline struct meshw_statistics *get_wireless_stats(MESH_DEVICE *dev)
{
	return (dev->get_wireless_stats ?dev->get_wireless_stats(dev):(struct meshw_statistics *) NULL);
}

#endif

