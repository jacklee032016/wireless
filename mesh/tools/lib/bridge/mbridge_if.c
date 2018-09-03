/*
 * $Id: mbridge_if.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "mesh_lib.h"
#include "mbridge.h"
#include "mbridge_private.h"

int mbridge_socket_fd = -1;

int mbridge_init(void)
{
	if ((mbridge_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return errno;
	return 0;
}

void mbridge_shutdown(void)
{
	close(mbridge_socket_fd);
	mbridge_socket_fd = -1;
}

int mbridge_foreach_bridge(int (*iterator)(const char *, void *), void *iarg)
{
	int i, ret=0, num;
	char ifname[IFNAMSIZ];
	int ifindices[MAX_BRIDGES];
	unsigned long args[3] = { BRCTL_GET_BRIDGES, (unsigned long)ifindices, MAX_BRIDGES };

	num = ioctl(mbridge_socket_fd, SIOCGIFBR, args);
	if (num < 0)
	{
		MESHU_ERR_INFO("Get bridge indices failed: %s\n", strerror(errno));
		return -errno;
	}

	for (i = 0; i < num; i++)
	{
		if (!if_indextoname(ifindices[i], ifname))
		{
			MESHU_ERR_INFO("get find name for ifindex %d\n", ifindices[i]);
			return -errno;
		}

		++ret;
		if(iterator(ifname, iarg)) 
			break;
		
	}

	return ret;

}

int mbridge_foreach_port(const char *brname, int (*iterator)(const char *br, const char *port, void *arg),void *arg)
{
	int i, err, count;
	struct ifreq ifr;
	char ifname[IFNAMSIZ];
	int ifindices[MAX_PORTS];
	unsigned long args[4] = { BRCTL_GET_PORT_LIST, (unsigned long)ifindices, MAX_PORTS, 0 };

	memset(ifindices, 0, sizeof(ifindices));
	strncpy(ifr.ifr_name, brname, IFNAMSIZ);
	ifr.ifr_data = (char *) &args;

	err = ioctl(mbridge_socket_fd, SIOCDEVPRIVATE, &ifr);
	if (err < 0)
	{
		MESHU_ERR_INFO("list ports for bridge:'%s' failed: %s\n", brname, strerror(errno));
		return -errno;
	}

	count = 0;
	for (i = 0; i < MAX_PORTS; i++)
	{
		if (!ifindices[i])
			continue;

		if (!if_indextoname(ifindices[i], ifname))
		{
			MESHU_ERR_INFO("can't find name for ifindex:%d\n", ifindices[i]);
			continue;
		}

		++count;
		if (iterator(brname, ifname, arg))
			break;
	}

	return count;
}
	
int mbridge_add_bridge(const char *brname)
{
	int ret;
	char _br[IFNAMSIZ];
	unsigned long arg[3] = { BRCTL_ADD_BRIDGE, (unsigned long) _br };

	strncpy(_br, brname, IFNAMSIZ);
	ret = ioctl(mbridge_socket_fd, SIOCSIFBR, arg);

	return ret < 0 ? errno : 0;
}

int mbridge_del_bridge(const char *brname)
{
	int ret;
	char _br[IFNAMSIZ];
	unsigned long arg[3] = { BRCTL_DEL_BRIDGE, (unsigned long) _br };

	strncpy(_br, brname, IFNAMSIZ);
	ret = ioctl(mbridge_socket_fd, SIOCSIFBR, arg);

	return  ret < 0 ? errno : 0;
}

int mbridge_add_interface(const char *bridge, const char *dev)
{
	struct ifreq ifr;
	int err;
	int ifindex = if_nametoindex(dev);
	unsigned long args[4] = { BRCTL_ADD_IF, ifindex, 0, 0 };

	if (ifindex == 0) 
		return ENODEV;
	
	strncpy(ifr.ifr_name, bridge, IFNAMSIZ);
					  
	ifr.ifr_data = (char *) args;
	err = ioctl(mbridge_socket_fd, SIOCDEVPRIVATE, &ifr);

	return err < 0 ? errno : 0;
}

int mbridge_del_interface(const char *bridge, const char *dev)
{
	struct ifreq ifr;
	int err;
	int ifindex = if_nametoindex(dev);
	unsigned long args[4] = { BRCTL_DEL_IF, ifindex, 0, 0 };

	if (ifindex == 0) 
		return ENODEV;
	
	strncpy(ifr.ifr_name, bridge, IFNAMSIZ);

	ifr.ifr_data = (char *) args;
	err = ioctl(mbridge_socket_fd, SIOCDEVPRIVATE, &ifr);

	return err < 0 ? errno : 0;
}

