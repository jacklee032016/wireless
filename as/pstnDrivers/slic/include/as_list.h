/*
 * $Author: lizhijie $
 * $Log: as_list.h,v $
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
#ifndef _AS_LIST_H_
#define _AS_LIST_H_


#ifdef __cplusplus
extern "C"
{
#endif

typedef struct __as_node __as_node_t;

struct __as_node
{
	void *next;			/* next __node_t */
	void *element;
};

typedef struct as_list as_list_t;

struct as_list
{
	int nb_elt;
	__as_node_t *node;
};

/**
 * Initialise a osip_list_t element.
 * NOTE: this element MUST be previously allocated.
 */
int as_list_init ( as_list_t * li);
/**
 * Free a list of element.
 * Each element will be free with the method given as the second parameter.
 */
void as_list_special_free ( as_list_t * li, void *(*free_func) (void *));

/**
 * Free a list of element where elements are pointer to 'char'.
 * @param li The element to work on.
 */
void as_list_ofchar_free ( as_list_t * li);

/**
 * Get the size of a list of element.
 * @param li The element to work on.
 */
int as_list_size (const as_list_t * li);

/**
 * Check if the end of list is detected .
 * @param li The element to work on.
 * @param pos The index of the possible element.
 */
  int  as_list_eol (const as_list_t * li, int pos);
/**
 * Add an element in a list.
 * @param li The element to work on.
 * @param element The pointer on the element to add.
 * @param pos the index of the element to add. (or -1 to append the element at the end)
 */
  int as_list_add ( as_list_t * li, void *element, int pos);
/**
 * Get an element from a list.
 * @param li The element to work on.
 * @param pos the index of the element to get.
 */
void *as_list_get ( const as_list_t * li, int pos);
/**
 * Remove an element from a list.
 * @param li The element to work on.
 * @param pos the index of the element to remove.
 */
int  as_list_remove ( as_list_t * li, int pos);


#ifdef __cplusplus
}
#endif

#endif

