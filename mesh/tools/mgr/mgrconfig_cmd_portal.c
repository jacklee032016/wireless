/*
* $Id: mgrconfig_cmd_portal.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/
#include "_mgrconfig.h"
#include "mbridge.h"

struct netdevs
{
	char				name[IFNAMSIZ];
	struct	netdevs	*next;
};

static int __portal_cmd_add_bridge(char *bridgeName)
{
	int err;

	switch (err = mbridge_add_bridge(bridgeName) )
	{
		case 0:
			break;

		case EEXIST:
			MESHU_ERR_INFO( "device %s already exists; can't create bridge with the same name\n", bridgeName);
			break;

		default:
			MESHU_ERR_INFO( "add bridge failed: %s\n", strerror(err));
			break;
	}
	return err;
}

static int __portal_cmd_del_bridge(char *brName)
{
	int err;

	switch (err = mbridge_del_bridge(brName) )
	{
		case 0:
			break;

		case ENXIO:
			MESHU_ERR_INFO( "bridge %s doesn't exist; can't delete it\n", brName);
			break;

		case EBUSY:
			MESHU_ERR_INFO( "bridge %s is still up; can't delete it\n", brName);
			break;

		default:
			MESHU_ERR_INFO( "can't delete bridge %s: %s\n", brName, strerror(err));
			break;
	}

	return err;
}

static int __portal_cmd_add_bridge_if(char *brName, char *devName)
{
	int err = 0;
	
	err = mbridge_add_interface(brName, devName);
	switch(err)
	{
		case 0:
			break;

		case ENODEV:
			MESHU_ERR_INFO( "interface %s does not exist!\n", devName);
			break;

		case EBUSY:
			MESHU_ERR_INFO( "device %s is already a member of a bridge; can't enslave it to bridge %s.\n", devName, brName);
			break;
		case ELOOP:
			MESHU_ERR_INFO( "device %s is a bridge device itself; can't enslave a bridge device to a bridge device.\n", devName);
			break;

		default:
			MESHU_ERR_INFO( "can't add %s to bridge %s: %s\n", devName, brName, strerror(err));
			break;
	}

	return err;
}

static int __portal_cmd_del_bridge_if(const char *brName, const char *devName, void *data)
{
	int err;

	err = mbridge_del_interface(brName, devName);
	switch (err)
	{
		case 0:
			break;
		case ENODEV:
			MESHU_ERR_INFO("interface %s does not exist!\n", devName);
			break;

		case EINVAL:
			MESHU_ERR_INFO( "device %s is not a slave of %s\n", devName, brName);
			break;

		default:
			MESHU_ERR_INFO( "can't delete %s from %s: %s\n", devName, brName, strerror(err));
			break;
	}

	return err;
}

static inline char *__get_netif_name(char *name, int nsize,char *buf)
{
	char *	end;

	/* Skip leading spaces */
	while(isspace(*buf))
		buf++;
#ifndef IW_RESTRIC_ENUM
  /* Get name up to the last ':'. Aliases may contain ':' in them,
   * but the last one should be the separator */
	end = strrchr(buf, ':');
#else
  /* Get name up to ": "
   * Note : we compare to ": " to make sure to process aliased interfaces
   * properly. Doesn't work on /proc/net/dev, because it doesn't guarantee
   * a ' ' after the ':'*/
	end = strstr(buf, ": ");
#endif

	/* Not found ??? To big ??? */
	if((end == NULL) || (((end - buf) + 1) > nsize))
		return(NULL);

	memcpy(name, buf, (end - buf));
	name[end - buf] = '\0';

	return (end + 2);
}

struct netdevs *_portal_enum_net_devices(char *filter)
{
#define PROC_NET_DEV		"/proc/net/dev"

	char		buff[1024];
	FILE *	fh;
	struct	netdevs *header= NULL, *last = NULL, *tmp;
	
	fh = fopen(PROC_NET_DEV, "r");
	if(fh != NULL)
	{
		/* Eat 2 lines of header */
		fgets(buff, sizeof(buff), fh);
		fgets(buff, sizeof(buff), fh);

		/* Read each device line */
		while(fgets(buff, sizeof(buff), fh))
		{
			char	name[IFNAMSIZ + 1];
			char *s;

			/* Extract interface name */
			s = __get_netif_name(name, sizeof(name), buff);
			if(!s)
				MESHU_ERR_INFO( "Cannot parse " PROC_NET_DEV "\n");
			else if(!strncmp(name, filter, strlen(filter) ) )
			{
				tmp = (struct netdevs *)malloc(sizeof(struct netdevs) );
				if(!tmp )
					return NULL;
				memset(tmp, 0 , sizeof(struct netdevs) );
				sprintf(tmp->name, "%s", name);

				MESHU_DEBUG_INFO("NET DEVICE '%s'\n", name);
				if(!header )
					header = tmp;
				if(last)
					last->next = tmp;
				last = tmp;
				
				/* Got it, print info about this interface */
//				(*fn)( name, NULL, 0);
			}	
		}

		fclose(fh);
	}

	return header;
}

