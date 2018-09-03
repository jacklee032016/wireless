/*
 * $Id: meshpriv.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#define	TOOL_NAME	"MESH_RPIV"

#include "meshwu_lib.h"

static const char *	argtype[] =
{
	"     ", "byte ", "char ", "", "int  ", "float", "addr "
};

static void meshpriv_usage(void)
{
	fprintf(stderr, "Usage: meshpriv interface [private-command [private-arguments]]\n");
}

/* Execute a private command on the interface */
static int set_private_cmd(int devFd, char *	args[], int count, char *	ifname,
	char	*cmdname, iwprivargs *priv, int priv_num)
{
	struct meshw_req	wrq;
	u_char	buffer[4096];	/* Only that big in v25 and later */
	int		i = 0;		/* Start with first command arg */
	int		k;		/* Index in private description table */
	int		temp;
	int		subcmd = 0;	/* sub-ioctl index */
	int		offset = 0;	/* Space for sub-ioctl index */

	/* Check if we have a token index.
	* Do it now so that sub-ioctl takes precedence, and so that we
	* don't have to bother with it later on... */
	if((count >= 1) && (sscanf(args[0], "[%i]", &temp) == 1))
	{
		subcmd = temp;
		args++;
		count--;
	}

	/* Search the correct ioctl */
	k = -1;
	while((++k < priv_num) && strcmp(priv[k].name, cmdname));

	/* If not found... */
	if(k == priv_num)
	{
		MESHU_ERR_INFO( "Invalid command : %s\n", cmdname);
		return(-1);
	}

	/* Watch out for sub-ioctls ! */
	if(priv[k].cmd < SIOCDEVPRIVATE)
	{
		int	j = -1;

		/* Find the matching *real* ioctl */
		while((++j < priv_num) && ((priv[j].name[0] != '\0') ||
			(priv[j].set_args != priv[k].set_args) || (priv[j].get_args != priv[k].get_args)));

		/* If not found... */
		if(j == priv_num)
		{
			MESHU_ERR_INFO( "Invalid private ioctl definition for : %s\n", cmdname);
			return(-1);
		}

		/* Save sub-ioctl number */
		subcmd = priv[k].cmd;

		/* Reserve one int (simplify alignment issues) */
		offset = sizeof(__u32);
		/* Use real ioctl definition from now on */
		k = j;
#if 0
      printf("<mapping sub-ioctl %s to cmd 0x%X-%d>\n", cmdname,
	     priv[k].cmd, subcmd);
#endif
	}

	/* If we have to set some data */
	if((priv[k].set_args & MESHW_PRIV_TYPE_MASK) && (priv[k].set_args & MESHW_PRIV_SIZE_MASK))
	{
		switch(priv[k].set_args & MESHW_PRIV_TYPE_MASK)
		{
			case MESHW_PRIV_TYPE_BYTE:
				/* Number of args to fetch */
				wrq.u.data.length = count;
				if(wrq.u.data.length > (priv[k].set_args & MESHW_PRIV_SIZE_MASK))
					wrq.u.data.length = priv[k].set_args & MESHW_PRIV_SIZE_MASK;

				/* Fetch args */
				for(; i < wrq.u.data.length; i++)
				{
					sscanf(args[i], "%i", &temp);
					buffer[i] = (char) temp;
				}
				break;

			case MESHW_PRIV_TYPE_INT:
				/* Number of args to fetch */
				wrq.u.data.length = count;
				if(wrq.u.data.length > (priv[k].set_args & MESHW_PRIV_SIZE_MASK))
					wrq.u.data.length = priv[k].set_args & MESHW_PRIV_SIZE_MASK;

				/* Fetch args */
				for(; i < wrq.u.data.length; i++)
				{
					sscanf(args[i], "%i", &temp);
					((__s32 *) buffer)[i] = (__s32) temp;
				}
				break;

			case MESHW_PRIV_TYPE_CHAR:
				if(i < count)
				{
					/* Size of the string to fetch */
					wrq.u.data.length = strlen(args[i]) + 1;
					if(wrq.u.data.length > (priv[k].set_args & MESHW_PRIV_SIZE_MASK))
						wrq.u.data.length = priv[k].set_args & MESHW_PRIV_SIZE_MASK;

					/* Fetch string */
					memcpy(buffer, args[i], wrq.u.data.length);
					buffer[sizeof(buffer) - 1] = '\0';
					i++;
				}
				else
				{
					wrq.u.data.length = 1;
					buffer[0] = '\0';
				}
				break;

			case MESHW_PRIV_TYPE_FLOAT:
				/* Number of args to fetch */
				wrq.u.data.length = count;
				if(wrq.u.data.length > (priv[k].set_args & MESHW_PRIV_SIZE_MASK))
					wrq.u.data.length = priv[k].set_args & MESHW_PRIV_SIZE_MASK;
#if __ARM_IXP__
#else
				/* Fetch args */
				for(; i < wrq.u.data.length; i++)
				{
					double		freq;
					if(sscanf(args[i], "%lg", &(freq)) != 1)
					{
						printf("Invalid float [%s]...\n", args[i]);
						return(-1);
					}

					if(index(args[i], 'G')) 
						freq *= GIGA;
					if(index(args[i], 'M')) 
						freq *= MEGA;
					if(index(args[i], 'k')) 
						freq *= KILO;

					sscanf(args[i], "%i", &temp);

					meshwu_float2freq(freq, ((struct meshw_freq *) buffer) + i);
				}
#endif				
				break;

			case MESHW_PRIV_TYPE_ADDR:
				/* Number of args to fetch */
				wrq.u.data.length = count;
				if(wrq.u.data.length > (priv[k].set_args & MESHW_PRIV_SIZE_MASK))
					wrq.u.data.length = priv[k].set_args & MESHW_PRIV_SIZE_MASK;

				/* Fetch args */
				for(; i < wrq.u.data.length; i++)
				{
					if(meshwu_in_addr(devFd, ifname, args[i], ((struct sockaddr *) buffer) + i) < 0)
					{
						MESHU_ERR_INFO("Invalid address [%s]...\n", args[i]);
						return(-1);
					}
				}
				break;

			default:
				fprintf(stderr, "Not implemented...\n");
				return(-1);
		}


		if((priv[k].set_args & MESHW_PRIV_SIZE_FIXED) &&
			(wrq.u.data.length != (priv[k].set_args & MESHW_PRIV_SIZE_MASK)))
		{
			printf("The command %s needs exactly %d argument(s)...\n", cmdname, priv[k].set_args & MESHW_PRIV_SIZE_MASK);
			return(-1);
		}

	}	/* if args to set */
	else
	{
		wrq.u.data.length = 0L;
	}

	strncpy(wrq.ifr_name, ifname, MESHNAMSIZ);

	/* Those two tests are important. They define how the driver
	* will have to handle the data */
	if((priv[k].set_args & MESHW_PRIV_SIZE_FIXED) && ((meshwu_get_priv_size(priv[k].set_args) + offset) <= MESHNAMSIZ))
	{/* First case : all SET args fit within wrq */
		if(offset)
			wrq.u.mode = subcmd;
		memcpy(wrq.u.name + offset, buffer, MESHNAMSIZ - offset);
	}
	else
	{
		if((priv[k].set_args == 0) && (priv[k].get_args & MESHW_PRIV_SIZE_FIXED) &&
			(meshwu_get_priv_size(priv[k].get_args) <= MESHNAMSIZ))
		{ /* Second case : no SET args, GET args fit within wrq */
			if(offset)
				wrq.u.mode = subcmd;
		}
		else
		{ /* Third case : args won't fit in wrq, or variable number of args */
			wrq.u.data.pointer = (caddr_t) buffer;
			wrq.u.data.flags = subcmd;
		}
	}

	/* Perform the private ioctl */
	if(ioctl(devFd, priv[k].cmd, &wrq) < 0)
	{
		MESHU_ERR_INFO( "Interface doesn't accept private ioctl...\n");
		MESHU_ERR_INFO( "%s (%X): %s\n", cmdname, priv[k].cmd, strerror(errno));
		return(-1);
	}

	/* If we have to get some data */
	if((priv[k].get_args & MESHW_PRIV_TYPE_MASK) && (priv[k].get_args & MESHW_PRIV_SIZE_MASK))
	{
		int	j;
		int	n = 0;		/* number of args */

		printf("%-8.16s  %s:", ifname, cmdname);

		/* Check where is the returned data */
		if((priv[k].get_args & MESHW_PRIV_SIZE_FIXED) && (meshwu_get_priv_size(priv[k].get_args) <= MESHNAMSIZ))
		{
			memcpy(buffer, wrq.u.name, MESHNAMSIZ);
			n = priv[k].get_args & MESHW_PRIV_SIZE_MASK;
		}
		else
			n = wrq.u.data.length;

		switch(priv[k].get_args & MESHW_PRIV_TYPE_MASK)
		{
			case MESHW_PRIV_TYPE_BYTE:
				/* Display args */
				for(j = 0; j < n; j++)
					printf("%d  ", buffer[j]);
				printf("\n");
				break;

			case MESHW_PRIV_TYPE_INT:
				/* Display args */
				for(j = 0; j < n; j++)
					printf("%d  ", ((__s32 *) buffer)[j]);
				printf("\n");
				break;

			case MESHW_PRIV_TYPE_CHAR:
				/* Display args */
				buffer[n] = '\0';
				printf("%s\n", buffer);
				break;

			case MESHW_PRIV_TYPE_FLOAT:
			{
#if __ARM_IXP__
#else
				double freq;
				/* Display args */
				for(j = 0; j < n; j++)
				{
					freq = meshwu_freq2float(((struct meshw_freq *) buffer) + j);
					if(freq >= GIGA)
						printf("%gG  ", freq / GIGA);
					else if(freq >= MEGA)
						printf("%gM  ", freq / MEGA);
					else
						printf("%gk  ", freq / KILO);
				}
#endif				
				printf("\n");
			}
			break;

		case MESHW_PRIV_TYPE_ADDR:
		{
			char		scratch[128];
			struct sockaddr *	hwa;

			/* Display args */
			for(j = 0; j < n; j++)
			{
				hwa = ((struct sockaddr *) buffer) + j;
				if(j)
					printf("           %.*s", (int) strlen(cmdname), "                ");

				printf("%s\n", meshwu_saether_ntop(hwa, scratch));
			}
		}
		break;

	default:
		MESHU_ERR_INFO("Not yet implemented...\n");
		return(-1);
	}
	}	/* if args to set */

	return(0);
}

