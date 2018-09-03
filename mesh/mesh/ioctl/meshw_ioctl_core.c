/*
* $Id: meshw_ioctl_core.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include 		"_meshw_ioctl.h"

static inline int __dev_iwstats(MESH_DEVICE *dev, struct meshw_req *wrq)
{
	/* Get stats from the driver */
	struct meshw_statistics *stats;

	stats = get_wireless_stats(dev);
	if (stats != (struct meshw_statistics *) NULL)
	{
		/* Copy statistics to the user buffer */
		if(copy_to_user(wrq->u.data.pointer, stats, sizeof(struct meshw_statistics)))
			return -EFAULT;

		/* Check if we need to clear the update flag */
		if(wrq->u.data.flags != 0)
			stats->qual.updated = 0;
		return 0;
	}
	else
		return -EOPNOTSUPP;
}

/* Export the driver private handler definition
 * They will be picked up by tools like iwpriv... */
static inline int __ioctl_export_private(MESH_DEVICE *dev,  struct meshw_req *iwr)
{
	/* Check if the driver has something to export */
	if((dev->wireless_handlers->num_private_args == 0) || (dev->wireless_handlers->private_args == NULL))
	{
		ktrace;
		return -EOPNOTSUPP;
	}
	
	/* Check NULL pointer */
	if(iwr->u.data.pointer == NULL)
	{
		ktrace;
		return -EFAULT;
	}	
#ifdef WE_STRICT_WRITE
	/* Check if there is enough buffer up there */
	if(iwr->u.data.length < dev->wireless_handlers->num_private_args)
	{
		MESH_ERR_INFO( "%s (WE) : Buffer for request MESHW_IO_R_PRIV too small (%d<%d)\n", dev->name, iwr->u.data.length, dev->wireless_handlers->num_private_args);
		return -E2BIG;
	}
#endif	/* WE_STRICT_WRITE */

	/* Set the number of available ioctls. */
	iwr->u.data.length = dev->wireless_handlers->num_private_args;

	/* Copy structure to the user buffer. */
	if (copy_to_user(iwr->u.data.pointer, dev->wireless_handlers->private_args,
			 sizeof(struct meshw_priv_args)*iwr->u.data.length))
		return -EFAULT;

	return 0;
}

