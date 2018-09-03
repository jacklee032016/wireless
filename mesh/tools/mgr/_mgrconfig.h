/*
* $Id: _mgrconfig.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/
#ifndef	___MGR_CONFIG_H__
#define	___MGR_CONFIG_H__

#include "mesh_lib.h"

typedef int (*lib_mgr_handler)(int devFd, char *cmd, char *args[],int count);

typedef struct mgrconfig_entry
{
	char			*cmd;		/* Command line shorthand */
	lib_mgr_handler		fn;			/* Subroutine */
	int					min_count;
	int					request;		/* WE numerical ID */
	const char			*name;		/* Human readable string */
	const char			*argsname;	/* Args as human readable string */
}mgrconfig_cmd;

const mgrconfig_cmd *mgr_find_command(const char *cmd);

extern	mgrconfig_cmd mgrCmds[];

int mgr_fwd_ctrl(int devFd, char *ifname, char *args[], int count);

int mgr_portal_enable(int devFd, char *devName, char *bname);
int mgr_portal_disable(int devFd, char *bname);
int _mgr_portal_show_bridge(int devFd, char *cmd, char *args[], int count);

#endif

