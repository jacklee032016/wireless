/*
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	$Id: if_bridge.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_IF_BRIDGE_H
#define _LINUX_IF_BRIDGE_H

#include <linux/types.h>

#ifdef __KERNEL__

#include <linux/netdevice.h>

struct net_bridge;
struct net_bridge_port;

extern int (*br_ioctl_hook)(unsigned long arg);
extern int (*br_handle_frame_hook)(struct sk_buff *skb);
extern int (*br_should_route_hook)(struct sk_buff **pskb);

#endif

#endif
