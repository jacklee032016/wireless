/*
 * $Author: lizhijie $
 * $Log: as_fifo.h,v $
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
#ifndef _AS_FIFO_H_
#define _AS_FIFO_H_

#include "as_list.h"

#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_LEN 1000
typedef enum { 
	ok, 
	plein,  	/* FIFO is full*/
	vide 	/* FIFO is empty */
}as_fifo_etat;

typedef struct as_fifo as_fifo_t;

struct as_fifo
{
	pthread_mutex_t  	qislocked; 	/* mutext : is locked */
	pthread_cond_t 	qisempty;	/* cond : is empty */
	as_list_t *queue;
	int nb_elt;
	as_fifo_etat etat;
};

/**
 * Initialise a osip_fifo_t element.
 * NOTE: this element MUST be previously allocated.
 */
void  as_fifo_init ( as_fifo_t * fifo);

void  as_fifo_free ( as_fifo_t * ff);

int as_fifo_insert ( as_fifo_t * ff, void *element);

int as_fifo_add ( as_fifo_t *ff, void *element);

int  as_fifo_size ( as_fifo_t *ff);

void *as_fifo_get (as_fifo_t * ff);

void *as_fifo_tryget ( as_fifo_t *ff);

#ifdef __cplusplus
}
#endif


#endif

