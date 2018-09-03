/*
* $Id: ip_dev.c,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
*/

#include <asm/uaccess.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/netfilter.h>
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

static int __ip_dev_init_sock(route_dev_t *routeDev, u_int32_t ip )
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
	sin.sin_addr.s_addr = ip;
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
		printk(KERN_ERR "%s : Error, %d  binding socket. This means that some other \n", routeDev->engine->name,  error);
		printk(KERN_ERR "        daemon is (or was a short time axgo) using port %i.\n", AODVPORT);
		return 0;
	}

	return 0;
}

static int _ip_dev_unicast(route_dev_t *routeDev, u_int32_t dst_ip, u_int8_t ttl, void *data, const size_t datalen )
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
	sin.sin_addr.s_addr = dst_ip;
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

	curr_time = getcurrtime(routeDev->engine->epoch);

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
		printk(KERN_WARNING "AODV: Error sending! err no: %d, Dst: %s\n", len, inet_ntoa(dst_ip));
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
//	unsigned long  curr_time = getcurrtime(engine->epoch);

	if (ttl < 1)
	{
		return 0;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = routeDev->engine->broadcastIp;
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
int __ip_task_insert(route_engine_t *engine, route_task_type_t  type, struct sk_buff *skb, void *priv)
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

		task->priv = priv;

		ROUTE_DPRINTF(engine, ROUTE_DEBUG_DEV, "create task with Route Device %s\n", task->dev->name );
		//create space for the data and copy it there
		task->data = kmalloc(task->data_len, GFP_ATOMIC);
		if (!task->data)
		{
			kfree(task);
			printk(KERN_WARNING "AODV: Not enough memory to create Event Queue Data Entry\n");
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


static route_op_type_t  _ip_packet_input(struct _route_dev  *routeDev, struct sk_buff *skb, void *priv)
{
	struct iphdr *ip = skb->nh.iph;
	
	// Create engine message types
	u_int8_t aodv_type;

	//The packets which come in still have their headers from the IP and UDP
	int start_point = sizeof(struct udphdr) + sizeof(struct iphdr);

	/*   if (strcmp(dev->name, "lo") == 0)
	{
		return NF_DROP;
	}*/
	//For all AODV packets the type is the first byte.
	aodv_type = (int) skb->data[start_point];
	if (!routeDev->engine->protoOps->valid_protocol_pdu(skb->len - start_point, aodv_type, skb->data + start_point))
	{
		ROUTE_DPRINTF(routeDev->engine, ROUTE_DEBUG_DEV,  "Packet of type: %d and of size %u from: %s failed packet check!\n", aodv_type, skb->len - start_point, inet_ntoa(ip->saddr));
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

	__ip_task_insert(routeDev->engine, aodv_type, skb, priv);
	return ROUTE_OP_FORWARD;
}

static route_op_type_t   _ip_packet_output(struct _route_dev *routeDev, struct sk_buff *skb)
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
		printk("AODV : Not Found route to %s, so begin AODV RREQ\n", inet_ntoa(ipHeader->daddr));
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

	res = __ip_dev_init_sock(routeDev, routeDev->ip);

	return res;
}

static void _ip_dev_destroy(route_dev_t *routeDev)
{
	sock_release(routeDev->sock);
}

void _ip_engine_destroy(route_engine_t *engine)
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

