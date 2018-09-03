/*
* $Id: meshw_ioctl_spy.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include 		"_meshw_ioctl.h"

/********************** ENHANCED IWSPY SUPPORT **********************/
/*
 * In the old days, the driver was handling spy support all by itself.
 * Now, the driver can delegate this task to Wireless Extensions.
 * It needs to use those standard spy meshw_handler in struct meshw_handler_def,
 * push data to us via XXX and include struct meshw_spy_data in its
 * private part.
 * One of the main advantage of centralising spy support here is that
 * it becomes much easier to improve and extend it without having to touch
 * the drivers. One example is the addition of the Spy-Threshold events.
 * Note : MESHW_WIRELESS_SPY is defined in meshw_handler.h
 */

/* Standard Wireless Handler : set Spy List */
int iw_handler_set_spy(MESH_DEVICE *dev, struct meshw_request_info *info, union meshw_req_data *wrqu, char *extra)
{
#ifdef MESHW_WIRELESS_SPY
	struct meshw_spy_data *	spydata = (dev->priv + dev->wireless_handlers->spy_offset);
	struct sockaddr *	address = (struct sockaddr *) extra;

	/* Disable spy collection while we copy the addresses.
	 * As we don't disable interrupts, we need to do this to avoid races.
	 * As we are the only writer, this is good enough. */
	spydata->spy_number = 0;

	/* Are there are addresses to copy? */
	if(wrqu->data.length > 0)
	{
		int i;

		/* Copy addresses */
		for(i = 0; i < wrqu->data.length; i++)
			memcpy(spydata->spy_address[i], address[i].sa_data, ETH_ALEN);
		/* Reset stats */
		memset(spydata->spy_stat, 0, sizeof(struct meshw_quality) * MESHW_MAX_SPY);

#ifdef WE_SPY_DEBUG
		MESH_DEBUG_INFO(MESH_DEBUG_IOCTL, "iw_handler_set_spy() :  offset %ld, spydata %p, num %d\n", dev->wireless_handlers->spy_offset, spydata, wrqu->data.length);
		for (i = 0; i < wrqu->data.length; i++)
			MESH_DEBUG_INFO(MESH_DEBUG_IOCTL, 
			       "%02X:%02X:%02X:%02X:%02X:%02X \n",
			       spydata->spy_address[i][0],
			       spydata->spy_address[i][1],
			       spydata->spy_address[i][2],
			       spydata->spy_address[i][3],
			       spydata->spy_address[i][4],
			       spydata->spy_address[i][5]);
#endif	/* WE_SPY_DEBUG */
	}
	/* Enable addresses */
	spydata->spy_number = wrqu->data.length;

	return 0;
#else /* MESHW_WIRELESS_SPY */
	return -EOPNOTSUPP;
#endif /* MESHW_WIRELESS_SPY */
}

/* Standard Wireless Handler : get Spy List */
int iw_handler_get_spy(MESH_DEVICE *dev,
		       struct meshw_request_info *	info,
		       union meshw_req_data *	wrqu,
		       char *			extra)
{
#ifdef MESHW_WIRELESS_SPY
	struct meshw_spy_data *	spydata = (dev->priv +
					   dev->wireless_handlers->spy_offset);
	struct sockaddr *	address = (struct sockaddr *) extra;
	int			i;

	wrqu->data.length = spydata->spy_number;

	/* Copy addresses. */
	for(i = 0; i < spydata->spy_number; i++)
	{
		memcpy(address[i].sa_data, spydata->spy_address[i], ETH_ALEN);
		address[i].sa_family = AF_UNIX;
	}

	/* Copy stats to the user buffer (just after). */
	if(spydata->spy_number > 0)
		memcpy(extra  + (sizeof(struct sockaddr) *spydata->spy_number),
		       spydata->spy_stat,
		       sizeof(struct meshw_quality) * spydata->spy_number);
	/* Reset updated flags. */
	for(i = 0; i < spydata->spy_number; i++)
		spydata->spy_stat[i].updated = 0;
	return 0;
#else /* MESHW_WIRELESS_SPY */
	return -EOPNOTSUPP;
#endif /* MESHW_WIRELESS_SPY */
}

/* Standard Wireless Handler : set spy threshold */
int iw_handler_set_thrspy(MESH_DEVICE *dev,
			  struct meshw_request_info *info,
			  union meshw_req_data *	wrqu,
			  char *		extra)
{
#ifdef MESHW_WIRELESS_THRSPY
	struct meshw_spy_data *	spydata = (dev->priv +dev->wireless_handlers->spy_offset);
	struct meshw_thrspy *	threshold = (struct meshw_thrspy *) extra;

	/* Just do it */
	memcpy(&(spydata->spy_thr_low), &(threshold->low),
	       2 * sizeof(struct meshw_quality));

	/* Clear flag */
	memset(spydata->spy_thr_under, '\0', sizeof(spydata->spy_thr_under));

#ifdef WE_SPY_DEBUG
	MESH_DEBUG_INFO(MESH_DEBUG_IOCTL, "iw_handler_set_thrspy() :  low %d ; high %d\n", spydata->spy_thr_low.level, spydata->spy_thr_high.level);
#endif	/* WE_SPY_DEBUG */

	return 0;
#else /* MESHW_WIRELESS_THRSPY */
	return -EOPNOTSUPP;
#endif /* MESHW_WIRELESS_THRSPY */
}

