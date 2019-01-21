/*
 * $Id: ashfc_init_utils.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */
 
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


#if 0
static int ashfc_layermask[MAX_PORTS] =
{
	AS_ISDN_TE_DEV_LAYERS, AS_ISDN_NT_DEV_LAYERS,
	AS_ISDN_NT_DEV_LAYERS, AS_ISDN_NT_DEV_LAYERS|ISDN_LAYER(2) 
};

static u_int ashfc_protocol[MAX_PORTS] = 
{
	ASISDN_TE_D_PROTOCOL_STACK ,//| ASISDN_TE_B_PROTOCOL_STACK, 
	ASISDN_NT_D_PROTOCOL_STACK ,//| ASISDN_NT_B_PROTOCOL_STACK,
	ASISDN_NT_D_PROTOCOL_STACK ,//| ASISDN_NT_B_PROTOCOL_STACK,
	ASISDN_NT_D_PROTOCOL_STACK |ISDN_PID_L2_LAPD_NET //| ASISDN_NT_B_PROTOCOL_STACK//|ISDN_PID_L2_B_TRANS
};
#endif

/* Li Zhijie, changed as following, 2005.11.17 */
static int ashfc_layermask[MAX_PORTS] =
{
	AS_ISDN_TE_DEV_LAYERS, AS_ISDN_TE_DEV_LAYERS,
	AS_ISDN_TE_DEV_LAYERS, AS_ISDN_TE_DEV_LAYERS 
};

static u_int ashfc_protocol[MAX_PORTS] = 
{
	ASISDN_TE_D_PROTOCOL_STACK ,
	ASISDN_TE_D_PROTOCOL_STACK ,
	ASISDN_TE_D_PROTOCOL_STACK ,
	ASISDN_TE_D_PROTOCOL_STACK 
};


int ashfc_manager(void *data, u_int prim, void *arg)
{
	ashfc_t *hc;
	AS_ISDNinstance_t *inst = data;
	struct sk_buff *skb;
	dchannel_t *dch = NULL;
	bchannel_t *bch = NULL;
	int ch = -1;
	int i;

	if (!data)
	{
		MGR_HASPROTOCOL_HANDLER(prim,arg,&ASHFC_obj)
		printk(KERN_ERR "%s: no data prim %x arg %p\n", __FUNCTION__, prim, arg);
		return(-EINVAL);
	}

	/* find channel and card */
	list_for_each_entry(hc, &ASHFC_obj.ilist, list)
	{
		i = 0;
		while(i < 32)
		{
//printk(KERN_ERR  "comparing (D-channel) card=%08x inst=%08x with inst=%08x\n", hc, &hc->dch[i].inst, inst);
			if (hc->chan[i].dch)
			{
				if (&hc->chan[i].dch->inst == inst)
				{
					ch = i;
					dch = hc->chan[i].dch;
					break;
				}	
			}
			
			if (hc->chan[i].bch)
			{
				if (&hc->chan[i].bch->inst == inst)
				{
					ch = i;
					bch = hc->chan[i].bch;
					break;
				}	
			}
			i++;
		}
		if (ch >= 0)
			break;
	}
	if (ch < 0)
	{
		printk(KERN_ERR "%s: no card/channel found  data %p prim %x arg %p\n", __FUNCTION__, data, prim, arg);
		return(-EINVAL);
	}
	
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_MGR)
		printk(KERN_ERR  "%s: channel %d (0..31)  data %p prim %x arg %p\n", __FUNCTION__, ch, data, prim, arg);
#endif

	switch(prim)
	{
		case MGR_REGLAYER | CONFIRM:
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_MGR)
				printk(KERN_ERR  "%s: MGR_REGLAYER\n", __FUNCTION__);
#endif
			if (dch)
				dch_set_para(dch, &inst->st->para);
			if (bch)
				bch_set_para(bch, &inst->st->para);
			break;

		case MGR_UNREGLAYER | REQUEST:
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_MGR)
				printk(KERN_ERR  "%s: MGR_UNREGLAYER\n", __FUNCTION__);
#endif
			if (dch)
			{
				inst->down.fdata = dch;
				if ((skb = create_link_skb(PH_CONTROL | REQUEST, HW_DEACTIVATE, 0, NULL, 0))) 
				{
					if (ashfc_l1hw(&inst->down, skb))
						dev_kfree_skb(skb);
				}
			}
			else if (bch)
			{
				inst->down.fdata = bch;
				if ((skb = create_link_skb(MGR_DISCONNECT | REQUEST, 0, 0, NULL, 0))) 
				{
					if (ashfc_l2l1(&inst->down, skb))
						dev_kfree_skb(skb);
				}
			}
			ASHFC_obj.ctrl(inst->up.peer, MGR_DISCONNECT | REQUEST, &inst->up);
			ASHFC_obj.ctrl(inst, MGR_UNREGLAYER | REQUEST, NULL);
			break;

		case MGR_CLRSTPARA | INDICATION:
			arg = NULL;
			// fall through

		case MGR_ADDSTPARA | INDICATION:
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_MGR)
				printk(KERN_ERR  "%s: MGR_***STPARA\n", __FUNCTION__);
