/*
*/

#include <stdlib.h>
#include <stdio.h>

#include "as_global.h"

#include <errno.h>

/* always use this method to initiate osip_fifo_t.
*/
void as_fifo_init ( as_fifo_t * ff)
{
	if(!ff)
	{
		printf("FIFO is null for initializing \r\n");
		return;
	}
trace	
	pthread_mutex_init( &ff->qislocked ,NULL);
  /*INIT SEMA TO BLOCK ON GET() WHEN QUEUE IS EMPTY */
trace
  pthread_cond_init( &ff->qisempty , NULL);
trace
	ff->queue = ( as_list_t *) malloc (sizeof ( as_list_t));
	as_list_init (ff->queue);
  /* ff->nb_elt = 0; */
	ff->etat = vide;
}

int as_fifo_add ( as_fifo_t  *ff, void *el)
{
	pthread_mutex_lock ( &ff->qislocked);

	if (ff->etat != plein)
	{
		as_list_add (ff->queue, el, -1);	/* insert at end of queue */
	}
	else
	{
		pthread_mutex_unlock ( &ff->qislocked);
		return -1;		/* stack is full */
	}

  /* if (ff->nb_elt >= MAX_LEN) */
	if ( as_list_size (ff->queue) >= MAX_LEN)
		ff->etat = plein;
	else
		ff->etat = ok;

	pthread_mutex_unlock ( &ff->qislocked);
//	pthread_cond_signal ( &ff->qisempty);

	return 0;
}


int  as_fifo_insert ( as_fifo_t  *ff, void *el)
{
	pthread_mutex_lock ( &ff->qislocked);

	if (ff->etat != plein)
	{
      /* ff->nb_elt++; */
		as_list_add (ff->queue, el, 0);	/* insert at end of queue */
	}
	else
	{
		pthread_mutex_unlock ( &ff->qislocked);
		return -1;		/* stack is full */
	}
	
	if ( as_list_size (ff->queue) >= MAX_LEN)
		ff->etat = plein;
	else
		ff->etat = ok;

	pthread_mutex_unlock ( &ff->qislocked);
//	pthread_cond_signal ( &ff->qisempty);

	return 0;
}

int  as_fifo_size ( as_fifo_t  *ff)
{
	int i;

	pthread_mutex_lock ( &ff->qislocked);

	i = as_list_size (ff->queue);
	pthread_mutex_unlock ( &ff->qislocked);

	return i;
}

void  *as_fifo_get ( as_fifo_t  *ff)
{
	void *el;

	pthread_mutex_lock ( &ff->qislocked);
//	pthread_cond_wait ( &ff->qisempty,  &ff->qislocked);

	if (ff->etat != vide)
	{
		el = as_list_get (ff->queue, 0);
		as_list_remove (ff->queue, 0);
	}
	else
	{
		pthread_mutex_unlock ( &ff->qislocked);
		return 0;			/* pile vide */
	}
	
	if ( as_list_size (ff->queue) <= 0)
		ff->etat = vide;
	else
		ff->etat = ok;

	pthread_mutex_unlock ( &ff->qislocked);

	return el;
}

void *as_fifo_tryget ( as_fifo_t * ff)
{
	void *el;
	int res;
	struct timeval  now;
	struct timespec delay ;

	if(gettimeofday(&now, NULL) )
	{
		printf("get current time failed, '%s'\r\n" ,strerror(errno));
		return NULL;
	}
	
	delay.tv_sec = now.tv_sec + 1;
	delay.tv_nsec = now.tv_usec * 1000; 

	pthread_mutex_lock ( &ff->qislocked);
//	res = pthread_cond_timedwait (  &ff->qisempty, &ff->qislocked, &delay);
	if(res == ETIMEDOUT )
	{				/* no elements... */
		pthread_mutex_unlock ( &ff->qislocked);
		printf("Timeout in FIFO tryget()\r\n");
		return NULL;
	}

	if (ff->etat != vide)
	{
		el = as_list_get ( ff->queue, 0);
		as_list_remove ( ff->queue, 0);
	}
	else
	{				/* this case MUST never happen... */
		pthread_mutex_unlock ( &ff->qislocked);
		return NULL;
	}

	if ( as_list_size (ff->queue) <= 0)
		ff->etat = vide;
	else
		ff->etat = ok;

	pthread_mutex_unlock ( &ff->qislocked);

	return el;
}

void as_fifo_free ( as_fifo_t * ff)
{
	if (ff == NULL)
		return;
	pthread_mutex_destroy ( &ff->qislocked);
  /* seems that pthread_mutex_destroy does not free space by itself */
	pthread_cond_destroy ( &ff->qisempty);

	free (ff->queue);
	free (ff);
}

