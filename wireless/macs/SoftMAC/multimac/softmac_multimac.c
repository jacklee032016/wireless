/*****************************************************************************
 *  Copyright 2005, Univerity of Colorado at Boulder.                        *
 *                                                                           *
 *                        All Rights Reserved                                *
 *                                                                           *
 *  Permission to use, copy, modify, and distribute this software and its    *
 *  documentation for any purpose other than its incorporation into a        *
 *  commercial product is hereby granted without fee, provided that the      *
 *  above copyright notice appear in all copies and that both that           *
 *  copyright notice and this permission notice appear in supporting         *
 *  documentation, and that the name of the University not be used in        *
 *  advertising or publicity pertaining to distribution of the software      *
 *  without specific, written prior permission.                              *
 *                                                                           *
 *  UNIVERSITY OF COLORADO DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS      *
 *  SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND        *
 *  FITNESS FOR ANY PARTICULAR PURPOSE.  IN NO EVENT SHALL THE UNIVERSITY    *
 *  OF COLORADO BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL         *
 *  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA       *
 *  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER        *
 *  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR         *
 *  PERFORMANCE OF THIS SOFTWARE.                                            *
 *                                                                           * 
 ****************************************************************************/


/**
 * @file softmac_multimac.c
 *  MultiMAC description
 */

#include <linux/module.h>
#if __ARM__
#include <asm/uaccess.h>
#else
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include "cu_softmac_api.h"
#include "cu_softmac_ath_api.h"
#include "softmac_netif.h"
#include "softmac_multimac.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Neufeld, Christian Doerr");

/*
** Per-instance data for the MAC
*/


/**  Some global constants */
enum 
{
	CHEESYMAC_PROCDIRNAME_LEN = 64,
};

typedef struct MACS_t
{
	char name[CU_SOFTMAC_NAME_SIZE];
			
	rwlock_t mac_busy;

	int (*mytxfunc)(void*,struct sk_buff* thepacket, int intop);
	int (*myrxfunc)(void*,struct sk_buff* thepacket, int intop);

	CU_SOFTMAC_MACLAYER_INFO* mac;

}MAC_INSTANCE;

#define MULTIMAC_MAX_MACS 			10

/**
 *  This is the structure containing all of the state information
 * required for each MultiMAC instance.
 */
typedef struct CHEESYMAC_INSTANCE_t
{
	/***  Use a Linux kernel linked list to keep track of all instances   */
	struct list_head list;

	/***  Lock access to MAC when altering basic parameters.   */
	rwlock_t mac_busy;

	/***  Keep a handle to the currently attached PHY layer.
	* XXX turn this into a read/write spinlock?   */
	CU_SOFTMAC_PHYLAYER_INFO *myphy;

	CU_SOFTMAC_PHYLAYER_INFO *multimac_fake_phy;

	/***  Pointer to macinfo structure representing this MAC instance   */
	CU_SOFTMAC_MACLAYER_INFO *mymac;

	/***  Network interface   */
	CU_SOFTMAC_NETIF_HANDLE *netif;

	/***  Keep a pointer to the "packet receive" function   */
	CU_SOFTMAC_MAC_RX_FUNC myrxfunc;

	void* myrxfunc_priv;

	CU_SOFTMAC_MAC_UNLOAD_NOTIFY_FUNC myunloadnotifyfunc;

	void* myunloadnotifyfunc_priv; 

	/***  We keep a unique ID for each instance we create in order to
	* do things like create separate proc directories for the settings
	* on each one.   */
	int instanceid;
	int passedup;
	int gotthere;
	char *preferred;

	/***  Keep a handle to the root procfs directory for this instance   */
	struct proc_dir_entry* my_procfs_root;

	/***  The name of our proc filesystem directory   */
	char procdirname[CHEESYMAC_PROCDIRNAME_LEN];

	/***  The proc directory entry structure corresponding to our
	* proc filesystem directory.   */
	struct proc_dir_entry* my_procfs_dir;

	/***  Track all of the proc filesystem entries allocated for
	* this MultiMAC instance.   */
	struct list_head my_procfs_data;

	/***  Transmit bitrate encoding to use.   */
	unsigned char txbitrate;

	/***  Setting the "defertx" flag results in transmission always
	* being deferred to the "work" callback, even when the packet comes
	* down while in the bottom half.   */
	int defertx;

	/***  Setting the "defertxdone" flag causes handling of the "txdone"
	* to always be deferred to the "work" callback.   */
	int defertxdone;

	/***  Setting the "deferrx" flag causes handling of the "txdone"
	* to always be deferred to the "work" callback.
	*/
	int deferrx;

	/***  The "maxinflight" variable determines the maximum number
	* of packets that are allowed to be pending transmission at any given time.   */
	int maxinflight;

	int runningmacs;
	char *names[MULTIMAC_MAX_MACS];
	MAC_INSTANCE* macs[MULTIMAC_MAX_MACS];

	/** The cheesymac uses Linux sk_buff queues when it needs
	* to keep packets around for deferred handling.   */

	/***  A Linux kernel sk_buff packet queue containing packets
	* whose transmission has been deferred. Generally not an issue
	* since packet transmission in Linux is handled in the bottom half
	* inside of a softirq.   */
	struct sk_buff_head tx_skbqueue;

	/***  A Linux kernel sk_buff packet queue containing packets
	* that have been transmitted but not yet handled. Because notification
	* of "transmit complete" often occurs in the top half of an interrupt
	* it may frequently make sense to use this queue in order to limit the
	* amount of work performed with interrupts disabled.	*/
	struct sk_buff_head txdone_skbqueue;

	/***  A Linux kernel sk_buff packet queue containing packets
	* that have been received but not yet handled. Because notification
	* of packet reception generally occurs in the top half of an interrupt
	* it may frequently make sense to use this queue in order to limit the
	* amount of work performed with interrupts disabled. For example,
	* the Linux cryptographic API functions must <b>not</b> be called from the
	* top half. Decrypting received packets must therefore be deferred
	* to handling in the bottom half, <i>i.e.</i> the "work" callback.
	*/
	struct sk_buff_head rx_skbqueue;

	int raw_mode;
} CHEESYMAC_INSTANCE;

/**
 *  Information about each proc filesystem entry for instance
 * parameters. An array of these will be used to specify
 * the proc entries to create for each MAC instance.
 */
typedef struct
{
	const char* name;

	/**  Read/write permissions, e.g. "0644" means read/write
	* for owner (root), and read-only for group and other users.   */

	mode_t mode;
	
	/***  A unique ID passed to the read and write handler functions
	* when data is written to or read from the particular entry.   */
	int entryid;
} CHEESYMAC_INST_PROC_ENTRY;

/***  Constants for proc entries for each MultiMAC instance */
enum
{
	CHEESYMAC_INST_PROC_TXBITRATE,
	CHEESYMAC_INST_PROC_DEFERTX,
	CHEESYMAC_INST_PROC_DEFERTXDONE,
	CHEESYMAC_INST_PROC_DEFERRX,
	CHEESYMAC_INST_PROC_MAXINFLIGHT,
	CHEESYMAC_INST_PROC_RUNNINGMAC,
	CHEESYMAC_INST_PROC_ADDMAC,
	CHEESYMAC_INST_PROC_DELETEMAC,
	CHEESYMAC_INST_PROC_RAW_MODE,
	CHEESYMAC_INST_PROC_COUNT,
	CHEESYMAC_INST_PROC_INCOMING,
	CHEESYMAC_INST_PROC_PASSED,
	CHEESYMAC_INST_PROC_PREFERRED,
	CHEESYMAC_INST_PROC_RESETCOUNTERS
};

/**
 *  Proc filesystem entries for each cheesymac instance. The "data"
 * field will be passed in to a generic read/write routine that
 * will use it to determine how to handle the read/write.
 */