/*
 * Standard Wireless Handler : get spy threshold
 */
int iw_handler_get_thrspy(MESH_DEVICE *dev, struct meshw_request_info *info, union meshw_req_data *wrqu, char *extra)
{
#ifdef MESHW_WIRELESS_THRSPY
	struct meshw_spy_data *	spydata = (dev->priv + dev->wireless_handlers->spy_offset);
	struct meshw_thrspy *	threshold = (struct meshw_thrspy *) extra;

	/* Just do it */
	memcpy(&(threshold->low), &(spydata->spy_thr_low), 2 * sizeof(struct meshw_quality));

	return 0;
#else /* MESHW_WIRELESS_THRSPY */
	return -EOPNOTSUPP;
#endif /* MESHW_WIRELESS_THRSPY */
}

#ifdef MESHW_WIRELESS_THRSPY
/*
 * Prepare and send a Spy Threshold event
 */
static void iw_send_thrspy_event(MESH_DEVICE *	dev,	 struct meshw_spy_data *spydata, unsigned char *address, struct meshw_quality *wstats)
{
	union meshw_req_data	wrqu;
	struct meshw_thrspy	threshold;

	/* Init */
	wrqu.data.length = 1;
	wrqu.data.flags = 0;
	/* Copy address */
	memcpy(threshold.addr.sa_data, address, ETH_ALEN);
	threshold.addr.sa_family = ARPHRD_ETHER;
	/* Copy stats */
	memcpy(&(threshold.qual), wstats, sizeof(struct meshw_quality));
	/* Copy also thresholds */
	memcpy(&(threshold.low), &(spydata->spy_thr_low), 2 * sizeof(struct meshw_quality));

#ifdef WE_SPY_DEBUG
	MESH_DEBUG_INFO(MESH_DEBUG_IOCTL, "iw_send_thrspy_event() : address %02X:%02X:%02X:%02X:%02X:%02X, level %d, up = %d\n",
	       threshold.addr.sa_data[0],
	       threshold.addr.sa_data[1],
	       threshold.addr.sa_data[2],
	       threshold.addr.sa_data[3],
	       threshold.addr.sa_data[4],
	       threshold.addr.sa_data[5], threshold.qual.level);
#endif	/* WE_SPY_DEBUG */

	/* Send event to user space */
	wireless_send_event(dev, MESHW_IO_R_SPY_THR, &wrqu, (char *) &threshold);
}
#endif /* MESHW_WIRELESS_THRSPY */

/*
 * Call for the driver to update the spy data.
 * For now, the spy data is a simple array. As the size of the array is
 * small, this is good enough. If we wanted to support larger number of
 * spy addresses, we should use something more efficient...
 */
void wireless_spy_update(MESH_DEVICE *dev, unsigned char *address, struct meshw_quality *wstats)
{
#ifdef MESHW_WIRELESS_SPY
	struct meshw_spy_data *	spydata = (dev->priv +
					   dev->wireless_handlers->spy_offset);
	int			i;
	int			match = -1;

#ifdef WE_SPY_DEBUG
	MESH_DEBUG_INFO(MESH_DEBUG_IOCTL, "wireless_spy_update() :  offset %ld, spydata %p, address %02X:%02X:%02X:%02X:%02X:%02X\n", dev->wireless_handlers->spy_offset, spydata, address[0], address[1], address[2], address[3], address[4], address[5]);
#endif	/* WE_SPY_DEBUG */

	/* Update all records that match */
	for(i = 0; i < spydata->spy_number; i++)
		if(!memcmp(address, spydata->spy_address[i], ETH_ALEN))
		{
			memcpy(&(spydata->spy_stat[i]), wstats, sizeof(struct meshw_quality));
			match = i;
		}
#ifdef MESHW_WIRELESS_THRSPY
	/* Generate an event if we cross the spy threshold.
	 * To avoid event storms, we have a simple hysteresis : we generate
	 * event only when we go under the low threshold or above the
	 * high threshold. */
	if(match >= 0)
	{
		if(spydata->spy_thr_under[match])
		{
			if(wstats->level > spydata->spy_thr_high.level)
			{
				spydata->spy_thr_under[match] = 0;
				iw_send_thrspy_event(dev, spydata, address, wstats);
			}
		}
		else
		{
			if(wstats->level < spydata->spy_thr_low.level)
			{
				spydata->spy_thr_under[match] = 1;
				iw_send_thrspy_event(dev, spydata, address, wstats);
			}
		}
	}
#endif /* MESHW_WIRELESS_THRSPY */
#endif /* MESHW_WIRELESS_SPY */
}

