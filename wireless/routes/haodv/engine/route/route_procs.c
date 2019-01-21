/*
* $Id: route_procs.c,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
*/
 
#include "mesh_route.h"

int aodv_timer_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	route_node_t		*node = (route_node_t *)data;
	route_task_t 	*task;

	unsigned long  remainder, numerator;
	unsigned long  tmp_time;
	unsigned long  currtime;
	int 			length = 0;
	
	if(node==NULL)
		return 0;

	currtime = getcurrtime(node->epoch);

	length += sprintf(buffer+length, "\n%s Timer Queue\n--------------------------------------------------------------------------------\n", node->name );
	length += sprintf(buffer+length, "       ID      |     Type     |   sec/msec   |   Retries\n");
	length += sprintf(buffer+length, "--------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(node->timerLock);
	ROUTE_LIST_FOR_EACH(task, node->timers, list)
	{

		length += sprintf( buffer+length ,"    %-16s  ", inet_ntoa( task->dest.address));

		switch (task->subType)
		{
			case ROUTE_MGMT_TASK_RESEND_RREQ:
				length += sprintf( buffer+length, "RREQ    ");
				break;

			case ROUTE_MGMT_TASK_CLEANUP:
				length += sprintf( buffer+length, "Cleanup ");
				break;

			case ROUTE_MGMT_TASK_HELLO:
				length += sprintf( buffer+length, "Hello   ");
				break;

			case ROUTE_MGMT_TASK_NEIGHBOR:
				length += sprintf( buffer+length, "Neighbor");
				break;

			default :
				printk( "Not handled AODV task %d\n", task->type);
				break;
		}

		tmp_time=task->time - currtime;

		numerator = tmp_time/1000;
		remainder = tmp_time%1000;
		length += sprintf( buffer+length,"    %lu/%lu       %u\n", (unsigned long)numerator, (unsigned long)remainder, task->retries);
	}
	ROUTE_READ_UNLOCK(node->timerLock);

	length += sprintf( buffer+length,"--------------------------------------------------------------------------------\n");

	if (length <= offset+buffer_length) 
		*eof = 1;
	*buffer_location = buffer + offset;
	length -= offset;
	
	if (length >buffer_length) 
		length = buffer_length;

	if (length<0) 
		length = 0;

	return length;
}


int aodv_flood_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	route_node_t		*node = (route_node_t *)data;
	route_flood_t 	*flood;
	unsigned long remainder, numerator;
	unsigned long tmp_time, currtime;
	int 			length = 0;

	if(node==NULL)
		return 0;

	currtime = getcurrtime(node->epoch);

	length += sprintf(buffer+length ,"\nFlood Id Queue\n---------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(node->floodLock);
	ROUTE_LIST_FOR_EACH(flood, node->floods, list)
	{
//		length += sprintf(buffer+length, inet_ntoa( flood->dest.address));
		length += sprintf(buffer+length, "Src IP: %-16s  ", inet_ntoa( flood->orig.address));
		length += sprintf(buffer+length, "Dst IP: %-16s Flood ID: %-10u ", inet_ntoa( flood->dest.address), flood->reqId);
		tmp_time = flood->lifetime - currtime;
		numerator = tmp_time/100;
		remainder = tmp_time%1000 ;
		if (time_before( flood->lifetime, currtime) )
		{
			length += sprintf(buffer+length, " Expired!\n");
		}
		else
		{
			length += sprintf(buffer+length," sec/msec: %lu/%lu \n", (unsigned long)numerator, (unsigned long)remainder);
		}
	}
	ROUTE_READ_UNLOCK(node->floodLock);

	length += sprintf(buffer +length, "\n---------------------------------------------------------------------------------\n");

	*buffer_location= buffer;
	if (length <= offset+buffer_length)
		*eof = 1;

	*buffer_location = buffer + offset;
	length -= offset;
	if (length >buffer_length) 
		length = buffer_length;
	if (length <0) 
		length = 0;
	return length;
}


int aodv_dev_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	route_node_t		*node = (route_node_t *)data;
	route_dev_t 	*dev;
	int 			length = 0;
	
	if(node==0)
		return 0;
	
	length += sprintf(buffer+length,"\nA%s Device Table \n---------------------------------------------------------------------------------\n", node->name);
	length += sprintf(buffer+length,"        Name        |    IP    |   NetMask\n");
	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(node->devLock);
	ROUTE_LIST_FOR_EACH(dev, node->devs, list)
	{
		length += sprintf(buffer+length,"  %-16s     %-16s",dev->name, inet_ntoa(dev->address.address)  );
		length += sprintf(buffer+length,"       %-16s\n", inet_ntoa(dev->netmask) );
	}
	ROUTE_READ_UNLOCK(node->devLock);

	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n\n");
	*buffer_location = buffer + offset;
	length -= offset;
	if (length > buffer_length)
		length = buffer_length;
	else if (length < 0)
		length = 0;

	return length;
}