static const CHEESYMAC_INST_PROC_ENTRY cheesymac_inst_proc_entries[] =
{
	{
		"txbitrate",
		0644,
		CHEESYMAC_INST_PROC_TXBITRATE
	}, 

	{
		"defertx",
		0644,
		CHEESYMAC_INST_PROC_DEFERTX
	},

	{
		"defertxdone",
		0644,
		CHEESYMAC_INST_PROC_DEFERTXDONE
	},

	{
		"deferrx",
		0644,
		CHEESYMAC_INST_PROC_DEFERRX
	},

	{
		"maxinflight",
		0644,
		CHEESYMAC_INST_PROC_MAXINFLIGHT
	},

	{
		"listmaclayer",
		0644,
		CHEESYMAC_INST_PROC_RUNNINGMAC
	},

	{
		"addmaclayer",
		0644,
		CHEESYMAC_INST_PROC_ADDMAC
	},

	{
		"deletemaclayer",
		0644,
		CHEESYMAC_INST_PROC_DELETEMAC
	},

	{
		"raw_mode",
		0644,
		CHEESYMAC_INST_PROC_RAW_MODE
	},

	{
		"incoming",
		0644,
		CHEESYMAC_INST_PROC_INCOMING
	},

	{
		"broken",
		0644,
		CHEESYMAC_INST_PROC_PASSED
	},

	{
		"preferred",
		0644,
		CHEESYMAC_INST_PROC_PREFERRED
	}, 

	{
		"resetcounters",
		0644,
		CHEESYMAC_INST_PROC_RESETCOUNTERS
	},

	/** Using this as the "null terminator" for the item list   */
	{
		0,
		0,
		-1
	},
};

/**
 *  An instance of this data structure is created for each proc
 * filesystem entry and placed into a linked list associated
 * with each instance. This allows us to handle proc filesystem
 * read/write requests.
 */
typedef struct
{
	struct list_head list;
	CHEESYMAC_INSTANCE* inst;
	int entryid;
	char name[CHEESYMAC_PROCDIRNAME_LEN];
	struct proc_dir_entry* parentdir;  
} CHEESYMAC_INST_PROC_DATA;


/*** Declarations of functions exported via the MAC info table.
** ("public" members of the MAC base class)
*/

/**  Notify the MAC layer that it is time to do some work. Exported
 * via pointer as "cu_softmac_work" to the SoftMAC PHY.
 */
static int cu_softmac_mac_work_cheesymac(void* mydata, int intop);

/*
 * Notify the MAC layer that a packet has been received -- exported
 * via pointer as "cu_softmac_packet_rx" to the SoftMAC PHY
 */
static int cu_softmac_mac_packet_rx_cheesymac(void* mydata, struct sk_buff* packet, int intop);

/**
 *  Notify the MAC layer that a packet transmit has completed.
 * Exported via pointer as "cu_softmac_packet_tx_done" to the SoftMAC PHY.
 */
static int cu_softmac_mac_packet_tx_done_cheesymac(void* mydata,	struct sk_buff* packet,	int intop);

/** 
 *  Notify the MAC layer that an ethernet-encapsulated packet
 * has been received from up the protocol stack. Exported
 * via pointer as "cu_softmac_packet_tx".
 */
static int cu_softmac_mac_packet_tx_cheesymac(void* mydata, struct sk_buff* packet, int intop);


/***  Tell the MAC layer to attach to the specified PHY layer. */
static int cu_softmac_mac_attach_cheesymac(void* handle, CU_SOFTMAC_PHYLAYER_INFO* phyinfo);

/***  Tell the MAC layer to detach from the specified PHY layer.  */
static int cu_softmac_mac_detach_cheesymac(void* handle);

static int cu_softmac_mac_set_rx_func_cheesymac(void* handle, CU_SOFTMAC_MAC_RX_FUNC rxfunc,void* rxpriv);

static int cu_softmac_mac_set_unload_notify_func_cheesymac(void* handle,
						CU_SOFTMAC_MAC_UNLOAD_NOTIFY_FUNC unloadfunc,	void* unloadpriv);

/*
** Declarations of internal functions ("private" members)
*/
static void cheesymac_destroy_instance(void* mypriv);
static CHEESYMAC_INSTANCE *cheesymac_create_instance(CU_SOFTMAC_MACLAYER_INFO* macinfo, CU_SOFTMAC_CHEESYMAC_PARAMETERS* params);

static int cheesymac_make_procfs_entries(CHEESYMAC_INSTANCE* inst);
static int cheesymac_delete_procfs_entries(CHEESYMAC_INSTANCE* inst);
static int cheesymac_inst_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data);
static int cheesymac_inst_write_proc(struct file *file,const char __user *buffer, unsigned long count, void *data);

/*
** Module parameters
*/

/***  Initial values for global MAC default parameter values.
 * Can override these upon module load.
 */
enum
{
	CHEESYMAC_DEFAULT_DEFERTX 			= 0,
	CHEESYMAC_DEFAULT_DEFERTXDONE 		= 1,
	CHEESYMAC_DEFAULT_DEFERRX 			= 1,
	CHEESYMAC_DEFAULT_MAXINFLIGHT 		= 256,
	CHEESYMAC_DEFAULT_DEFERALLRX 		= 0,
	CHEESYMAC_DEFAULT_DEFERALLTXDONE 	= 0,
	CHEESYMAC_DEFAULT_TXBITRATE 		= 2,
};

/***  Keep a reference to the head of our linked list of instances. */
static LIST_HEAD(cheesymac_instance_list);

/***  The MultiMAC */
static CU_SOFTMAC_LAYER_INFO the_cheesymac;

/**
 *  Some operations, e.g. getting/setting the next instance ID
 * and accessing parameters, should be performed "atomically."
 */
static spinlock_t cheesymac_global_lock = SPIN_LOCK_UNLOCKED;


/***  First instance ID to use is 1 */
static int cheesymac_next_instanceid = 1;

/***  Bitrate encoding to use when transmitting packets. */
static int cheesymac_txbitrate = CHEESYMAC_DEFAULT_TXBITRATE;

/***  Whether or not to defer handling of packet transmission. */
static int cheesymac_defertx = CHEESYMAC_DEFAULT_DEFERTX;
static int cheesymac_defertxdone = CHEESYMAC_DEFAULT_DEFERTXDONE;
static int cheesymac_deferrx = CHEESYMAC_DEFAULT_DEFERRX;
static int cheesymac_maxinflight = CHEESYMAC_DEFAULT_MAXINFLIGHT;

#if 0
/*
 * XXX
 * use the ath-specific "deferallrx" and "deferalltxdone"?
 */
static int cheesymac_ath_deferallrx = CHEESYMAC_DEFAULT_DEFERALLRX;
static int cheesymac_ath_deferalltxdone = CHEESYMAC_DEFAULT_DEFERALLTXDONE;
#endif


/** Default root directory for cheesymac procfs entries */
static char *cheesymac_procfsroot = "multimac";
//static struct proc_dir_entry* cheesymac_procfsroot_handle = 0;
char *currentmacname;

/** Default network interface to use as a softmac phy layer */
static char* cheesymac_defaultphy = "ath0";

/*
 * Default network prefix to use for the OS-exported network interface
 * attached to the MAC layer. The unique cheesyMAC ID is used
 * to fill out the "%d" field
 */
static char* cheesymac_netiftemplate = "multi%d";

#if __ARM__
MODULE_PARM(cheesymac_defertx, "1i");
MODULE_PARM(cheesymac_defertxdone, "1i");
MODULE_PARM(cheesymac_deferrx, "1i");
MODULE_PARM(cheesymac_maxinflight, "1i");
MODULE_PARM(cheesymac_txbitrate, "1i");
MODULE_PARM(cheesymac_procfsroot, "1i");
MODULE_PARM(cheesymac_defaultphy, "1i");
#else
module_param(cheesymac_defertx, int, 0644);
module_param(cheesymac_defertxdone, int, 0644);
module_param(cheesymac_deferrx, int, 0644);
module_param(cheesymac_maxinflight, int, 0644);
module_param(cheesymac_txbitrate, int, 0644);
module_param(cheesymac_procfsroot, charp, 0444);
module_param(cheesymac_defaultphy, charp, 0644);
#endif

