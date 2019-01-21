/*
* $Id: ip_dev.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/

#include <asm/uaccess.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/netfilter.h>

#include <linux/slab.h>
#include "mesh_route.h"
#include "_ip_aodv.h"

#if ROUTE_DEBUG
char *aodv_packet_type[] =
{
		"RREQ",
		"RREP",
		"RERR",
		"RREP_ACK",
};
#endif

static int __ip_dev_init_sock(route_dev_t *routeDev )
{
	int 					error;
	struct sockaddr_in 	sin;
	struct ifreq 			infReq;
	mm_segment_t 		oldfs;

	error = sock_create(PF_INET, SOCK_DGRAM, 0, &(routeDev->sock));
	if (error < 0)
	{
//		kfree(routeDev);
//		read_unlock(&in_dev->lock);
		printk(KERN_ERR "Error during creation of socket; terminating, %d\n", error);
		return error;
	}
	
	//set the address we are sending from
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = routeDev->address.address;
	sin.sin_port = htons(AODVPORT);

	routeDev->sock->sk->reuse = 1;
	routeDev->sock->sk->allocation = GFP_ATOMIC;
	routeDev->sock->sk->priority = GFP_ATOMIC;

	error = routeDev->sock->ops->bind(routeDev->sock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));
	strncpy(infReq.ifr_ifrn.ifrn_name, routeDev->name, IFNAMSIZ);

	oldfs = get_fs();
	set_fs(KERNEL_DS);          //thank to Soyeon Anh and Dinesh Dharmaraju for spotting this bug!
	error = sock_setsockopt(routeDev->sock, SOL_SOCKET, SO_BINDTODEVICE, (char *) &infReq, sizeof(infReq)) < 0;
	set_fs(oldfs);

	if (error < 0)
	{
		printk(KERN_ERR "%s : Error, %d  binding socket. This means that some other \n", routeDev->node->name,  error);
		printk(KERN_ERR "        daemon is (or was a short time axgo) using port %i.\n", AODVPORT);
		return 0;
	}

	return 0;
}

static int _ip_dev_unicast(route_dev_t *routeDev, route_neigh_t *neigh, u_int8_t ttl, void *data, const size_t datalen )
{
	struct msghdr 		msg;
	struct iovec 			iov;
	mm_segment_t 		oldfs;
	unsigned long  		curr_time;
	struct sockaddr_in 	sin;
	u_int32_t 			space;
	int 					len;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = neigh->address.address;
	sin.sin_port = htons((unsigned short) AODVPORT);

	//define the message we are going to be sending out
	msg.msg_name = (void *) &(sin);
	msg.msg_namelen = sizeof(sin);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
	msg.msg_iov->iov_len = (__kernel_size_t) datalen;
	msg.msg_iov->iov_base = (char *) data;

	if (ttl == 0)
		return 0;

	curr_time = getcurrtime(routeDev->node->epoch);

	if ( routeDev == NULL)
	{
		printk(KERN_WARNING "%s : Error sending! Unable to find interface!\n", routeDev->name );
		return -ENODEV;
	}

	space = sock_wspace(routeDev->sock->sk);
	if (space < datalen)
	{
		printk(KERN_WARNING "AODV: Space: %d, Data: %d \n", space, datalen);
		return -ENOMEM;
	}

	routeDev->sock->sk->broadcast = 0;
	routeDev->sock->sk->protinfo.af_inet.ttl = ttl;
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	len = sock_sendmsg(routeDev->sock, &msg,(size_t) datalen);
	if (len < 0)
	{
		printk(KERN_WARNING "AODV: Error sending! err no: %d, Dst: %s\n", len, inet_ntoa(neigh->address.address));
	}
	set_fs(oldfs);
	return 0;
}

int _ip_dev_broadcast(route_dev_t *routeDev, u_int8_t ttl, void *data,const size_t datalen)
{
	struct msghdr 		msg;
	struct iovec 			iov;
	int					length =0;
	mm_segment_t 		oldfs;
	struct sockaddr_in 	sin;
//	unsigned long  curr_time = getcurrtime(node->epoch);

	if (ttl < 1)
	{
		return 0;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = routeDev->node->broadcastAddress.address;
	sin.sin_port = htons((unsigned short) AODVPORT);

	//define the message we are going to be sending out
	msg.msg_name = (void *) &(sin);
	msg.msg_namelen = sizeof(sin);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	//    msg.msg_flags = MSG_NOSIGNAL;
	msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
	msg.msg_iov->iov_len = (__kernel_size_t) datalen;
	msg.msg_iov->iov_base = (char *) data;

	if( (routeDev->sock) && (sock_wspace(routeDev->sock->sk) >= msg.msg_iov->iov_len) )
	{
		routeDev->sock->sk->broadcast = 1;
		routeDev->sock->sk->protinfo.af_inet.ttl = ttl;
		oldfs = get_fs();
		set_fs(KERNEL_DS);

		length = sock_sendmsg(routeDev->sock, &(msg), (size_t)msg.msg_iov->iov_len);
		if (length < 0)
			printk(KERN_WARNING "%s : Error sending! err no: %d,on interface: %s\n",routeDev->name, length, routeDev->name);
		set_fs(oldfs);
	}

	return length;
}

/* create a task from UDP AODV PDU */
int __ip_task_insert(route_dev_t *routeDev, task_subtype_t  subtype, struct sk_buff *skb, void *priv)
{
	pdu_task_t *task;
	struct iphdr *ipheader;
	int len = sizeof(pdu_task_t);

	int start_point = sizeof(struct udphdr) + sizeof(struct iphdr);

	MALLOC(task, (void *), len, (M_NOWAIT|M_ZERO), "PDU TASK" );

	if (!task)
	{
		return -ENOMEM;
	}

	ipheader = skb->nh.iph;
	task->header.src.address = ipheader->saddr;
	task->header.dest.address = ipheader->daddr;
	task->header.ttl = ipheader->ttl;
	task->header.type = ROUTE_TASK_PDU;
	task->header.subType = subtype;
	task->data_len = skb->len - start_point;

	task->header.dev = routeDev;
	task->header.priv = priv;

	ROUTE_DPRINTF(routeDev->node, ROUTE_DEBUG_DEV, "create task with Route Device %s\n", task->header.dev->name );
	//create space for the data and copy it there
	MALLOC(task->data, (void *), task->data_len, (M_NOWAIT|M_ZERO), "PDU DATA");
	if (!task->data)
	{
		kfree(task);
		return -ENOMEM;
	}
	memcpy(task->data, skb->data + start_point, task->data_len);

	ROUTE_DUMP_PDU(routeDev->node, ROUTE_DEBUG_RAWPDU , "RAW PDU", task->data, task->data_len );

	/* MAC address of lastHop */
	memcpy(&(task->srcHwAddr), &(skb->mac.ethernet->h_source), sizeof(unsigned char) * ETH_ALEN);

	routeDev->node->mgmtOps->task_insert(routeDev->node, (route_task_t *)task);
	return 0;
}


