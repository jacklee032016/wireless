/*
* $Id: mgrconfig_cmd_fwd.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_mgrconfig.h"

int mgr_fwd_ctrl(int devFd, char *ifname, char *args[], int count)
{
	fwd_request_t		fwdReq;
	int				i = 1;

	memset(&fwdReq, 0 ,sizeof(fwd_request_t) );
	if(!strcasecmp(args[0], "add"))
	{
		fwdReq.cmd = SWJTU_FWD_CMD_ADD;
	}
	else if(!strcasecmp(args[0], "del"))
	{
		fwdReq.cmd = SWJTU_FWD_CMD_DELETE;
	}	
	else if(!strcasecmp(args[0], "enable"))
	{
		fwdReq.cmd = SWJTU_FWD_CMD_ENABLE;
	}	
	else if(!strcasecmp(args[0], "disable"))
	{
		fwdReq.cmd = SWJTU_FWD_CMD_DISABLE;
	}	

	if(count < 5)
	{
		MESHU_ERR_INFO("No validate parameters are provided when manipulate FWD TABLE\n");
		return -1;	
	}
	
	while(i<count )
	{
		if(!strcasecmp(args[i], "mac"))
		{
			i++;
			if(i>= count )
			{
				MESHU_ERR_INFO("No MAC Address is provided\n");
				return -1;
			}
			
			meshu_mac_aton(args[i ], fwdReq.addr, ETH_ALEN);
			i++;
		}
		else if(!strcasecmp(args[i], "dev"))
		{
			i++;
			if(i>= count )
			{
				MESHU_ERR_INFO("No device name is provided\n");
				return -1;
			}
			snprintf(fwdReq.devName, MESHNAMSIZ, "%s", args[i] );
			i++;
		}
		else
			i++;
	}
	
//	if(!strlen(fwdReq.devName) ||!strlen(fwdReq.addr) )
	{
		char buf[20];
		meshu_ether_ntop((const struct ether_addr *)fwdReq.addr, buf);
		MESHU_ERR_INFO("device name(%s) or MAC address(%s) is set\n", fwdReq.devName, buf );
//		return -1;
	}

	meshu_fwd_cmd(devFd, &fwdReq );
	
	return(i);
}