#endif

			if (dch)
				dch_set_para(dch, arg);
			if (bch)
				bch_set_para(bch, arg);
			break;

		case MGR_RELEASE | INDICATION:
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_MGR)
				printk(KERN_ERR  "%s: MGR_RELEASE = remove port from AS_ISDN\n", __FUNCTION__);
#endif
			if (dch)
			{
//	TRACE();
				ashfc_release_port(hc, hc->chan[ch].port);
//	TRACE();
				ASHFC_obj.refcnt--;
			}

//			if (bch)
//				ASHFC_obj.refcnt--;
			break;

		case MGR_CONNECT | REQUEST:
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_MGR)
				printk(KERN_ERR  "%s: MGR_CONNECT\n", __FUNCTION__);
#endif
			return(AS_ISDN_ConnectIF(inst, arg));
		//break;

		case MGR_SETIF | REQUEST:
		case MGR_SETIF | INDICATION:
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_MGR)
				printk(KERN_ERR  "%s: MGR_SETIF\n", __FUNCTION__);
#endif
			if (dch)
				return(AS_ISDN_SetIF(inst, arg, prim, ashfc_l1hw, NULL, dch));
			if (bch)
				return(AS_ISDN_SetIF(inst, arg, prim, ashfc_l2l1, NULL, bch));
		//break;

		case MGR_DISCONNECT | REQUEST:
		case MGR_DISCONNECT | INDICATION:
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_MGR)
				printk(KERN_ERR  "%s: MGR_DISCONNECT\n", __FUNCTION__);
#endif
			return(AS_ISDN_DisConnectIF(inst, arg));
		//break;

		case MGR_SELCHANNEL | REQUEST:
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_MGR)
				printk(KERN_ERR  "%s: MGR_SELCHANNEL\n", __FUNCTION__);
#endif
			if (!dch)
			{
				printk(KERN_WARNING "%s(MGR_SELCHANNEL|REQUEST): selchannel not dinst\n", __FUNCTION__);
				return(-EINVAL);
			}
			return(ashfc_sel_free_B_chan(hc, ch, arg));
		//break;

		case MGR_SETSTACK | CONFIRM:
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_MGR)
				printk(KERN_ERR  "%s: MGR_SETSTACK\n", __FUNCTION__);
#endif
			if (bch && inst->pid.global==2)
			{
				inst->down.fdata = bch;
				if ((skb = create_link_skb(PH_ACTIVATE | REQUEST, 0, 0, NULL, 0)))
				{
					if (ashfc_l2l1(&inst->down, skb))
						dev_kfree_skb(skb);
				}

				if (inst->pid.protocol[2] == ISDN_PID_L2_B_TRANS)
					if_link(&inst->up, DL_ESTABLISH | INDICATION, 0, 0, NULL, 0);
				else
					if_link(&inst->up, PH_ACTIVATE | INDICATION, 0, 0, NULL, 0);
			}
			break;

		PRIM_NOT_HANDLED(MGR_CTRLREADY | INDICATION);
		PRIM_NOT_HANDLED(MGR_GLOBALOPT | REQUEST);
		default:
			printk(KERN_WARNING "%s: prim %x not handled\n", __FUNCTION__, prim);
			return(-EINVAL);
	}
	return(0);
}

int __init ashfc_init_object(AS_ISDNobject_t	*obj)
{
	memset(obj, 0, sizeof(AS_ISDNobject_t));
#ifdef MODULE
	obj->owner = THIS_MODULE;
#endif
	INIT_LIST_HEAD(&obj->ilist);
	obj->name = "Assist HFC-4/8S";
	obj->own_ctrl = ashfc_manager;

	obj->DPROTO.protocol[0] = ISDN_PID_L0_TE_S0 | ISDN_PID_L0_NT_S0;
	/* no TE_S0 for L1 */
	obj->DPROTO.protocol[1] = ISDN_PID_L1_NT_S0;

	obj->BPROTO.protocol[1] = ISDN_PID_L1_B_64TRANS | ISDN_PID_L1_B_64HDLC;
	obj->BPROTO.protocol[2] = ISDN_PID_L2_B_TRANS | ISDN_PID_L2_B_RAWDEV;

	return as_isdn_register( obj);
}