int aodv_routes_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	route_node_t		*node = (route_node_t *)data;
	mesh_route_t *route;
	int 			length = 0;
	
	unsigned long  remainder, numerator;
	unsigned long  tmp_time;
	unsigned long  currtime;
	
	if(node==0)
		return 0;
	
	currtime=getcurrtime(node->epoch);

	length += sprintf(buffer+length,"\nRoute Table \n---------------------------------------------------------------------------------\n");
	length += sprintf(buffer+length,"        IP        |    Seq    |   Hop Count  |     Next Hop  |   Type\n");
	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(node->routeLock);
	ROUTE_LIST_FOR_EACH(route, node->routes, list )
	{
		length += sprintf(buffer+length,"  %s/%d      ",inet_ntoa(route->address.address), calculate_prefix(route->netmask) );
//		length += sprintf(buffer+length,"  %-16s     %5u       %3d",inet_ntoa(route->netmask) , route->seq, route->metric );
		length += sprintf(buffer+length,"  %5u       %3d", route->seq, route->metric );
		length += sprintf(buffer+length,"         %-16s    %-10s",route->nextHop==NULL?"NULL":inet_ntoa( route->nextHop->address.address), (route->type==ROUTE_TYPE_AODV)?"AODV":"Kernel");

		if( route->selfRoute)
		{
			length += sprintf(buffer+length, " Self Route \n");
		}
		else
		{
			if ( route->routeValid)
				length += sprintf(buffer+length, " Valid ");

			tmp_time = route->lifetime-currtime;
			numerator = tmp_time/1000;
			remainder = tmp_time%1000;
			if (time_before( route->lifetime, currtime) )
			{
				length += sprintf(buffer+length," Expired!\n");
			}
			else
			{
				length += sprintf(buffer+length," sec/msec: %lu/%lu \n", (unsigned long)numerator, (unsigned long)remainder);
			}
		}
	}
	ROUTE_READ_UNLOCK(node->routeLock);

	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n\n");
	*buffer_location = buffer + offset;
	length -= offset;
	if (length > buffer_length)
		length = buffer_length;
	else if (length < 0)
		length = 0;

	return length;
}


int aodv_neigh_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	route_node_t		*node = (route_node_t *)data;
	route_neigh_t *neigh;
	int 			length = 0;
	
	unsigned long  remainder, numerator;
	unsigned long  tmp_time;
	unsigned long  currtime;
	
	if(node==0)
		return 0;

	currtime=getcurrtime(node->epoch);
	length += sprintf(buffer+length, "\n%s Neighbors \n---------------------------------------------------------------------------------\n", node->name);
	length += sprintf(buffer+length, "        IP        |   Link    |    Valid    |  Lifetime \n");
	length += sprintf(buffer+length, "---------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(node->neighLock);
	ROUTE_LIST_FOR_EACH(neigh, node->neighs, list)
	{
		length += sprintf( buffer+length,"   %-16s     %d        ",inet_ntoa(neigh->address.address) ,neigh->link );
		if (neigh->validLink)
		{
			length += sprintf(buffer+length, "+       ");
		}
		else
		{
			length += sprintf( buffer+length, "-       ");
		}

		tmp_time = neigh->lifetime-currtime;
		numerator = tmp_time/1000;
		remainder = tmp_time%1000;
		if (time_before(neigh->lifetime, currtime) )
		{
			length += sprintf(buffer+length," Expired!\n");
		}
		else
		{
			length += sprintf(buffer+length," sec/msec: %lu/%lu \n", (unsigned long)numerator, (unsigned long)remainder );
		}
	}
	ROUTE_READ_UNLOCK(node->neighLock);

	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n\n");

	*buffer_location = buffer + offset;
	length -= offset;
	if (length > buffer_length)
		length = buffer_length;
	else if (length < 0)
		length = 0;

	return length;
}

route_proc_entry  proc_table[] = 
{
	{
		name	:	"routes",
		read		:	aodv_routes_proc,
	},
	{
		name	:	"neighs",
		read		:	aodv_neigh_proc,
	},
	{
		name	:	"timers",
		read		:	aodv_timer_proc,
	},
	{
		name	:	"floods",
		read		:	aodv_flood_proc,
	},
	{
		name	:	"devices",
		read		:	aodv_dev_proc,
	},
	{
		read		:	0,
	}
};

