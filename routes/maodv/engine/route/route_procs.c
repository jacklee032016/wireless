/*
* $Id: route_procs.c,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
*/
 
#include "mesh_route.h"

int aodv_timer_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	route_engine_t		*engine = (route_engine_t *)data;
	route_task_t 	*task;

	unsigned long  remainder, numerator;
	unsigned long  tmp_time;
	unsigned long  currtime;
	int 			length = 0;
	
	if(engine==NULL)
		return 0;

	currtime = getcurrtime(engine->epoch);

	length += sprintf(buffer+length, "\n%s Timer Queue\n--------------------------------------------------------------------------------\n", engine->name );
	length += sprintf(buffer+length, "       ID      |     Type     |   sec/msec   |   Retries\n");
	length += sprintf(buffer+length, "--------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(engine->timerLock);
	ROUTE_LIST_FOR_EACH(task, engine->timers, list)
	{

		length += sprintf( buffer+length ,"    %-16s  ", inet_ntoa( task->dst_ip));

		switch (task->type)
		{
			case ROUTE_TASK_RESEND_RREQ:
				length += sprintf( buffer+length, "RREQ    ");
				break;

			case ROUTE_TASK_CLEANUP:
				length += sprintf( buffer+length, "Cleanup ");
				break;

			case ROUTE_TASK_HELLO:
				length += sprintf( buffer+length, "Hello   ");
				break;

			case ROUTE_TASK_NEIGHBOR:
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
	ROUTE_READ_UNLOCK(engine->timerLock);

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
	route_engine_t		*engine = (route_engine_t *)data;
	route_flood_t 	*flood;
	unsigned long remainder, numerator;
	unsigned long tmp_time, currtime;
	int 			length = 0;

	if(engine==NULL)
		return 0;

	currtime = getcurrtime(engine->epoch);

	length += sprintf(buffer+length ,"\nFlood Id Queue\n---------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(engine->floodLock);
	ROUTE_LIST_FOR_EACH(flood, engine->floods, list)
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
	ROUTE_READ_UNLOCK(engine->floodLock);

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
	route_engine_t		*engine = (route_engine_t *)data;
	route_dev_t 	*dev;
	int 			length = 0;
	
	if(engine==0)
		return 0;
	
	length += sprintf(buffer+length,"\nA%s Device Table \n---------------------------------------------------------------------------------\n", engine->name);
	length += sprintf(buffer+length,"        Name        |    IP    |   NetMask\n");
	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(engine->devLock);
	ROUTE_LIST_FOR_EACH(dev, engine->devs, list)
	{
		length += sprintf(buffer+length,"  %-16s     %-16s",dev->name, inet_ntoa(dev->ip)  );
		length += sprintf(buffer+length,"       %-16s\n", inet_ntoa(dev->netmask) );
	}
	ROUTE_READ_UNLOCK(engine->devLock);

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
	route_engine_t		*engine = (route_engine_t *)data;
	mesh_route_t *route;
	int 			length = 0;
	
	unsigned long  remainder, numerator;
	unsigned long  tmp_time;
	unsigned long  currtime;
	
	if(engine==0)
		return 0;
	
	currtime=getcurrtime(engine->epoch);

	length += sprintf(buffer+length,"\nRoute Table \n---------------------------------------------------------------------------------\n");
	length += sprintf(buffer+length,"        IP        |    Seq    |   Hop Count  |     Next Hop  |   Type\n");
	length += sprintf(buffer+length,"---------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(engine->routeLock);
	ROUTE_LIST_FOR_EACH(route, engine->routes, list )
	{
		length += sprintf(buffer+length,"  %-16s",inet_ntoa(route->ip) );
		length += sprintf(buffer+length,"  %-16s     %5u       %3d",inet_ntoa(route->netmask) , route->seq, route->metric );
		length += sprintf(buffer+length,"         %-16s    %-10s",inet_ntoa( route->nextHop), (route->type==ROUTE_TYPE_AODV)?"AODV":"Kernel");

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
	ROUTE_READ_UNLOCK(engine->routeLock);

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
	route_engine_t		*engine = (route_engine_t *)data;
	route_neigh_t *neigh;
	int 			length = 0;
	
	unsigned long  remainder, numerator;
	unsigned long  tmp_time;
	unsigned long  currtime;
	
	if(engine==0)
		return 0;

	currtime=getcurrtime(engine->epoch);
	length += sprintf(buffer+length, "\n%s Neighbors \n---------------------------------------------------------------------------------\n", engine->name);
	length += sprintf(buffer+length, "        IP        |   Link    |    Valid    |  Lifetime \n");
	length += sprintf(buffer+length, "---------------------------------------------------------------------------------\n");

	ROUTE_READ_LOCK(engine->neighLock);
	ROUTE_LIST_FOR_EACH(neigh, engine->neighs, list)
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
	ROUTE_READ_UNLOCK(engine->neighLock);

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

