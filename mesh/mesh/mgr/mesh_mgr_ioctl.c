/*
* $Id: mesh_mgr_ioctl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/
#include "mesh.h"

static int __mgr_ctrl(MESH_MGR *mgr, unsigned long data, int cmd)
{
	int error = 0;
	MESH_DEVICE	*port;
	
	switch(cmd)
	{
		case SWJTU_MGR_CTL_SRARTUP:
		{
/*   get mac address from bridge and set into portal->mesh; 
*     make portal->net as RUNNING state to fwd packet
*     enable all PHY and MAC layer drivers 
*/
			mesh_inode	*inode;
			int isStartup;
			int cmd;
			if (copy_from_user(&isStartup, (void *)data, sizeof(int) ) )
					return -EFAULT;

			cmd = (isStartup)? SWJTU_PHY_STARTUP : SWJTU_PHY_STOP;
			MESH_LIST_FOR_EACH(port , &(mgr->ports), node )
			{
				inode = &port->dev;
				down(&inode->ioctrlSema);
				MESH_DEBUG_INFO(MESH_DEBUG_IOCTL, "mesh device %s is %s\n", port->name, (isStartup)?"Startup":"Stop" );
				error = port->do_ioctl(port, data, cmd );
				up(&inode->ioctrlSema);
				if(error )
					break;
			}
			break;
		}
		default:
			MESH_WARN_INFO("IOCTRL cmd %d is not implemented now\n", cmd);
			break;
	}

	return error;
}

static int __fwd_ctrl(MESH_MGR *mgr, fwd_request_t *request)
{
	fwd_cmd_t	cmd = request->cmd;
	MESH_FWD_ITEM *item;
	int		res = 0;
	MESH_DEVICE *dev = swjtu_get_port_by_name(request->devName );
	if(!dev)
		return -ENODEV;

	item = mesh_fwd_db_get(mgr->fwdTable, request->addr );
	switch(cmd)
	{
		case SWJTU_FWD_CMD_ADD:
		{
			if(item )
			{
				MESH_WARN_INFO("%s of %s has exist\n", 
					swjtu_mesh_mac_sprintf(request->addr), dev->name );
				res = -EEXIST;
				break;
			}

			swjtu_mesh_fwd_db_insert(mgr->fwdTable, dev, request->addr, 0, 1);
			break;
		}
		case SWJTU_FWD_CMD_DELETE:
		{
			if(!item )
			{
				MESH_WARN_INFO("%s of %s is not found\n", 
					swjtu_mesh_mac_sprintf(request->addr), dev->name );
				res = -ENOENT;
				break;
			}

			mesh_fwd_db_put(item);
			break;
		}
		case SWJTU_FWD_CMD_ENABLE:
		{
			if(!item )
			{
				MESH_WARN_INFO("%s of %s is not found\n", 
					swjtu_mesh_mac_sprintf(request->addr), dev->name );
				res = -ENOENT;
				break;
			}
			MESH_DEBUG_INFO( MESH_DEBUG_FWD, "%s of %s is enabled\n",
				swjtu_mesh_mac_sprintf(item->addr.addr), dev->name );
			break;
		}
		case SWJTU_FWD_CMD_DISABLE:
		{
			if(!item )
			{
				MESH_WARN_INFO("%s of %s is not found\n", 
					swjtu_mesh_mac_sprintf(request->addr), dev->name );
				res = -ENOENT;
				break;
			}
			MESH_DEBUG_INFO( MESH_DEBUG_FWD, "%s of %s is disabled\n",
				swjtu_mesh_mac_sprintf(item->addr.addr), dev->name );
			break;
		}
		default:
			MESH_WARN_INFO("Not implemented command in FWD table management\n" );

			res = -ENOENT;
			break;
	}

	return res;
}

int swjtu_mgr_ioctl(MESH_MGR *mgr, unsigned long data, int cmd)
{
	int error = 0;

	MESH_DEBUG_INFO(MESH_DEBUG_IOCTL, "MGR CTRL CMD %d\n", cmd);

	switch (cmd)
	{
		case SWJTU_MGR_CTL_PORTAL_CTL:
		{
			int	status;

			if (copy_from_user(&status, (void *)data, sizeof(int)) )
					return -EFAULT;

			MESH_DEBUG_INFO(MESH_DEBUG_IOCTL, "MGR CTRL PORTAL CMD : %d\n" , status );
			if( status == SWJTU_MGR_PORTAL_DISABLE )
			{
				error = mgr->remove_portal(mgr, NULL);
			}
			else
			{
				error = mgr->add_portal(mgr, NULL);
			}
			break;
		}
		case SWJTU_MGR_CTL_SET_MESH_ID:
		{
			MESH_ID id;
			unsigned long flags;

			if (copy_from_user(id.meshAddr, (void *)data, ETH_ALEN) )
					return -EFAULT;
			MESH_WRITE_LOCK(mgr->rwLock, flags);
			memcpy(mgr->id.meshAddr, id.meshAddr, ETH_ALEN);
			MESH_WRITE_UNLOCK(mgr->rwLock, flags);
			break;
		}
		case SWJTU_MAC_FWD_CTRL:
		{
			fwd_request_t request;

			if (copy_from_user(&request, (void *)data, sizeof(fwd_request_t) ) )
					return -EFAULT;

			error = __fwd_ctrl(mgr, &request);

			break;
		}

		case SWJTU_MGR_CTL_SRARTUP:
		{
			error = __mgr_ctrl(mgr, data, cmd);

			break;
		}

		default:
			error = -EPERM;
			break;
	}

	MESH_DEBUG_INFO(MESH_DEBUG_IOCTL, "%s", __FUNCTION__);
	
	return error;
}

