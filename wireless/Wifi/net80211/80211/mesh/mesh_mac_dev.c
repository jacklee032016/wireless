/*
* $Id: mesh_mac_dev.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
*/

//#include <asm/uaccess.h>

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/if_vlan.h>

#include <linux/ip.h>
#include <linux/udp.h>

#include "if_llc.h"
#include "if_ethersubr.h"
#include "if_media.h"


#include <ieee80211_var.h>
#include "ieee80211_mesh.h"

#include "mesh_route.h"
//#include "_ip_aodv.h"

#if ROUTE_DEBUG
char *aodv_packet_type[] =
{
		"RREQ",
		"RREP",
		"RERR",
		"RREP_ACK",
};
#endif

#define IEEE80211_MESH_DATAQ_INIT(_dev, _name) do {              \
        spin_lock_init(&(_dev)->outputq.lock);			\
} while (0)

#define IEEE80211_MESH_DATAQ_DESTROY(_dev)

#define IEEE80211_MESH_DATAQ_QLEN(_dev)				\
        _IF_QLEN(&((_dev)->outputq)) 
        
#define IEEE80211_MESH_DATAQ_LOCK(_dev) do {			\
        IF_LOCK(&(_dev->outputq) );				\
} while (0)

#define IEEE80211_MESH_DATAQ_UNLOCK(_dev) do {			\
        IF_UNLOCK(&(_dev->outputq) );                           \
} while (0)

#define IEEE80211_MESH_DATAQ_DEQUEUE(_dev, _skb, _qlen) do {	\
        IEEE80211_MESH_DATAQ_LOCK(_dev);                         \
        IF_DEQUEUE(&(_dev->outputq), _skb);			\
        (_qlen) = IEEE80211_MESH_DATAQ_QLEN(_dev);               \
        IEEE80211_NODE_SAVEQ_UNLOCK(_dev);                       \
} while (0)

#define IEEE80211_MESH_DATAQ_DRAIN(_dev, _qlen) do {             \
        IEEE80211_NODE_SAVEQ_LOCK(_dev);                         \
        (_qlen) = IEEE80211_NODE_SAVEQ_QLEN(_dev);               \
        _IF_DRAIN(&(_dev->outputq) );                           \
        IEEE80211_NODE_SAVEQ_UNLOCK(_dev);                       \
} while (0)

/* XXX could be optimized */
#define _IEEE80211_MESH_DATAQ_DEQUEUE_HEAD(_dev, _skb) do {	\
        IF_DEQUEUE(&(_dev->outputq), _skb);			\
} while (0)

#define	_IEEE80211_MESH_DATAQ_ENQUEUE(_dev, _skb, _qlen, _age) { \
	skb_queue_tail(&(_dev->outputq), (_skb));		\
	(_age) -= M_AGE_GET(&(_dev->outputq.next) );		\
	M_AGE_SET((_skb), (_age));				\
	/* N.B.: qlen is incremented in skb_queue_tail() */	\
	(_qlen) = ((_dev)->outputq.qlen);			\
} while (0)

#define IEEE80211_MESH_DATAQ_FULL(_dev)		(_IF_QLEN(&_dev->outputq) >= IEEE80211_MESH_MAX_QUEUE)

unsigned char	__get_mesh_action_type(void *data)
{
	unsigned char *type = data;

#if 1
	switch( *type)
	{
		case 1:		/*	AODV_PACKET_RREQ 	*/
			return IEEE80211_ACTION_ACTION_ROUTE_REQUEST;
			break;
		case 2:		/*	AODV_PACKET_RREP	*/
			return IEEE80211_ACTION_ACTION_ROUTE_REPLY;
		case 3:		/*	AODV_PACKET_RERR	*/
			return IEEE80211_ACTION_ACTION_ROUTE_ERROR;
			break;	
		case 4:		/*	AODV_PACKET_RREP_ACK	*/
			return IEEE80211_ACTION_ACTION_ROUTE_REPLY_ACK;
			break;

		default:
			return IEEE80211_ACTION_ACTION_UNKNOWN;
	}
	return IEEE80211_ACTION_ACTION_UNKNOWN;
#else
	return (*type)+1;
#endif
}


