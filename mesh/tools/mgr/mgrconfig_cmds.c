/*
* $Id: mgrconfig_cmds.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_mgrconfig.h"

static int __mgr_cmd_null(int devFd, char *cmd, char *args[], int count)
{
	count = count;
	MESHU_DEBUG_INFO("%s is not implemented until now!\n", cmd);
	return 0;
}

static int __mgr_portal(int devFd, char *cmd, char *args[], int count)
{
	int isEnable = 1;
	count = count;

	if(!strcasecmp(args[0], "disable"))
	{
		isEnable = 0;
		return mgr_portal_disable( devFd, (count>1)?args[1]:NULL);
	}
	else
	{
		if(count<=1)
		{
			MESHU_ERR_INFO("NET DEVICE for transparent Bridge Port is not defined\n");
			return 1;
		}
		return mgr_portal_enable( devFd, args[1], (count>2)?args[2]:NULL );
	}

}

static int __mgr_start_dev(int devFd, char *cmd, char *args[], int count)
{
	int isStart = 1;
	count = count;

	if(!strcasecmp(args[0], "Stop"))
		isStart = 0;

	return meshu_device_startup( devFd, isStart);
}


mgrconfig_cmd mgrCmds[] = 
{
	{
		"meshID",
		__mgr_cmd_null,
		1,
		0,
		"Set MESH ID in this node",
		"{XX:XX:XX:XX:XX:XX}"
	},
	{
		"portal",
		__mgr_portal,
		1,
		0,
		"Portal Control",
		"{enable $devName [bridgeName]|disable[bridgeName]}"
	},
	{
		"showbridge",
		_mgr_portal_show_bridge,
		0,
		0,
		"Mesh Transparent Bridge Info",
		"{[bridgeName]}"
	},
	{/* startup on device file of mgr */
		"start",
		__mgr_start_dev,
		1,
		0,
		"Start or Stop Mesh Device",
		"{Yes|No}"
	},
	{
		"fwd",
		mgr_fwd_ctrl,
		1,
		0,
		"Manipuldate MAC fwd item",
		"{add|del|enable|disable mac XX:XX:XX:XX:XX:XX dev $devName }"
	},
	{ NULL, NULL, 0, 0, NULL, NULL }
};

const mgrconfig_cmd *mgr_find_command(const char *cmd)
{
	const mgrconfig_cmd *found = NULL;
	int				ambig = 0;
	unsigned int		len = strlen(cmd);
	int				i;

	/* Go through all commands */
	for(i = 0; mgrCmds[i].cmd != NULL; ++i)
	{/* No match -> next one */
		if(strncasecmp(mgrCmds[i].cmd, cmd, len) != 0)
			continue;

		/* Exact match -> perfect */
		if(len == strlen(mgrCmds[i].cmd))
			return &mgrCmds[i];

		/* Partial match */
		if(found == NULL)
			/* First time */
			found = &mgrCmds[i];
		else
			/* Another time */
			if (mgrCmds[i].fn != found->fn)
				ambig = 1;
	}

	if(found == NULL)
	{
		MESHU_ERR_INFO( "unknown command \"%s\"\n", cmd);
		return NULL;
	}

	if(ambig)
	{
		MESHU_ERR_INFO( "command \"%s\" is ambiguous\n", cmd);
		return NULL;
	}

	return found;
}


