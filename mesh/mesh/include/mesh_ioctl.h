/*
* $Id: mesh_ioctl.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef  	__MESH_IOCTL_H__
#define	__MESH_IOCTL_H__

#define	SWJTU_MESH_CTL_CODE						'J'	/* 0x4A */

#define	SWJTU_MESH_MAC_CTL_NUMBER				64
#define	SWJTU_MESH_PHY_CTL_NUMBER				128
#define	SWJTU_MESH_HW_CTL_NUMBER				192


/******************  MESH CTL  ********************/
/*  */
#define	SWJTU_MESH_CTL_FLUSH		_IOW (SWJTU_MESH_CTL_CODE, 3, int)

enum
{
	SWJTU_MGR_PORTAL_DISABLE	=	0,
	SWJTU_MGR_PORTAL_ENABLE		=	1
};

typedef	enum
{
	SWJTU_FWD_CMD_ADD	=	0,
	SWJTU_FWD_CMD_DELETE,
	SWJTU_FWD_CMD_ENABLE,
	SWJTU_FWD_CMD_DISABLE,
}fwd_cmd_t;

typedef	struct
{
	fwd_cmd_t		cmd;
	char				addr[ETH_ALEN];
	char				devName[MESHNAMSIZ];
}fwd_request_t;

#define	SWJTU_MGR_CTL_SET_MESH_ID		_IOW (SWJTU_MESH_CTL_CODE, 5, MESH_ID)

#define	SWJTU_MGR_CTL_PORTAL_CTL		_IOW (SWJTU_MESH_CTL_CODE, 6, int)

/* startup/stop mgr and all mesh device : 
*     get mac address from bridge and set into portal->mesh; 
*     make portal->net as RUNNING state to fwd packet
*     enable PHY and MAC layer drivers 
*/
#define	SWJTU_MGR_CTL_SRARTUP			_IOW (SWJTU_MESH_CTL_CODE, 7, int)


/******************  MAC CTL  ********************/
#define	SWJTU_MAC_STARTUP		_IOW (SWJTU_MESH_CTL_CODE, SWJTU_MESH_MAC_CTL_NUMBER, int)

#define	SWJTU_MAC_GET_STATS		_IOR (SWJTU_MESH_CTL_CODE, SWJTU_MESH_MAC_CTL_NUMBER+2, int)
/* manipulate one item of mac fwd table */
#define	SWJTU_MAC_FWD_CTRL		_IOW (SWJTU_MESH_CTL_CODE, SWJTU_MESH_MAC_CTL_NUMBER+3, fwd_request_t )


/******************  PHY CTL  ********************/
/*  */
#define	SWJTU_PHY_STARTUP		_IOW (SWJTU_MESH_CTL_CODE, SWJTU_MESH_PHY_CTL_NUMBER, int)
/*  */
#define	SWJTU_PHY_STOP			_IOW (SWJTU_MESH_CTL_CODE, SWJTU_MESH_PHY_CTL_NUMBER+1, int)

#define	SWJTU_PHY_GET_STATS		_IOR (SWJTU_MESH_CTL_CODE, SWJTU_MESH_PHY_CTL_NUMBER+2, int)


/******************  HW CTL  ********************/
#define	SWJTU_HW_STARTUP			_IOW (SWJTU_MESH_CTL_CODE, SWJTU_MESH_HW_CTL_NUMBER, int)

#define	SWJTU_HW_GET_STATS		_IOR (SWJTU_MESH_CTL_CODE, SWJTU_MESH_HW_CTL_NUMBER+2, int)

#include "mesh_if.h"

#endif