/* Execute a private command on the interface  */
static inline int set_private(int devFd,char *args[], int count,char *ifname)
{
	iwprivargs *	priv;
	int		number;		/* Max of private ioctl */
	int		ret;

	/* Read the private ioctls */
	number = meshwu_get_priv_info(devFd, ifname, &priv);
	/* Is there any ? */
	if(number <= 0)
	{/* Should I skip this message ? */
		fprintf(stderr, "%-8.16s  no private ioctls.\n\n",ifname);
		if(priv)
			free(priv);
		return(-1);
	}

	ret = set_private_cmd(devFd, args + 1, count - 1, ifname, args[0], priv, number);

	free(priv);
	return(ret);
}

static int print_priv_info(int devFd,char *ifname,char *args[],int count)
{
	int		k;
	iwprivargs *priv;
	int		n;

	args = args; count = count;
	/* Read the private ioctls */
	n = meshwu_get_priv_info(devFd, ifname, &priv);

	/* Is there any ? */
	if(n <= 0)
	{/* Should I skip this message ? */
		MESHU_ERR_INFO( "%-8.16s  no private ioctls.\n\n", ifname);
	}
	else
	{
		printf("%-8.16s  Available private ioctls :\n", ifname);
		/* Print them all */
		for(k = 0; k < n; k++)
			if(priv[k].name[0] != '\0')
				printf("          %-16.16s (%.4X) : set %3d %s & get %3d %s\n",
				priv[k].name, priv[k].cmd,
				priv[k].set_args & MESHW_PRIV_SIZE_MASK,
				argtype[(priv[k].set_args & MESHW_PRIV_TYPE_MASK) >> 12],
				priv[k].get_args & MESHW_PRIV_SIZE_MASK,
				argtype[(priv[k].get_args & MESHW_PRIV_TYPE_MASK) >> 12]);
		printf("\n");
	}

	if(priv)
		free(priv);

	return(0);
}

