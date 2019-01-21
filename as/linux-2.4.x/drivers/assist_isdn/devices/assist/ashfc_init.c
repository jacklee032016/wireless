/*
 * $Id: ashfc_init.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * Interrupt Service Routine for assist HFC-4S chip drivers
 * Li Zhijie, 2005.08.12
 */
 
#define CONFIG_PROC_FS			1
#include <linux/proc_fs.h>

#include <linux/config.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <linux/init.h>

//#include <linux/isdn_compat.h>

#include "dchannel.h"
#include "bchannel.h"
#include "layer1.h"
#include "debug.h"

#include "helper.h"

#define SPIN_DEBUG
#define LOCK_STATISTIC
#include "hw_lock.h"
#include "ashfc.h"

#define  AS_PROC_DIR_NAME		"astel"
#define  AS_PROC_NAME_LENGTH	32

#define HFC_MODULE_NAME		"HFC-4S/8S driver"

#include "as_isdn_version.h"

//static int HFC_cnt;
AS_ISDNobject_t	ASHFC_obj;

/**************** module stuff ****************/
static int pcm[MAX_PORTS];

int ashfc_poll = 128;

/* used to initiate R_TI_WD */
/* 6 : 128 bytes samples = 16ms */
/* 7 : 256 bytes samples = 32ms */
int ashfc_poll_timer = 7;	

static struct proc_dir_entry *as_proc_root_entry;

static struct proc_dir_entry *as_proc_hfc4s_entry;

/******************** remove port or complete card from stack ******************/
void ashfc_release_port(ashfc_t *hc, int port)
{
	int i = 0;
	int all = 1, any = 0;

#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s : entered release\n", hc->name );
#endif

#ifdef LOCK_STATISTIC
        printk(KERN_INFO "try_ok(%d) try_wait(%d) try_mult(%d) try_inirq(%d)\n", hc->lock.try_ok, hc->lock.try_wait, hc->lock.try_mult, hc->lock.try_inirq);
        printk(KERN_INFO "irq_ok(%d) irq_fail(%d)\n", hc->lock.irq_ok, hc->lock.irq_fail);
#endif

	if (port >= hc->type)
	{
		printk(KERN_WARNING "%s : ERROR port out of range (%d).\n", hc->name, port);
		return;
	}

	lock_dev(hc, 0);

// TRACE();
	if (port > -1)
	{
//	TRACE();
		i = 0;
		while(i < hc->type)
		{
			if (hc->created[i] && i!=port)
			{
				all = 0;
//				TRACE();
			}
			
			if (hc->created[i])
				any = 1;
			
			i++;
		}
		
		if (!any)
		{
			printk(KERN_WARNING "%s : ERROR card has no used stacks anymore.\n", hc->name);
			unlock_dev(hc);
			return;
		}
	}
	
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s : releasing port=%d all=%d any=%d\n", hc->name, port, all, any);

	printk(KERN_ERR "Port : %d all:%d\n", port, all );
#endif

//TRACE();
	if (port>-1 && !hc->created[port])
	{
		printk(KERN_WARNING "%s : ERROR given stack is not used by card (port=%d).\n", hc->name, port);
		unlock_dev(hc);
		return;
	}

//TRACE();
	if (all)
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_WARNING "%s : card has no more used stacks, so we release hardware.\n", hc->name);
#endif
//TRACE();
		if (hc->irq)
		{
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_INIT)
				printk(KERN_WARNING "%s : free irq %d\n", hc->name, hc->irq);
#endif
//TRACE();
			free_irq(hc->irq, hc);
			hc->irq = 0;
		}
	}
//TRACE();