/* init data structure for a S/T interface of a HFC card*/
static int __init _ashfc_init_port(AS_ISDNobject_t *obj, ashfc_t *hc, int port)
{
	int err, i;
	int ch/* D channel for a ST port*/, ch2/* B channels for this S/T port */;
	int bchperport =2, pt= port;	
	dchannel_t *dch;
	bchannel_t *bch;

	if (ashfc_protocol[port] == 0)
	{
		printk(KERN_ERR "Not enough 'protocol' values given.\n");
		err = -EINVAL;
		return err;
//		goto free_channels;
	}

/***************** initiate a D channel for this ST interface (No. pt) **********************/			
	/* port 	 	: 0;  		1;  				2;  				3
	    Channel	:(0<<2)+2=2; (1<<2)+2=6;		(2<<2)+2=10;	(3<<2)+2=14 */
	ch = (pt<<2)+2;	/* 4S/8S : 2 channels for every port */
			
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "ASHFC : Registering D-channel, PORT(%d-%d) CH(%d) PROTOCOL(%x)\n",  hc->id, pt+1, ch, ashfc_protocol[port]);
#endif
	hc->chan[ch].port = pt;	/* 0--3 hfc_chan[] is for pt ST interface  */
	hc->chan[ch].nt_timer = -1;

	dch = kmalloc(sizeof(dchannel_t), GFP_ATOMIC);
	if (!dch)
	{
		err = -ENOMEM;
		return err;
//		goto free_channels;
	}
	memset(dch, 0, sizeof(dchannel_t));
	dch->channel = ch;		/* channel number in 32 hfc_channel_t for this D channel*/
	//dch->debug = debug;
	dch->inst.obj = obj;
	dch->inst.lock = lock_dev;
	dch->inst.unlock = unlock_dev;
	/* added AS_ISDNinstance of this D channel */
	AS_ISDN_init_instance(&dch->inst, obj, hc);
	dch->inst.pid.layermask = ISDN_LAYER(0);	/* support Layer0 protocol for this D channel */
	sprintf(dch->inst.name, "ASHFC%d/%d", hc->id, pt+1);
	if (!(hc->chan[ch].rx_buf = kmalloc(MAX_DFRAME_LEN_L1, GFP_ATOMIC)))
	{
		err = -ENOMEM;
		return err;
//		goto free_channels;
	}

	if (as_isdn_init_dch(dch))
	{
		err = -ENOMEM;
		return err;
//		goto free_channels;
	}
	hc->chan[ch].dch = dch;

/********************************* allocate B channels for this ST port *******************/
	i=0;
	while(i < bchperport)
	{
		/*0,1,2(port 0); 4,5,6(port 1); 8,9,10(port 2); 12,13,14(port 3)*/
		ch2 = (pt<<2)+i;

#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "ASHFC : Registering B-channel, PORT(%d-%d) CH(%d) BCH(%d)\n",  hc->id, pt+1, ch2, i);
#endif
		hc->chan[ch2].port = pt;
		bch = kmalloc(sizeof(bchannel_t), GFP_ATOMIC);
		if (!bch)
		{
			err = -ENOMEM;
			return err;
//			goto free_channels;
		}
		memset(bch, 0, sizeof(bchannel_t));
		bch->channel = ch2;
		AS_ISDN_init_instance(&bch->inst, obj, hc);
//		bch->inst.pid.layermask = ISDN_LAYER(0);
		bch->inst.pid.layermask = ISDN_LAYER(0)|ISDN_LAYER(1)|ISDN_LAYER(2);
		bch->inst.lock = lock_dev;
		bch->inst.unlock = unlock_dev;
		//bch->debug = ashfc_debug;
		sprintf(bch->inst.name, "%s B%d", dch->inst.name, i+1);

		if (as_isdn_init_bch(bch))
		{
			kfree(bch);
			err = -ENOMEM;
			return err;
//			goto free_channels;
		}
		skb_queue_head_init(&hc->chan[ch2].dtmfque);
		hc->chan[ch2].bch = bch;
		if (bch->dev)
		{
//			TRACE();
			bch->dev->wport.pif.func =	ashfc_l2l1;
			bch->dev->wport.pif.fdata = bch;
		}
		i++;
	}

	/* S/T */
	/* set master clock of this card : only one ST interface can be used as TE mode ??? */
	/* following code should be move to other place ??? ,ch and pt is not assigned */
	if (ashfc_protocol[port] & 0x10000)
	{
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s: PROTOCOL set master clock: card(%d) port(%d)\n", __FUNCTION__, hc->id, pt);
		if (test_bit(HFC_CFG_NTMODE, &hc->chan[ch].cfg))
		{
			printk(KERN_ERR "Error: Master clock for port(%d) of card(%d) is only possible with TE-mode\n", pt+1, hc->id);
			err = -EINVAL;
			return err;
		}
					
		if (hc->masterclk >= 0)
		{
			printk(KERN_ERR "Error: Master clock for port(%d) of card(%d) already defined for port(%d)\n", pt+1, hc->id, hc->masterclk+1);
			err = -EINVAL;
			return err;
		}
//		hc->masterclk = pt;
		hc->masterclk = 1;
	}