MODULE_PARM_DESC(cheesymac_defertx, "Queue packets and defer transmit to tasklet");
MODULE_PARM_DESC(cheesymac_defertxdone, "Queue packets that are finished transmitting and defer handling to tasklet");
MODULE_PARM_DESC(cheesymac_deferrx, "Queue received packets and defer handling to tasklet");
MODULE_PARM_DESC(cheesymac_maxinflight, "Limit the number of packets allowed to be in the pipeline for transmission");
MODULE_PARM_DESC(cheesymac_txbitrate, "Default bitrate to use when transmitting packets");
MODULE_PARM_DESC(cheesymac_procfsroot, "Subdirectory in procfs to use for cheesymac parameters/statistics");
MODULE_PARM_DESC(cheesymac_defaultphy, "Network interface to use for SoftMAC PHY layer");

static int  multimac_set_rx_func(char *name, void* mydata, CU_SOFTMAC_MAC_RX_FUNC rxfunc, void* rxpriv)
{
	int result = -1;
	CHEESYMAC_INSTANCE* inst = mydata;

	if (inst)
	{
		write_lock(&(inst->mac_busy));

		int i;
		for(i=0; i<MULTIMAC_MAX_MACS; i++)
		{
			if(inst->macs[i])
			{
				if(strncmp(name, inst->macs[i]->name, CU_SOFTMAC_NAME_SIZE)==0)
				{
					if(rxfunc)
						inst->myrxfunc = rxfunc;
					result=0;
					break;
				}
			}
		}

		write_unlock(&(inst->mac_busy));
	}

	return result;
}


static int multimac_set_tx_func(char *name, void* mydata,
		    CU_SOFTMAC_MAC_RX_FUNC txfunc, void* txpriv)
{
	int result = -1;
	CHEESYMAC_INSTANCE* inst = mydata;
	if (inst)
	{
		write_lock(&(inst->mac_busy));

		int i;
		for(i=0; i<MULTIMAC_MAX_MACS; i++)
		{
			if(inst->macs[i])
			{
				if(strncmp(name, inst->macs[i]->name, CU_SOFTMAC_NAME_SIZE)==0)
				{
					//if(txfunc)
					//	inst->mytxfunc = txfunc;
					result=0;
					break;
				}
			}
		}
		write_unlock(&(inst->mac_busy));
	}

	return result;
}

static int multimac_netif_rxhelper(void* priv, struct sk_buff* packet, int intop)
{
	CHEESYMAC_INSTANCE* inst = priv;

	if (inst && inst->myphy)
	{
		//printk("MultiMAC: packet received %p %d\n", inst, inst->runningmacs);
		int i;

		/* emulate athphy raw mode */
		if (!inst->raw_mode && cu_softmac_ath_issoftmac(inst->myphy->phy_private, packet))
			packet = cu_softmac_ath_decapsulate(inst->myphy->phy_private, packet);

		int claimed = 0;
		int broken = 0;

		for(i=0; i<MULTIMAC_MAX_MACS; i++)
		{
			if(inst->macs[i])
			{
				int txresult = -1;
				if(inst->macs[i]!=NULL && inst->macs[i]->myrxfunc!=NULL)
				{
					struct sk_buff *skb = skb_copy(packet, GFP_ATOMIC);
					txresult = (inst->macs[i]->myrxfunc)(inst->macs[i]->mac->mac_private, skb,0);
					if (txresult == MULTIMAC_CLAIMED_PACKET)
					{
						claimed++;
					}
					else
					{
						if (txresult == MULTIMAC_BROKEN_PACKET)
							broken++;
						kfree_skb(skb);
					}
				}
			}
		}

		if(claimed>=1)
			inst->passedup++;

		if(broken>=1)
		{
			inst->gotthere++;
			if(claimed==0)
				inst->passedup++;
		}

		(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, packet);
	}

	else
	{
		dev_kfree_skb(packet);
	}
	return 0;
}


static int multimac_netif_txhelper(CU_SOFTMAC_NETIF_HANDLE nif,void* priv, struct sk_buff* packet)
{
	CHEESYMAC_INSTANCE* inst = priv;

	if (inst)
	{
		/** Call the cheesymac tx function. The "intop" indicator
		* is always zero for the particular netif implementation
		* we're using -- in Linux the packet transmission is
		* instigated by a softirq in the bottom half.     */
		int i;
		int txresult;

		for (i=0; i<MULTIMAC_MAX_MACS; i++)
		{
			if (inst->macs[i] && inst->macs[i]->mytxfunc)
			{
				if(strncmp(inst->preferred, "*", 1)==0 || strncmp(inst->preferred, inst->macs[i]->name, CU_SOFTMAC_NAME_SIZE)==0)
				{
					struct sk_buff *skb = skb_copy(packet, GFP_ATOMIC);
					txresult = (inst->macs[i]->mytxfunc)((inst->macs[i])->mac->mac_private,skb,0);
				}
			}
		} 
		dev_kfree_skb(packet);
	}

	return 0;
}

#if 0
    int i;
    for(i=0; i<MULTIMAC_MAX_MACS->; i++)
    {
    	if(inst->macs[i])
	{
		if(strncmp(currentmacname, (inst->macs[i])->name, CU_SOFTMAC_NAME_SIZE)==0)
    			break;
	}
    }
     
    int txresult = -1;
    if(inst->macs[i]!=NULL && inst->macs[i]->mytxfunc!=NULL)
    {	
	txresult = (inst->macs[i]->mytxfunc)((inst->macs[i])->mac->mac_private,packet,0);
    }else
    	printk("Autsch!\n");

    // RR Code
    if (inst->macs[0] && inst->macs[1]) {
	if (!strncmp(currentmacname, (inst->macs[0])->name, CU_SOFTMAC_NAME_SIZE)) {
	    strncpy(currentmacname, (inst->macs[1])->name, CU_SOFTMAC_NAME_SIZE);
	} else {
	    strncpy(currentmacname, (inst->macs[0])->name, CU_SOFTMAC_NAME_SIZE);
	}
    }
//txresult = cu_softmac_mac_packet_tx_cheesymac(inst,packet,0);

    if (CU_SOFTMAC_MAC_TX_OK != txresult)
    {
      dev_kfree_skb(packet);
    }
 }
 return 0;
}
#endif

/*
** SoftMAC Netif stuff
*/
static int cheesymac_netif_txhelper(CU_SOFTMAC_NETIF_HANDLE nif, void* priv, struct sk_buff* packet) 
{
	CHEESYMAC_INSTANCE* inst = priv;
	if (inst)
	{/** Call the cheesymac tx function. The "intop" indicator
		* is always zero for the particular netif implementation
		* we're using -- in Linux the packet transmission is
		* instigated by a softirq in the bottom half.     */
		int txresult = cu_softmac_mac_packet_tx_cheesymac(inst,packet,0);
		if (CU_SOFTMAC_MAC_TX_OK != txresult)
		{
			dev_kfree_skb(packet);
		}
	}
	return 0;
}

static int cheesymac_netif_unload_helper(CU_SOFTMAC_NETIF_HANDLE netif, void* priv) 
{
	CHEESYMAC_INSTANCE* inst = priv;
	if (inst)
	{
		cu_softmac_mac_set_rx_func_cheesymac(inst,0,0);
	}
	return 0;
}

/*
 * Create a cheesymac network interface and attach to it
 */
