/*
 * $Id: mbridge_devif.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/fcntl.h>

#include "mesh_lib.h"
#include "mbridge.h"
#include "mbridge_private.h"

static const char *state_names[5] =
{
	[BR_STATE_DISABLED] 	= "disabled", 
	[BR_STATE_LISTENING] 	= "listening", 
	[BR_STATE_LEARNING] 	= "learning", 
	[BR_STATE_FORWARDING] 	= "forwarding", 
	[BR_STATE_BLOCKING] 	= "blocking",
};

const char *mbridge_get_state_name(int state)
{
	if (state >= 0 && state <= 4)
		return state_names[state];

	return "<INVALID STATE>";
}

int __br_hz_internal;

int __get_hz(void)
{
	const char * s = getenv("HZ");
	return s ? atoi(s) : HZ;
}

/*
 * Convert device name to an index in the list of ports in bridge.
 *
 * Old API does bridge operations as if ports were an array
 * inside bridge structure.
 */
static int get_portno(const char *brname, const char *ifname)
{
	int i;
	int ifindex = if_nametoindex(ifname);
	int ifindices[MAX_PORTS];
	unsigned long args[4] = { BRCTL_GET_PORT_LIST,
				  (unsigned long)ifindices, MAX_PORTS, 0 };
	struct ifreq ifr;

	if (ifindex <= 0)
		goto error;

	memset(ifindices, 0, sizeof(ifindices));
	strncpy(ifr.ifr_name, brname, IFNAMSIZ);
	ifr.ifr_data = (char *) &args;

	if (ioctl(mbridge_socket_fd, SIOCDEVPRIVATE, &ifr) < 0)
	{
		MESHU_ERR_INFO("get_portno: get ports of %s failed: %s\n",  brname, strerror(errno));
		goto error;
	}

	for (i = 0; i < MAX_PORTS; i++)
	{
		if (ifindices[i] == ifindex)
			return i;
	}

	dprintf("%s is not a in bridge %s\n", ifname, brname);
 error:
	return -1;
}

/* get information via ioctl */
int mbridge_get_bridge_info(const char *bridge, struct bridge_info *info)
{
	struct ifreq ifr;
	struct __bridge_info i;
	unsigned long args[4] = { BRCTL_GET_BRIDGE_INFO,
				  (unsigned long) &i, 0, 0 };

	memset(info, 0, sizeof(*info));
	strncpy(ifr.ifr_name, bridge, IFNAMSIZ);
	ifr.ifr_data = (char *) &args;

	if (ioctl(mbridge_socket_fd, SIOCDEVPRIVATE, &ifr) < 0)
	{
		MESHU_ERR_INFO("%s: can't get info %s\n", bridge, strerror(errno));
		return errno;
	}

	memcpy(&info->designated_root, &i.designated_root, 8);
	memcpy(&info->bridge_id, &i.bridge_id, 8);
	info->root_path_cost = i.root_path_cost;
	info->root_port = i.root_port;
	info->topology_change = i.topology_change;
	info->topology_change_detected = i.topology_change_detected;
	info->stp_enabled = i.stp_enabled;
	__jiffies_to_tv(&info->max_age, i.max_age);
	__jiffies_to_tv(&info->hello_time, i.hello_time);
	__jiffies_to_tv(&info->forward_delay, i.forward_delay);
	__jiffies_to_tv(&info->bridge_max_age, i.bridge_max_age);
	__jiffies_to_tv(&info->bridge_hello_time, i.bridge_hello_time);
	__jiffies_to_tv(&info->bridge_forward_delay, i.bridge_forward_delay);
	__jiffies_to_tv(&info->ageing_time, i.ageing_time);
	__jiffies_to_tv(&info->hello_timer_value, i.hello_timer_value);
	__jiffies_to_tv(&info->tcn_timer_value, i.tcn_timer_value);
	__jiffies_to_tv(&info->topology_change_timer_value, 
			i.topology_change_timer_value);
	__jiffies_to_tv(&info->gc_timer_value, i.gc_timer_value);

	return 0;
}

int mbridge_get_port_info(const char *brname, const char *port, struct port_info *info)
{
	struct __port_info i;
	int index;

	memset(info, 0, sizeof(*info));

	index = get_portno(brname, port);
	if (index < 0)
		return errno;
	else
	{
		struct ifreq ifr;
		unsigned long args[4] = { BRCTL_GET_PORT_INFO, (unsigned long) &i, index, 0 };
	
		strncpy(ifr.ifr_name, brname, IFNAMSIZ);
		ifr.ifr_data = (char *) &args;
		
		if (ioctl(mbridge_socket_fd, SIOCDEVPRIVATE, &ifr) < 0)
		{
			MESHU_ERR_INFO("old can't get port %s(%d) info %s\n", brname, index, strerror(errno));
			return errno;
		}
	}

	info->port_no = index;
	memcpy(&info->designated_root, &i.designated_root, 8);
	memcpy(&info->designated_bridge, &i.designated_bridge, 8);
	info->port_id = i.port_id;
	info->designated_port = i.designated_port;
	info->path_cost = i.path_cost;
	info->designated_cost = i.designated_cost;
	info->state = i.state;
	info->top_change_ack = i.top_change_ack;
	info->config_pending = i.config_pending;
	__jiffies_to_tv(&info->message_age_timer_value, 
			i.message_age_timer_value);
	__jiffies_to_tv(&info->forward_delay_timer_value, 
			i.forward_delay_timer_value);
	__jiffies_to_tv(&info->hold_timer_value, i.hold_timer_value);
	return 0;
}