static route_op_type_t  _ip_packet_input(struct _route_dev  *routeDev, struct sk_buff *skb, void *priv)
{
	struct iphdr *ip = skb->nh.iph;
	route_neigh_t *lastHop;
	route_node_t	*node = routeDev->node;
	
	// Create node message types
	u_int8_t aodv_type;

	//The packets which come in still have their headers from the IP and UDP
	int start_point = sizeof(struct udphdr) + sizeof(struct iphdr);

	/*   if (strcmp(dev->name, "lo") == 0)
	{
		return NF_DROP;
	}*/
	//For all AODV packets the type is the first byte.
	aodv_type = (int) skb->data[start_point];
	if (!node->protoOps->valid_protocol_pdu(skb->len - start_point, aodv_type, skb->data + start_point))
	{
		ROUTE_DPRINTF(routeDev->node, ROUTE_DEBUG_DEV,  "Packet of type: %d and of size %u from: %s failed packet check!\n", aodv_type, skb->len - start_point, inet_ntoa(ip->saddr));
		return ROUTE_OP_DENY;
	}

	lastHop = node->mgmtOps->neigh_lookup_mac(node, skb->mac.ethernet->h_source );
	if (lastHop != NULL)
	{
		node->mgmtOps->timer_delete(node, &lastHop->address, ROUTE_MGMT_TASK_NEIGHBOR, 0);
		node->mgmtOps->timer_insert(node, &lastHop->address, ROUTE_MGMT_TASK_NEIGHBOR, HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 100 );
		node->mgmtOps->timers_update(node);
	}

	//place packet in the event queue!
	ROUTE_DPRINTF(routeDev->node, ROUTE_DEBUG_DEV, " rx %s %s Packet\n", routeDev->node->name, aodv_packet_type[aodv_type-1]);

	__ip_task_insert(routeDev, aodv_type, skb, priv);
	return ROUTE_OP_FORWARD;
}

