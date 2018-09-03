/*
* $Id: meshconfig.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef 	__MESH_CONFIG_H__
#define	__MESH_CONFIG_H__

#define	TOOL_NAME		"meshconfig"

#include "meshwu_lib.h"


/************************** SET DISPATCHER **************************/
/*
 * This is a modified version of the dispatcher in iwlist.
 * The main difference is that here we may have multiple commands per
 * line. Also, most commands here do take arguments, and most often
 * a variable number of them.
 * Therefore, the handler *must* return how many args were consumed...
 *
 * Note that the use of multiple commands per line is not advised
 * in scripts, as it makes error management hard. All commands before
 * the error are executed, but commands after the error are not
 * processed.
 * We also try to give as much clue as possible via stderr to the caller
 * on which command did fail, but if there are two time the same command,
 * you don't know which one failed...
 */

/* Map command line arguments to the proper procedure... */
typedef struct iwconfig_entry
{
	const char			*cmd;		/* Command line shorthand */
	meshwu_enum_handler	fn;			/* Subroutine */
	int					min_count;
	int					request;		/* WE numerical ID */
	const char			*name;		/* Human readable string */
	const char			*argsname;	/* Args as human readable string */
} iwconfig_cmd;


extern	int	errarg;
extern	int	errmax;

extern	struct iwconfig_entry iwconfig_cmds[];

int config_print_info(int devFd, char *ifname, char *args[], int count);

#endif

