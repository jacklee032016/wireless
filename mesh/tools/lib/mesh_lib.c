/*
* $Id: mesh_lib.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/
#include "mesh_lib.h"

/* network byte order to print format */
char *meshu_mac_ntop(const unsigned char *mac,int maclen,char *buf, int buflen)
{
	int	i;
	/* Overflow check (don't forget '\0') */
	if(buflen < (maclen * 3 - 1 + 1))
		return(NULL);
	sprintf(buf, "%02X", mac[0]);
	for(i = 1; i < maclen; i++)
		sprintf(buf + (i * 3) - 1, ":%02X", mac[i]);
	return(buf);
}

/* Input an arbitrary length MAC address and convert to binary. Return address size. */
int meshu_mac_aton(const char *orig, unsigned char *mac, int macmax)
{
	const char *p = orig;
	int		maclen = 0;

	/* Loop on all bytes of the string */
	while(*p != '\0')
	{
		int	temph;
		int	templ;
		int	count;
		/* Extract one byte as two chars */
		count = sscanf(p, "%1X%1X", &temph, &templ);
		if(count != 2)
			break;			/* Error -> non-hex chars */
		/* Output two chars as one byte */
		templ |= temph << 4;
		mac[maclen++] = (unsigned char) (templ & 0xFF);

		/* Check end of string */
		p += 2;
		if(*p == '\0')
		{
#if 1//def DEBUG
			char buf[20];
			meshu_ether_ntop((const struct ether_addr *) mac, buf);
			fprintf(stderr, "meshu_mac_aton(%s): %s\n", orig, buf);
#endif
			return(maclen);		/* Normal exit */
		}

		/* Check overflow */
		if(maclen >= macmax)
		{
#ifdef DEBUG
			fprintf(stderr, "meshu_mac_aton(%s): trailing junk!\n", orig);
#endif
			errno = E2BIG;
			return(0);			/* Error -> overflow */
		}

		/* Check separator */
		if(*p != ':')
			break;
		p++;
	}

	MESHU_ERR_INFO("%s: invalid ether address!\n", orig);

	errno = EINVAL;
	
	return(0);
}


void meshu_ether_ntop(const struct ether_addr *eth,  char *buf)
{
	sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
		eth->ether_addr_octet[0], eth->ether_addr_octet[1],
		eth->ether_addr_octet[2], eth->ether_addr_octet[3],
		eth->ether_addr_octet[4], eth->ether_addr_octet[5]);
}

/* Input an Ethernet address and convert to binary. */
int meshu_ether_aton(const char *orig, struct ether_addr *eth)
{
	int	maclen;
	maclen = meshu_mac_aton(orig, (unsigned char *) eth, ETH_ALEN);
	if((maclen > 0) && (maclen < ETH_ALEN))
	{
		errno = EINVAL;
		maclen = 0;
	}
	return(maclen);
}

int meshu_set_mesh_ID(int fd, unsigned char *address, int length)
{
	int res;
	int	cmd = 0;//SWJTU_MGR_CTL_SET_MESH_ID;

	MESHU_DEBUG_INFO("MESH ID is set as \r\n" );
	MESHU_IOCTL(fd, cmd, address, res);

	return res;	
}

int meshu_fwd_cmd(int fd, fwd_request_t *fwdReq )
{
	int res;
	int	cmd = SWJTU_MAC_FWD_CTRL;

	MESHU_DEBUG_INFO("MESH MAC FWD Table is controlled\r\n" );
	MESHU_IOCTL(fd, cmd, fwdReq, res);

	return res;	
}


int meshu_portal_enable( int fd , int isEnable)
{
	int param = 1;
	int res;
	int	cmd = SWJTU_MGR_CTL_PORTAL_CTL;
	param =	(isEnable==0)?SWJTU_MGR_PORTAL_DISABLE: SWJTU_MGR_PORTAL_ENABLE;

	MESHU_DEBUG_INFO("%s Portal Device\r\n",(isEnable==0)?"Disable":"Enable" );
	MESHU_IOCTL(fd, cmd, &param, res);

	return res;	
}

/* startup MESH mgr and all ports registered */
int meshu_device_startup( int fd , int isStart)
{
	int res;
	int	cmd = SWJTU_MGR_CTL_SRARTUP;
	int param = isStart;
	
	MESHU_DEBUG_INFO("%s MESH Devices\r\n",(isStart==0)?"Stop":"Start" );
	MESHU_IOCTL(fd, cmd, &param, res);

	return res;	
}

int meshu_device_open(int index)
{
	int fd;
	char		dev[128];
	
	sprintf(dev, "%s%d", MESH_DEVICE_NAME, index);
	fd = open(dev, O_RDWR);
	if (fd < 0)
	{
		MESHU_ERR_INFO( "Unable to open %s: %s\n", dev, strerror(errno));
		exit(1);
	}
	
	return fd;
}


