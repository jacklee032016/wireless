/*
* $Id: _route.h,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/

#ifndef  __ROUTE_H__
#define __ROUTE_H__

#define   ROUTE_RREQ_TO_HB(request)  \
	request->rreq_id = ntohl(request->rreq_id);		\
	request->dst_seq = ntohl(request->dst_seq);	\
	request->src_seq = ntohl(request->src_seq)

#define 	ROUTE_RREQ_TO_NB(request)	\
	request->rreq_id = htonl(request->rreq_id);	\
	request->dst_seq = htonl(request->dst_seq);	\
	request->src_seq = htonl(request->src_seq)

#define ROUTE_REPLY_TO_HB( reply) \
	reply->dst_seq = ntohl(reply->dst_seq); \
	reply->lifetime = ntohl(reply->lifetime)

#define ROUTE_REPLY_TO_NB( reply) \
	reply->dst_seq = htonl(reply->dst_seq); \
	reply->lifetime = htonl(reply->lifetime)



/* local reference for Node MGMT */
int _task_insert_from_timer(route_node_t *node, route_task_t * timer_task);
void _timer_update(route_node_t *node );
int _timer_insert(route_node_t *node, aodv_address_t *address, task_subtype_t subtype, u_int32_t delay);
route_task_t *_timer_lookup(route_node_t *node, u_int32_t id, task_subtype_t type);
void _timer_delete(route_node_t *node,aodv_address_t *address, task_subtype_t subtype, u_int32_t id);

route_flood_t *_flood_lookup(route_node_t *node, aodv_address_t *orig, u_int32_t reqId);
int _flood_insert(route_node_t *node, aodv_address_t *orig, aodv_address_t *dest, u_int32_t reqId, unsigned long  lifetime);

int _task_enqueue(route_node_t *node, route_task_t *task);
void _node_destroy(route_node_t *node);


route_neigh_t *_neigh_lookup(route_node_t *node, pdu_task_t *task);
route_neigh_t *_neigh_lookup_mac(route_node_t *node, char *hw_addr);
void _neigh_update_link(route_node_t *node, char *hw_addr, u_int8_t link);
int _neigh_delete(route_node_t *node, aodv_address_t *address );
route_neigh_t *_neigh_insert(route_node_t *node, pdu_task_t *task);
void _neigh_update_route(route_node_t *node, route_neigh_t *nextHop, pdu_task_t *task);
int _neigh_update(route_node_t *node, route_neigh_t *neigh, aodv_pdu_reply *reply);

int _route_valid( mesh_route_t * route);
int _route_update(route_node_t *node, aodv_address_t *destAdd, route_neigh_t *nextHop, u_int8_t metric );
mesh_route_t *_route_lookup(route_node_t *node, aodv_address_t *address, route_type_t type);
mesh_route_t *_route_insert(route_node_t *node, route_neigh_t *nextHop, aodv_address_t *address, route_type_t type);
int _route_delete(route_node_t *node, aodv_address_t *dest);
/* end of local reference */

/* local reference for Task Ops */
int _dev_rx_request(route_dev_t *dev, pdu_task_t *task);
int _dev_rx_error(route_dev_t *dev, pdu_task_t *task);
int _dev_rx_reply(route_dev_t *dev, pdu_task_t *task);
int _dev_rx_hello(route_dev_t *dev, pdu_task_t *task);
int  _dev_rx_reply_ack(route_dev_t *dev, pdu_task_t *task);

int _dev_send_request(route_dev_t *dev, route_task_t  *retry);
void _dev_send_reply_ack(route_dev_t *dev, pdu_task_t *task);
int _dev_send_reply(route_dev_t *dev, pdu_task_t *task);

/* end of local reference */

int node_init(route_node_t *node, char *localSubnet);

void _route_expire(mesh_route_t *route);

unsigned long  getAodvEpoch();

route_task_t *route_task_create(route_node_t *node, task_subtype_t type);

void _timer_enqueue(route_node_t *node, route_task_t *timer);


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

extern	void mesh_sysctl_register(route_node_t *node);
extern	void mesh_sysctl_unregister(route_node_t *node);
#endif /* CONFIG_SYSCTL */


extern	mgmt_ops_t		node_mops;
extern	proto_ops_t		aodv_ops;
extern	route_proc_entry  	proc_table[];


#endif

