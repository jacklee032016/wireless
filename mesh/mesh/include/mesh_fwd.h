/*
 * $Id: mesh_fwd.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#ifndef __MESH_FWD_H__
#define __MESH_FWD_H__


#define	MESH_FWD_VERSION								1

#define	MESH_FWDCTL_GET_VERSION						0
#define	MESH_FWDCTL_GET_BRIDGES						1
#define	MESH_FWDCTL_ADD_BRIDGE						2
#define	MESH_FWDCTL_DEL_BRIDGE						3
#define	MESH_FWDCTL_ADD_IF							4
#define	MESH_FWDCTL_DEL_IF							5
#define	MESH_FWDCTL_GET_BRIDGE_INFO				6
#define	MESH_FWDCTL_GET_PORT_LIST					7
#define	MESH_FWDCTL_SET_BRIDGE_FORWARD_DELAY		8
#define	MESH_FWDCTL_SET_BRIDGE_HELLO_TIME			9
#define	MESH_FWDCTL_SET_BRIDGE_MAX_AGE			10
#define	MESH_FWDCTL_SET_AGEING_TIME				11
#define	MESH_FWDCTL_SET_GC_INTERVAL					12
#define	MESH_FWDCTL_GET_PORT_INFO					13
#define	MESH_FWDCTL_SET_BRIDGE_STP_STATE			14
#define	MESH_FWDCTL_SET_BRIDGE_PRIORITY			15
#define	MESH_FWDCTL_SET_PORT_PRIORITY				16
#define	MESH_FWDCTL_SET_PATH_COST					17
#define	MESH_FWDCTL_GET_FDB_ENTRIES				18

#define	MESH_FWD_STATE_DISABLED						0
#define	MESH_FWD_STATE_LISTENING					1
#define	MESH_FWD_STATE_LEARNING						2
#define	MESH_FWD_STATE_FORWARDING					3
#define	MESH_FWD_STATE_BLOCKING						4

struct __mesh_fwd_table_info
{
	__u64			designated_root;
	__u64			bridge_id;
	__u32			root_path_cost;
	__u32			max_age;
	__u32			hello_time;
	__u32			forward_delay;
	__u32			bridge_max_age;
	__u32			bridge_hello_time;
	__u32			bridge_forward_delay;
	__u8			topology_change;
	__u8			topology_change_detected;
	__u8			root_port;
	__u8			stp_enabled;
	__u32			ageing_time;
	__u32			gc_interval;
	__u32			hello_timer_value;
	__u32			tcn_timer_value;
	__u32			topology_change_timer_value;
	__u32			gc_timer_value;
};

#if 0
struct __port_info
{
	__u64			designated_root;
	__u64			designated_bridge;
	__u16			port_id;
	__u16			designated_port;
	__u32			path_cost;
	__u32			designated_cost;
	__u8			state;
	__u8			top_change_ack;
	__u8			config_pending;
	__u8			unused0;
	__u32			message_age_timer_value;
	__u32			forward_delay_timer_value;
	__u32			hold_timer_value;
};
#endif

struct __mesh_fwd_entry
{
	char			devName[MESHNAMSIZ];
	__u8		mac_addr[6];
	__u16		port_no;
	__u32		isLocal;
	__u32		isStatic;
	__u32		ageing_timer_value;
	__u32		unused;
};


#ifdef __KERNEL__

#include <linux/netdevice.h>
#include <linux/miscdevice.h>
#include <linux/if_bridge.h>

#define	MESH_FWD_HASH_BITS 		8
#define	MESH_FWD_HASH_SIZE 		(1 << MESH_FWD_HASH_BITS)

#define	MESH_FWD_HOLD_TIME 		(1*HZ)

#define	MESH_IS_MULTICAST(macAddr)		((macAddr[0]) & 0x01)

struct fwd_timer
{
	int 				running;
	unsigned long		expires;		/* in unit of jiffies */
};

extern __inline__ void fwd_timer_clear(struct fwd_timer *t)
{
	t->running = 0;
}

extern __inline__ unsigned long fwd_timer_get_residue(struct fwd_timer *t)
{
	if (t->running)
		return jiffies - t->expires;

	return 0;
}

extern __inline__ void fwd_timer_set(struct fwd_timer *t, unsigned long x)
{
	t->expires = x;
	t->running = 1;
}

extern __inline__ int fwd_timer_is_running(struct fwd_timer *t)
{
	return t->running;
}

