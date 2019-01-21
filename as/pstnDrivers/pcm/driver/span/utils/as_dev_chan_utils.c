/*
 * $Revision: 1.1.1.1 $
 * $Author: lizhijie $
 * $Id: as_dev_chan_utils.c,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/

#include "asstel.h"
#include "as_fxs_common.h" /* for proc file read the wcfxs info */

#include <linux/slab.h>


#if AS_DEBUG_TIGER
/* debug card info for interrupt, lizhijie 2004.12.08 */
char *as_cards_debug_status_info(void *data)
{
	char *buf = NULL;
	int len = 0;
	int index = 0;
	struct wcfxs *wc = (struct wcfxs *)data;
	
	buf = (char *)kmalloc(4096, GFP_KERNEL);
	if(!buf)
	{
		return NULL;
	}

	while(wc)
	{
		char *tmp = NULL;
		index ++;
		len += sprintf(buf + len, "Card No. : %2d \tname : %s\r\n", index, wc->name );
		tmp = (wc->read_info)(wc);
		if(tmp)
		{
			len += sprintf(buf + len, "%s\r\n", tmp );
			kfree(tmp);
		}
		else
		{
			len += sprintf(buf + len, "Read card status info failed\r\n" );
		}

		wc = wc->next;
	}
	
	return buf;
}
#endif


/* added for multi-cards support , lizhijie 2004.11.26 */
char *as_cards_debug_info(void *data)
{
	char *buf = NULL;
	int len = 0;
	int i= 0;
	struct wcfxs *wc = (struct wcfxs *)data;

	buf = (char *)kmalloc(4096, GFP_KERNEL);
	if(!buf)
	{
		return NULL;
	}
	
	if(!wc)
	{
		len += sprintf(buf + len, "\tCard is not provided\r\n");
		return buf;
	}		
	len += sprintf(buf + len, AS_VERSION_INFO("pcm"));
	while(wc)
	{
		i++;
		len += sprintf(buf + len, "\tCard No. :%2d \tname : %s, \tmodules : %d \r\n", 
			i, wc->name, wc->module );
		wc =wc->next;
	}
	
	return buf;
}


/* realloc the buffer for a channel 
 *  paramters : j : buffer size  numbufs : number of buffer 
*/
int  as_channel_reallocbufs(struct as_dev_chan *ss, int size, int numbufs)
{
	unsigned char *newbuf, *oldbuf;
	unsigned long flags;
	int x;
	/* Check numbufs */
	if (numbufs < 2)
		numbufs = 2;
	if (numbufs > AS_MAX_NUM_BUFS)
		numbufs = AS_MAX_NUM_BUFS;

	/* We need to allocate our buffers now */
	if (size ) 
	{
		newbuf = kmalloc(size * 2 * numbufs, GFP_KERNEL);
		if (!newbuf) 
			return (-ENOMEM);
	} 
	else
	{
		newbuf = NULL;
	}	

	/* Now that we've allocated our new buffer, we can safely move things around... */
	spin_lock_irqsave(&ss->lock, flags);
	
	ss->blocksize = size ; /* set the blocksize */
	ss->numbufs = numbufs;
	
	oldbuf = ss->readbuf[0]; /* Keep track of the old buffer */
	ss->readbuf[0] = NULL;
	
	if (newbuf) 
	{/* open channel */
		for (x=0;x<numbufs;x++) 
		{
			ss->readbuf[x] = newbuf + x * size ;
			ss->writebuf[x] = newbuf + (numbufs + x) * size;
		}
	} 
	else 
	{/* close channel */
		for (x=0;x<numbufs;x++) 
		{
			ss->readbuf[x] = NULL;
			ss->writebuf[x] = NULL;
		}
	}
	
	/* Mark all buffers as empty */
	for (x=0;x<numbufs;x++) 
		ss->writen[x] = 
		ss->writeidx[x]=
		ss->readn[x]=
		ss->readidx[x] = 0;
	
	/* Keep track of where our data goes (if it goes anywhere at all) */
	if (newbuf) 
	{/* open channel */
		ss->inreadbuf = 0;
		ss->inwritebuf = 0;
	} 
	else 
	{/* close channel */
		ss->inreadbuf = -1;
		ss->inwritebuf = -1;
	}
	
	ss->outreadbuf = -1;
	ss->outwritebuf = -1;
	
#if AS_POLICY_ENBALE	
	if (ss->txbufpolicy == AS_POLICY_WHEN_FULL)
		ss->txdisable = 1;
	else
		ss->txdisable = 0;

	if (ss->rxbufpolicy == AS_POLICY_WHEN_FULL)
		ss->rxdisable = 1;
	else
		ss->rxdisable = 0;
#endif
	spin_unlock_irqrestore(&ss->lock, flags);
	
	if (oldbuf)
		kfree(oldbuf);
	
	return 0;
}

