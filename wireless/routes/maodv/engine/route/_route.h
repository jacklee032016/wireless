/*
* $Id: _route.h,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
*/

#ifndef  __ROUTE_H__
#define __ROUTE_H__


int engine_init(route_engine_t *engine, char *localSubnet);

int task_insert(route_engine_t *engine, route_task_type_t  type, struct sk_buff *packet);

void _route_expire(mesh_route_t *route);

unsigned long  getAodvEpoch();

route_task_t *route_task_create(route_engine_t *engine, route_task_type_t type);


extern	mgmt_ops_t		engine_mops;
extern	proto_ops_t		aodv_ops;
extern	route_proc_entry  	proc_table[];


#endif