static int print_priv_all(int devFd, char *ifname, char *args[],int count)
{
	int		k;
	iwprivargs *priv;
	int		n;

	args = args; count = count;

	/* Read the private ioctls */
	n = meshwu_get_priv_info(devFd, ifname, &priv);
	/* Is there any ? */
	if(n <= 0)
	{
		MESHU_ERR_INFO( "%-8.16s  no private ioctls.\n\n", ifname);
	}
	else
	{
		printf("%-8.16s  Available read-only private ioctl :\n", ifname);
		/* Print them all */
		for(k = 0; k < n; k++)
			/* We call all ioctls that don't have a null name, don't require args and return some (avoid triggering "reset" commands) */
			if((priv[k].name[0] != '\0') && (priv[k].set_args == 0) && (priv[k].get_args != 0))
				set_private_cmd(devFd, NULL, 0, ifname, priv[k].name, priv, n);

		printf("\n");
	}

	if(priv)
		free(priv);
	return(0);
}

#if 0
/*
 * Set roaming mode on and off
 * Found in wavelan_cs driver
 * Note : this is obsolete, most 802.11 devices should use the
 * MESHW_IO_W_AP_MAC request.
 */
static int set_roaming(int		devFd,		/* Socket */
	    char *	args[],		/* Command line args */
	    int		count,		/* Args count */
	    char *	ifname)		/* Dev name */
{
  u_char	buffer[1024];
  struct meshw_req		wrq;
  int		i = 0;		/* Start with first arg */
  int		k;
  iwprivargs *	priv;
  int		number;
  int		roamcmd;
  char		RoamState;		/* buffer to hold new roam state */
  char		ChangeRoamState=0;	/* whether or not we are going to
					   change roam states */

  /* Read the private ioctls */
  number = meshwu_get_priv_info(devFd, ifname, &priv);

  /* Is there any ? */
  if(number <= 0)
    {
      /* Should I skip this message ? */
      fprintf(stderr, "%-8.16s  no private ioctls.\n\n",
	      ifname);
      if(priv)
	free(priv);
      return(-1);
    }

  /* Get the ioctl number */
  k = -1;
  while((++k < number) && strcmp(priv[k].name, "setroam"));
  if(k == number)
    {
      fprintf(stderr, "This device doesn't support roaming\n");
      free(priv);
      return(-1);
    }
  roamcmd = priv[k].cmd;

  /* Cleanup */
  free(priv);

  if(count != 1)
    {
      iw_usage();
      return(-1);
    }

  if(!strcasecmp(args[i], "on"))
    {
      printf("%-8.16s  enable roaming\n", ifname);
      if(!number)
	{
	  fprintf(stderr, "This device doesn't support roaming\n");
	  return(-1);
	}
      ChangeRoamState=1;
      RoamState=1;
    }
  else
    if(!strcasecmp(args[i], "off"))
      {
	i++;
	printf("%-8.16s  disable roaming\n",  ifname);
	if(!number)
	  {
	    fprintf(stderr, "This device doesn't support roaming\n");
	    return(-1);
	  }
	ChangeRoamState=1;
	RoamState=0;
      }
    else
      {
	iw_usage();
	return(-1);
      }

  if(ChangeRoamState)
    {
      strncpy(wrq.ifr_name, ifname, MESHNAMSIZ);

      buffer[0]=RoamState;

      memcpy(wrq.u.name, &buffer, MESHNAMSIZ);

      if(ioctl(devFd, roamcmd, &wrq) < 0)
	{
	  fprintf(stderr, "Roaming support is broken.\n");
	  return(-1);
	}
    }

  return(0);
}

