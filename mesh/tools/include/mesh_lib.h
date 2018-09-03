/*
* $Id: mesh_lib.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef	__MESH_LIB_H__
#define	__MESH_LIB_H__

//#include <sys/types.h>
#include <sys/socket.h>		/* For AF_INET & struct sockaddr */
#include <linux/if.h>
#include <net/if_arp.h>		/* For ARPHRD_ETHER */
#include <netinet/in.h>         /* For struct sockaddr_in */
#include <netinet/if_ether.h>

#include "mesh.h"
#include "meshw_ioctl.h"

#include <sys/ioctl.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>		/* struct timeval */
#include <unistd.h>
#include <net/ethernet.h>	/* struct ether_addr */
#include <netdb.h>		/* gethostbyname, getnetbyname */

#define	MESH_DEVICE_NAME			"/dev/mesh/"

/* Error codes defined for setting args  */
#define MESHWU_ERR_ARG_NUM		-2
#define MESHWU_ERR_ARG_TYPE		-3
#define MESHWU_ERR_ARG_SIZE		-4
#define MESHWU_ERR_ARG_CONFLICT	-5
#define MESHWU_ERR_SET_EXT		-6
#define MESHWU_ERR_GET_EXT		-7

#ifndef	TOOL_NAME
#define	TOOL_NAME			"SWJTU_MESH"
#endif

#define	MESHU_ERR_INFO(_fmt, ...)	do{ 					\
	printf( TOOL_NAME " ERROR : " _fmt, ##__VA_ARGS__);	\
}while(0)

#define	MESHU_WARN_INFO(_fmt, ...)	do{ 					\
	printf(TOOL_NAME " WARN : " _fmt, ##__VA_ARGS__);	\
}while(0)


#define	MESHU_DEBUG_INFO( _fmt, ...)	do{ 			\
	printf(TOOL_NAME " INFO : " _fmt, ##__VA_ARGS__);		\
}while(0)


#define	MESHU_IOCTL(fd, cmd, param, res)	do{	\
	res = ioctl(fd, cmd, param);			\
	if(res ) MESHU_ERR_INFO("IOCTL command %x failed: %s\n", cmd, strerror(errno));		\
}while(0)


#define	MESH_DEVICE_CLOSE(devFd)	\
	close(devFd)

typedef int (*netdev_enum_handler)(char *brname, char *args[], int count);

char 	*meshu_mac_ntop(const unsigned char *mac,int maclen,char *buf, int buflen);
int 		meshu_mac_aton(const char *orig, unsigned char *mac, int macmax);
void 	meshu_ether_ntop(const struct ether_addr *eth,  char *buf);
int 		meshu_ether_aton(const char *orig, struct ether_addr *eth);

int		meshu_device_open(int index);

int meshu_device_startup( int fd , int isStart);
int meshu_portal_enable( int fd , int isEnable);
int meshu_set_mesh_ID(int fd, unsigned char *address, int length);
int meshu_fwd_cmd(int fd, fwd_request_t *fwdReq );


extern int mbridge_init(void);
extern void mbridge_shutdown(void);

extern int mbridge_add_bridge(const char *brname);
extern int mbridge_del_bridge(const char *brname);
extern int mbridge_add_interface(const char *br, const char *dev);
extern int mbridge_del_interface(const char *br, const char *dev);

#endif