#if WITH_ASHFC_DEBUG_INIT
	/* disable D-channels & B-channels */
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s : disable all channels (d and b)\n", hc->name);
#endif

	i = 0;
	while(i < hc->type)
	{
		if (all || port==i) 
		if (hc->created[i])
		{

			hc->chan[(i<<2)+2].slot_tx = -1;
			hc->chan[(i<<2)+2].slot_rx = -1;
			hc->chan[(i<<2)+2].conf = -1;

			ashfc_chan_mode(hc, (i<<2)+2, ISDN_PID_NONE, -1, 0, -1, 0); //d

			hc->chan[i<<2].slot_tx = -1;
			hc->chan[i<<2].slot_rx = -1;
			hc->chan[i<<2].conf = -1;

			ashfc_chan_mode(hc, i<<2, ISDN_PID_NONE, -1, 0, -1, 0); //b1

			hc->chan[(i<<2)+1].slot_tx = -1;
			hc->chan[(i<<2)+1].slot_rx = -1;
			hc->chan[(i<<2)+1].conf = -1;

			ashfc_chan_mode(hc, (i<<2)+1, ISDN_PID_NONE, -1, 0, -1, 0); //b2

		}
		i++;
	}

	i = 0;
	while(i < 32)
	{
		if (hc->chan[i].dch)
		if (hc->created[hc->chan[i].port])
		if (hc->chan[i].dch->dbusytimer.function != NULL && (all || port==i))
		{
			del_timer(&hc->chan[i].dch->dbusytimer);
			hc->chan[i].dch->dbusytimer.function = NULL;
		}
		i++;
	}

	/* free channels */
	i = 0;
	while(i < 32)
	{
//TRACE();
		if (hc->created[hc->chan[i].port])
		if (hc->chan[i].port==port || all)
		{
			if (hc->chan[i].dch)
			{
#if WITH_ASHFC_DEBUG_INIT
				if (ashfc_debug & ASHFC_DEBUG_INIT)
					printk(KERN_DEBUG  "%s : free port %d D-channel %d (1..32)\n", hc->name, hc->chan[i].port, i);
#endif
				as_isdn_free_dch(hc->chan[i].dch);
//TRACE();
				ASHFC_obj.ctrl(hc->chan[i].dch->inst.up.peer, MGR_DISCONNECT | REQUEST, &hc->chan[i].dch->inst.up);
//TRACE();
				ASHFC_obj.ctrl(&hc->chan[i].dch->inst, MGR_UNREGLAYER | REQUEST, NULL);
//TRACE();
				kfree(hc->chan[i].dch);
				hc->chan[i].dch = NULL;
//TRACE();
			}
			
//TRACE();
			if (hc->chan[i].rx_buf)
			{
				kfree(hc->chan[i].rx_buf);
				hc->chan[i].rx_buf = NULL;
			}
			
			if (hc->chan[i].bch)
			{
#if WITH_ASHFC_DEBUG_INIT
				if (ashfc_debug & ASHFC_DEBUG_INIT)
					printk(KERN_DEBUG  "%s : free port %d B-channel %d (1..32)\n", hc->name, hc->chan[i].port, i);
#endif
				discard_queue(&hc->chan[i].dtmfque);
				as_isdn_free_bch(hc->chan[i].bch);
				kfree(hc->chan[i].bch);
				hc->chan[i].bch = NULL;
//TRACE();
			}
		}
		i++;
	}

//TRACE();
	i = 0;
	while(i < 8)
	{
		if (i==port || all)
			hc->created[i] = 0;
		i++;
	}

//TRACE();
	/* dimm leds */
	if (hc->leds)
		ashfc_leds(hc);

//TRACE();
	/* release IO & remove card */
	if (all)
	{
//TRACE();
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s : release IO resources\n", hc->name);
#endif

//	TRACE();


		ashfc_release_io(hc);

//	TRACE();

#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s : removing object from listbase\n", hc->name);
#endif		
		list_del(&hc->list);
		unlock_dev(hc);
		kfree(hc);
	}
	else
		unlock_dev(hc);
	
//	TRACE();
}

static char *__as_dch_info(dchannel_t *dch)
{
	char *buf;
	int length =0;
	AS_ISDNlayer_t	*hl, *hln;
	
	buf=(char *)kmalloc(2048, GFP_ATOMIC);
	if(!buf)
		return NULL;
	
	memset(buf, 0, 2048);

	length += sprintf(buf+length, "\tStack ID : %x\n",dch->inst.st->id );
	list_for_each_entry_safe(hl, hln, &dch->inst.st->layerlist, list)
	{
		length += sprintf(buf+length, "\t\t\tLayer name : %s \tID : %x ", hl->inst->name,hl->inst->id );
		length += sprintf(buf+length, "\t\t\tLayerMask : %x\n",dch->inst.pid.layermask );
	}
	return buf;
}

static char *__as_bch_info(bchannel_t *bch)
{
	char *buf;
	int length =0;
	AS_ISDNlayer_t	*hl, *hln;
	
	buf=(char *)kmalloc(2048, GFP_ATOMIC);
	if(!buf)
		return NULL;
	
	memset(buf, 0, 2048);
	length += sprintf(buf+length, "\tStack ID : %x\n",bch->st->id );
	list_for_each_entry_safe(hl, hln, &bch->st->layerlist, list)
	{
		length += sprintf(buf+length, "\t\t\tLayer name : %s\t ID : %x ", hl->inst->name, hl->inst->id );
		length += sprintf(buf+length, "\tLayerMask : %x\n", hl->inst->pid.layermask );
	}
	return buf;
}