/* Get and set the port type
 * Found in wavelan2_cs and wvlan_cs drivers
 * TODO : Add support for HostAP ?
 */
static int port_type(int		devFd,		/* Socket */
	  char *	args[],		/* Command line args */
	  int		count,		/* Args count */
	  char *	ifname)		/* Dev name */
{
  struct meshw_req	wrq;
  int		i = 0;		/* Start with first arg */
  int		k;
  iwprivargs *	priv;
  int		number;
  char		ptype = 0;
  char *	modes[] = { "invalid", "managed (BSS)", "reserved", "ad-hoc" };

  /* Read the private ioctls */
  number = meshwu_get_priv_info(devFd, ifname, &priv);

  /* Is there any ? */
  if(number <= 0)
    {
      /* Should I skip this message ? */
      fprintf(stderr, "%-8.16s  no private ioctls.\n\n", ifname);
      if(priv)
	free(priv);
      return(-1);
    }

  /* Arguments ? */
  if(count == 0)
    {
      /* So, we just want to see the current value... */
      k = -1;
      while((++k < number) && strcmp(priv[k].name, "gport_type") &&
	     strcmp(priv[k].name, "get_port"));
      if(k == number)
	{
	  fprintf(stderr, "This device doesn't support getting port type\n");
	  goto err;
	}
      strncpy(wrq.ifr_name, ifname, MESHNAMSIZ);

      /* Get it */
      if(ioctl(devFd, priv[k].cmd, &wrq) < 0)
	{
	  fprintf(stderr, "Port type support is broken.\n");
	  goto err;
	}
      ptype = *wrq.u.name;

      /* Display it */
      printf("%-8.16s  Current port mode is %s <port type is %d>.\n\n",
	     ifname, modes[(int) ptype], ptype);

      free(priv);
      return(0);
    }

  if(count != 1)
    {
      iw_usage();
      goto err;
    }

  /* Read it */
  /* As a string... */
  k = 0;
  while((k < 4) && strncasecmp(args[i], modes[k], 2))
    k++;
  if(k < 4)
    ptype = k;
  else
    /* ...or as an integer */
    if(sscanf(args[i], "%i", (int *) &ptype) != 1)
      {
	iw_usage();
	goto err;
      }
  
  k = -1;
  while((++k < number) && strcmp(priv[k].name, "sport_type") &&
	strcmp(priv[k].name, "set_port"));
  if(k == number)
    {
      fprintf(stderr, "This device doesn't support setting port type\n");
      goto err;
    }
  strncpy(wrq.ifr_name, ifname, MESHNAMSIZ);

  *(wrq.u.name) = ptype;

  if(ioctl(devFd, priv[k].cmd, &wrq) < 0)
    {
      fprintf(stderr, "Invalid port type (or setting not allowed)\n");
      goto err;
    }

  free(priv);
  return(0);

 err:
  free(priv);
  return(-1);
}
#endif