static int cheesymac_create_and_attach_netif(void *mypriv)
{
	CHEESYMAC_INSTANCE *inst = mypriv;
	CU_SOFTMAC_MACLAYER_INFO *macinfo = inst->mymac;
	int ret = 0;
	struct net_device *checknet = 0;

	checknet = dev_get_by_name(macinfo->name);
	if (checknet)
	{
		printk(KERN_DEBUG "MultiMAC: Attaching to %s\n", macinfo->name);
		inst->netif = cu_softmac_netif_from_dev(checknet);
		dev_put(checknet);
		/*cu_softmac_netif_set_tx_callback(inst->netif, cheesymac_netif_txhelper, (void *)inst);*/

		cu_softmac_netif_set_tx_callback(inst->netif,multimac_netif_txhelper, (void *)inst);
	}
	else
	{
		printk(KERN_DEBUG "MultiMAC: Creating %s\n", macinfo->name);
		/*inst->netif = cu_softmac_netif_create_eth(macinfo->name, 0,cheesymac_netif_txhelper,	inst);*/
		inst->netif = cu_softmac_netif_create_eth(macinfo->name, 0,multimac_netif_txhelper,inst);
	}

	if (inst->netif)
	{//printk(KERN_DEBUG "MultiMAC: Setting mac unload notify func\n");
		(macinfo->cu_softmac_mac_set_unload_notify_func)(inst,	 cu_softmac_netif_detach, inst->netif);
		//printk(KERN_DEBUG "MultiMAC: Setting netif unload callback func\n");
		cu_softmac_netif_set_unload_callback(inst->netif,  cheesymac_netif_unload_helper, inst);
		(macinfo->cu_softmac_mac_set_rx_func)(inst, cu_softmac_netif_rx_packet, inst->netif);
	}
	else
	{
		printk(KERN_ALERT "MultiMAC: Unable to attach to netif!\n");
		/** Whack the cheesymac instance we just created	 */
		cheesymac_destroy_instance(inst);
		ret = -1;
	}
	return ret;
}

static void cu_softmac_cheesymac_prep_skb(CHEESYMAC_INSTANCE* inst, struct sk_buff* skb)
{
	if (inst && inst->myphy)
	{   /** XXX use of atheros-specific PHY calls!!!     */
		cu_softmac_ath_set_default_phy_props(inst->myphy->phy_private, skb);
		cu_softmac_ath_set_tx_bitrate(inst->myphy->phy_private, skb, inst->txbitrate);
	}
}

static struct sk_buff* multimac_phy_alloc_skb(void *mydata, int len)
{
	CHEESYMAC_INSTANCE* inst = mydata;
	struct sk_buff *skb = 0;

	read_lock(&(inst->mac_busy));
	if (inst->myphy)
		skb = inst->myphy->cu_softmac_phy_alloc_skb(inst->myphy->phy_private, len);
	read_unlock(&(inst->mac_busy));

	return skb;
}

static int multimac_phy_sendpacket(void* mydata, int max_inflight, struct sk_buff* packet)
{
	CHEESYMAC_INSTANCE* inst = mydata;
	int status = CU_SOFTMAC_MAC_TX_OK;
	int txresult = CU_SOFTMAC_PHY_SENDPACKET_OK;

	if (inst && inst->myphy)
	{
		read_lock(&(inst->mac_busy));

		/** Check to see if we're in the top half or bottom half, i.e. are
		* we taking an interrupt right now?     */
		struct sk_buff* skb = 0;

		/** NOT in top half -- process our transmit queue       */

		if (packet)
		{
			skb_queue_tail(&(inst->tx_skbqueue),packet);
		}

		/** Walk our transmit queue, shovelling out packets as we go...       */
		while ((skb = skb_dequeue(&(inst->tx_skbqueue))))
		{
			if (!inst->raw_mode)
				cu_softmac_ath_encapsulate(inst->myphy->phy_private, packet);

			cu_softmac_cheesymac_prep_skb(inst, packet);
			txresult = (inst->myphy->cu_softmac_phy_sendpacket)(inst->myphy->phy_private, inst->maxinflight, packet);
			if (CU_SOFTMAC_PHY_SENDPACKET_OK != txresult)
			{
				printk(KERN_ALERT "SoftMAC MultiMAC: tasklet packet tx failed: %d\n",txresult);
				/** N.B. we return an "OK" for the transmit because
				* we're handling the sk_buff freeing down here --
				* as far as the caller is concerned we were successful.	   */
			}
		}
		read_unlock(&(inst->mac_busy));
	}
	else
	{   /** Could not get our instance handle -- let the caller know...     */
		printk(KERN_ALERT "MultiMAC: packet_tx -- no instance handle!\n");
		status = CU_SOFTMAC_MAC_TX_FAIL;
	}

	return status;
}

/**
 * \brief Implementation of "cu_softmac_mac_packet_tx"
 * cu_softmac_mac_packet_tx_cheesymac
 *
 */
static int cu_softmac_mac_packet_tx_cheesymac(void* mydata, struct sk_buff* packet, int intop)
{
	CHEESYMAC_INSTANCE* inst = mydata;
	int status = CU_SOFTMAC_MAC_TX_OK;
	int txresult = CU_SOFTMAC_PHY_SENDPACKET_OK;

	if (inst && inst->myphy)
	{
		read_lock(&(inst->mac_busy));

		/** Check to see if we're in the top half or bottom half, i.e. are
		* we taking an interrupt right now?     */
		if (intop)
		{
			/** As a general rule, try to keep the stuff you do in the top
			* half to a minimum. This is during the immediate handling
			* of an interrupt and the place for time-critical tasks to occur.       */
			if (inst->defertx && packet)
			{
				/* Queue the packet in tx_skbqueue, schedule the "work" method to run. */
				skb_queue_tail(&(inst->tx_skbqueue),packet);
				(inst->myphy->cu_softmac_phy_schedule_work_asap)(inst->myphy->phy_private);
				status = CU_SOFTMAC_MAC_TX_OK;
			}
			else if (packet)
			{
				/** Send the packet now	 */
				cu_softmac_cheesymac_prep_skb(inst, packet);
				txresult = (inst->myphy->cu_softmac_phy_sendpacket)(inst->myphy->phy_private, inst->maxinflight,  packet);
				if (CU_SOFTMAC_PHY_SENDPACKET_OK != txresult)
				{
					printk(KERN_ALERT "SoftMAC MultiMAC: top half packet tx failed: %d\n",txresult);
				}
				status = CU_SOFTMAC_MAC_TX_OK;
			}
		}
		else
		{
			struct sk_buff* skb = 0;
			/** NOT in top half -- process our transmit queue       */
			if (packet)
			{
				skb_queue_tail(&(inst->tx_skbqueue),packet);
			}

			/** Walk our transmit queue, shovelling out packets as we go...       */
			while ((skb = skb_dequeue(&(inst->tx_skbqueue))))
			{
				cu_softmac_cheesymac_prep_skb(inst, packet);
				txresult = (inst->myphy->cu_softmac_phy_sendpacket)(inst->myphy->phy_private, inst->maxinflight, packet);
				if (CU_SOFTMAC_PHY_SENDPACKET_OK != txresult)
				{
					printk(KERN_ALERT "SoftMAC MultiMAC: tasklet packet tx failed: %d\n",txresult);
					/** N.B. we return an "OK" for the transmit because
					* we're handling the sk_buff freeing down here --
					* as far as the caller is concerned we were successful.	   */
				}
			}
		}
		read_unlock(&(inst->mac_busy));
	}

	else
	{
		/** Could not get our instance handle -- let the caller know...     */
		printk(KERN_ALERT "MultiMAC: packet_tx -- no instance handle!\n");
		status = CU_SOFTMAC_MAC_TX_FAIL;
	}

	return status;
}

static int cu_softmac_mac_packet_tx_done_cheesymac(void* mydata, struct sk_buff* packet, int intop) 
{
	int status = CU_SOFTMAC_MAC_NOTIFY_OK;
	CHEESYMAC_INSTANCE* inst = mydata;

	if (inst && inst->myphy)
	{
		read_lock(&(inst->mac_busy));

		/** Check to see if we're supposed to defer handling     */
		if (intop)
		{
			if (inst->defertxdone && packet)
			{
				/* Queue the packet in txdone_skbqueue, schedule the tasklet	 */
				skb_queue_tail(&(inst->txdone_skbqueue), packet);
				status = CU_SOFTMAC_MAC_NOTIFY_RUNAGAIN;
			}
			else if (packet)
			{
				/** Free the packet immediately, do not run again	 */
				(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, packet);
				packet = 0;
				status = CU_SOFTMAC_MAC_NOTIFY_OK;
			}
		}
		else
		{
			struct sk_buff* skb = 0;

			/** In bottom half -- process any deferred packets       */
			if (packet)
			{
				skb_queue_tail(&(inst->txdone_skbqueue), packet);
			}

			while ((skb = skb_dequeue(&(inst->txdone_skbqueue))))
			{
				(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, skb);
			}
		}

		read_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_ALERT "MultiMAC: packet_tx_done -- no instance handle!\n");
		status = CU_SOFTMAC_MAC_NOTIFY_HOSED;
	}

	return status;
}