static int __mesh_tx(route_dev_t *routeDev, void *data, const size_t datalen, int isbroadcast)
{
	struct ieee80211com *ic;
	unsigned char			action_type;
	struct ieee80211_node *ni;

	action_type = __get_mesh_action_type( data);
	if(action_type == 	IEEE80211_ACTION_ACTION_UNKNOWN)
		return 0;
	
//	ni = routeDev->engine->lookup;

	ic = ni->ni_ic;

	return ieee80211_mesh_send_mgmt(ic, action_type, datalen, data, isbroadcast );
}

static int _mac_dev_unicast(route_dev_t *routeDev, u_int32_t dst_ip, u_int8_t ttl, void *data, const size_t datalen )
{
	if (ttl == 0)
		return 0;

	return __mesh_tx(routeDev, data, datalen, 0);
//	return 0;
}

int _mac_dev_broadcast(route_dev_t *routeDev, u_int8_t ttl, void *data,const size_t datalen)
{

	if (ttl < 1)
	{
		return 0;
	}

	return __mesh_tx(routeDev, data, datalen, 1);

//	return length;
}

/* create a task from UDP AODV PDU */
int __mac_task_insert(route_engine_t *engine, route_task_type_t  type, struct sk_buff *skb)
{
	route_task_t *task;
	struct iphdr *ip;

	int start_point = sizeof(struct udphdr) + sizeof(struct iphdr);

	task = route_task_create(engine, type);
	if (!task)
	{
		printk(KERN_WARNING "AODV: Not enough memory to create Task\n");
		return -ENOMEM;
	}

	if (type < ROUTE_TASK_RESEND_RREQ)
	{
		ip = skb->nh.iph;
		task->src_ip = ip->saddr;
		task->dst_ip = ip->daddr;
		task->ttl = ip->ttl;
		task->dev = engine->dev_lookup(engine, skb->dev);
		task->data_len = skb->len - start_point;

		ROUTE_DPRINTF(engine, ROUTE_DEBUG_DEV, "create task with Route Device %s\n", task->dev->name );
		//create space for the data and copy it there
		task->data = kmalloc(task->data_len, GFP_ATOMIC);
		if (!task->data)
		{
			kfree(task);
			printk(KERN_WARNING ": Not enough memory to create Event Queue Data Entry\n");
			return -ENOMEM;
		}

		memcpy(task->data, skb->data + start_point, task->data_len);
	}

	switch (type)
	{
		case ROUTE_TASK_RREP:
			memcpy(&(task->srcHwAddr), &(skb->mac.ethernet->h_source), sizeof(unsigned char) * ETH_ALEN);
			break;
		default:
			break;
	}

	engine->mgmtOps->task_insert(engine, task);
	
	return 0;
}


static route_op_type_t  _mac_packet_input(struct _route_dev  *routeDev, struct sk_buff *skb, void *priv)
{
	struct ieee80211_action_header *actionheader;
	struct ieee80211_frame *wh;
	unsigned char *pdu;
	int pdu_length;
	
	wh = (struct ieee80211_frame *) skb->data;
	actionheader = (struct ieee80211_action_header *)&wh[1];
	pdu = (unsigned char *)actionheader;
	pdu = pdu+sizeof(struct ieee80211_action_header); 
	
	// Create engine message types
	u_int8_t aodv_type;

	//For all AODV packets the type is the first byte.
	aodv_type = (int) *pdu;
	pdu_length = skb->len - sizeof(struct ieee80211_action_header) - sizeof(struct ieee80211_frame);
	if (!routeDev->engine->protoOps->valid_protocol_pdu(pdu_length, aodv_type, pdu ) )
	{
		ROUTE_DPRINTF(routeDev->engine, ROUTE_DEBUG_DEV,  "Packet of type: %d and of size %u, failed packet check!\n", aodv_type, pdu_length );
		return ROUTE_OP_DENY;
	}

	/*	tmp_neigh = find_aodv_neigh_by_hw(&(packet->mac.ethernet->h_source));
	if (tmp_neigh != NULL)
	{
		delete_timer(tmp_neigh->ip, ROUTE_TASK_NEIGHBOR);
		insert_timer(ROUTE_TASK_NEIGHBOR, HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 100, tmp_neigh->ip);
		update_timer_queue();
	}
	*/

	//place packet in the event queue!
	ROUTE_DPRINTF(routeDev->engine, ROUTE_DEBUG_DEV, " rx %s %s Packet\n", routeDev->engine->name, aodv_packet_type[aodv_type-1]);

	__mac_task_insert(routeDev->engine, aodv_type, skb);
	return ROUTE_OP_FORWARD;
}

