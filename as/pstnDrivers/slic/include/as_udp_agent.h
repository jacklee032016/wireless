/*
 * $Author: lizhijie $
 * $Log: as_udp_agent.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/04/26 06:06:10  lizhijie
 * *** empty log message ***
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:46  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:04  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:51  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:09  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#ifndef __AS_UDP_AGENT_H__
#define __AS_UDP_AGENT_H__

#include<sys/wait.h>
#include<sys/socket.h>
#include<netdb.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#define AS_ADDRESS_LENGTH		20

struct AS_PEER
{
	int 				remote_call_port;			/* receive port for call control info */
	int 				remote_data_port;			/* receive port for audio data */
	unsigned char 	remote_ip[AS_ADDRESS_LENGTH];
};

typedef struct AS_PEER as_peer_t;

struct AS_UDP_AGENT
{
	int 				local_call_service_sock;
	int 				local_data_service_sock;
	int				local_call_client_sock;
	int				local_data_client_sock;
	
	as_peer_t		peer;
	
	fd_set 			readfds;
	struct timeval 	tv;
};

typedef struct AS_UDP_AGENT		as_udp_agent_t;

#define AS_CALL_TYPE_INVITE		100
#define AS_CALL_TYPE_ACK			200
#define AS_CALL_TYPE_INPROGRESS	300
#define AS_CALL_TYPR_END			400
#define AS_CALL_TYPE_ERROR			500

struct AS_CALL_PACKET
{
	unsigned short 	type;
	as_peer_t		peer;
	int 				length;
	unsigned char		*msg;		/* such as DTMF caller ID */
};

typedef struct AS_CALL_PACKET as_call_packet_t;

#endif

