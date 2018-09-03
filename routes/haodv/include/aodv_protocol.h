/*
* $Id: aodv_protocol.h,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/
 
#ifndef  __AODV_PROTOCOL_H__
#define __AODV_PROTOCOL_H__

#define AODVPORT					654

#define AODV_PACKET_RREQ			1
#define AODV_PACKET_RREP			2
#define AODV_PACKET_RERR			3
#define AODV_PACKET_RREP_ACK		4


//Route reply message type
typedef struct 
{
	u_int8_t 		type;
} aodv_pdu_reply_ack;

typedef struct 
{
	u_int8_t type;
	
#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned int a:1;
	unsigned int reserved1:7;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned int reserved1:7;
	unsigned int a:1;
#else
	#error "Please fix <asm/byteorder.h>"
#endif
	u_int8_t 		reserved2;
	u_int8_t 		metric;
	u_int32_t 	dst_ip;
	u_int32_t 	dst_seq;
	u_int32_t 	src_ip;
	u_int32_t 	lifetime;
}aodv_pdu_reply;

typedef struct 
{
	u_int8_t type;
#if defined(__BIG_ENDIAN_BITFIELD)
	u_int8_t j:1;
	u_int8_t r:1;
	u_int8_t g:1;
	u_int8_t d:1;
	u_int8_t u:1;
	u_int8_t reserved:3;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	u_int8_t reserved:3;
	u_int8_t u:1;
	u_int8_t d:1;
	u_int8_t g:1;
	u_int8_t r:1;
	u_int8_t j:1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	u_int8_t second_reserved;
	u_int8_t metric;
	u_int32_t rreq_id;
	u_int32_t dst_ip;
	u_int32_t dst_seq;
	u_int32_t src_ip;
	u_int32_t src_seq;
} aodv_pdu_request;


typedef struct 
{
	u_int8_t type;

#if defined(__BIG_ENDIAN_BITFIELD)
    unsigned int n:1;
    unsigned int reserved:15;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
    unsigned int reserved:15;
    unsigned int n:1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	unsigned int dst_count:8;
} aodv_pdu_error;


typedef struct 
{
	u_int32_t 	ip;
	u_int32_t 	seq;
} aodv_dest_t;

struct _add_seq_pair
{
	unsigned int	address;
	unsigned int	sequence;
};

typedef struct _add_seq_pair 	aodv_address_t;

#define  ADDRESS_ASSIGN(left, right)	\
	(left)->address = (right)->address;	\
	(left)->sequence = (right)->sequence

#define ADDRESS_EQUAL(left,right)	\
	((left)->address == (right)->address) 

// See section 10 of the AODV draft : Times in milliseconds
#define ACTIVE_ROUTE_TIMEOUT		3000
#define ALLOWED_HELLO_LOSS		2
#define BLACKLIST_TIMEOUT			RREQ_RETRIES * NET_TRAVERSAL_TIME
#define DELETE_PERIOD				ALLOWED_HELLO_LOSS * HELLO_INTERVAL
#define HELLO_INTERVAL				1000*10
#define LOCAL_ADD_TTL				2
#define MAX_REPAIR_TTL				0.3 * NET_DIAMETER
#define MY_ROUTE_TIMEOUT			ACTIVE_ROUTE_TIMEOUT
#define NET_DIAMETER				10
#define NODE_TRAVERSAL_TIME		40
#define NET_TRAVERSAL_TIME			2 * NODE_TRAVERSAL_TIME * NET_DIAMETER
#define NEXT_HOP_WAIT				NODE_TRAVERSAL_TIME + 10
#define PATH_DISCOVERY_TIME		2 * NET_TRAVERSAL_TIME
#define RERR_RATELIMIT				10
#define RING_TRAVERSAL_TIME		2 * NODE_TRAVERSAL_TIME * ( TTL_VALUE + TIMEOUT_BUFFER)
#define RREQ_RETRIES				2
#define RREQ_RATELIMIT				10
#define TIMEOUT_BUFFER				2
#define TTL_START					2
#define TTL_INCREMENT				2
#define TTL_THRESHOLD				7
#define TTL_VALUE					3


// Message Types
#define RREQ_MESSAGE        			1
#define RREP_MESSAGE        			2
#define RERR_MESSAGE        			3
#define RREP_ACK_MESSAGE 			4

#define ROUTE_SEQUENCE_UNKNOWN			0


#define 	L3_AODV_RREQ_DETS					8
#define 	L3_AODV_RREQ_SRC						12

#define 	L3_AODV_RREP_DEST						4

#endif