int main(int argc,char **argv)
{
	int devFd;
	int goterr = 0;

	/* Create a channel to the NET kernel. */
	if((devFd = meshu_device_open(1)) < 0)
	{
		perror("MESH Device Open Failed");
		return(-1);
	}

#if 0
	/* No argument : show the list of all devices + ioctl list */
	if(argc == 1)
		iw_enum_devices(devFd, &print_priv_info, NULL, 0);
	else
    /* Special cases take one... */
    /* All */
	if((!strncmp(argv[1], "-a", 2)) || (!strcmp(argv[1], "--all")))
		iw_enum_devices(devFd, &print_priv_all, NULL, 0);
	else
#endif	
		if((!strncmp(argv[1], "-h", 2)) || (!strcmp(argv[1], "--help")))
	{
		meshpriv_usage();
	}
	else if(argc == 2)
	{/* The device name must be the first argument */
		/* Name only : show for that device only */
		print_priv_info(devFd, argv[1], NULL, 0);
	}
	else if((!strncmp(argv[2], "-a", 2)) ||(!strcmp(argv[2], "--all")))
		print_priv_all(devFd, argv[1], NULL, 0);

#if 0	
	else if(!strncmp(argv[2], "roam", 4))
	{/* Roaming */
		goterr = set_roaming(devFd, argv + 3, argc - 3, argv[1]);
	}
	else if(!strncmp(argv[2], "port", 4))
	{
		goterr = port_type(devFd, argv + 3, argc - 3, argv[1]);
	}
#endif	
	else
	{ /* Otherwise, it's a private ioctl */
		goterr = set_private(devFd, argv + 2, argc - 2, argv[1]);
	}
	return(goterr);
}

