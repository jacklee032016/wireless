#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "as_global.h"

#define MAX_MSG		4096

#ifndef MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#endif

static int __as_service_socket_init( int port )
{
	struct sockaddr_in sa;
	int sock;

	/* Create UDP socket */
	sock = socket(AF_INET,SOCK_DGRAM,0);
	if( sock<0 ) 
		return AS_FAIL;

        /* Bind the UDP socket */
	sa.sin_family=AF_INET;
	sa.sin_port=htons(port);
	sa.sin_addr.s_addr=INADDR_ANY;
		
	if(bind( sock, (struct sockaddr *)&sa,sizeof(struct sockaddr_in)))
	{ 
		printf("error in %s\n", strerror(errno) );
		close( sock);
		return AS_FAIL;
	}
	return sock;
}

int as_net_init(as_net_agent_t *agent, int call_service_port, int data_service_port) 
{
        /* UDP socket is ready to receive messages */
	agent->local_call_service_sock = __as_service_socket_init(call_service_port);
	if(agent->local_call_service_sock < 0)
		return AS_FAIL;
		
	agent->local_data_service_sock = __as_service_socket_init(data_service_port);
	if(agent->local_data_service_sock < 0)
		return AS_FAIL;
		
	agent->local_client_sock = socket(AF_INET, SOCK_DGRAM, 0 );
	if( agent->local_client_sock<0 ) 
	{
		printf( "invalidate socket, %s\n" , strerror( errno ) );
		return 0;
	}

	FD_ZERO(&agent->readfds);
	FD_SET(agent->local_call_service_sock,&agent->readfds);
	FD_SET(agent->local_data_service_sock,&agent->readfds);
	
	agent->tv.tv_sec=10;
	agent->tv.tv_usec=0;

	//agent->remote_call_sa = 
	return AS_OK;
}

int as_net_close(as_net_agent_t *agent) 
{
	close(agent->local_call_service_sock);
	close(agent->local_data_service_sock);
	close(agent->local_client_sock );
	return AS_OK;
}

/* 
 * as_udp_check_message() - Returns 1 if there is a message to be read or 0 otherwise.
 */
int as_net_check_message(as_net_agent_t *agent) 
{
	int ret;
	int max_fd = -1;
	
	FD_ZERO(&agent->readfds);
	FD_SET(agent->local_call_service_sock, &agent->readfds);
	FD_SET(agent->local_data_service_sock, &agent->readfds);
        /* Check for new messages */

	max_fd = MAX(agent->local_call_service_sock,agent->local_data_service_sock);
	ret = select( max_fd+1, & agent->readfds,NULL,NULL, NULL/*&agent->tv*/);	
	if( ret==-1) 
	{/* select failed */
		printf( "select failed, %s\n", strerror(errno) );
		return 0;
	}
	else if( ret == 0 )
	{/* timeout */
		printf("%s Timeout for UDP socket\n" , ((as_thread_t *)agent->private)->name );
		return 0;
	}
	else if( FD_ISSET(agent->local_call_service_sock ,&agent->readfds)) 
	{
		printf("%s call available\n" , ((as_thread_t *)agent->private)->name);
	FD_ZERO(&agent->readfds);
		return AS_CALL_AVAILABLE;
	}
	else if( FD_ISSET(agent->local_data_service_sock ,&agent->readfds)) 
	{
		printf("%s data available\n" , ((as_thread_t *)agent->private)->name);
	FD_ZERO(&agent->readfds);
		return AS_DATA_AVAILABLE;
	}

	return 0;
}

/* 
 * as_udp_read_from() - Reads a message into *p and returns length read or 0 on error.
 */
static int __as_net_read_from(int sock,  void *p, unsigned char *from_ip) 
{
	int n,sl;
	struct sockaddr_in sa;

	/* Read message */
	sl=sizeof(struct sockaddr_in);
	n=recvfrom(sock, p, MAX_MSG, 0, (struct sockaddr *)&sa, &sl);
	if(n<=0 || n >MAX_MSG) 
	{
		return AS_FAIL;
	}	

	sprintf(from_ip, "%s", inet_ntoa(sa.sin_addr) );
	
//	memcpy(from,&sa.sin_addr,sizeof(struct in_addr));
	return n;
}

int as_net_read_data_from(as_net_agent_t *agent,  void *p,unsigned char *from_ip) 
{
	return __as_net_read_from(agent->local_data_service_sock,  p, from_ip);
}

int as_net_read_call_from(as_net_agent_t *agent,  void *p,unsigned char *from_ip) 
{
	return __as_net_read_from(agent->local_call_service_sock,  p, from_ip);
}

static int __as_net_init_socket_addr(struct sockaddr_in *sa, char *ip, int port)
{
	struct in_addr inp;
	int res;
	
	res = inet_aton(ip, &inp);
	if(!res)
	{
		printf("%s is not a validate IP address\r\n", ip);
		return AS_FAIL;
	}
	
	sa->sin_family=AF_INET;
	sa->sin_port = htons( port );
	sa->sin_addr.s_addr = inp.s_addr;
	return AS_OK;
}

int as_net_init_socket_addr(as_net_agent_t *agent, char *call_ip, int call_port, char *data_ip, int data_port)
{
	if(__as_net_init_socket_addr(&agent->remote_call_sa, call_ip, call_port) == AS_FAIL)
		return AS_FAIL;
	if(__as_net_init_socket_addr(&agent->remote_data_sa, data_ip, data_port) == AS_FAIL)
		return AS_FAIL;
	return AS_OK;
}

int as_net_sendout_data(as_net_agent_t *agent, void *p, int len ) 
{
	int r;

	r=sendto(agent->local_client_sock, p, len, 0, (struct sockaddr *)&agent->remote_data_sa,	sizeof(struct sockaddr_in));
	return(r);
}

int as_net_sendout_call(as_net_agent_t *agent, void *p, int len ) 
{
	int r;

	r=sendto(agent->local_client_sock, p, len, 0, (struct sockaddr *)&agent->remote_call_sa,	sizeof(struct sockaddr_in));
	return(r);
}

int as_net_sendto_ip(void  *p, int len, char *ip, int port)
{
 	struct sockaddr_in sa;
	int s;
	int res;
	
	if(__as_net_init_socket_addr(&sa, ip, port) == AS_FAIL)
	{
		return AS_FAIL;
	}

	s =socket(AF_INET, SOCK_DGRAM, 0 );
	if( s<0 ) 
	{
		printf( "invalidate socket, %s\n" , strerror( errno ) );
		return AS_FAIL;
	}

        /* Send message request */
	res =sendto(s, p, len, 0, (struct sockaddr *)&sa,	sizeof(struct sockaddr_in));
	close(s);

	return res ;
}

void as_net_debug_peer(as_peer_t *peer)
{
	printf("PEER call port=%d\r\n", peer->call_port);
	printf("PEER data port=%d\r\n", peer->data_port);
	printf("PEER IP Address=%s\r\n", peer->ip);
}

void as_net_debug_packet(as_call_packet_t *packet)
{
	printf("PACKET type=%d\r\n", packet->type);
	as_net_debug_peer( &packet->peer);
	printf("PACKET length=%d\r\n", packet->length);
	printf("PACKET msg=%s\r\n", packet->msg);
	
}

