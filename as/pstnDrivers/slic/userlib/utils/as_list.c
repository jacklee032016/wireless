/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_list.c,v $
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

#include <stdlib.h>
#include <stdio.h>

#include "as_list.h"

int as_list_init ( as_list_t  *list)
{
	list->nb_elt = 0;
	return 0;			/* ok */
}

void as_list_special_free ( as_list_t  *list, void *(*free_func) (void *))
{
	int pos = 0;
	void *element;

	if (list == NULL)
		return;
	while (!as_list_eol (list, pos))
	{
		element = (void *) as_list_get (list, pos);
		as_list_remove (list, pos);
		free_func (element);
	}
	free (list);
}

void as_list_ofchar_free ( as_list_t * list)
{
	int pos = 0;
	char *chain;

	if (list == NULL)
		return;
	while (!as_list_eol (list, pos))
	{
		chain = (char *)  as_list_get (list, pos);
		as_list_remove (list, pos);
		free (chain);
	}
	free (list);
}

int as_list_size (const as_list_t * list)
{
	if (list != NULL)
		return list->nb_elt;
	else
		return -1;
}

int as_list_eol (const as_list_t * list, int i)
{
	if(list==NULL) 
		return -1;
	if (i < list->nb_elt)
		return 0;			/* not end of list */
	return 1;			/* end of list */
}

/* index starts from 0; */
int as_list_add ( as_list_t  *list, void *el, int pos)
{
	__as_node_t *ntmp;
	int i = 0;

	if (pos == -1 || pos >= list->nb_elt)
	{				/* insert at the end  */
		pos = list->nb_elt;
	}

	if (list->nb_elt == 0)
	{
		list->node = (__as_node_t *)  malloc (sizeof (__as_node_t));
		list->node->element = el;
		list->nb_elt++;
		return list->nb_elt;
	}

	ntmp = list->node;		/* exist because nb_elt>0  */

	if (pos == 0)
	{
		list->node = (__as_node_t *) malloc (sizeof (__as_node_t));
		list->node->element = el;
		list->node->next = ntmp;
		list->nb_elt++;
		return list->nb_elt;
	}
  /* pos = 0 insert before first elt  */

	while (pos > i + 1)
	{
		i++;
      /* when pos>i next node exist  */
		ntmp = (__as_node_t *) ntmp->next;
	}

  /* if pos==nb_elt next node does not exist  */
	if (pos == list->nb_elt)
	{
		ntmp->next = (__as_node_t *) malloc (sizeof (__as_node_t));
		ntmp = (__as_node_t *) ntmp->next;
		ntmp->element = el;
		list->nb_elt++;
		return list->nb_elt;
	}

  /* here pos==i so next node is where we want to insert new node */
	{
		__as_node_t *nextnode = (__as_node_t *) ntmp->next;

		ntmp->next = (__as_node_t *) malloc (sizeof (__as_node_t));
		ntmp = (__as_node_t *) ntmp->next;
		ntmp->element = el;
		ntmp->next = nextnode;
		list->nb_elt++;
	}
	return list->nb_elt;
}

/* index starts from 0 */
void *as_list_get (const as_list_t *list, int pos)
{
	__as_node_t *ntmp;
	int i = 0;

	if (pos < 0 || pos >= list->nb_elt)
    /* element does not exist */
		return 0;

	ntmp = list->node;		/* exist because nb_elt>0 */

	while (pos > i)
	{
		i++;
		ntmp = (__as_node_t *) ntmp->next;
	}
	return ntmp->element;
}

/* return -1 if failed */
int as_list_remove ( as_list_t  *list, int pos)
{
	__as_node_t *ntmp;
	int i = 0;

	if (pos < 0 || pos >= list->nb_elt)
    /* element does not exist */
		return -1;

	ntmp = list->node;		/* exist because nb_elt>0 */

	if ((pos == 0))
	{				/* special case  */
		list->node = (__as_node_t *) ntmp->next;
		list->nb_elt--;
		free (ntmp);
		return list->nb_elt;
	}

	while (pos > i + 1)
	{
		i++;
		ntmp = (__as_node_t *) ntmp->next;
	}

  /* if pos==nb_elt next node is the last one */
/* Unreachable code!
  if (pos == li->nb_elt)
    {
      osip_free (ntmp->next);
      li->nb_elt--;
      return li->nb_elt;
    }
*/

  /* insert new node */
	{
		__as_node_t *remnode;

		remnode = (__as_node_t *) ntmp->next;
		ntmp->next = ((__as_node_t *) ntmp->next)->next;
		free (remnode);
		list->nb_elt--;
	}
	return list->nb_elt;
}


/*  
   To Modify the element in the List
 */
/*
int  list_set(osip_list_t * li, void *el, int pos)
{
  __node_t *ntmp;
  int i = 0;

  if (pos < 0 || pos >= li->nb_elt)
    // element does not exist
    return 0;


  ntmp = li->node;              //exist because nb_elt>0

  while (pos > i)
    {
      i++;
      ntmp = (__node_t *) ntmp->next;
    }
  ntmp->element = el;
  return li->nb_elt;
}
*/

