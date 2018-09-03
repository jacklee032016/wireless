/*
* $Id: mesh.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
* Mesh core module which is independent on 80211 or 80216
* just a group of kernel service for Mesh MAC
*/
#ifndef  __MESH_H__
#define	__MESH_H__

#ifdef	__KERNEL__

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#include <linux/moduleparam.h>
#endif
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/interrupt.h>

#include <linux/poll.h>
#include <linux/if.h>
#include <asm/uaccess.h>

#include "linux_compat.h"

#endif	/* __KERNEL__ */

#define	MESHNAMSIZ	16

#ifndef 	WITH_MISC_MODE
#define	WITH_MISC_MODE	0
#endif

#ifndef ETH_ALEN 
#define ETH_ALEN	6		/* Octets in one ethernet addr	 */
#endif

#ifndef 	WITH_QOS
#define	WITH_QOS			0
#endif

#include	"mesh_ioctl.h"


#define	MESH_VENDOR							"SWJTU"

#define	MESH_VERSION							"00.01.00.00-SWJTU"
#define	MESH_INFO								"mesh"
#define	MESH_WLAN_PHY_INFO					"PHY"
#define	MESH_WLAN_MAC_INFO					"MAC"
#define	MESH_WLAN_HW_INFO					"HW"

#define 	SWJTU_MESH_DEV_MAJOR				199
#define 	SWJTU_MESH_DEV_NAME					"mesh"
#define 	SWJTU_MESH_NAME_PORTAL				"portal"
#define 	SWJTU_MESH_NAME_FWD					"mac_fwd"

#define	SWJTU_MESH_SYSCTRL_ROOT_DEV		CTL_DEV		/* /proc/sys/mesh */
#define	SWJTU_MESH_SYSCTRL_DEVICE			8			/* /proc/sys/mesh/dev */		
#define	SWJTU_MESH_SYSCTRL_DEV_WIFI			-2			/* /proc/sys/mesh/dev/wifi, auto ID for sysctl */
#define	SWJTU_MESH_SYSCTRL_DEV_AUTO		-2

#define	SWJTU_MESH_SYSCTRL_DEVICE_NAME		"dev"
#define	SWJTU_MESH_SYSCTRL_WIFI_NAME		"wifi"

#define	SWJTU_MESH_DEFAULT_MESHID			"021212"

#define	MESH_BRIDGE_NAME						"mbr"

#define	SWJTU_DEVICE_MINOR_MGR				0
#define	SWJTU_DEVICE_MINOR_PORT				1		/* begin from 1 */

#define	SWJTU_MESH_PROC_NAME				MESH_INFO


#define	SWJTU_MESH_NAME_LENGTH				32

#ifdef __KERNEL__
typedef int (*mesh_read_proc_t)(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data);

typedef struct
{
	unsigned char			name[SWJTU_MESH_NAME_LENGTH];
	mesh_read_proc_t		read;
	struct proc_dir_entry	*proc;
}mesh_proc_entry;


/* device file definations, common for MESH_MGR and MESH_DEVICE, so it must be first field of these type */
typedef	struct
{
	int						index;
	int						openMode;
	
	struct semaphore			inSema;						/* READ/WRITE simutaneously, lizhjie,2006.1.1 */
	struct semaphore			outSema;
	struct semaphore			ioctrlSema;						/* added for IOCTL operation,2006.1.1 */
}mesh_inode;

struct _mesh_id
{
	unsigned char		meshAddr[ETH_ALEN];
};

//typedef	int		MESH_ID;
typedef	struct _mesh_id		MESH_ID;

struct _mesh_packet_type;
struct _mesh_device;
struct _mesh_route_table;
struct _mesh_route_entry;
struct _mesh_node;
struct _mesh_mgr;
struct _mesh_portal_dev;
struct _mesh_fwd_table;
	
struct _mesh_route_entry
{
};

struct _mesh_route_table
{
};

struct _mesh_node
{
	MESH_LIST_NODE			neighbors;
};

struct _mesh_mgr
{
	mesh_inode				dev;

	int						isInitted;
	
	MESH_ID					id;
	char						name[SWJTU_MESH_NAME_LENGTH];

	int						debug;

#ifdef CONFIG_SYSCTL
	char						meshProcname[12];		/* /proc/sys/net/mesh%d */
	struct ctl_table_header		*meshSysctlHeader;
	struct ctl_table			*meshSysctls;
#endif

	/* portal interface for this node */
	int						(*add_portal)(struct _mesh_mgr *mgr, void *data);
	int						(*remove_portal)(struct _mesh_mgr *mgr, void *data);

	int						(*do_ioctl)(struct _mesh_mgr *mgr,unsigned long data, int cmd);

	mesh_proc_entry			*mgrProc;

	atomic_t					refcnt;		/* reference count */

	mesh_rw_lock_t			*rwLock;
	mesh_lock_t				lock;

	MESH_TASK				rxTask;
	MESH_TASK				txTask;

	struct _mesh_packet_type	*ptype_base[16];		/* 16 way hashed list */
	struct _mesh_packet_type	*ptype_all;				/* Taps */
	
	struct _mesh_node		*myself;

	struct _mesh_portal_dev	*portal;

	struct _mesh_fwd_table	*fwdTable;
	
	MESH_LIST_NODE			ports;
	MESH_LIST_NODE			neighbors;
};


