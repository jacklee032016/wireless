/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_network.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1  2004/11/25 07:24:15  lizhijie
 * move these files from user to userlib
 *
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "as_udp_agent.h"

#define MAX_MSG		4096

int as_udp_init(as_udp_agent_t *agent,  int port) 
{
	struct sockaddr_in sa;

	/* Create UDP socket */
	agent->as_udp_sock=socket(AF_INET,SOCK_DGRAM,0);
	if(agent->as_udp_sock<0) 
		return(1);

        /* Bind the UDP socket */
	sa.sin_family=AF_INET;
	sa.sin_port=htons(port);
	sa.sin_addr.s_addr=INADDR_ANY;
		
	if(bind(agent->as_udp_sock,(struct sockaddr *)&sa,sizeof(struct sockaddr_in)))
        { 
		printf("error in %s\n", strerror(errno) );
		close(agent->as_udp_sock);
		return(1);
	}
        /* UDP socket is ready to receive messages */

	FD_ZERO(&agent->readfds);
	FD_SET(agent->as_udp_sock,&agent->readfds);
	agent->tv.tv_sec=0;
	agent->tv.tv_usec=0;
	
	return(0);
}

/*
 * as_udp_close() - Closes sockets associated with UDP incoming ports
 * Updates:   as_udp_sock only
 * Returns:   0
 */
int as_udp_close(as_udp_agent_t *agent) 
{
	close(agent->as_udp_sock);
	return(0);
}

/* 
 * as_udp_check_message() - Returns 1 if there is a message to be read or 0 otherwise.
 */
int as_udp_check_message(as_udp_agent_t *agent) 
{
	int ret;
	
	FD_ZERO(&agent->readfds);
	FD_SET(agent->as_udp_sock, &agent->readfds);
        /* Check for new messages */

	ret = select(agent->as_udp_sock+1,& agent->readfds,NULL,NULL,&agent->tv);	
	if( ret==-1) 
	{/* select failed */
		printf( "select failed, %s\n", strerror(errno) );
		return 0;
	}
	else if( ret == 0 )
	{/* timeout */
/*		DSYS_LOG(LOG_ERR, "Timeout for UDP socket\n");*/
		return 0;
	}
	else if(!FD_ISSET(agent->as_udp_sock,&agent->readfds)) 
	{
		return(0);
	}

	return(1);
}

/* 
 * as_udp_read_from() - Reads a message into *p and returns length read or 0 on error.
 */
int as_udp_read_from(as_udp_agent_t *agent,  unsigned char *p,int *from) 
{
	int n,sl;

	struct sockaddr_in sa;

	/* Read message */
	sl=sizeof(struct sockaddr_in);
	n=recvfrom(agent->as_udp_sock, p, MAX_MSG, 0, (struct sockaddr *)&sa, &sl);
	if(n<=0 || n >MAX_MSG) 
	{
		return(0);
	}	

	printf( "dest add=%s\n", inet_ntoa(sa.sin_addr) );
	
	memcpy(from,&sa.sin_addr,sizeof(struct in_addr));
	return(n);
}

/* 
 * dhis_net_sendout_to() - Writes a message from *p and returns the number of bytes sent or 0 on error.
 */
int as_udp_sendout_to(unsigned char *p, int len, int toaddr, int toport) 
{
 	struct sockaddr_in sa;
	int s;

	int r;

	sa.sin_family=AF_INET;
	sa.sin_port=htons(toport);
	sa.sin_addr.s_addr=toaddr;

/*	DSYS_LOG(LOG_INFO, "dest add=%s\n", inet_ntoa(sa.sin_addr) );*/
	
        /* set destination */
	s =socket(AF_INET, SOCK_DGRAM, 0 );
	if( s<0 ) 
	{
		printf( "invalidate socket, %s\n" , strerror( errno ) );
		return 0;
	}

        /* Send message request */
	r=sendto(s, p, len, 0, (struct sockaddr *)&sa,	sizeof(struct sockaddr_in));
	close(s);

	return(r);
}

int as_udp_sendto_ip(unsigned char *p, int len, char *ip, int port)
{
	struct in_addr inp;
	int res;
	
	res = inet_aton(ip, &inp);
	if(!res)
	{
		printf("%s is not a validate IP address\r\n", ip);
		return -1;
	}

	res = inp.s_addr;
	return as_udp_sendout_to( p,  len, res , port);
}