static int cu_softmac_mac_packet_rx_cheesymac(void* mydata, struct sk_buff* packet, int intop)
{
	CHEESYMAC_INSTANCE* inst = mydata;
	int status = CU_SOFTMAC_MAC_NOTIFY_OK;

	// XXX check to see if MAC is active...
	if (inst)
	{
		read_lock(&(inst->mac_busy));
		//printk(KERN_DEBUG "cheesymac: packet rx\n");
		if (intop)
		{
			/* We defer handling to the bottom half if we're either
			* told to explicitly, or if we're using encryption.       */
			if (inst->deferrx && packet)
			{
				/** Queue packet for later processing	 */
				//printk(KERN_DEBUG "cheesymac: packet rx deferring\n");
				skb_queue_tail(&(inst->rx_skbqueue), packet);
				status = CU_SOFTMAC_MAC_NOTIFY_RUNAGAIN;
			}
			else if (packet)
			{
				/* Just send the packet up the network stack if we've got
				* an attached rxfunc, otherwise whack the packet.	 */
				if (inst->myrxfunc)
				{
					printk(KERN_DEBUG "cheesymac: packet rx -- calling receive\n");
					(inst->myrxfunc)(inst->myrxfunc_priv, packet);
				}
				else
				{ 
					//if (inst->myphy->cu_softmac_free_skb) {
					//printk(KERN_DEBUG "cheesymac: packet rx -- freeing skb\n");
					(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, packet);
				}
				//else {
				//printk(KERN_DEBUG "cheesymac: packet rx -- mac hosed\n");
				//status = CU_SOFTMAC_MAC_NOTIFY_HOSED;
				//}
			}
		}
		else
		{
			struct sk_buff* skb = 0;

			/** Not in top half - walk our rx queue and send packets up the stack       */
			//printk(KERN_DEBUG "cheesymac: packet rx -- bottom half\n");
			if (packet)
			{
				skb_queue_tail(&(inst->rx_skbqueue), packet);
			}

			while ((skb = skb_dequeue(&(inst->rx_skbqueue))))
			{
				if (inst->myrxfunc)
				{
					(inst->myrxfunc)(inst->myrxfunc_priv, packet);
				}
				else
				{
					/** No rx function available? Ask phy layer to free the packet.	   */
					(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, packet);
				}
			}
		}

		read_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_ALERT "MultiMAC: packet_rx -- no instance handle!\n");
		status = CU_SOFTMAC_MAC_NOTIFY_HOSED;
	}
	return status;
}

static int cu_softmac_mac_work_cheesymac(void* mydata, int intop) 
{
	CHEESYMAC_INSTANCE* inst = mydata;
	int status = CU_SOFTMAC_MAC_NOTIFY_OK;

	if (inst)
	{
		int txresult;
		struct sk_buff* skb = 0;

		/** Walk our receive queue, pumping packets up to the system as we go...     */
		while ((skb = skb_dequeue(&(inst->rx_skbqueue))))
		{
			if (inst->myrxfunc)
			{
				//printk(KERN_DEBUG "cheesymac: have rxfunc -- receiving\n");
				(inst->myrxfunc)(inst->myrxfunc_priv,skb);
			}
			else
			{
				printk(KERN_DEBUG "cheesymac: packet rx -- no rxfunc freeing skb\n");
				(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, skb);
			}
		}

		/** Walk our transmit queue, shovelling out packets as we go...     */
		while ((skb = skb_dequeue(&(inst->tx_skbqueue))))
		{
			cu_softmac_cheesymac_prep_skb(inst, skb);
			txresult = (inst->myphy->cu_softmac_phy_sendpacket)(inst->myphy->phy_private, inst->maxinflight, skb);
			if (CU_SOFTMAC_PHY_SENDPACKET_OK != txresult)
			{
				printk(KERN_ALERT "SoftMAC MultiMAC: work packet tx failed: %d\n",txresult);
			}
		}

		/** Walk through the "transmit done" queue, freeing packets as we go...     */
		while ((skb = skb_dequeue(&(inst->txdone_skbqueue))))
		{
			(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, skb);
		}
	}
	else
	{
		status = CU_SOFTMAC_MAC_NOTIFY_HOSED;
	}

	return status;
}

void cu_softmac_cheesymac_set_macinfo_functions(CU_SOFTMAC_MACLAYER_INFO *macinfo) 
{
  // XXX lock
	macinfo->cu_softmac_mac_packet_tx = cu_softmac_mac_packet_tx_cheesymac;
	macinfo->cu_softmac_mac_packet_tx_done = cu_softmac_mac_packet_tx_done_cheesymac;
	macinfo->cu_softmac_mac_packet_rx = cu_softmac_mac_packet_rx_cheesymac;
	macinfo->cu_softmac_mac_work = cu_softmac_mac_work_cheesymac;
	macinfo->cu_softmac_mac_attach = cu_softmac_mac_attach_cheesymac;
	macinfo->cu_softmac_mac_detach = cu_softmac_mac_detach_cheesymac;
	macinfo->cu_softmac_mac_set_rx_func = cu_softmac_mac_set_rx_func_cheesymac;
	macinfo->cu_softmac_mac_set_unload_notify_func = cu_softmac_mac_set_unload_notify_func_cheesymac;
}

static CHEESYMAC_INSTANCE *cheesymac_create_instance(CU_SOFTMAC_MACLAYER_INFO* macinfo,
						     CU_SOFTMAC_CHEESYMAC_PARAMETERS* params) 
{
	CHEESYMAC_INSTANCE* inst;

	/** Create a new instance,
	* make it part of cheesymac_instance_list linked list, return it   */

	inst = kmalloc(sizeof(CHEESYMAC_INSTANCE), GFP_ATOMIC);
	if (inst)
	{
		memset(inst, 0, sizeof(CHEESYMAC_INSTANCE));

		/* lock the inst before showing it to anyone else */
		inst->mac_busy = RW_LOCK_UNLOCKED;
		write_lock(&(inst->mac_busy));

		/* add new instance to list */
		INIT_LIST_HEAD(&inst->list);
		list_add_tail(&inst->list, &cheesymac_instance_list);
		INIT_LIST_HEAD(&inst->my_procfs_data);

		/* fire up the instance... */
		inst->runningmacs = 0;
		inst->myphy = 0;
		inst->mymac = macinfo;
		inst->passedup = 0;
		inst->gotthere = 0;
		inst->preferred = kmalloc(CU_SOFTMAC_NAME_SIZE, GFP_KERNEL);
		sprintf(inst->preferred, "*");

		/* access the global cheesymac variables safely */
		spin_lock(&cheesymac_global_lock);
		inst->instanceid = cheesymac_next_instanceid;
		cheesymac_next_instanceid++;
		inst->txbitrate = cheesymac_txbitrate;
		inst->defertx = cheesymac_defertx;
		inst->defertxdone = cheesymac_defertxdone;
		inst->deferrx = cheesymac_deferrx;
		inst->maxinflight = cheesymac_maxinflight;
		spin_unlock(&cheesymac_global_lock);

		/* setup params if any were passed in */
		if (params)
		{
			inst->txbitrate = params->txbitrate;
			inst->defertx = params->defertx;
			inst->defertxdone = params->defertxdone;
			inst->deferrx = params->deferrx;
			inst->maxinflight = params->maxinflight;
		}

		/* initialize our packet queues */
		skb_queue_head_init(&(inst->tx_skbqueue));
		skb_queue_head_init(&(inst->txdone_skbqueue));
		skb_queue_head_init(&(inst->rx_skbqueue));

		/* load up the function table so that other layers can communicate with us */
		cu_softmac_cheesymac_set_macinfo_functions(macinfo);
		macinfo->cu_softmac_mac_packet_rx = multimac_netif_rxhelper;

		/* finish macinfo setup */
		macinfo->layer = &the_cheesymac;
		macinfo->mac_private = inst;
		snprintf(macinfo->name, CU_SOFTMAC_NAME_SIZE, cheesymac_netiftemplate, inst->instanceid);

		/* fake phy that multimac attaches to mac layers */
		inst->multimac_fake_phy = cu_softmac_phyinfo_alloc();
		inst->multimac_fake_phy->phy_private = inst;
		snprintf(inst->multimac_fake_phy->name, CU_SOFTMAC_NAME_SIZE, "%s_fake_phy", macinfo->name);
		inst->multimac_fake_phy->cu_softmac_phy_sendpacket = multimac_phy_sendpacket;
		inst->multimac_fake_phy->cu_softmac_phy_alloc_skb = multimac_phy_alloc_skb;

		// RR-Thing - Remove!    
		currentmacname = kmalloc(CU_SOFTMAC_NAME_SIZE, GFP_KERNEL);
		sprintf(currentmacname, "mac1");

		/* release write lock */
		write_unlock(&(inst->mac_busy));

		/* create and attach to our Linux network interface */
		cheesymac_create_and_attach_netif(inst);
	}
	else
	{
		printk(KERN_ALERT "MultiMAC create_instance: Unable to allocate memory!\n");
	}

	return inst;
}