#include "mesh_dev.h"

#include <linux/netdevice.h>
struct net_device;
struct net_device_stats;

struct	_mesh_portal_dev;


struct 	_mesh_portal_bridge
{
	char		name[MESHNAMSIZ];
	struct	_mesh_portal_dev		*portal;

	struct	net_device			*master;	/* Master device of this transparent bridge, eg. ethernet device */
	struct	_mesh_portal_bridge	*next;
};

typedef	struct _mesh_portal_bridge		MESH_BRIDGE;

struct	_mesh_portal_dev
{
	int							count;		/* count of MESH_BRIDGE */
	MESH_BRIDGE				*mbridges;
	
	struct	_mesh_device			mesh;
	struct	_mesh_device_stats	meshStats;
	
	struct	net_device			net;
	struct	net_device_stats 		netStats;

	struct	_mesh_mgr		*mgr;

	/* should add lock for portal device */
};

typedef 	struct _mesh_route_entry	MESH_ROUTE_ENTRY;
typedef	struct _mesh_route_table	MESH_ROUTE_TABLE;
typedef	struct _mesh_node		MESH_NODE;
typedef	struct _mesh_mgr			MESH_MGR;
typedef	struct _mesh_portal_dev	MESH_PORTAL;

#include "mesh_fwd.h"

#include "meshw_ioctl_handler.h"	

#define	MESH_ERR_INFO(_fmt, ...)	do{ 					\
	printk(KERN_ERR MESH_VENDOR " : " _fmt, ##__VA_ARGS__);	\
}while(0)

#define	MESH_WARN_INFO(_fmt, ...)	do{ 					\
	printk(KERN_WARNING MESH_VENDOR " : " _fmt, ##__VA_ARGS__);	\
}while(0)


#define	MESH_DEBUG_INFO( opt, _fmt, ...)	do{ 			\
	if ( swjtu_get_mgr()->debug & (opt))								\
	printk(KERN_INFO MESH_VENDOR " : " _fmt, ##__VA_ARGS__);		\
}while(0)

#define	MESH_DEBUG_INIT			0x00000001
#define	MESH_DEBUG_PACKET		0x00000002

#define	MESH_DEBUG_MGR			0x00000010
#define	MESH_DEBUG_PORT			0x00000020

#define	MESH_DEBUG_IOCTL			0x00000100

#define	MESH_DEBUG_PORTAL		0x00001000		/* debug for portal device*/

#define	MESH_DEBUG_FWD			0x00010000

#define	MESH_DEBUG_DATAIN		0x00100000
#define	MESH_DEBUG_DATAOUT		0x00100000

#ifdef CONFIG_SYSCTL
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)
#define	MESH_SYSCTL_DECL(f, ctl, write, filp, buffer, lenp, ppos) \
	f(ctl_table *ctl, int write, struct file *filp, void *buffer, \
		size_t *lenp)		
#define	MESH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos) \
	proc_dointvec(ctl, write, filp, buffer, lenp)
#else
#define	MESH_SYSCTL_DECL(f, ctl, write, filp, buffer, lenp, ppos) \
	f(ctl_table *ctl, int write, struct file *filp, void *buffer,\
		size_t *lenp, loff_t *ppos)
#define	MESH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos) \
	proc_dointvec(ctl, write, filp, buffer, lenp, ppos)
#endif

extern	void swjtu_mesh_sysctl_register(MESH_MGR *mgr);
extern	void swjtu_mesh_sysctl_unregister(MESH_MGR *mgr);


#endif /* CONFIG_SYSCTL */

#define	MESH_GET_MGR()	swjtu_get_mgr()

extern	int 	meshdev_max_backlog;

#define 	ktrace			printk(KERN_ERR "%s_%s()_%d lines\n", __FILE__, __FUNCTION__, __LINE__ )

MESH_MGR	*swjtu_get_mgr(void );
MESH_DEVICE *swjtu_get_port_from_mgr(int index);
MESH_DEVICE *swjtu_get_port_by_name(char *devname);

int	swjtu_mesh_register_port( MESH_DEVICE *port);
void	swjtu_mesh_unregister_port(MESH_DEVICE *port);

void swjtu_mesh_register_packet(MESH_PACKET *pt);
void swjtu_mesh_unregister_packet(MESH_PACKET *pt);

int swjtu_mesh_process_backlog(MESH_DEVICE *backlog_dev, int *budget);
void swjtu_mesh_rx_action(unsigned long data);//struct softirq_action *h);
int swjtu_meshif_rx(struct sk_buff *skb);

int swjtu_mesh_queue_xmit(struct sk_buff *skb);
void swjtu_mesh_tx_action(unsigned long data);//struct softirq_action *h)

int swjtu_mgr_ioctl(MESH_MGR *mgr, unsigned long data, int cmd);
int swjtu_remove_portal(MESH_MGR *mgr,void *data);
int swjtu_create_portal(MESH_MGR *mgr, void *data);

extern	const char *swjtu_mesh_mac_sprintf(const u_int8_t *);
extern	void swjtu_mesh_dump_rawpkt(const u_int8_t *buf, int len, int direction, char *msg);

void swjtu_mesh_fwd_db_insert(MESH_FWD_TABLE *table, MESH_DEVICE *source, unsigned char *addr, int isLocal, int isStatic);

#endif /* __KERNEL__ */


#endif