static int __as_proc_read_isdn(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	AS_ISDNobject_t  	*hfcObj;
	ashfc_t *hfc, *next;
	hfc_chan_t *chan;			/* D channel for this port */
//	dchannel_t *dch;
//	bchannel_t *bch;
	int i, j;
	char *buf = NULL;

	if (off > 0)
		return 0;
		
	hfcObj = (AS_ISDNobject_t *)data;
	if (!hfcObj)
		return 0;

	len += sprintf(page + len, "Assist ISDN(INS network) Interface : \n" );
	len += sprintf(page + len,  AS_VERSION_INFO(HFC_MODULE_NAME) );
	len +=sprintf(page+len , "Reference Count : \n\n" );

	list_for_each_entry_safe(hfc, next, &hfcObj->ilist, list)
	{
		len += sprintf(page + len, "Card : %s\n", hfc->name );
//		len += sprintf(page + len, "\tID : %d\r\n", hfc->id );
		for(i=0; i<hfc->type; i++)
		{
			len += sprintf(page + len, "\tPort %d Info : \r\n",i );
			chan = &hfc->chan[(i<<2)+2];
			len += sprintf(page + len, "\t\tMode : %s", (chan->nt_mode)?"NT":"TE");
			len += sprintf(page + len, "\tPHY State : %d\r\n", chan->dch->ph_state);

			buf = __as_dch_info(hfc->chan[(i<<2)+2].dch);
			len += sprintf(page + len, "\t\tD Channel Stack Info : %s", buf);
			kfree(buf);

			for(j=0; j<2; j++)
			{
				buf = __as_bch_info( hfc->chan[(i<<2)+j].bch);
				if(buf)
				{
					len += sprintf(page + len, "\t\tB%d Channel Stack Info : %s", j, buf);
					kfree(buf);
				}
			}
		}
		len += sprintf(page + len, "\n\n" );

	}
	
	return len;
}


int __init  ashfc_init(void)
{
	int err, err2, i;
	ashfc_t *hc,*next;
	char tempfile[AS_PROC_NAME_LENGTH];

	int hfc_cnt;

	printk(KERN_INFO  AS_VERSION_INFO(HFC_MODULE_NAME));

/****************** Step 1 AS_ISDNobject ******************************/
#if 0
	switch(ashfc_poll)
	{
		case 8:
			ashfc_poll_timer = 2;
			break;
		case 16:
			ashfc_poll_timer = 3;
			break;
		case 32:
			ashfc_poll_timer = 4;
			break;
		case 64:
			ashfc_poll_timer = 5;
			break;
		case 128: 
		case 0:
			ashfc_poll_timer = 6;
			ashfc_poll = 128;
			break;
		case 256:
			ashfc_poll_timer = 7;
			break;
		default:
			printk(KERN_ERR "%s : Wrong poll value (%d).\n", __FUNCTION__, ashfc_poll);
			err = -EINVAL;
			return(err);
		
	}
#endif

	if ((err = ashfc_init_object(&ASHFC_obj)))
	{
		printk(KERN_ERR "Can't register ASHFC cards error(%d)\n", err);
		return(err);
	}
	
	as_proc_root_entry = proc_mkdir(AS_PROC_DIR_NAME , NULL);
	sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, "isdn" );
	as_proc_hfc4s_entry = create_proc_read_entry(tempfile, 0444, NULL , __as_proc_read_isdn, (int *)(long)&ASHFC_obj );
	

/****************** Step 2 : ashfc_t for every card ******************************/
	/* for every card onboard */
	hfc_cnt = 0;
	while (hfc_cnt < MAX_CARDS )//&&  type[hfc_cnt] > 0)
	{
		if (!(hc = kmalloc(sizeof(ashfc_t), GFP_ATOMIC)))
		{
			printk(KERN_ERR "No kmem for HFC-Multi card\n");
			err = -ENOMEM;
			goto free_object;
		}
		memset(hc, 0, sizeof(ashfc_t));
		hc->id = hfc_cnt+ 1;
		hc->pcm = pcm[hfc_cnt];
		
		err = ashfc_init_ashfc(&ASHFC_obj, hc);
		if(err)
			goto free_channels;
		hfc_cnt++;
	}

	if (hfc_cnt == 0)
	{
		printk(KERN_INFO "ASHFC: No cards defined, read the documentation.\n");
		err = -EINVAL;
		goto free_object;
	}