/** Detach/delete this cheesymac instance */
static void cheesymac_destroy_instance(void* mypriv) 
{
	CHEESYMAC_INSTANCE* inst = mypriv;    
	struct sk_buff* skb = 0;

	if (inst)
	{
		list_del(&(inst->list));

		/* Notify folks that we're going away... */
		inst->mymac->cu_softmac_mac_detach(inst);
		if (inst->myunloadnotifyfunc)
		{
			(inst->myunloadnotifyfunc)(inst->myunloadnotifyfunc_priv);
		}

		/* Now lock the instance and shut it down	 */
		write_lock(&(inst->mac_busy));

		/* remove procfs entries */
		cheesymac_delete_procfs_entries(inst);

		/* Drain queues */
		while ((skb = skb_dequeue(&(inst->tx_skbqueue))))
		{
			(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, skb);
		}

		while ((skb = skb_dequeue(&(inst->txdone_skbqueue))))
		{
			(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, skb);
		}

		while ((skb = skb_dequeue(&(inst->rx_skbqueue))))
		{
			(inst->myphy->cu_softmac_phy_free_skb)(inst->myphy->phy_private, skb);
		}

		inst->mymac = 0;
		inst->myphy = 0;
		inst->multimac_fake_phy->phy_private = 0;
		cu_softmac_phyinfo_free(inst->multimac_fake_phy);
		write_unlock(&(inst->mac_busy));

		kfree(inst);
	}
}

void *cu_softmac_cheesymac_new_instance(void *layer_priv)
{
	CU_SOFTMAC_MACLAYER_INFO *macinfo;
	CHEESYMAC_INSTANCE *inst;

	macinfo = cu_softmac_macinfo_alloc();
	if (!macinfo)
	{
		return 0;
	}

	inst = cheesymac_create_instance(macinfo, 0);
	if (!inst)
	{
		cu_softmac_macinfo_free(macinfo);
		return 0;
	}

	/* setup macinfo and register it with softmac */
	cu_softmac_macinfo_register(macinfo);

	/* create procfs entries */
	inst->my_procfs_root = inst->mymac->proc;
	cheesymac_make_procfs_entries(inst);

	/* instances don't keep references to themselves */
	cu_softmac_macinfo_free(macinfo);

	return macinfo;
}

void cu_softmac_cheesymac_free_instance(void *layer_priv, void *info)
{
	CU_SOFTMAC_MACLAYER_INFO *macinfo = info;
	CHEESYMAC_INSTANCE *inst = macinfo->mac_private;
	cheesymac_destroy_instance(inst);
}
  
static int cheesymac_make_procfs_entries(CHEESYMAC_INSTANCE* inst) 
{
	int result = 0;

	if (inst)
	{
		int i = 0;
		struct proc_dir_entry* curprocentry = 0;
		CHEESYMAC_INST_PROC_DATA* curprocdata = 0;

		/** First make the directory. For right now, we're just using the unique
		* MAC layer ID that was assigned upon creation as a name.
		*/

		//snprintf(inst->procdirname,CHEESYMAC_PROCDIRNAME_LEN,"%d",inst->instanceid);
		//inst->my_procfs_dir = proc_mkdir(inst->procdirname,inst->my_procfs_root);
		//inst->my_procfs_dir->owner = THIS_MODULE;
		inst->my_procfs_dir = inst->my_procfs_root;

		/** Make individual entries. Stop when we get either a null string
		* or an empty string for a name.		*/

		i = 0;

		while (cheesymac_inst_proc_entries[i].name && cheesymac_inst_proc_entries[i].name[0])
		{
			//printk(KERN_ALERT "MultiMAC: Creating proc entry %s, number %d\n",cheesymac_inst_proc_entries[i].name,i);
			curprocentry = create_proc_entry(cheesymac_inst_proc_entries[i].name, cheesymac_inst_proc_entries[i].mode, inst->my_procfs_dir);
			curprocentry->owner = THIS_MODULE;

			/** Allocate and fill out a proc data structure, add it to the linked list for the instance.       */

			curprocdata = kmalloc(sizeof(CHEESYMAC_INST_PROC_DATA),GFP_ATOMIC);
			INIT_LIST_HEAD((&curprocdata->list));
			list_add_tail(&(curprocdata->list),&(inst->my_procfs_data));
			curprocdata->inst = inst;
			curprocdata->entryid = cheesymac_inst_proc_entries[i].entryid;
			strncpy(curprocdata->name,cheesymac_inst_proc_entries[i].name,CHEESYMAC_PROCDIRNAME_LEN);
			curprocdata->parentdir = inst->my_procfs_dir;

			/** Hook up the new proc entry data       */
			curprocentry->data = curprocdata;

			/** Set read/write functions for the proc entry.       */
			curprocentry->read_proc = cheesymac_inst_read_proc;
			curprocentry->write_proc = cheesymac_inst_write_proc;
			i++;
		}
	}

	return result;
}

static int cheesymac_delete_procfs_entries(CHEESYMAC_INSTANCE* inst) 
{
	int result = 0;
	if (inst)
	{
		struct list_head* tmp = 0;
		struct list_head* p = 0;
		CHEESYMAC_INST_PROC_DATA* proc_entry_data = 0;

		/** First remove individual entries and delete their data     */
		list_for_each_safe(p,tmp,&inst->my_procfs_data)
		{
			proc_entry_data = list_entry(p,CHEESYMAC_INST_PROC_DATA,list);
			list_del(p);
			remove_proc_entry(proc_entry_data->name,proc_entry_data->parentdir);
			kfree(proc_entry_data);
			proc_entry_data = 0;
		}

		/** Lastly, remove the directory     */

		/*
		if (inst->my_procfs_root)
		{
			remove_proc_entry(inst->procdirname,inst->my_procfs_root);
			inst->my_procfs_root = 0;
		}
		*/
	}

	return result;
}

