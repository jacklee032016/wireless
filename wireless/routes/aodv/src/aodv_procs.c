/*
* $Id: aodv_procs.c,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
*/
 
#include "aodv.h"

int aodv_timer_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	aodv_t		*aodv = (aodv_t *)data;
	aodv_task_t 	*task;

	unsigned long  remainder, numerator;
	unsigned long  tmp_time;
	unsigned long  currtime;
	int 			length = 0;
	
	if(aodv==NULL)
		return 0;

	currtime = getcurrtime(aodv->epoch);

	length += sprintf(buffer+length, "\nAODV Timer Queue\n--------------------------------------------------------------------------------\n");
	length += sprintf(buffer+length, "       ID      |     Type     |   sec/msec   |   Retries\n");
	length += sprintf(buffer+length, "--------------------------------------------------------------------------------\n");

	AODV_READ_LOCK(aodv->timerLock);
	AODV_LIST_FOR_EACH(task, aodv->timers, list)
	{

		length += sprintf( buffer+length ,"    %-16s  ", inet_ntoa( task->dst_ip));

		switch (task->type)
		{
			case AODV_TASK_RESEND_RREQ:
				length += sprintf( buffer+length, "RREQ    ");
				break;

			case AODV_TASK_CLEANUP:
				length += sprintf( buffer+length, "Cleanup ");
				break;

			case AODV_TASK_HELLO:
				length += sprintf( buffer+length, "Hello   ");
				break;

			case AODV_TASK_NEIGHBOR:
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
	AODV_READ_UNLOCK(aodv->timerLock);

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
	aodv_t		*aodv = (aodv_t *)data;
	aodv_flood_t 	*flood;
	unsigned long remainder, numerator;
	unsigned long tmp_time, currtime;
	int 			length = 0;

	if(aodv==NULL)
		return 0;

	currtime = getcurrtime(aodv->epoch);

	length += sprintf(buffer+length ,"\nFlood Id Queue\n---------------------------------------------------------------------------------\n");

	AODV_READ_LOCK(aodv->floodLock);
	AODV_LIST_FOR_EACH(flood, aodv->floods, list)
	{
		length += sprintf(buffer+length, inet_ntoa( flood->dst_ip));
		length += sprintf(buffer+length, "Src IP: %-16s  ", inet_ntoa( flood->src_ip));
		length += sprintf(buffer+length, "Dst IP: %-16s Flood ID: %-10u ", inet_ntoa( flood->dst_ip), flood->id);
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
	AODV_READ_UNLOCK(aodv->floodLock);

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
	aodv_t		*aodv = (aodv_t *)data;
	aodv_dev_t 	*dev;
	int 			length = 0;
	
	if(aodv==0)
		return 0;
	
	length += sprintf(buffer+length,"\nAODV Device Table \n---------------------------------------------------------------------------------\n");
	length += sprintf(buffer+length,"        Name        |    IP    |   NetMask\n");
	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n");

	AODV_LIST_FOR_EACH(dev, aodv->devs, list)
	{
		length += sprintf(buffer+length,"  %-16s     %-16s",dev->name, inet_ntoa(dev->ip)  );
		length += sprintf(buffer+length,"       %-16s\n", inet_ntoa(dev->netmask) );
	}

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
	aodv_t		*aodv = (aodv_t *)data;
	aodv_route_t *route;
	int 			length = 0;
	
	unsigned long  remainder, numerator;
	unsigned long  tmp_time;
	unsigned long  currtime;
	
	if(aodv==0)
		return 0;
	
	currtime=getcurrtime(aodv->epoch);

	length += sprintf(buffer+length,"\nRoute Table \n---------------------------------------------------------------------------------\n");
	length += sprintf(buffer+length,"        IP        |    Seq    |   Hop Count  |     Next Hop  |   Type\n");
	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n");

	AODV_READ_LOCK(aodv->routeLock);
	AODV_LIST_FOR_EACH(route, aodv->routes, list )
	{
		length += sprintf(buffer+length,"  %-16s",inet_ntoa(route->ip) );
		length += sprintf(buffer+length,"  %-16s     %5u       %3d",inet_ntoa(route->netmask) , route->seq, route->metric );
		length += sprintf(buffer+length,"         %-16s    %-10s",inet_ntoa( route->next_hop), (route->type==AODV_ROUTE_AODV)?"AODV":"Kernel");

		if( route->self_route)
		{
			length += sprintf(buffer+length, " Self Route \n");
		}
		else
		{
			if ( route->route_valid)
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
	AODV_READ_UNLOCK(aodv->routeLock);

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
	aodv_t		*aodv = (aodv_t *)data;
	aodv_neigh_t *neigh;
	int 			length = 0;
	
	unsigned long  remainder, numerator;
	unsigned long  tmp_time;
	unsigned long  currtime;
	
	if(aodv==0)
		return 0;

	currtime=getcurrtime(aodv->epoch);
	length += sprintf(buffer+length, "\nAODV Neighbors \n---------------------------------------------------------------------------------\n");
	length += sprintf(buffer+length, "        IP        |   Link    |    Valid    |  Lifetime \n");
	length += sprintf(buffer+length, "---------------------------------------------------------------------------------\n");

	AODV_READ_LOCK(aodv->neighLock);
	AODV_LIST_FOR_EACH(neigh, aodv->neighs, list)
	{
		length += sprintf( buffer+length,"   %-16s     %d        ",inet_ntoa(neigh->ip) ,neigh->link );
		if (neigh->valid_link)
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
	AODV_READ_UNLOCK(aodv->neighLock);

	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n\n");

	*buffer_location = buffer + offset;
	length -= offset;
	if (length > buffer_length)
		length = buffer_length;
	else if (length < 0)
		length = 0;

	return length;
}

aodv_proc_entry  proc_table[] = 
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

