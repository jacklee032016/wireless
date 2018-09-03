/*
 * $Id: meshconfig_main.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "meshconfig.h"

int	errarg;
int	errmax;

/* Find the most appropriate command matching the command line */
static __inline const iwconfig_cmd *__find_command(const char *cmd)
{
	const iwconfig_cmd *found = NULL;
	int				ambig = 0;
	unsigned int		len = strlen(cmd);
	int				i;

	/* Go through all commands */
	for(i = 0; iwconfig_cmds[i].cmd != NULL; ++i)
	{/* No match -> next one */
		if(strncasecmp(iwconfig_cmds[i].cmd, cmd, len) != 0)
			continue;

		/* Exact match -> perfect */
		if(len == strlen(iwconfig_cmds[i].cmd))
			return &iwconfig_cmds[i];

		/* Partial match */
		if(found == NULL)
			/* First time */
			found = &iwconfig_cmds[i];
		else
			/* Another time */
			if (iwconfig_cmds[i].fn != found->fn)
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

/*
 * Set the wireless options requested on command line
 * Find the individual commands and call the appropriate subroutine
 */
static int __set_info(int	devFd, char *args[], int count, char *ifname)	
{
	const iwconfig_cmd *iwcmd;
	int				ret;

	/* Loop until we run out of args... */
	while(count > 0)
	{
		/* find the command matching the keyword */
		iwcmd = __find_command(args[0]);
		if(iwcmd == NULL)
		{
			return(-1);
		}

		/* One arg is consumed (the command name) */
		args++;
		count--;

		/* Check arg numbers */
		if(count < iwcmd->min_count)
			ret = MESHWU_ERR_ARG_NUM;
		else
			ret = 0;

		/* Call the command */
		if(!ret)
			ret = (*iwcmd->fn)(devFd, ifname, args, count);

		/* Deal with various errors */
		if(ret < 0)
		{
			int	request = iwcmd->request;

			if(ret == MESHWU_ERR_GET_EXT)
				request++;	/* Transform the SET into GET */

			MESHU_ERR_INFO( "Error for wireless request \"%s\" (%X) :\n", iwcmd->name, request);
			switch(ret)
			{
				case MESHWU_ERR_ARG_NUM:
					MESHU_ERR_INFO("    too few arguments.\n");
					break;
				case MESHWU_ERR_ARG_TYPE:
					if(errarg < 0)
						errarg = 0;
					if(errarg >= count)
						errarg = count - 1;
					MESHU_ERR_INFO("    invalid argument \"%s\".\n", args[errarg]);
					break;

				case MESHWU_ERR_ARG_SIZE:
					MESHU_ERR_INFO( "    argument too big (max %d)\n", errmax);
					break;

				case MESHWU_ERR_ARG_CONFLICT:
					if(errarg < 0)
						errarg = 0;
					if(errarg >= count)
						errarg = count - 1;
					MESHU_ERR_INFO( "    conflicting argument \"%s\".\n", args[errarg]);
					break;

				case MESHWU_ERR_SET_EXT:
					MESHU_ERR_INFO( "    SET failed on device %-1.16s ; %s.\n", ifname, strerror(errno));
					break;
				case MESHWU_ERR_GET_EXT:
					MESHU_ERR_INFO("    GET failed on device %-1.16s ; %s.\n", ifname, strerror(errno));
					break;
			}

			/* Stop processing, we don't know if we are in a consistent state in reading the command line */
			return(ret);
		}

		/* Substract consumed args from command line */
		args += ret;
		count -= ret;
	}
	return(0);
}

static inline void __config_usage(void)
{
	int i;
	fprintf(stderr,   "Usage: meshconfig [interface]\n");
	for(i = 0; iwconfig_cmds[i].cmd != NULL; ++i)
		fprintf(stderr, "                interface %s %s\n", iwconfig_cmds[i].cmd, iwconfig_cmds[i].argsname);
	fprintf(stderr,   "       Check man pages for more details.\n");
}

int main(int argc, char **argv)
{
	int devFd;		/* generic raw socket desc.	*/
	int goterr = 0;

	/* Create a channel to the NET kernel. */
	if((devFd = meshu_device_open(1) ) < 0)
	{
		perror("MESH Device Open Failed");
		exit(-1);
	}
#if 0
	/* No argument : show the list of all device + info */
	if(argc == 1)
		iw_enum_devices(devFd, &config_print_info, NULL, 0);
	else
#endif		
		if((!strcmp(argv[1], "-h")) || (!strcmp(argv[1], "--help")))
	{
		__config_usage();
	}
	else
	{
		if((argc > 2) && !strcmp(argv[1], "--"))
		{/* '--' escape device name */
			argv++;
			argc--;
		}

		/* The device name must be the first argument */
		if(argc == 2)
			config_print_info(devFd, argv[1], NULL, 0);
		else
			/* The other args on the line specify options to be set... */
			goterr = __set_info(devFd, argv + 2, argc - 2, argv[1]);
	}

	MESH_DEVICE_CLOSE(devFd);

	return(goterr);
}