static int cheesymac_inst_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int result = 0;

	CHEESYMAC_INST_PROC_DATA* procdata = data;
	if (procdata && procdata->inst)
	{
		CHEESYMAC_INSTANCE* inst = procdata->inst;
		char* dest = (page + off);
		int intval = 0;

		switch (procdata->entryid)
		{
			case CHEESYMAC_INST_PROC_TXBITRATE:
				read_lock(&(inst->mac_busy));
				intval = inst->txbitrate;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			case CHEESYMAC_INST_PROC_DEFERTX:
				read_lock(&(inst->mac_busy));
				intval = inst->defertx;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			case CHEESYMAC_INST_PROC_DEFERTXDONE:
				read_lock(&(inst->mac_busy));
				intval = inst->defertxdone;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			case CHEESYMAC_INST_PROC_DEFERRX:
				read_lock(&(inst->mac_busy));
				intval = inst->deferrx;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			case CHEESYMAC_INST_PROC_INCOMING:
				read_lock(&(inst->mac_busy));
				intval = inst->passedup;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			case CHEESYMAC_INST_PROC_PASSED:
				read_lock(&(inst->mac_busy));
				intval = inst->gotthere;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			case CHEESYMAC_INST_PROC_MAXINFLIGHT:
				read_lock(&(inst->mac_busy));
				intval = inst->maxinflight;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			case CHEESYMAC_INST_PROC_RUNNINGMAC:
				read_lock(&(inst->mac_busy));
				//intval = inst->maxinflight;
				char *layers = kmalloc(80, GFP_KERNEL);
				char *tmp = kmalloc(80, GFP_KERNEL);

				int i;
				sprintf(layers, "\0");

				for(i=0; i<MULTIMAC_MAX_MACS; i++)
				{
					if(inst->macs[i]!=NULL)
					{
						sprintf(tmp, "%s", layers);
						if(i>0) sprintf(layers, "%s %s", tmp, inst->macs[i]->name); else sprintf(layers, "%s", inst->macs[i]->name);
					}
				}

				result = snprintf(dest,count,"%s", layers);
				kfree(layers);
				kfree(tmp);
				read_unlock(&(inst->mac_busy));
				*eof = 1;
				break;

			case CHEESYMAC_INST_PROC_RAW_MODE:
				read_lock(&(inst->mac_busy));
				intval = inst->raw_mode;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			default:
				/** Unknown entry -- do something benign   */
				result = 0;
				*eof = 1;
				break;
		}

	}
	return result;
}

static int cheesymac_inst_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int result = 0;
	CHEESYMAC_INST_PROC_DATA* procdata = data;
	
	if (procdata && procdata->inst)
	{
		CHEESYMAC_INSTANCE* inst = procdata->inst;
		static const int maxkdatalen = 256;
		char kdata[maxkdatalen];
		char* endp = 0;
		long intval = 0;

		/** Drag the data over into kernel land     */

		if (maxkdatalen <= count)
		{
			copy_from_user(kdata,buffer,(maxkdatalen-1));
			kdata[maxkdatalen-1] = 0;
			result = (maxkdatalen-1);
		}
		else
		{
			copy_from_user(kdata,buffer,count);
			result = count;
		}

		switch (procdata->entryid)
		{
			case CHEESYMAC_INST_PROC_TXBITRATE:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->txbitrate = intval;
				write_unlock(&(inst->mac_busy));
				break;

			case CHEESYMAC_INST_PROC_DEFERTX:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->defertx = intval;
				write_unlock(&(inst->mac_busy));
				break;

			case CHEESYMAC_INST_PROC_DEFERTXDONE:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->defertxdone = intval;
				write_unlock(&(inst->mac_busy));
				break;

			case CHEESYMAC_INST_PROC_DEFERRX:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->deferrx = intval;
				write_unlock(&(inst->mac_busy));
				break;

			case CHEESYMAC_INST_PROC_MAXINFLIGHT:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->maxinflight = intval;
				write_unlock(&(inst->mac_busy));
				break;

			case CHEESYMAC_INST_PROC_RESETCOUNTERS:
				write_lock(&(inst->mac_busy));
				inst->passedup = 0;
				inst->gotthere = 0;
				write_unlock(&(inst->mac_busy));
				break;

			case CHEESYMAC_INST_PROC_PREFERRED:
				if (result > CU_SOFTMAC_NAME_SIZE)
					result = CU_SOFTMAC_NAME_SIZE;
				kdata[result-1] = 0;
				write_lock(&(inst->mac_busy));
				strncpy(inst->preferred, kdata, CU_SOFTMAC_NAME_SIZE);
				write_unlock(&(inst->mac_busy));
				break;

			case CHEESYMAC_INST_PROC_ADDMAC:
			{
				int i;
				int success=0;
				CU_SOFTMAC_MACLAYER_INFO *mac;

				if (result > CU_SOFTMAC_NAME_SIZE)
					result = CU_SOFTMAC_NAME_SIZE;
				kdata[result-1] = 0;

				printk("Adding mac layer %s %d.\n", kdata, result);

				// Create and fetch a new instance
				mac = cu_softmac_layer_new_instance(kdata);  
				mac = cu_softmac_macinfo_get(mac);

				if (!mac)
				{
					printk("Didn't find %s!\n", kdata);
					break;
				}

				write_lock(&(inst->mac_busy));
				for(i=0; i<MULTIMAC_MAX_MACS; i++)
				{
					if(inst->macs[i]==NULL)
					{
						inst->macs[i] = kmalloc(sizeof(MAC_INSTANCE), GFP_KERNEL);
						memset(inst->macs[i], 0, sizeof(MAC_INSTANCE));
						strncpy((inst->macs[i])->name, mac->name, CU_SOFTMAC_NAME_SIZE);
						(inst->macs[i])->mac = mac;
						(inst->macs[i])->myrxfunc = mac->cu_softmac_mac_packet_rx;
						(inst->macs[i])->mytxfunc = mac->cu_softmac_mac_packet_tx;
						mac->cu_softmac_mac_set_rx_func(mac->mac_private, cu_softmac_netif_rx_packet,inst->netif);
						mac->cu_softmac_mac_attach(mac->mac_private,inst->multimac_fake_phy);

						printk("Added %s to slot %d\n", kdata, i);
						inst->runningmacs++;
						success = 1;
						break;
					} 
				}
				write_unlock(&(inst->mac_busy));

				if(success==0)
					printk("Could not add mac layer. No available slots found.\n");
			}
			break;

			case CHEESYMAC_INST_PROC_DELETEMAC:
				intval = simple_strtol(kdata,&endp,10);
				if (result > CU_SOFTMAC_NAME_SIZE)
					result = CU_SOFTMAC_NAME_SIZE;
				kdata[result-1] = 0;

				write_lock(&(inst->mac_busy));

				if((inst->runningmacs)>=1)
				{
					int i;
					for(i=0; i<MULTIMAC_MAX_MACS; i++)
					{
						if(inst->macs[i]!=NULL)
						{
							if(strncmp((inst->macs[i])->name, kdata, CU_SOFTMAC_NAME_SIZE)==0)
							{
								printk("Removing mac layer %s.\n", (inst->macs[i])->name);

								CU_SOFTMAC_MACLAYER_INFO *mac = (inst->macs[i])->mac;
								mac->cu_softmac_mac_set_rx_func(mac->mac_private, 0, 0);
								mac->cu_softmac_mac_detach(mac->mac_private);
								cu_softmac_macinfo_free(mac);
								kfree(inst->macs[i]);
								inst->macs[i] = NULL;
								inst->runningmacs--;
								break;
							}
						}
					}
				}
				write_unlock(&(inst->mac_busy));
				break;

			case CHEESYMAC_INST_PROC_RAW_MODE:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->raw_mode = intval;
				write_unlock(&(inst->mac_busy));
				break;

			default:
				break;
		}
	}

	else
	{
		result = count;
	}

	return result;
}

static int cu_softmac_mac_attach_cheesymac(void* handle, CU_SOFTMAC_PHYLAYER_INFO* phyinfo)
{
	CHEESYMAC_INSTANCE* inst = handle;
	int result = 0;

	if (inst && phyinfo)
	{
		//printk(KERN_DEBUG "SoftMAC MultiMAC: Attaching to PHY -- getting lock\n");
		write_lock(&(inst->mac_busy));
		//printk(KERN_DEBUG "SoftMAC MultiMAC: Attaching to PHY -- got lock\n");

		if (inst->myphy)
		{
			/** Already attached -- bail out    */
			printk(KERN_ALERT "SoftMAC MultiMAC: Attempting to attach to a phy layer while still attached to a phy layer!\n");
			result = -1;
		}
		else
		{
			/** Set the phy info and then attach a cheesymac instance       */
			inst->myphy = cu_softmac_phyinfo_get(phyinfo);
		}
		//printk(KERN_DEBUG "SoftMAC MultiMAC: Unlocking MAC\n");
		write_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_ALERT "SoftMAC MultiMAC: Invalid MAC/PHY data on attach!\n");
		result = -1;
	}

	return result;
}