extern __inline__ int fwd_timer_has_expired(struct fwd_timer *t, unsigned long to)
{
	return t->running && time_after_eq(jiffies, t->expires + to);
}

typedef struct meshmac_addr meshmac_addr;

//typedef __u16 port_id;

#if WITH_BR_PORT
struct bridge_id
{
	unsigned char		prio[2];
	unsigned char		addr[6];
};
typedef struct bridge_id bridge_id;
#endif
struct meshmac_addr
{
	unsigned char		addr[6];
	unsigned char		pad[2];
};

 
/*
 */
struct _mesh_mib
{
 	unsigned long		MeshInReceives;
 	unsigned long		MeshInHdrErrors;
 	unsigned long		MeshInAddrErrors;
 	unsigned long		MeshForwDatagrams;
 	unsigned long		MeshInUnknownProtos;
 	unsigned long		MeshInDiscards;
 	unsigned long		MeshInDelivers;
 	unsigned long		MeshOutRequests;
 	unsigned long		MeshOutDiscards;
 	unsigned long		MeshOutNoRoutes;
 	unsigned long		MeshReasmTimeout;
 	unsigned long		MeshReasmReqds;
 	unsigned long		MeshReasmOKs;
 	unsigned long		MeshReasmFails;
 	unsigned long		MeshFragOKs;
 	unsigned long		MeshFragFails;
 	unsigned long		MeshFragCreates;

	unsigned long		rxPackets;
	unsigned long		rxBytes;

	unsigned long		txDropped;
	
	unsigned long		__pad[0]; 
} ____cacheline_aligned;


struct _mesh_fwd_entry
{
	struct _mesh_fwd_entry	*next_hash;
	struct _mesh_fwd_entry	**pprev_hash;
	
	atomic_t					use_count;
	meshmac_addr				addr;

	MESH_DEVICE			*dest;
//	struct net_bridge_port		*dst;
	
	unsigned long				ageing_timer;
	unsigned					isLocal;// :1;
	unsigned					isStatic; //:1;
};

#if 0
struct net_bridge_port
{
	struct net_bridge_port		*next;
	struct net_bridge		*br;
	struct net_device		*dev;
	int				port_no;

	/* STP */
	port_id				port_id;
	int				state;
	int				path_cost;
	bridge_id			designated_root;
	int				designated_cost;
	bridge_id			designated_bridge;
	port_id				designated_port;
	unsigned			topology_change_ack:1;
	unsigned			config_pending:1;
	int				priority;

	struct fwd_timer			forward_delay_timer;
	struct fwd_timer			hold_timer;
	struct fwd_timer			message_age_timer;
};
#endif

struct _mesh_fwd_table
{
	char						name[MESHNAMSIZ];
	mesh_rw_lock_t			lock;

	mesh_rw_lock_t			hashRwLock;
	struct _mesh_fwd_entry	*hash[MESH_FWD_HASH_SIZE];
	struct timer_list			tick;

	MESH_MGR				*mgr;
	
	/* STP */
#if WITH_BR_PORT
	bridge_id					designated_root;
	bridge_id					bridge_id;
#endif
	int						root_path_cost;
	int						root_port;
	int						max_age;
	int						hello_time;
	int						forward_delay;
	int						bridge_max_age;
	int						bridge_hello_time;
	int						bridge_forward_delay;
	unsigned					stp_enabled:1;
	unsigned					topology_change:1;
	unsigned					topology_change_detected:1;

	struct fwd_timer			hello_timer;
	struct fwd_timer			tcn_timer;
	struct fwd_timer			topology_change_timer;
	struct fwd_timer			gc_timer;

	int						ageing_time;
	int						gc_interval;

	struct _mesh_mib			mib;

	mesh_proc_entry			procEntry;

	void 					*priv;
};

typedef	struct _mesh_fwd_entry	MESH_FWD_ITEM;
typedef	struct _mesh_fwd_table	MESH_FWD_TABLE;
typedef	struct _mesh_mib			MESH_MIB;

extern unsigned char bridge_ula[6];


MESH_FWD_TABLE *swjtu_fwd_init_table(MESH_MGR *mgr, char *name);
int swjtu_fwd_add_dev(MESH_FWD_TABLE *table, MESH_DEVICE *dev);
int swjtu_fwd_del_dev(MESH_FWD_TABLE *table, MESH_DEVICE *dev);

#endif	/* __KERNEL__ */

#endif

