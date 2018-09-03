#ifndef	___FWD_H__
#define	___FWD_H__
/*
* $Id: _fwd.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "mesh.h"

void mesh_fwd_db_delete_by_dev(MESH_FWD_TABLE *table, MESH_DEVICE *dev);

MESH_FWD_ITEM *mesh_fwd_db_get(MESH_FWD_TABLE *table, unsigned char *addr);
void mesh_fwd_db_put(MESH_FWD_ITEM *item);

void fwd_deliver(MESH_DEVICE *to, struct sk_buff *skb);
void fwd_forward(MESH_DEVICE *to, struct sk_buff *skb);
void mesh_fwd_flood_deliver(MESH_FWD_TABLE *table, struct sk_buff *skb, int clone);
void mesh_fwd_flood_forward(MESH_FWD_TABLE *table, struct sk_buff *skb, int clone);

int mesh_fwd_handle_frame(struct sk_buff *skb, MESH_DEVICE *dev, MESH_PACKET *packet);


#endif