static int cu_softmac_mac_detach_cheesymac(void* handle)
{
	CHEESYMAC_INSTANCE* inst = handle;
	int result = 0;

	write_lock(&(inst->mac_busy));
	cu_softmac_phyinfo_free(inst->myphy);
	inst->myphy = 0;

	write_unlock(&(inst->mac_busy));
	return result;
}

static int cu_softmac_mac_set_rx_func_cheesymac(void* mydata,CU_SOFTMAC_MAC_RX_FUNC rxfunc, void* rxpriv)
{
	int result = 0;
	CHEESYMAC_INSTANCE* inst = mydata;

	if (inst)
	{
		write_lock(&(inst->mac_busy));
		inst->myrxfunc = rxfunc;
		inst->myrxfunc_priv = rxpriv;
		write_unlock(&(inst->mac_busy));
	}

	return result;
}

static int cu_softmac_mac_set_unload_notify_func_cheesymac(void* mydata,
						CU_SOFTMAC_MAC_UNLOAD_NOTIFY_FUNC unloadfunc,	void* unloadpriv)
{
	int result = 0;
	CHEESYMAC_INSTANCE* inst = mydata;

	if (inst)
	{
		write_lock(&(inst->mac_busy));
		inst->myunloadnotifyfunc = unloadfunc;
		inst->myunloadnotifyfunc_priv = unloadpriv;
		write_unlock(&(inst->mac_busy));
	}

	return result;
}


/* Get the default parameters used to initialize new MultiMAC instances */
void cu_softmac_cheesymac_get_default_params(CU_SOFTMAC_CHEESYMAC_PARAMETERS* params)
{
	if (params)
	{
		spin_lock(&cheesymac_global_lock);
		params->txbitrate = cheesymac_txbitrate;
		params->defertx = cheesymac_defertx;
		params->defertxdone = cheesymac_defertxdone;
		params->deferrx = cheesymac_deferrx;
		params->maxinflight = cheesymac_maxinflight;
		spin_unlock(&cheesymac_global_lock);
	}
	else
	{
		printk(KERN_DEBUG "SoftMAC MultiMAC: Called get_default_params with null parameters!\n");
	}
}

/* Set the default parameters used to initialize new MultiMAC instances */
void cu_softmac_cheesymac_set_default_params(CU_SOFTMAC_CHEESYMAC_PARAMETERS* params)
{
	if (params)
	{
		spin_lock(&cheesymac_global_lock);
		cheesymac_txbitrate = params->txbitrate;
		cheesymac_defertx = params->defertx;
		cheesymac_defertxdone = params->defertxdone;
		cheesymac_deferrx = params->deferrx;
		cheesymac_maxinflight = params->maxinflight;
		spin_unlock(&cheesymac_global_lock);
	}
	else
	{
		printk(KERN_DEBUG "SoftMAC MultiMAC: Called set_default_params with null parameters!\n");
	}
}

/** Get the parameters of a specific MultiMAC instance */
void cu_softmac_cheesymac_get_instance_params(void* macpriv, CU_SOFTMAC_CHEESYMAC_PARAMETERS* params)
{
	if (macpriv && params)
	{
		CHEESYMAC_INSTANCE* inst = macpriv;
		read_lock(&(inst->mac_busy));
		params->txbitrate = inst->txbitrate;
		params->defertx = inst->defertx;
		params->defertxdone = inst->defertxdone;
		params->deferrx = inst->deferrx;
		params->maxinflight = inst->maxinflight;
		read_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_DEBUG "SoftMAC MultiMAC: Called get_instance_params with bad data!\n");
	}
}

/** Set the parameters of a specific MultiMAC instance */
void cu_softmac_cheesymac_set_instance_params(void* macpriv, CU_SOFTMAC_CHEESYMAC_PARAMETERS* params)
{
	if (macpriv && params)
	{
		CHEESYMAC_INSTANCE* inst = macpriv;
		write_lock(&(inst->mac_busy));
		inst->txbitrate = params->txbitrate;
		inst->defertx = params->defertx;
		inst->defertxdone = params->defertxdone;
		inst->deferrx = params->deferrx;
		inst->maxinflight = params->maxinflight;
		write_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_DEBUG "SoftMAC MultiMAC: Called set_instance_params with bad data!\n");
	}
}

static int __init softmac_cheesymac_init(void)
{
	printk(KERN_DEBUG "Loading MultiMAC\n");

	//cheesymac_procfsroot_handle = proc_mkdir(cheesymac_procfsroot,0);
	//cheesymac_procfsroot_handle->owner = THIS_MODULE;

	strncpy(the_cheesymac.name, "multimac", CU_SOFTMAC_NAME_SIZE);
	the_cheesymac.cu_softmac_layer_new_instance = cu_softmac_cheesymac_new_instance;
	the_cheesymac.cu_softmac_layer_free_instance = cu_softmac_cheesymac_free_instance;
	cu_softmac_layer_register(&the_cheesymac);

	return 0;
}

static void __exit softmac_cheesymac_exit(void)
{
	printk(KERN_DEBUG "Unloading MultiMAC\n");

	/** Tell any/all softmac PHY layers that we're leaving   */
	spin_lock(&cheesymac_global_lock);
	if (!list_empty(&cheesymac_instance_list))
	{
		printk(KERN_DEBUG "MultiMAC: Deleting instances\n");
		CHEESYMAC_INSTANCE* cheesy_instance = 0;
		struct list_head* tmp = 0;
		struct list_head* p = 0;

		/** Walk through all instances, remove from the linked 
		* list and then dispose of them cleanly.     */
		list_for_each_safe(p,tmp,&cheesymac_instance_list)
		{
			cheesy_instance = list_entry(p,CHEESYMAC_INSTANCE,list);
			printk(KERN_DEBUG "MultiMAC: Detaching and destroying instance ID %d\n",cheesy_instance->instanceid);
			cheesymac_destroy_instance(cheesy_instance);
		}
	}
	else
	{
		printk(KERN_DEBUG "MultiMAC: No instances found\n");
	}

	/** Remove the root procfs directory very last of all...   */

	/*
	if (cheesymac_procfsroot_handle)
	{
		remove_proc_entry(cheesymac_procfsroot,0);
		cheesymac_procfsroot_handle = 0;
	}
	*/

	cu_softmac_layer_unregister(&the_cheesymac);
	spin_unlock(&cheesymac_global_lock);
}

module_init(softmac_cheesymac_init);
module_exit(softmac_cheesymac_exit);

EXPORT_SYMBOL(cu_softmac_cheesymac_new_instance);
EXPORT_SYMBOL(cu_softmac_cheesymac_free_instance);
EXPORT_SYMBOL(cu_softmac_cheesymac_set_macinfo_functions);
EXPORT_SYMBOL(cu_softmac_cheesymac_get_default_params);
EXPORT_SYMBOL(cu_softmac_cheesymac_set_default_params);
EXPORT_SYMBOL(cu_softmac_cheesymac_get_instance_params);
EXPORT_SYMBOL(cu_softmac_cheesymac_set_instance_params);

#if 0
/*
 * XXX keeping these around as examples...
 */
static short int myshort = 1;
static int myint = 420;
static long int mylong = 9999;
static char *mystring = "blah";

/* 
 * module_param(foo, int, 0000)
 * The first param is the parameters name
 * The second param is it's data type
 * The final argument is the permissions bits, 
 * for exposing parameters in sysfs (if non-zero) at a later stage.
 */

module_param(myshort, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(myshort, "A short integer");
module_param(myint, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(myint, "An integer");
module_param(mylong, long, S_IRUSR);
MODULE_PARM_DESC(mylong, "A long integer");
module_param(mystring, charp, 0000);
MODULE_PARM_DESC(mystring, "A character string");
#endif