#if WITH_ASHFC_DEBUG_INIT
	printk(KERN_INFO "ASHFC driver: %d cards with total of %d ports installed. Refcnt=%d\n", hc->id, hc->type, ASHFC_obj.refcnt);
#endif
	return(0);

	/* DONE */

	/* if an error ocurred */
free_channels:
	i = 0;
	while(i < 32)
	{
		if (hc->chan[i].dch)
		{
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_INIT)
				printk(KERN_ERR  "%s : free D-channel %d (1..32)\n", hc->name, i);
#endif
			as_isdn_free_dch(hc->chan[i].dch);
			kfree(hc->chan[i].dch);
			hc->chan[i].dch = NULL;
		}
		if (hc->chan[i].rx_buf)
		{
			kfree(hc->chan[i].rx_buf);
			hc->chan[i].rx_buf = NULL;
		}
		if (hc->chan[i].bch)
		{
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_INIT)
				printk(KERN_ERR  "%s : free B-channel %d (1..32)\n", hc->name, i);
#endif
			discard_queue(&hc->chan[i].dtmfque);
			as_isdn_free_bch(hc->chan[i].bch);
			kfree(hc->chan[i].bch);
			hc->chan[i].bch = NULL;
		}
		i++;
	}
	
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s : before REMOVE_FROM_LIST (refcnt = %d)\n", ASHFC_obj.name, ASHFC_obj.refcnt);
#endif

	list_del(&hc->list);
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: after REMOVE_FROM_LIST (refcnt = %d)\n", ASHFC_obj.name, ASHFC_obj.refcnt);
#endif
	kfree(hc);

free_object:
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: before ISDN_unregister (refcnt = %d)\n", ASHFC_obj.name, ASHFC_obj.refcnt);
#endif
	if ((err2 = as_isdn_unregister(&ASHFC_obj)))
	{
		printk(KERN_ERR "Can't unregister %s cards error(%d)\n", ASHFC_obj.name, err);
	}

	list_for_each_entry_safe(hc, next, &ASHFC_obj.ilist, list)
	{
		printk(KERN_ERR "HFC PCI card struct not empty refs %d\n", ASHFC_obj.refcnt);
		ashfc_release_port(hc, -1); /* all ports */
	}

#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s : after ISDN_unregister (refcnt = %d)\n", ASHFC_obj.name, ASHFC_obj.refcnt);
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s : exitting with error %d\n", ASHFC_obj.name, err);
#endif
	return(err);
}

#ifdef MODULE
void __exit ashfc_cleanup(void)
{
	ashfc_t *hc,*next;
	int err;

#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: entered (refcnt = %d)\n", ASHFC_obj.name, ASHFC_obj.refcnt);
#endif

//	TRACE();
	
	if ((err = as_isdn_unregister(&ASHFC_obj)))
	{
		printk(KERN_ERR "Can't unregister %s cards error(%d)\n", ASHFC_obj.name, err);
	}
	
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: now checking ilist (refcnt = %d)\n", ASHFC_obj.name, ASHFC_obj.refcnt);
#endif
	
	list_for_each_entry_safe(hc, next, &ASHFC_obj.ilist, list)
	{
//	TRACE();
		printk(KERN_ERR "ASHFC PCI card struct not empty refs %d\n", ASHFC_obj.refcnt);
		ashfc_release_port(hc, -1); /* all ports */
//	TRACE();
	}
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: done (refcnt = %d)\n", ASHFC_obj.name, ASHFC_obj.refcnt);
#endif
	return;
}

#define MODULE_CARDS_T	"1-16i"
#define MODULE_PORTS_T	"1-128i" /* 16 cards can have 128 ports */

MODULE_AUTHOR("Li Zhijie <lizhijie@assistcn.com>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
MODULE_PARM(ashfc_debug, "1i");
MODULE_PARM(ashfc_poll, "1i");
MODULE_PARM(type, MODULE_CARDS_T);
MODULE_PARM(pcm, MODULE_CARDS_T);
MODULE_PARM(protocol, MODULE_PORTS_T);
MODULE_PARM(layermask, MODULE_PORTS_T);

module_init(ashfc_init);
module_exit(ashfc_cleanup);

#endif