struct meshw_ioctl_description	standard_ioctl[] =
{
	/* MESHW_IO_W_COMMIT */
	{ MESHW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_NAME */
	{ MESHW_HEADER_TYPE_CHAR, 0, 0, 0, 0, MESHW_DESCR_FLAG_DUMP},
	/* MESHW_IO_W_NWID */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, MESHW_DESCR_FLAG_EVENT},
	/* MESHW_IO_R_NWID */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, MESHW_DESCR_FLAG_DUMP},
	/* MESHW_IO_W_FREQ */
	{ MESHW_HEADER_TYPE_FREQ, 0, 0, 0, 0, MESHW_DESCR_FLAG_EVENT},
	/* MESHW_IO_R_FREQ */
	{ MESHW_HEADER_TYPE_FREQ, 0, 0, 0, 0, MESHW_DESCR_FLAG_DUMP},
	/* MESHW_IO_W_MODE */
	{ MESHW_HEADER_TYPE_UINT, 0, 0, 0, 0, MESHW_DESCR_FLAG_EVENT},
	/* MESHW_IO_R_MODE */
	{ MESHW_HEADER_TYPE_UINT, 0, 0, 0, 0, MESHW_DESCR_FLAG_DUMP},
	/* MESHW_IO_W_SENS */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_SENS */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_W_RANGE */
	{ MESHW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_RANGE */
	{ MESHW_HEADER_TYPE_POINT, 0, 1, 0, sizeof(struct meshw_range), MESHW_DESCR_FLAG_DUMP},
	/* MESHW_IO_W_PRIV */
	{ MESHW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_PRIV (handled directly by us) */
	{ MESHW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* MESHW_IO_W_STATS */
	{ MESHW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_STATS (handled directly by us) */
	{ MESHW_HEADER_TYPE_NULL, 0, 0, 0, 0, MESHW_DESCR_FLAG_DUMP},
	/* MESHW_IO_W_SPY */
	{ MESHW_HEADER_TYPE_POINT, 0, sizeof(struct sockaddr), 0, MESHW_MAX_SPY, 0},
	/* MESHW_IO_R_SPY */
	{ MESHW_HEADER_TYPE_POINT, 0, (sizeof(struct sockaddr) + sizeof(struct meshw_quality)), 0, MESHW_MAX_SPY, 0},
	/* MESHW_IO_W_SPY_THR */
	{ MESHW_HEADER_TYPE_POINT, 0, sizeof(struct meshw_thrspy), 1, 1, 0},
	/* MESHW_IO_R_SPY_THR */
	{ MESHW_HEADER_TYPE_POINT, 0, sizeof(struct meshw_thrspy), 1, 1, 0},
	/* MESHW_IO_W_AP_MAC */
	{ MESHW_HEADER_TYPE_ADDR, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_AP_MAC */
	{ MESHW_HEADER_TYPE_ADDR, 0, 0, 0, 0, MESHW_DESCR_FLAG_DUMP},
	/* -- hole -- */
	{ MESHW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_AP_LIST */
	{ MESHW_HEADER_TYPE_POINT, 0, (sizeof(struct sockaddr) + sizeof(struct meshw_quality)), 0, MESHW_MAX_AP, 0},
	/* MESHW_IO_W_SCAN */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_SCAN */
	{ MESHW_HEADER_TYPE_POINT, 0, 1, 0, MESHW_SCAN_MAX_DATA, 0},
	/* MESHW_IO_W_ESSID */
	{ MESHW_HEADER_TYPE_POINT, 0, 1, 0, MESHW_ESSID_MAX_SIZE + 1, MESHW_DESCR_FLAG_EVENT},
	/* MESHW_IO_R_ESSID */
	{ MESHW_HEADER_TYPE_POINT, 0, 1, 0, MESHW_ESSID_MAX_SIZE + 1, MESHW_DESCR_FLAG_DUMP},
	/* MESHW_IO_W_NICKNAME */
	{ MESHW_HEADER_TYPE_POINT, 0, 1, 0, MESHW_ESSID_MAX_SIZE + 1, 0},
	/* MESHW_IO_R_NICKNAME */
	{ MESHW_HEADER_TYPE_POINT, 0, 1, 0, MESHW_ESSID_MAX_SIZE + 1, 0},
	/* -- hole -- */
	{ MESHW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* -- hole -- */
	{ MESHW_HEADER_TYPE_NULL, 0, 0, 0, 0, 0},
	/* MESHW_IO_W_RATE */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_RATE */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_W_RTS */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_RTS */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_W_FRAG_THR */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_FRAG_THR */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_W_TX_POW */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_TX_POW */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_W_RETRY */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_RETRY */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_W_ENCODE */
	{ MESHW_HEADER_TYPE_POINT, 0, 1, 0, MESHW_ENCODING_TOKEN_MAX, MESHW_DESCR_FLAG_EVENT | MESHW_DESCR_FLAG_RESTRICT},
	/* MESHW_IO_R_ENCODE */
	{ MESHW_HEADER_TYPE_POINT, 0, 1, 0, MESHW_ENCODING_TOKEN_MAX, MESHW_DESCR_FLAG_DUMP | MESHW_DESCR_FLAG_RESTRICT},
	/* MESHW_IO_W_POWER_MGMT */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
	/* MESHW_IO_R_POWER_MGMT */
	{ MESHW_HEADER_TYPE_PARAM, 0, 0, 0, 0, 0},
};
int standard_ioctl_num = (sizeof(standard_ioctl) /sizeof(struct meshw_ioctl_description));


/*
 * Wrapper to call a standard Wireless Extension handler.
 * We do various checks and also take care of moving data between
 * user space and kernel space.
 */
static inline int __ioctl_standard_call(MESH_DEVICE *dev,struct meshw_req *iwr, unsigned int cmd, meshw_handler	handler)
{
	const struct meshw_ioctl_description	*descr;
	struct meshw_request_info			info;
	int					ret = -EINVAL;
	int					user_size = 0;

	/* Get the description of the IOCTL */
	if((cmd - MESHW_IO_FIRST) >= standard_ioctl_num)
		return -EOPNOTSUPP;
	descr = &(standard_ioctl[cmd - MESHW_IO_FIRST]);

#ifdef MESHW_IOCTL_DEBUG
	MESH_DEBUG_INFO( MESH_DEBUG_IOCTL,  "%s (MESHWLAN) : Found standard handler for 0x%04X\n", dev->name, cmd);
	MESH_DEBUG_INFO( MESH_DEBUG_IOCTL, "%s (MESHWLAN) : Header type : %d, Token type : %d, size : %d, token : %d\n", dev->name, descr->header_type, descr->token_type, descr->token_size, descr->max_tokens);
#endif	/* MESHW_IOCTL_DEBUG */

	/* Prepare the call */
	info.cmd = cmd;
	info.flags = 0;
	
	ktrace;

	/* Check if we have a pointer to user space data or not */
	if(descr->header_type != MESHW_HEADER_TYPE_POINT)
	{
		/* No extra arguments. Trivial to handle */
		ret = handler(dev, &info, &(iwr->u), NULL);

#ifdef WE_SET_EVENT
		/* Generate an event to notify listeners of the change */
		if((descr->flags & MESHW_DESCR_FLAG_EVENT) &&
			((ret == 0) || (ret == -EIWCOMMIT)))
			wireless_send_event(dev, cmd, &(iwr->u), NULL);
#endif	/* WE_SET_EVENT */
	}
	else
	{
		char *	extra;
		int	err;

		/* Check what user space is giving us */
		if(MESHW_IS_WRITE(cmd))
		{
			/* Check NULL pointer */
			if((iwr->u.data.pointer == NULL) &&
			   (iwr->u.data.length != 0))
				return -EFAULT;
			/* Check if number of token fits within bounds */
			if(iwr->u.data.length > descr->max_tokens)
				return -E2BIG;
			if(iwr->u.data.length < descr->min_tokens)
				return -EINVAL;
		}
		else
		{
			/* Check NULL pointer */
			if(iwr->u.data.pointer == NULL)
				return -EFAULT;
			/* Save user space buffer size for checking */
			user_size = iwr->u.data.length;
		}

#ifdef MESHW_IOCTL_DEBUG
		MESH_DEBUG_INFO( MESH_DEBUG_IOCTL, "%s (MESHWLAN) : Malloc %d bytes\n", dev->name, descr->max_tokens * descr->token_size);
#endif	/* MESHW_IOCTL_DEBUG */

		/* Always allocate for max space. Easier, and won't last long... */
		extra = kmalloc(descr->max_tokens * descr->token_size, GFP_KERNEL);
		if (extra == NULL)
		{
			return -ENOMEM;
		}

		/* If it is a SET, get all the extra data in here */
		if(MESHW_IS_WRITE(cmd) && (iwr->u.data.length != 0))
		{
			err = copy_from_user(extra, iwr->u.data.pointer, iwr->u.data.length*descr->token_size);
			if (err)
			{
				kfree(extra);
				return -EFAULT;
			}
#ifdef MESHW_IOCTL_DEBUG
			MESH_DEBUG_INFO( MESH_DEBUG_IOCTL, "%s (WE) : Got %d bytes\n", dev->name, iwr->u.data.length * descr->token_size);
#endif	/* MESHW_IOCTL_DEBUG */
		}

		/* Call the handler */
		ret = handler(dev, &info, &(iwr->u), extra);

		/* If we have something to return to the user */
		if (!ret && MESHW_IS_READ(cmd))
		{
#ifdef WE_STRICT_WRITE
			/* Check if there is enough buffer up there */
			if(user_size < iwr->u.data.length)
			{
				MESH_ERR_INFO( "%s (WE) : Buffer for request %04X too small (%d<%d)\n", dev->name, cmd, user_size, iwr->u.data.length);
				kfree(extra);
				return -E2BIG;
			}
#endif	/* WE_STRICT_WRITE */

			err = copy_to_user(iwr->u.data.pointer, extra, iwr->u.data.length*descr->token_size);
			if (err)
				ret =  -EFAULT;				   
#ifdef MESHW_IOCTL_DEBUG
			MESH_DEBUG_INFO( MESH_DEBUG_IOCTL, "%s (WE) : Wrote %d bytes\n",dev->name, iwr->u.data.length * descr->token_size);
#endif	/* MESHW_IOCTL_DEBUG */
		}

#ifdef WE_SET_EVENT
		/* Generate an event to notify listeners of the change */
		if((descr->flags & MESHW_DESCR_FLAG_EVENT) &&
			((ret == 0) || (ret == -EIWCOMMIT)))
		{
			if(descr->flags & MESHW_DESCR_FLAG_RESTRICT)
				/* If the event is restricted, don't export the payload */
				wireless_send_event(dev, cmd, &(iwr->u), NULL);
			else
				wireless_send_event(dev, cmd, &(iwr->u), extra);
		}
#endif	/* WE_SET_EVENT */

		/* Cleanup - I told you it wasn't that long ;-) */
		kfree(extra);
	}

	/* Call commit handler if needed and defined */
	if(ret == -EIWCOMMIT)
		ret = call_commit_handler(dev);

	/* Here, we will generate the appropriate event if needed */
	return ret;
}

/* Size (in bytes) of the various private data types */
static const char __priv_type_size[] =
{
	0,				/* MESHW_PRIV_TYPE_NONE */
	1,				/* MESHW_PRIV_TYPE_BYTE */
	1,				/* MESHW_PRIV_TYPE_CHAR */
	0,				/* Not defined */
	sizeof(__u32),			/* MESHW_PRIV_TYPE_INT */
	sizeof(struct meshw_freq),		/* MESHW_PRIV_TYPE_FLOAT */
	sizeof(struct sockaddr),	/* MESHW_PRIV_TYPE_ADDR */
	0,				/* Not defined */
};

/* Number of private arguments */
static inline int __get_priv_size(__u16	args)
{
	int	num = args & MESHW_PRIV_SIZE_MASK;
	int	type = (args & MESHW_PRIV_TYPE_MASK) >> 12;

	return num * __priv_type_size[type];
}

/*
 * Wrapper to call a private Wireless Extension handler.
 * We do various checks and also take care of moving data between
 * user space and kernel space.
 * It's not as nice and slimline as the standard wrapper. The cause
 * is struct meshw_priv_args, which was not really designed for the
 * job we are going here.
 *
 * IMPORTANT : This function prevent to set and get data on the same
 * IOCTL and enforce the SET/GET convention. Not doing it would be
 * far too hairy...
 * If you need to set and get data at the same time, please don't use
 * a meshw_handler but process it in your ioctl handler (i.e. use the
 * old driver API).
 */
static inline int __ioctl_private_call(MESH_DEVICE *dev,struct meshw_req *iwr, unsigned int cmd,meshw_handler	handler)
{
	struct meshw_priv_args *		descr = NULL;
	struct meshw_request_info		info;
	int				extra_size = 0;
	int				i;
	int				ret = -EINVAL;

ktrace;
	/* Get the description of the IOCTL */
	for(i = 0; i < dev->wireless_handlers->num_private_args; i++)
		if(cmd == dev->wireless_handlers->private_args[i].cmd)
		{
			descr = &(dev->wireless_handlers->private_args[i]);
			break;
		}

#ifdef MESHW_IOCTL_DEBUG
	MESH_DEBUG_INFO( MESH_DEBUG_IOCTL, "%s (WE) : Found private handler for 0x%04X\n", dev->name, cmd);
	if(descr)
	{
		MESH_DEBUG_INFO( MESH_DEBUG_IOCTL, "%s (WE) : Name %s, set %X, get %X\n", dev->name, descr->name, descr->set_args, descr->get_args);
	}
#endif	/* MESHW_IOCTL_DEBUG */

	/* Compute the size of the set/get arguments */
	if(descr != NULL)
	{
		if(MESHW_IS_WRITE(cmd))
		{
			int	offset = 0;	/* For sub-ioctls */
			/* Check for sub-ioctl handler */
			if(descr->name[0] == '\0')
				/* Reserve one int for sub-ioctl index */
				offset = sizeof(__u32);

			/* Size of set arguments */
			extra_size = __get_priv_size(descr->set_args);

			/* Does it fits in iwr ? */
			if((descr->set_args & MESHW_PRIV_SIZE_FIXED) &&
				((extra_size + offset) <= MESHNAMSIZ))
				extra_size = 0;
		}
		else
		{
			/* Size of set arguments */
			extra_size = __get_priv_size(descr->get_args);

			/* Does it fits in iwr ? */
			if((descr->get_args & MESHW_PRIV_SIZE_FIXED) && (extra_size <= MESHNAMSIZ))
				extra_size = 0;
		}
	}

	/* Prepare the call */
	info.cmd = cmd;
	info.flags = 0;

	/* Check if we have a pointer to user space data or not. */
	if(extra_size == 0)
	{
		/* No extra arguments. Trivial to handle */
		ret = handler(dev, &info, &(iwr->u), (char *) &(iwr->u));
	}
	else
	{
		char *	extra;
		int	err;

		/* Check what user space is giving us */
		if(MESHW_IS_WRITE(cmd))
		{/* Check NULL pointer */
			if((iwr->u.data.pointer == NULL) && (iwr->u.data.length != 0))
				return -EFAULT;

			/* Does it fits within bounds ? */
			if(iwr->u.data.length > (descr->set_args & MESHW_PRIV_SIZE_MASK))
				return -E2BIG;
		}
		else
		{
			/* Check NULL pointer */
			if(iwr->u.data.pointer == NULL)
				return -EFAULT;
		}

#ifdef MESHW_IOCTL_DEBUG
		MESH_DEBUG_INFO( MESH_DEBUG_IOCTL, "%s (WE) : Malloc %d bytes\n", dev->name, extra_size);
#endif

		/* Always allocate for max space. Easier, and won't last long... */
		extra = kmalloc(extra_size, GFP_KERNEL);
		if (extra == NULL)
		{
			return -ENOMEM;
		}

		/* If it is a SET, get all the extra data in here */
		if(MESHW_IS_WRITE(cmd) && (iwr->u.data.length != 0))
		{
			err = copy_from_user(extra, iwr->u.data.pointer,extra_size);
			if (err)
			{
				kfree(extra);
				return -EFAULT;
			}
#ifdef MESHW_IOCTL_DEBUG
			MESH_DEBUG_INFO(MESH_DEBUG_IOCTL,  "%s (WE) : Got %d elem\n", dev->name, iwr->u.data.length);
#endif	/* MESHW_IOCTL_DEBUG */
		}

		/* Call the handler */
		ret = handler(dev, &info, &(iwr->u), extra);

		/* If we have something to return to the user */
		if (!ret && MESHW_IS_READ(cmd))
		{
			err = copy_to_user(iwr->u.data.pointer, extra, extra_size);
			if (err)
				ret =  -EFAULT;				   
#ifdef MESHW_IOCTL_DEBUG
			MESH_DEBUG_INFO( MESH_DEBUG_IOCTL, "%s (WE) : Wrote %d elem\n", dev->name, iwr->u.data.length);
#endif	/* MESHW_IOCTL_DEBUG */
		}

		/* Cleanup - I told you it wasn't that long ;-) */
		kfree(extra);
	}


	/* Call commit handler if needed and defined */
	if(ret == -EIWCOMMIT)
		ret = call_commit_handler(dev);

	return ret;
}

/* Main IOCTl dispatcher */
int meshw_ioctl_process(MESH_DEVICE *dev, unsigned long data, unsigned int cmd)
{
	meshw_handler	handler;
	struct meshw_req 	*iwr = (struct meshw_req *)data;

	/* A bunch of special cases, then the generic case...
	 * Note that 'cmd' is already filtered in dev_ioctl() with
	 * (cmd >= MESHW_IO_FIRST && cmd <= MESHW_IO_LAST) */
	switch(cmd) 
	{
		case MESHW_IO_R_STATS:
			/* Get Wireless Stats */
			return __dev_iwstats(dev, iwr);

		case MESHW_IO_R_PRIV:
			if(dev->wireless_handlers != NULL)
			{
				ktrace;
				return __ioctl_export_private(dev, iwr);
			}
			// ## Fall-through for old API ##
		default:
			/* Generic IOCTL */
			/* Basic check */
			ktrace;
#if 0			
			if (!meshif_device_present(dev))
				return -ENODEV;
#endif			
			ktrace;

			/* New driver API : try to find the handler */
			handler = get_handler(dev, cmd);
			if(handler != NULL)
			{
				/* Standard and private are not the same */
				if(cmd < MESHW_IO_FIRST_PRIV)
					return __ioctl_standard_call(dev,iwr, cmd, handler);
				else
					return __ioctl_private_call(dev, iwr,cmd, handler);
			}
#if 0			
			/* Old driver API : call driver ioctl handler */
			if (dev->do_ioctl)
			{
				return dev->do_ioctl(dev,data, cmd);
			}
#endif			
			return -EOPNOTSUPP;
	}
	/* Not reached */
	return -EINVAL;
}