int mgr_portal_enable(int devFd, char *devName, char *bname)
{
//	static int	index = 0;
	char	brName[IFNAMSIZ];
	int	res = 0;
	struct netdevs *tmp;
	struct netdevs *devs = _portal_enum_net_devices( devName );
	if(!devs)
	{
		MESHU_ERR_INFO("NET DEVICE %s is not available now\n", devName);
		return -1;
	}
	
	if (mbridge_init())
	{
		MESHU_ERR_INFO("can't setup bridge control: %s\n", strerror(errno) );
		return 1;
	}
	
	res = meshu_portal_enable( devFd, 1);
	if(res )
		return res;

	tmp = devs;
	while(tmp)
	{
//		sprintf(brName, "%s%d", MESH_BRIDGE_NAME, index++ );
		if(bname)
			sprintf(brName, "%s", bname );
		else
			sprintf(brName, "%s", MESH_BRIDGE_NAME );

		if((res = __portal_cmd_add_bridge(brName) ))
			return res;
		
		/* in order to set the MAC of bridge as ethernet device, is must be added into bridge firstly.
		refer to br_stp.if.c -->br_stp_recalculate_bridge_id */
		if( (res = __portal_cmd_add_bridge_if(brName, tmp->name) ) )
			return res;

		if( (res = __portal_cmd_add_bridge_if(brName, SWJTU_MESH_DEV_NAME ) ) )
			return res;
		
		tmp = tmp->next;
	}
	
	mbridge_shutdown( );

	return 0;
}

int mgr_portal_disable(int devFd, char *bname)
{
	char	brName[IFNAMSIZ];
	int	res = 0;
	
	if (mbridge_init())
	{
		MESHU_ERR_INFO("can't setup bridge control: %s\n", strerror(errno) );
		return 1;
	}
	if(!bname)
		sprintf(brName, "%s", MESH_BRIDGE_NAME );
	else
		sprintf(brName, "%s", bname );

	res = mbridge_foreach_port(brName, __portal_cmd_del_bridge_if, NULL);
	if(res<0 )
		return res;

	res = __portal_cmd_del_bridge( brName);
	if(res )
		return res;
	
	res = meshu_portal_enable( devFd, 0);
	
	mbridge_shutdown( );

	return res;
}


static int __first;

static int __dump_interface(const char *b, const char *p, void *arg)
{

	if (__first) 
		__first = 0;
	else
		printf("\n\t\t\t\t\t\t\t");

	printf("%s", p);

	return 0;
}

void __dump_interface_list(const char *br)
{
	int err;

	__first = 1;
	err = mbridge_foreach_port(br, __dump_interface, NULL);
	if (err < 0)
		printf(" can't get port info: %s\n", strerror(-err));
	else
		printf("\n");
}

static int __compare_fdbs(const void *_f0, const void *_f1)
{
	const struct fdb_entry *f0 = _f0;
	const struct fdb_entry *f1 = _f1;

	return memcmp(f0->mac_addr, f1->mac_addr, 6);
}

void __show_timer(const struct timeval *tv)
{
	printf("%4i.%.2i", (int)tv->tv_sec, (int)tv->tv_usec/10000);
}

static int __show_macs(char *brname)
{
#define CHUNK 128
	int i, n;
	struct fdb_entry *fdb = NULL;
	int offset = 0;

	for(;;)
	{
		fdb = realloc(fdb, (offset + CHUNK) * sizeof(struct fdb_entry));
		if (!fdb)
		{
			MESHU_ERR_INFO("Out of memory\n");
			return 1;
		}
			
		n = mbridge_read_fdb(brname, fdb+offset, offset, CHUNK);
		if (n == 0)
			break;
		if (n < 0)
		{
			MESHU_ERR_INFO("read of forward table failed: %s\n", strerror(errno));
			return 1;
		}

		offset += n;
	}

	qsort(fdb, offset, sizeof(struct fdb_entry), __compare_fdbs);

	printf("Port No\t\tMAC Addr\t\tIsLocal?\tAgeing Timer\n");
	for (i = 0; i < offset; i++)
	{
		const struct fdb_entry *f = fdb + i;
		printf("%3i\t", f->port_no);
		printf("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\t",
		       f->mac_addr[0], f->mac_addr[1], f->mac_addr[2],
		       f->mac_addr[3], f->mac_addr[4], f->mac_addr[5]);
		printf("%s\t\t", f->is_local?"yes":"no");
		__show_timer(&f->ageing_timer_value);
		printf("\n");
	}
	return 0;
}

int _mgr_portal_show_bridge(int devFd, char *cmd, char *args[], int count)
{
	struct bridge_info info;
	unsigned char *x;
	char		brname[IFNAMSIZ];

	if (mbridge_init())
	{
		MESHU_ERR_INFO("can't setup bridge control: %s\n", strerror(errno) );
		return 1;
	}
	
	if(count>= 1)
		sprintf(brname, "%s", args[0] );
	else		
		sprintf(brname, "%s", MESH_BRIDGE_NAME );
	
	printf("Bridge Name\tBridge ID\t\tSTP enabled\tInterfaces\n");
	printf("%s\t\t", brname);
	fflush(stdout);

	if (mbridge_get_bridge_info(brname, &info))
	{
		MESHU_ERR_INFO( "can't get info %s\n", strerror(errno));
		return 1;
	}

	x = (unsigned char *)&info.bridge_id;
	printf("%.2x%.2x.%.2x%.2x%.2x%.2x%.2x%.2x", x[0], x[1], x[2], x[3],
	       x[4], x[5], x[6], x[7]);
	printf("\t%s\t\t", info.stp_enabled?"yes":"no");

	__dump_interface_list(brname);

	__show_macs(brname);

	mbridge_shutdown( );
	return 0;
}