/*************************** keep ST interface configuration in D channel of this interface  *******/
	/* set transmitter line to non capacitive */
	if (ashfc_protocol[port] & 0x20000)
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s: PROTOCOL set non capacitive transmitter: card(%d) port(%d)\n", __FUNCTION__, hc->id, pt);
#endif
		test_and_set_bit(HFC_CFG_NONCAP_TX, &hc->chan[ch].cfg);
	}
	/* disable E-channel */
	if (ashfc_protocol[port] & 0x40000)
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s: PROTOCOL disable E-channel: card(%d) port(%d)\n", __FUNCTION__, hc->id, pt);
#endif
		test_and_set_bit(HFC_CFG_DIS_ECHANNEL, &hc->chan[ch].cfg);
	}
	/* register E-channel */
	if (ashfc_protocol[port] & 0x80000)
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s: PROTOCOL register E-channel: card(%d) port(%d)\n", __FUNCTION__, hc->id, pt);
#endif
		test_and_set_bit(HFC_CFG_REG_ECHANNEL, &hc->chan[ch].cfg);
	}
			
	return 0;
}

static int __init _ashfc_init_stack4port(AS_ISDNobject_t *obj, ashfc_t *hc, int port)
{
	int err, i;
	int bchperport =2;	
	dchannel_t *dch;
	bchannel_t *bch;
	AS_ISDNstack_t *dst = NULL; /* make gcc happy */
	AS_ISDN_pid_t pid;

#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "ASHFC : PORT(%d-%d):  Adding D-stack, ",  hc->id, port+1);
#endif
	/* set D-channel : protocols and layers*/
	AS_ISDN_set_dchannel_pid(&pid, ashfc_protocol[port], ashfc_layermask[port]);

	/* get D channel of this ST interface */
	dch = hc->chan[(port<<2)+2].dch;

	/* set protocol ashfc_type for this port (only D channel) */
	if (ashfc_protocol[port] & 0x100 )
	{/* NT-mode */
		dch->inst.pid.protocol[0] = ISDN_PID_L0_NT_S0;
		dch->inst.pid.protocol[1] = ISDN_PID_L1_NT_S0;
		pid.protocol[0] = ISDN_PID_L0_NT_S0;
		pid.protocol[1] = ISDN_PID_L1_NT_S0;
		dch->inst.pid.layermask |= ISDN_LAYER(1);
		pid.layermask |= ISDN_LAYER(1);
		if (ashfc_layermask[port] & ISDN_LAYER(2))
			pid.protocol[2] = ISDN_PID_L2_LAPD_NET;
		test_and_set_bit(HFC_CFG_NTMODE, &hc->chan[(port<<2)+2].cfg);

		hc->chan[(port<<2)+2].nt_mode = 1;
	}
	else
	{/* TE-mode */
//		printk(KERN_ERR "TE Mode on port %d\n", port);
		dch->inst.pid.protocol[0] = ISDN_PID_L0_TE_S0;
		pid.protocol[0] = ISDN_PID_L0_TE_S0;

		hc->chan[(port<<2)+2].nt_mode = 0;
	}

	if ((err = obj->ctrl(NULL, MGR_NEWSTACK | REQUEST, &dch->inst)))
	{
		printk(KERN_ERR  "MGR_ADDSTACK REQUEST D ch err(%d)\n", err);
free_release:
		ashfc_release_port(hc, -1); /* all ports */
		return err;
//		goto free_object;
	}
//	obj->refcnt++;
	/* indicate that this stack is created */
	hc->created[port] = 1;
	dst = dch->inst.st;

	i = 0;
	while(i < bchperport)
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk( " Adding stack of BCH(%d) ",  i+1);
#endif
		bch = hc->chan[(port<<2) + i].bch;
		if ((err = obj->ctrl(dst, MGR_NEWSTACK | REQUEST, &bch->inst)))
		{
			printk(KERN_ERR "MGR_ADDSTACK Bchan error %d\n", err);
free_delstack:
			obj->ctrl(dst, MGR_DELSTACK | REQUEST, NULL);
			goto free_release;
		}
		bch->st = bch->inst.st;
		i++;
	}
