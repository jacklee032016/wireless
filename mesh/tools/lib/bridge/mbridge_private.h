#ifndef __MBRIDGE_PRIVATE_H__
#define __MBRIDGE_PRIVATE_H__

/*
 * $Id: mbridge_private.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include <linux/sockios.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/if_bridge.h>
#include <asm/param.h>

#define MAX_BRIDGES	1024
#define MAX_PORTS	1024

#define dprintf(fmt,arg...)

extern int mbridge_socket_fd;

static inline unsigned long __tv_to_jiffies(const struct timeval *tv)
{
	unsigned long long jif;

	jif = 1000000ULL * tv->tv_sec + tv->tv_usec;

	return (HZ*jif)/1000000;
}

static inline void __jiffies_to_tv(struct timeval *tv, unsigned long jiffies)
{
	unsigned long long tvusec;

	tvusec = (1000000ULL*jiffies)/HZ;
	tv->tv_sec = tvusec/1000000;
	tv->tv_usec = tvusec - 1000000 * tv->tv_sec;
}

#endif