static int mbridge_set(const char *bridge, const char *name,
		  unsigned long value, unsigned long oldcode)
{
	int ret;
	struct ifreq ifr;
	unsigned long args[4] = { oldcode, value, 0, 0 };
	
	strncpy(ifr.ifr_name, bridge, IFNAMSIZ);
	ifr.ifr_data = (char *) &args;
	ret = ioctl(mbridge_socket_fd, SIOCDEVPRIVATE, &ifr);

	return ret < 0 ? errno : 0;
}

int mbridge_set_bridge_forward_delay(const char *br, struct timeval *tv)
{
	return mbridge_set(br, "forward_delay", __tv_to_jiffies(tv), BRCTL_SET_BRIDGE_FORWARD_DELAY);
}

int mbridge_set_bridge_hello_time(const char *br, struct timeval *tv)
{
	return mbridge_set(br, "hello_time", __tv_to_jiffies(tv), BRCTL_SET_BRIDGE_HELLO_TIME);
}

int mbridge_set_bridge_max_age(const char *br, struct timeval *tv)
{
	return mbridge_set(br, "max_age", __tv_to_jiffies(tv), BRCTL_SET_BRIDGE_MAX_AGE);
}

int mbridge_set_ageing_time(const char *br, struct timeval *tv)
{
	return mbridge_set(br, "ageing_time", __tv_to_jiffies(tv), BRCTL_SET_AGEING_TIME);
}

int mbridge_set_stp_state(const char *br, int stp_state)
{
	return mbridge_set(br, "stp_state", stp_state, BRCTL_SET_BRIDGE_STP_STATE);
}

int mbridge_set_bridge_priority(const char *br, int bridge_priority)
{
	return mbridge_set(br, "priority", bridge_priority, BRCTL_SET_BRIDGE_PRIORITY);
}

static int port_set(const char *bridge, const char *ifname, 
		    const char *name, unsigned long value, 
		    unsigned long oldcode)
{
	int ret, index;
	if ( (index = get_portno(bridge, ifname)) < 0)
		ret = index;

	else
	{
		struct ifreq ifr;
		unsigned long args[4] = { oldcode, index, value, 0 };

		strncpy(ifr.ifr_name, bridge, IFNAMSIZ);
		ifr.ifr_data = (char *) &args;
		ret = ioctl(mbridge_socket_fd, SIOCDEVPRIVATE, &ifr);
	}

	return ret < 0 ? errno : 0;
}

int mbridge_set_port_priority(const char *bridge, const char *port, int priority)
{
	return port_set(bridge, port, "priority", priority, BRCTL_SET_PORT_PRIORITY);
}

int mbridge_set_path_cost(const char *bridge, const char *port, int cost)
{
	return port_set(bridge, port, "path_cost", cost, BRCTL_SET_PATH_COST);
}

static inline void __copy_fdb(struct fdb_entry *ent, const struct __fdb_entry *f)
{
	memcpy(ent->mac_addr, f->mac_addr, 6);
	ent->port_no = f->port_no;
	ent->is_local = f->is_local;
	__jiffies_to_tv(&ent->ageing_timer_value, f->ageing_timer_value);
}

int mbridge_read_fdb(const char *bridge, struct fdb_entry *fdbs, unsigned long offset, int num)
{
	int i, fd = -1, n;
	struct __fdb_entry fe[num];
		/* old kernel, use ioctl */
	unsigned long args[4] = { BRCTL_GET_FDB_ENTRIES,
				  (unsigned long) fe,
				  num, offset };
	struct ifreq ifr;
	int retries = 0;

	strncpy(ifr.ifr_name, bridge, IFNAMSIZ);
	ifr.ifr_data = (char *) args;

retry:
	n = ioctl(mbridge_socket_fd, SIOCDEVPRIVATE, &ifr);

	/* table can change during ioctl processing */
	if (n < 0 && errno == EAGAIN && ++retries < 10) {
		sleep(0);
		goto retry;
	}

	for (i = 0; i < n; i++) 
		__copy_fdb(fdbs+i, fe+i);

	if (fd > 0)
		close(fd);
	
	return n;
}