#if WITH_ASHFC_DEBUG_INIT
	printk( "\n");

	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: (before MGR_SETSTACK REQUEST) layermask=0x%x\n", __FUNCTION__, pid.layermask);
#endif

	if ((err = obj->ctrl(dst, MGR_SETSTACK | REQUEST, &pid)))
	{
		printk(KERN_ERR "MGR_SETSTACK REQUEST dch err(%d)\n", err);
		goto free_delstack;
	}

#if 1
	i = 0;
	while(i < bchperport)
	{

		pid.protocol[1] = ISDN_PID_L1_B_64TRANS;	//ISDN_PID_L1_B_64HDLC;lizhijie,2005.12.18 
		pid.protocol[2] = ISDN_PID_L2_B_RAWDEV;//ISDN_PID_L2_B_TRANS;
		pid.layermask = ISDN_LAYER(1) | ISDN_LAYER(2);
		
		bch = hc->chan[(port<<2) + i].bch;
		
		if ((err = obj->ctrl(bch->st, MGR_SETSTACK | REQUEST, &pid)))
		{
			printk(KERN_ERR "MGR_SETSTACK REQUEST bch err(%d)\n", err);
			goto free_delstack;
		}

		i++;
	}
#endif

#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: (after MGR_SETSTACK REQUEST) layermask=0x%x\n", __FUNCTION__, pid.layermask);
#endif

	/* delay some time */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout((100*HZ)/1000); /* Timeout 100ms ? 10 ms */

	/* tell stack, that we are ready */
	obj->ctrl(dst, MGR_CTRLREADY | INDICATION, NULL);

	return 0;
}

static void __init _ashfc_chip_options(ashfc_t *hc)
{
/*************** set chip configuration in hc->chip ***********/

#if 1	
	test_and_set_bit(HFC_CHIP_ULAW, &hc->chip);
	ashfc_silence = 0xff; /* ulaw silence */
#else
	ashfc_silence = 0x2a; /* alaw silence */
#endif

	test_and_set_bit(HFC_CHIP_DTMF, &hc->chip);

#if 0
	test_and_set_bit(HFC_CHIP_PCM_SLAVE, &hc->chip);
#endif

#if 0
	test_and_set_bit(HFC_CHIP_CLOCK_IGNORE, &hc->chip);
#endif

#if 0
	test_and_set_bit(HFC_CHIP_RX_SYNC, &hc->chip);
#endif

#if 0
	test_and_set_bit(HFC_CHIP_EXRAM_128, &hc->chip);
	test_and_set_bit(HFC_CHIP_EXRAM_512, &hc->chip);
#endif

	hc->slots = 32;
#if 0	
	hc->slots = 64;
	hc->slots = 128;
#endif

#if 0
	hc->masterclk = 3;
#endif
}

int __init ashfc_init_ashfc(AS_ISDNobject_t *obj, ashfc_t *hc)
{
	int err;
//	AS_ISDNstack_t *dst = NULL; /* make gcc happy */
	int pt;
//	int hfc = hc->id-1 ;

/****************** Step 2.1 : create a ashfc_t instance and add into AS_ISDNobject->ilist  ******************************/
	/* set chip specific features */
	hc->masterclk = -1;	/* default, no master clock is provided */
	hc->type = ASHFC_CHIP_TYPE_4S;			/* ashfc_type[ hfc] & 0xff;*/

	_ashfc_chip_options(hc);
#if 0
	sprintf(hc->name, "ASHFC-%dS#%d", hc->type, hc->id);
#endif
	sprintf(hc->name, "ASHFC-%dS", hc->type );

	list_add_tail(&hc->list, &obj->ilist);
		
	lock_HW_init(&hc->lock);

	/* loop of every ST port/interface for a card */
	pt = 0;
	while (pt < hc->type) /* number of port/interface is 4 or 8 */
	{
		err = _ashfc_init_port(obj, hc, pt);
		if(err)
			return err;
		pt++;
	}
	/* end of port loop */

	err = ashfc_setup_pci(hc);
	if(err)
		return err;

	/* add stacks for every ST interface */
	pt = 0;
	while(pt < hc->type)
	{
		err = _ashfc_init_stack4port(obj, hc, pt);
		if(err)
			return err;
		pt++;
	}

	err = ashfc_hw_init(hc);
	if(err)
		return err;

	return 0;
}

