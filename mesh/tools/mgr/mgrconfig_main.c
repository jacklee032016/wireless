/*
* $Id: mgrconfig_main.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_mgrconfig.h"

static int __mgr_set_info(int devFd, char *args[], int count)	
{
	const mgrconfig_cmd *mgrcmd;
	int				ret;

	/* Loop until we run out of args... */
//	while(count > 0)
	{
		/* find the command matching the keyword */
		mgrcmd = mgr_find_command(args[0]);
		if(mgrcmd == NULL)
		{
			return(-1);
		}

		args++;
		count--;

		/* Check arg numbers */
		if(count < mgrcmd->min_count)
			ret = MESHWU_ERR_ARG_NUM;
		else
			ret = 0;

		/* Call the command */
		if(!ret)
			ret = (*mgrcmd->fn)(devFd, mgrcmd->cmd, args, count);

		/* Deal with various errors */
		if(ret < 0)
		{
			int	request = mgrcmd->request;

//			if(ret == MESHWU_ERR_GET_EXT)
//				request++;	/* Transform the SET into GET */

			MESHU_ERR_INFO( "Error for MESH Config request \"%s\" (%X) :\n", mgrcmd->name, request);

			switch(ret)
			{
				case MESHWU_ERR_ARG_NUM:
					MESHU_ERR_INFO("    too few arguments.\n");
					break;
#if 0					
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
#endif
				case MESHWU_ERR_SET_EXT:
					MESHU_ERR_INFO( "    SET failed on device %s.\n",  strerror(errno));
					break;
				case MESHWU_ERR_GET_EXT:
					MESHU_ERR_INFO("    GET failed on device %s.\n", strerror(errno));
					break;
				default:
					MESHU_ERR_INFO("    failed on device %s.\n", strerror(errno));
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

static inline void __mgr_usage(void)
{
	int i;
	fprintf(stderr,   "Usage: mgrconfig [interface]\n");
	for(i = 0; mgrCmds[i].cmd != NULL; ++i)
		fprintf(stderr, "                interface %s %s\n", mgrCmds[i].cmd, mgrCmds[i].argsname);
}

int main(int argc, char **argv)
{
	int devFd;
	int goterr = 0;
	int index;
	
	if((argc==1) ||(!strcmp(argv[1], "-h")) || (!strcmp(argv[1], "--help")))
	{
		__mgr_usage();
		return 0;
	}
	
	index = atoi(argv[1]);
	if((devFd = meshu_device_open(index) ) < 0)
	{
		perror("MESH Device Open Failed");
		exit(-1);
	}

	{

#if 0
		if(argc == 2)
			config_print_info(devFd, argv[1], NULL, 0);
		else
#endif			
			/* The other args on the line specify options to be set... */
			goterr = __mgr_set_info(devFd, argv + 2, argc - 2);
	}

	MESH_DEVICE_CLOSE(devFd);

	return(goterr);
}