static route_op_type_t   _ip_packet_output(struct _route_dev *routeDev, struct sk_buff *skb)
{
	mesh_route_t *route;
	struct iphdr *ipHeader = skb->nh.iph;
	aodv_address_t dest;

	if (ipHeader->daddr == routeDev->node->broadcastAddress.address)
	{
		return ROUTE_OP_FORWARD;
	}

	dest.address = ipHeader->daddr;
	if(routeDev->node->supportGateway && !routeDev->node->check_subnet(routeDev->node, &dest) )
	{
		return ROUTE_OP_FORWARD;
	}
	
	//Try to get a route to the destination
	route = routeDev->node->mgmtOps->route_lookup(routeDev->node, &dest, ROUTE_TYPE_AODV);
	if ((route == NULL) || !(route->routeValid))
	{
		route_task_t *retryTask;
	//	unsigned long flags;
		MALLOC(retryTask, route_task_t *, sizeof(route_task_t), M_NOWAIT|M_ZERO, "RETRY TASK" );
		ADDRESS_ASSIGN( &retryTask->dest, &dest);
		ADDRESS_ASSIGN( &retryTask->src, &routeDev->address);
		retryTask->dev = routeDev;
		retryTask->retries = RREQ_RETRIES;
		retryTask->ttl = 30;
		retryTask->type = ROUTE_TASK_MGMT;
		retryTask->subType = ROUTE_MGMT_TASK_RESEND_RREQ;
	
		ROUTE_DPRINTF(routeDev->node, ROUTE_DEBUG_RREQ, "Not Found route to %s, so begin RREQ\n", inet_ntoa(retryTask->dest.address));
		routeDev->taskOps->send_request(routeDev , retryTask );
		return ROUTE_OP_QUEUE;
	}

	if (( route != NULL) && (route->routeValid))
	{/* refresh route with data packet */
		if (!route->selfRoute)
			route->lifetime =  getcurrtime(routeDev->node->epoch) + ACTIVE_ROUTE_TIMEOUT;
	}

	ROUTE_DPRINTF(routeDev->node, ROUTE_DEBUG_DEV, "Found route to %s\n", inet_ntoa(ipHeader->daddr));
	return ROUTE_OP_FORWARD;
}

int _ip_dev_enqueue(route_dev_t *dev, struct sk_buff *skb)
{
	return 0;
}

int _ip_dev_resend(route_dev_t *dev, unsigned int destIp)
{
	ipq_send_ip( destIp);
	return 0;
}

int _ip_dev_drop(route_dev_t *dev, unsigned int destIp)
{
	ipq_drop_ip( destIp);
	return 0;
}


static int  _ip_dev_init(route_dev_t *routeDev)
{
	int res = 0;

	res = __ip_dev_init_sock(routeDev);

	return res;
}

static void _ip_dev_destroy(route_dev_t *routeDev)
{
	sock_release(routeDev->sock);
}

void _ip_node_destroy(route_node_t *node)
{
}


dev_ops_t	ip_dev_ops = 
{
	output_packet		:	_ip_packet_output,
	enqueue			:	_ip_dev_enqueue,
	resend			:	_ip_dev_resend,
	drop				:	_ip_dev_drop,

//	dequeue			:	,

	input_packet		:	_ip_packet_input,
//	task_insert		:	,

	broadcast		:	_ip_dev_broadcast,
	unicast			:	_ip_dev_unicast,

	init				:	_ip_dev_init,	
	destroy			:	_ip_dev_destroy,
};

