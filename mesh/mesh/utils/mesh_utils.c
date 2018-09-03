/*
* $Id: mesh_utils.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "mesh.h"
/* utils functions for all modules in our mesh subsystem
*/

/* Format an Ethernet MAC for printing */
const char* swjtu_mesh_mac_sprintf(const u_int8_t *mac)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",	mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}

void swjtu_mesh_dump_rawpkt(const u_int8_t *buf, int len, int direction, char *msg)
{
	int i;

	printk("%s, Direction : %s", msg, (direction==MESH_DEBUG_DATAIN)?"<--":"-->");
	if (len > 0)
	{
		for (i = 0; i < len; i++)
		{
			if ((i & 1) == 0)
				printk(" ");
			if((i%16)== 0 )
				printk("\n\t");
			printk("%02x", buf[i]);
		}
		printk("\n");
	}
	printk("\n");
}