static route_op_type_t   _mac_packet_output(struct _route_dev *routeDev, struct sk_buff *skb)
{
	mesh_route_t *route;
	struct iphdr *ipHeader = skb->nh.iph;

	if (ipHeader->daddr == routeDev->engine->broadcastIp )
	{
		return ROUTE_OP_FORWARD;
	}

	if(routeDev->engine->supportGateway && !routeDev->engine->check_subnet(routeDev->engine, ipHeader->daddr))
	{
		return ROUTE_OP_FORWARD;
	}
	
	//Try to get a route to the destination
	route = routeDev->engine->mgmtOps->route_lookup(routeDev->engine, ipHeader->daddr, ROUTE_TYPE_AODV);
	if ((route == NULL) || !(route->route_valid))
	{
		printk("Mehs : Not Found route to %s, so begin AODV RREQ\n", inet_ntoa(ipHeader->daddr));
		routeDev->ops->enqueue(routeDev, skb);
		routeDev->engine->protoOps->send_request(routeDev->engine,  ipHeader->daddr);
		return ROUTE_OP_QUEUE;
	}


	if (( route != NULL) && (route->route_valid))
	{
		if (!route->self_route)
			route->lifetime =  getcurrtime(routeDev->engine->epoch) + ACTIVE_ROUTE_TIMEOUT;
	}

	ROUTE_DPRINTF(routeDev->engine, ROUTE_DEBUG_DEV, "Found route to %s\n", inet_ntoa(ipHeader->daddr));
	return ROUTE_OP_FORWARD;
}

int _mac_dev_enqueue(route_dev_t *dev, struct sk_buff *skb)
{
	int qlen, age;

	IEEE80211_MESH_DATAQ_LOCK(dev);
	if (IEEE80211_MESH_DATAQ_FULL(dev))
	{
		struct sk_buff *skb2;
		IEEE80211_MESH_DATAQ_DEQUEUE( dev ,skb2, qlen );
//		IEEE80211_MESH_DATAQ_UNLOCK(dev);
		dev_kfree_skb(skb2);
//		return;
	}
	
	_IEEE80211_MESH_DATAQ_ENQUEUE(dev, skb, qlen, age);
	IEEE80211_MESH_DATAQ_UNLOCK(dev);

	return 0;
}

int _mac_dev_resend(route_dev_t *dev, unsigned int destIp)
{
	return 0;
}

int _mac_dev_drop(route_dev_t *dev, unsigned int destIp)
{
	return 0;
}


static int  _mac_dev_init(route_dev_t *routeDev)
{
	int res = 0;

	return res;
}

static void _mac_dev_destroy(route_dev_t *routeDev)
{
}


dev_ops_t	mac_dev_ops = 
{
	output_packet		:	_mac_packet_output,
	enqueue			:	_mac_dev_enqueue,
	resend			:	_mac_dev_resend,
	drop				:	_mac_dev_drop,

//	dequeue			:	,

	input_packet		:	_mac_packet_input,
//	task_insert		:	,

	broadcast		:	_mac_dev_broadcast,
	unicast			:	_mac_dev_unicast,

	init				:	_mac_dev_init,	
	destroy			:	_mac_dev_destroy,
};


