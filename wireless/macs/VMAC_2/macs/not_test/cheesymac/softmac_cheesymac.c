

/*  CheesyMAC: an example of an "Alohaesque" wireless MAC constructed using the VMAC framework. */

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
#include "vmac.h"
#include "vmac_ath.h"
#include "vnetif.h"
#include "softmac_cheesymac.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Neufeld");

/*
** Per-instance data for the MAC
*/

enum
{ /***  Maximum length of a proc filesystem entry   */
	CHEESYMAC_PROCDIRNAME_LEN = 64,
};

/**
 *  This is the structure containing all of the state information
 * required for each CheesyMAC instance.
 */
typedef struct CHEESYMAC_INSTANCE_t {
  /**
   *  Use a Linux kernel linked list to keep track of all instances
   */
  struct list_head list;

  /**
   *  Lock access to MAC when altering basic parameters.
   */
  rwlock_t mac_busy;

  /**
   *  Keep a handle to the currently attached PHY layer.
   */
  vmac_phy_t *myphy;
  vmac_phy_t *defaultphy;

  /**
   *  Pointer to macinfo structure representing this MAC instance
   */
  vmac_mac_t *mymac;

  /**
   *  Network interface
   */
  vnet_handle *netif;

  /**
   *  Keep a pointer to the "packet receive" function
   */
  mac_rx_func_t myrxfunc;
  void* myrxfunc_priv;
  mac_unload_notify_func_t myunloadnotifyfunc;
  void* myunloadnotifyfunc_priv; 

  /**
   *  We keep a unique ID for each instance we create in order to
   * do things like create separate proc directories for the settings
   * on each one.
   */
  int instanceid;

  /**
   *  Keep a handle to the root procfs directory for this instance
   */
  struct proc_dir_entry* my_procfs_root;

  /**
   *  The name of our proc filesystem directory
   */
  char procdirname[CHEESYMAC_PROCDIRNAME_LEN];

  /**
   *  The proc directory entry structure corresponding to our
   * proc filesystem directory.
   */
  struct proc_dir_entry* my_procfs_dir;

  /**
   *  Track all of the proc filesystem entries allocated for
   * this CheesyMAC instance.
   */
  struct list_head my_procfs_data;


  /**
   *  Transmit bitrate encoding to use.
   */
  unsigned char txbitrate;

  /**
   *  Setting the "defertx" flag results in transmission always
   * being deferred to the "work" callback, even when the packet comes
   * down while in the bottom half.
   */
  int defertx;

  /**
   *  Setting the "defertxdone" flag causes handling of the "txdone"
   * to always be deferred to the "work" callback.
   */
  int defertxdone;

  /**
   *  Setting the "deferrx" flag causes handling of the "txdone"
   * to always be deferred to the "work" callback.
   */
  int deferrx;

  /**
   *  The "maxinflight" variable determines the maximum number
   * of packets that are allowed to be pending transmission at any
   * given time.
   */
  int maxinflight;

  /*
   * The cheesymac uses Linux sk_buff queues when it needs
   * to keep packets around for deferred handling.
   */

  /**
   *  A Linux kernel sk_buff packet queue containing packets
   * whose transmission has been deferred. Generally not an issue
   * since packet transmission in Linux is handled in the bottom half
   * inside of a softirq.
   */
  struct sk_buff_head tx_skbqueue;

  /**
   *  A Linux kernel sk_buff packet queue containing packets
   * that have been transmitted but not yet handled. Because notification
   * of "transmit complete" often occurs in the top half of an interrupt
   * it may frequently make sense to use this queue in order to limit the
   * amount of work performed with interrupts disabled.
   */
  struct sk_buff_head txdone_skbqueue;

  /**
   *  A Linux kernel sk_buff packet queue containing packets
   * that have been received but not yet handled. Because notification
   * of packet reception generally occurs in the top half of an interrupt
   * it may frequently make sense to use this queue in order to limit the
   * amount of work performed with interrupts disabled. For example,
   * the Linux cryptographic API functions must <b>not</b> be called from the
   * top half. Decrypting received packets must therefore be deferred
   * to handling in the bottom half, <i>i.e.</i> the "work" callback.
   */
  struct sk_buff_head rx_skbqueue;

  /*
   * CODEX STEP 1 BEGIN
   * Add an instance variable to control use of encryption
   */
  /**
   *  Do a cheesy rot 128 "encryption"
   */
  int use_crypto;
  /*
   * CODEX STEP 1 END
   */

} CHEESYMAC_INSTANCE;

/**
 *  Information about each proc filesystem entry for instance
 * parameters. An array of these will be used to specify
 * the proc entries to create for each MAC instance.
 */
typedef struct {
  /**
   *  Name that will appear in proc directory
   */
  const char* name;
  /**
   *  Read/write permissions, e.g. "0644" means read/write
   * for owner (root), and read-only for group and other users.
   */
  mode_t mode;
  /**
   *  A unique ID passed to the read and write handler functions
   * when data is written to or read from the particular entry.
   */
  int entryid;
} CHEESYMAC_INST_PROC_ENTRY;

/**
 *  Constants for proc entries for each CheesyMAC instance
 */
enum {
  CHEESYMAC_INST_PROC_TXBITRATE,
  CHEESYMAC_INST_PROC_DEFERTX,
  CHEESYMAC_INST_PROC_DEFERTXDONE,
  CHEESYMAC_INST_PROC_DEFERRX,
  CHEESYMAC_INST_PROC_MAXINFLIGHT,
  /*
   * CODEX STEP 2 BEGIN
   * Add a constant for the proc file entry
   */
  /*
   * Allow user to turn encryption on or off
   */
  CHEESYMAC_INST_PROC_USECRYPTO,
  /*
   * CODEX STEP 2 END
   */

  /*
   * CHEESYMAC_INST_PROC_COUNT must be left as the last entry
   */
  CHEESYMAC_INST_PROC_COUNT
};

/**
 *  Proc filesystem entries for each cheesymac instance. The "data"
 * field will be passed in to a generic read/write routine that
 * will use it to determine how to handle the read/write.
 */
static const CHEESYMAC_INST_PROC_ENTRY cheesymac_inst_proc_entries[] = {
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
  /*
   * CODEX STEP 3 BEGIN
   * Add a description of the proc entry
   * See CHEESYMAC_INST_PROC_ENTRY
   */
  /*
   * Allow user to turn encryption on or off
   */
  {
    /*
     * Name of the proc entry
     */
    "usecrypto",
    /*
     * Proc entry file permissions
     */
    0644,
    /*
     * Unique ID passed in to the read/write handlers
     */
    CHEESYMAC_INST_PROC_USECRYPTO
  },
  /*
   * CODEX STEP 3 END
   */
  /*
   * Using this as the "null terminator" for the item list
   */
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
typedef struct {
  struct list_head list;
  CHEESYMAC_INSTANCE* inst;
  int entryid;
  char name[CHEESYMAC_PROCDIRNAME_LEN];
  struct proc_dir_entry* parentdir;  
} CHEESYMAC_INST_PROC_DATA;

/*
**
** Declarations of functions exported via the MAC info table.
** ("public" members of the MAC base class)
**
*/

/**
 *  Notify the MAC layer that it is time to do some work. Exported
 * via pointer as "cu_softmac_work" to the VMAC PHY.
 */
static int
cu_softmac_mac_work_cheesymac(void* mydata, int intop);

/*
 * Notify the MAC layer that a packet has been received -- exported
 * via pointer as "cu_softmac_packet_rx" to the VMAC PHY
 */
static int
cu_softmac_mac_packet_rx_cheesymac(void* mydata,
				   struct sk_buff* packet,
				   int intop);

/**
 *  Notify the MAC layer that a packet transmit has completed.
 * Exported via pointer as "cu_softmac_packet_tx_done" to the VMAC PHY.
 */
static int
cu_softmac_mac_packet_tx_done_cheesymac(void* mydata,
					struct sk_buff* packet,
					int intop);

/** 
 *  Notify the MAC layer that an ethernet-encapsulated packet
 * has been received from up the protocol stack. Exported
 * via pointer as "cu_softmac_packet_tx".
 */
static int
cu_softmac_mac_packet_tx_cheesymac(void* mydata,
				   struct sk_buff* packet,
				   int intop);

/**
 *  Tell the MAC layer to attach to the specified PHY layer.
 */
static int
cu_softmac_mac_attach_cheesymac(void* handle,
				    vmac_phy_t* phyinfo);

/**
 *  Tell the MAC layer to detach from the specified PHY layer.
 */
static int
cu_softmac_mac_detach_cheesymac(void* handle);

static int 
cu_softmac_mac_set_rx_func_cheesymac(void* handle,
				     mac_rx_func_t rxfunc,
				     void* rxpriv);

static int
cu_softmac_mac_set_unload_notify_func_cheesymac(void* handle,
						mac_unload_notify_func_t unloadfunc,

						void* unloadpriv);

/*
**
** Declarations of internal functions ("private" members)
**
*/
static void cheesymac_destroy_instance(void* mypriv);
static CHEESYMAC_INSTANCE *cheesymac_create_instance(vmac_mac_t* macinfo,
						     CU_SOFTMAC_CHEESYMAC_PARAMETERS* params);

static int cheesymac_make_procfs_entries(CHEESYMAC_INSTANCE* inst);
static int cheesymac_delete_procfs_entries(CHEESYMAC_INSTANCE* inst);
static int cheesymac_inst_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data);
static int cheesymac_inst_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data);
/*
 * CODEX STEP 6 BEGIN
 * Declare the "encryption" function
 */
/*
 * Simple "encryption/decryption" function
 */
static void cheesymac_rot128(struct sk_buff* skb);
/*
 * CODEX STEP 6 END
 */

/*
**
** Module parameters
**
*/

/**
 *  Initial values for global MAC default parameter values.
 * Can override these upon module load.
 */
enum {
  CHEESYMAC_DEFAULT_DEFERTX = 0,
  CHEESYMAC_DEFAULT_DEFERTXDONE = 1,
  CHEESYMAC_DEFAULT_DEFERRX = 1,
  CHEESYMAC_DEFAULT_MAXINFLIGHT = 256,
  CHEESYMAC_DEFAULT_DEFERALLRX = 0,
  CHEESYMAC_DEFAULT_DEFERALLTXDONE = 0,
  CHEESYMAC_DEFAULT_TXBITRATE = 2,
};

/**
 *  Keep a reference to the head of our linked list of instances.
 */
static LIST_HEAD(cheesymac_instance_list);

/**
 *  The CheesyMAC
 */
static vmac_layer_t the_cheesymac;

/**
 *  Some operations, e.g. getting/setting the next instance ID
 * and accessing parameters, should be performed "atomically."
 */
static spinlock_t cheesymac_global_lock = SPIN_LOCK_UNLOCKED;

/**
 *  First instance ID to use is 1
 */
static int cheesymac_next_instanceid = 1;

/**
 *  Bitrate encoding to use when transmitting packets.
 */
static int cheesymac_txbitrate = CHEESYMAC_DEFAULT_TXBITRATE;

/**
 *  Whether or not to defer handling of packet transmission.
 */
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

/*
 * Root directory for cheesymac procfs entries
 */
static struct proc_dir_entry* cheesymac_procfsroot_handle = 0;

/*
 * Default network prefix to use for the OS-exported network interface
 * attached to the MAC layer. The unique cheesyMAC ID is used
 * to fill out the "%d" field
 */
static char* cheesymac_netiftemplate = "cheesy%d";

#if __ARM__
MODULE_PARM(cheesymac_defertx, "1i");
MODULE_PARM(cheesymac_defertxdone, "1i");
MODULE_PARM(cheesymac_deferrx, "1i");
MODULE_PARM(cheesymac_maxinflight, "1i");
MODULE_PARM(cheesymac_txbitrate, "1i");
#else
module_param(cheesymac_defertx, int, 0644);
module_param(cheesymac_defertxdone, int, 0644);
module_param(cheesymac_deferrx, int, 0644);
module_param(cheesymac_maxinflight, int, 0644);
module_param(cheesymac_txbitrate, int, 0644);
#endif

MODULE_PARM_DESC(cheesymac_defertx, "Queue packets and defer transmit to tasklet");
MODULE_PARM_DESC(cheesymac_defertxdone, "Queue packets that are finished transmitting and defer handling to tasklet");
MODULE_PARM_DESC(cheesymac_deferrx, "Queue received packets and defer handling to tasklet");
MODULE_PARM_DESC(cheesymac_maxinflight, "Limit the number of packets allowed to be in the pipeline for transmission");
MODULE_PARM_DESC(cheesymac_txbitrate, "Default bitrate to use when transmitting packets");

/*
** VMAC Netif stuff
*/

static int cheesymac_netif_txhelper(vnet_handle nif,
				    void* priv,
				    struct sk_buff* packet) 
{
  CHEESYMAC_INSTANCE* inst = priv;
  if (inst) {
    /*
     * Call the cheesymac tx function. The "intop" indicator
     * is always zero for the particular netif implementation
     * we're using -- in Linux the packet transmission is
     * instigated by a softirq in the bottom half.
     */
    int txresult = cu_softmac_mac_packet_tx_cheesymac(inst,packet,0);
    if (MAC_TX_OK != txresult) {
      dev_kfree_skb(packet);
    }
  }
  return 0;
}

static int cheesymac_netif_unload_helper(vnet_handle netif, 
					 void* priv) 
{
  CHEESYMAC_INSTANCE* inst = priv;
  if (inst) {  
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
    vmac_mac_t *macinfo = inst->mymac;
    int ret = 0;
    struct net_device *checknet = 0;

    checknet = dev_get_by_name(macinfo->name);
    if (checknet) {
      printk(KERN_DEBUG "CheesyMAC: Attaching to %s\n", macinfo->name);
      inst->netif = vmac_get_vnetif(checknet);
      dev_put(checknet);
      vmac_vnetif_set_tx_callback(inst->netif, 
				       cheesymac_netif_txhelper,
				       (void *)inst);
    }
    else {
      printk(KERN_DEBUG "CheesyMAC: Creating %s\n", macinfo->name);
      inst->netif = vmac_create_vnetif(macinfo->name,
						0,
						cheesymac_netif_txhelper,
						inst);
    }
    if (inst->netif) {
	//printk(KERN_DEBUG "CheesyMAC: Setting mac unload notify func\n");
	cu_softmac_mac_set_unload_notify_func_cheesymac(inst,
							vmac_vnetif_detach,
							inst->netif);
	//printk(KERN_DEBUG "CheesyMAC: Setting netif unload callback func\n");
	vmac_netif_set_unload_callback(inst->netif,
					     cheesymac_netif_unload_helper,
					     inst);
	cu_softmac_mac_set_rx_func_cheesymac(inst,
					     vmac_vnetif_rx,
					     inst->netif);
    }
    else {
	printk(KERN_ALERT "CheesyMAC: Unable to attach to netif!\n");
	/*
	 * Whack the cheesymac instance we just created
	 */
	cheesymac_destroy_instance(inst);
	ret = -1;
    }
    return ret;
}

static void cu_softmac_cheesymac_prep_skb(CHEESYMAC_INSTANCE* inst, struct sk_buff* skb)
{
  if (inst && (inst->myphy != inst->defaultphy)) {
    /*
     * XXX use of atheros-specific PHY calls!!!
     */
      vmac_ath_set_default_phy_props(inst->myphy->phy_private, skb);
      vmac_ath_set_tx_bitrate(inst->myphy->phy_private, skb, inst->txbitrate);    
  }
}

/**
 * \brief Implementation of "mac_tx"
 * cu_softmac_mac_packet_tx_cheesymac
 *
 */
static int cu_softmac_mac_packet_tx_cheesymac(void* mydata,
					      struct sk_buff* packet,
					      int intop)
{
  CHEESYMAC_INSTANCE* inst = mydata;
  int status = MAC_TX_OK;
  int txresult = VMAC_PHY_SENDPACKET_OK;

  if (inst) {
    read_lock(&(inst->mac_busy));
    /*
     * Check to see if we're in the top half or bottom half, i.e. are
     * we taking an interrupt right now?
     */
    if (intop) {
      /*
       * As a general rule, try to keep the stuff you do in the top
       * half to a minimum. This is during the immediate handling
       * of an interrupt and the place for time-critical tasks to
       * occur.
       */
      if ((inst->defertx || inst->use_crypto) && packet) {
	/*
	 * Queue the packet in tx_skbqueue, schedule the "work" method
	 * to run.
	 */
	skb_queue_tail(&(inst->tx_skbqueue),packet);
	(inst->myphy->phy_schedule_work_asap)(inst->myphy->phy_private);
	status = MAC_TX_OK;
      }
      else if (packet) {
	/*
	 * Send the packet now
	 */
	cu_softmac_cheesymac_prep_skb(inst, packet);
	txresult = (inst->myphy->phy_sendpacket)(inst->myphy->phy_private,
							    inst->maxinflight, 
							    packet);
	if (VMAC_PHY_SENDPACKET_OK != txresult) {
	  printk(KERN_ALERT "VMAC CheesyMAC: top half packet tx failed: %d\n",txresult);
	}

	status = MAC_TX_OK;
      }
    }
    else {
      struct sk_buff* skb = 0;
      /*
       * NOT in top half -- process our transmit queue
       */
      if (packet) {
	skb_queue_tail(&(inst->tx_skbqueue),packet);
      }
      /*
       * Walk our transmit queue, shovelling out packets as we go...
       */
      while ((skb = skb_dequeue(&(inst->tx_skbqueue)))) {
	/*
	 * CODEX STEP 8 BEGIN
	 * Call encryption when send function is in bottom half
	 */
	/*
	 * "Encrypt" packets on the way out if applicable.
	 */
	if (inst->use_crypto) {
	  cheesymac_rot128(skb);
	}
	/*
	 * CODEX STEP 8 END
	 */
	cu_softmac_cheesymac_prep_skb(inst, packet);
	txresult = (inst->myphy->phy_sendpacket)(inst->myphy->phy_private, 
							    inst->maxinflight, 
							    packet);
	if (VMAC_PHY_SENDPACKET_OK != txresult) {
	  printk(KERN_ALERT "VMAC CheesyMAC: tasklet packet tx failed: %d\n",txresult);
	  /*
	   * N.B. we return an "OK" for the transmit because
	   * we're handling the sk_buff freeing down here --
	   * as far as the caller is concerned we were successful.
	   */
	}
      }
    }
    read_unlock(&(inst->mac_busy));
  }
  else {
    /*
     * Could not get our instance handle -- let the caller know...
     */
    printk(KERN_ALERT "CheesyMAC: packet_tx -- no instance handle!\n");
    status = MAC_TX_FAIL;
  }

  return status;
}

static int cu_softmac_mac_packet_tx_done_cheesymac(void* mydata,
						   struct sk_buff* packet,
						   int intop) 
{
  int status = VMAC_MAC_NOTIFY_OK;
  CHEESYMAC_INSTANCE* inst = mydata;

  if (inst) {
    read_lock(&(inst->mac_busy));
    /*
     * Check to see if we're supposed to defer handling
     */
    if (intop) {
      if (inst->defertxdone && packet) {
	/*
	 * Queue the packet in txdone_skbqueue, schedule the tasklet
	 */
	skb_queue_tail(&(inst->txdone_skbqueue), packet);
	status = VMAC_MAC_NOTIFY_RUNAGAIN;
      }
      else if (packet) {
	/*
	 * Free the packet immediately, do not run again
	 */
	(inst->myphy->phy_free_skb)(inst->myphy->phy_private, packet);
	packet = 0;
	status = VMAC_MAC_NOTIFY_OK;
      }
    }
    else {
      struct sk_buff* skb = 0;
      /*
       * In bottom half -- process any deferred packets
       */
      if (packet) {
	skb_queue_tail(&(inst->txdone_skbqueue), packet);
      }
      while ((skb = skb_dequeue(&(inst->txdone_skbqueue)))) {
	(inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
      }
    }
    read_unlock(&(inst->mac_busy));
  }
  else {
    printk(KERN_ALERT "CheesyMAC: packet_tx_done -- no instance handle!\n");
    status = VMAC_MAC_NOTIFY_HOSED;
  }

  return status;
}


static int cu_softmac_mac_packet_rx_cheesymac(void* mydata,
					      struct sk_buff* packet,
					      int intop) 
{
  CHEESYMAC_INSTANCE* inst = mydata;
  int status = VMAC_MAC_NOTIFY_OK;

  // XXX check to see if MAC is active...
  if (inst) {
    read_lock(&(inst->mac_busy));
    //printk(KERN_DEBUG "cheesymac: packet rx\n");
    if (intop) {
      /*
       * We defer handling to the bottom half if we're either
       * told to explicitly, or if we're using encryption.
       */
      if ((inst->deferrx || inst->use_crypto) && packet) {
	/*
	 * Queue packet for later processing
	 */
	//printk(KERN_DEBUG "cheesymac: packet rx deferring\n");
	skb_queue_tail(&(inst->rx_skbqueue), packet);
	status = VMAC_MAC_NOTIFY_RUNAGAIN;
      }
      else if (packet) {
	/*
	 * Just send the packet up the network stack if we've got
	 * an attached rxfunc, otherwise whack the packet.
	 */
	if (inst->myrxfunc) {
	  //printk(KERN_DEBUG "cheesymac: packet rx -- calling receive\n");
	  (inst->myrxfunc)(inst->myrxfunc_priv, packet);
	}
	else { //if (inst->myphy->cu_softmac_free_skb) {
	  //printk(KERN_DEBUG "cheesymac: packet rx -- freeing skb\n");
	  (inst->myphy->phy_free_skb)(inst->myphy->phy_private, packet);
	}
	//else {
	//printk(KERN_DEBUG "cheesymac: packet rx -- mac hosed\n");
	//status = VMAC_MAC_NOTIFY_HOSED;
	//}
      }
    }
    else {
      struct sk_buff* skb = 0;
      /*
       * Not in top half - walk our rx queue and send packets
       * up the stack
       */
      //printk(KERN_DEBUG "cheesymac: packet rx -- bottom half\n");
      if (packet) {
	skb_queue_tail(&(inst->rx_skbqueue), packet);
      }
      while ((skb = skb_dequeue(&(inst->rx_skbqueue)))) {
	if (inst->myrxfunc) {
	  /*
	   * CODEX STEP 10 BEGIN
	   * Decrypt packets in bottom half of receive function
	   */
	  /*
	   * Decrypt packet if applicable
	   */
	  if (inst->use_crypto) {
	    cheesymac_rot128(skb);
	  }
	  /*
	   * CODEX STEP 10 END
	   */
	  (inst->myrxfunc)(inst->myrxfunc_priv, packet);
	}
	else {
	  /*
	   * No rx function available? Ask phy layer to free the packet.
	   */
	  (inst->myphy->phy_free_skb)(inst->myphy->phy_private, packet);
	}
      }
    }
    read_unlock(&(inst->mac_busy));
  }
  else {
    printk(KERN_ALERT "CheesyMAC: packet_rx -- no instance handle!\n");
    status = VMAC_MAC_NOTIFY_HOSED;
  }

  return status;
}

static int cu_softmac_mac_work_cheesymac(void* mydata, int intop) 
{
  CHEESYMAC_INSTANCE* inst = mydata;
  int status = VMAC_MAC_NOTIFY_OK;

  if (inst) {
    int txresult;
    struct sk_buff* skb = 0;
    /*
     * Walk our receive queue, pumping packets up to the system as we go...
     */
    while ((skb = skb_dequeue(&(inst->rx_skbqueue)))) {
      if (inst->myrxfunc) {
	//printk(KERN_DEBUG "cheesymac: have rxfunc -- receiving\n");
	/*
	 * CODEX STEP 11 BEGIN
	 * Decrypt incoming packets in "work" function
	 */
	/*
	 * Decrypt packets if applicable
	 */
	if (inst->use_crypto) {
	  cheesymac_rot128(skb);
	}
	/*
	 * CODEX STEP 11 END
	 */
	(inst->myrxfunc)(inst->myrxfunc_priv,skb);
      }
      else {
	printk(KERN_DEBUG "cheesymac: packet rx -- no rxfunc freeing skb\n");
	(inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
      }
    }

    /*
     * Walk our transmit queue, shovelling out packets as we go...
     */
    while ((skb = skb_dequeue(&(inst->tx_skbqueue)))) {
      cu_softmac_cheesymac_prep_skb(inst, skb);
      /*
       * CODEX STEP 9 BEGIN
       * In bottom half, so encrypt packets on their way out if applicable
       */
      /*
       * "Encrypt" packets on the way out
       */
      if (inst->use_crypto) {
	cheesymac_rot128(skb);
      }
      /*
       * CODEX STEP 9 END
       */
      txresult = (inst->myphy->phy_sendpacket)(inst->myphy->phy_private, 
							  inst->maxinflight,
							  skb);
      if (VMAC_PHY_SENDPACKET_OK != txresult) {
	printk(KERN_ALERT "VMAC CheesyMAC: work packet tx failed: %d\n",txresult);
      }
    }

    /*
     * Walk through the "transmit done" queue, freeing packets as we go...
     */
    while ((skb = skb_dequeue(&(inst->txdone_skbqueue)))) {
      (inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
    }
  }
  else {
    status = VMAC_MAC_NOTIFY_HOSED;
  }
  
  return status;
}

void cu_softmac_cheesymac_set_macinfo_functions(vmac_mac_t *macinfo) 
{
  // XXX lock
  macinfo->mac_tx = cu_softmac_mac_packet_tx_cheesymac;
  macinfo->mac_tx_done = cu_softmac_mac_packet_tx_done_cheesymac;
  macinfo->rx_packet = cu_softmac_mac_packet_rx_cheesymac;
  macinfo->mac_work = cu_softmac_mac_work_cheesymac;
  macinfo->attach_phy = cu_softmac_mac_attach_cheesymac;
  macinfo->detach_phy = cu_softmac_mac_detach_cheesymac;
  macinfo->set_rx_func = cu_softmac_mac_set_rx_func_cheesymac;
  macinfo->set_unload_notify_func = cu_softmac_mac_set_unload_notify_func_cheesymac;
}

static CHEESYMAC_INSTANCE *cheesymac_create_instance(vmac_mac_t* macinfo,
						     CU_SOFTMAC_CHEESYMAC_PARAMETERS* params) 
{
  CHEESYMAC_INSTANCE* inst;

  /*
   * Create a new instance,
   * make it part of cheesymac_instance_list linked list,
   * return it
   */

  inst = kmalloc(sizeof(CHEESYMAC_INSTANCE), GFP_ATOMIC);
  if (inst) {
    memset(inst, 0, sizeof(CHEESYMAC_INSTANCE));

    /* lock the inst before showing it to anyone else */
    inst->mac_busy = RW_LOCK_UNLOCKED;
    write_lock(&(inst->mac_busy));

    /* add new instance to list */
    INIT_LIST_HEAD(&inst->list);
    list_add_tail(&inst->list, &cheesymac_instance_list);
    INIT_LIST_HEAD(&inst->my_procfs_data);

    /* fire up the instance... */
    inst->defaultphy = vmac_alloc_phy();
    inst->myphy = inst->defaultphy;
    inst->mymac = macinfo;

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
    if (params) {
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
    
    /* finish macinfo setup */
    macinfo->layer = &the_cheesymac;
    macinfo->mac_private = inst;
    snprintf(macinfo->name, VMAC_NAME_SIZE, cheesymac_netiftemplate, inst->instanceid);

    /* release write lock */
    write_unlock(&(inst->mac_busy));    

    /* create and attach to our Linux network interface */
    cheesymac_create_and_attach_netif(inst);

  }
  else {
    printk(KERN_ALERT "CheesyMAC create_instance: Unable to allocate memory!\n");
  }

  return inst;
}

/*
 * Detach/delete this cheesymac instance
 */
static void cheesymac_destroy_instance(void* mypriv) 
{
    CHEESYMAC_INSTANCE* inst = mypriv;    
    struct sk_buff* skb = 0;
    
    if (inst) {
	list_del(&(inst->list));
	
	/* detach from phy */
	cu_softmac_mac_detach_cheesymac(mypriv);

	/* Notify folks that we're going away... */
	if (inst->myunloadnotifyfunc) {
	    (inst->myunloadnotifyfunc)(inst->myunloadnotifyfunc_priv);
	}
	
	/*
	 * Now lock the instance and shut it down
	 */
	write_lock(&(inst->mac_busy));
	
	/* remove procfs entries */
	cheesymac_delete_procfs_entries(inst);
	
	/* Drain queues */
	while ((skb = skb_dequeue(&(inst->tx_skbqueue)))) {
	    (inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
	}
	while ((skb = skb_dequeue(&(inst->txdone_skbqueue)))) {
	    (inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
	}
	while ((skb = skb_dequeue(&(inst->rx_skbqueue)))) {
	    (inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
	}
	
	inst->mymac = 0;
	inst->myphy = 0;
	vmac_free_phy(inst->defaultphy);
	inst->defaultphy = 0;

	write_unlock(&(inst->mac_busy));

	kfree(inst);
    }
}

void *cu_softmac_cheesymac_new_instance(void *layer_priv)
{
    vmac_mac_t *macinfo;
    CHEESYMAC_INSTANCE *inst;

    macinfo = vmac_alloc_mac();
    if (!macinfo) {
	return 0;
    }

    inst = cheesymac_create_instance(macinfo, 0);
    if (!inst) {
	vmac_free_mac(macinfo);
	return 0;
    }

    /* write lock */
    write_lock(&(inst->mac_busy));

    /* setup macinfo and register it with softmac */
    vmac_register_mac(macinfo);

    /* create procfs entries -- must come after vmac_register_mac */
    inst->my_procfs_root = cheesymac_procfsroot_handle;
    cheesymac_make_procfs_entries(inst);

    /* instances don't keep references to themselves */
    vmac_free_mac(macinfo);

    /* release write lock */
    write_unlock(&(inst->mac_busy));

    return macinfo;
}

void cu_softmac_cheesymac_free_instance(void *layer_priv, void *info)
{
    vmac_mac_t *macinfo = info;
    CHEESYMAC_INSTANCE *inst = macinfo->mac_private;
    cheesymac_destroy_instance(inst);
}
  
static int cheesymac_make_procfs_entries(CHEESYMAC_INSTANCE* inst) 
{
  int result = 0;

  if (inst) {
    int i = 0;
    struct proc_dir_entry* curprocentry = 0;
    CHEESYMAC_INST_PROC_DATA* curprocdata = 0;
    vmac_mac_t *macinfo = inst->mymac;

    /* root directory created by softmac_core */
    inst->my_procfs_dir = macinfo->proc;

    /*
     * Make individual entries. Stop when we get either a null string
     * or an empty string for a name.
     */
    i = 0;
    while (cheesymac_inst_proc_entries[i].name && cheesymac_inst_proc_entries[i].name[0]) {
      //printk(KERN_ALERT "CheesyMAC: Creating proc entry %s, number %d\n",cheesymac_inst_proc_entries[i].name,i);
      curprocentry = create_proc_entry(cheesymac_inst_proc_entries[i].name,
				       cheesymac_inst_proc_entries[i].mode,
				       inst->my_procfs_dir);
      curprocentry->owner = THIS_MODULE;

      /*
       * Allocate and fill out a proc data structure, add it
       * to the linked list for the instance.
       */
      curprocdata = kmalloc(sizeof(CHEESYMAC_INST_PROC_DATA),GFP_ATOMIC);
      INIT_LIST_HEAD((&curprocdata->list));
      list_add_tail(&(curprocdata->list),&(inst->my_procfs_data));
      curprocdata->inst = inst;
      curprocdata->entryid = cheesymac_inst_proc_entries[i].entryid;
      strncpy(curprocdata->name,cheesymac_inst_proc_entries[i].name,CHEESYMAC_PROCDIRNAME_LEN);
      curprocdata->parentdir = inst->my_procfs_dir;

      /*
       * Hook up the new proc entry data
       */
      curprocentry->data = curprocdata;

      /*
       * Set read/write functions for the proc entry.
       */
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
  if (inst) {
    struct list_head* tmp = 0;
    struct list_head* p = 0;
    CHEESYMAC_INST_PROC_DATA* proc_entry_data = 0;

    /*
     * Remove individual entries and delete their data
     */
    list_for_each_safe(p,tmp,&inst->my_procfs_data) {
      proc_entry_data = list_entry(p,CHEESYMAC_INST_PROC_DATA,list);
      list_del(p);
      remove_proc_entry(proc_entry_data->name,proc_entry_data->parentdir);
      kfree(proc_entry_data);
      proc_entry_data = 0;
    }

  }
  return result;
}

static int
cheesymac_inst_read_proc(char *page, char **start, off_t off,
			 int count, int *eof, void *data) {
  int result = 0;
  CHEESYMAC_INST_PROC_DATA* procdata = data;
  if (procdata && procdata->inst) {
    CHEESYMAC_INSTANCE* inst = procdata->inst;
    char* dest = (page + off);
    int intval = 0;

    switch (procdata->entryid) {
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
    case CHEESYMAC_INST_PROC_MAXINFLIGHT:
      read_lock(&(inst->mac_busy));
      intval = inst->maxinflight;
      read_unlock(&(inst->mac_busy));
      result = snprintf(dest,count,"%d\n",intval);
      *eof = 1;
      break;

      /*
       * CODEX STEP 4 BEGIN
       * Add a read handler for the proc file entry
       */
      /*
       * Allow user to check encryption status
       */
    case CHEESYMAC_INST_PROC_USECRYPTO:
      read_lock(&(inst->mac_busy));
      intval = inst->use_crypto;
      read_unlock(&(inst->mac_busy));
      result = snprintf(dest,count,"%d\n",intval);
      *eof = 1;
      break;
      /*
       * CODEX STEP 4 END
       */

    default:
      /*
       * Unknown entry -- do something benign
       */
      result = 0;
      *eof = 1;
      break;
    }
  }
  return result;
}

static int
cheesymac_inst_write_proc(struct file *file, const char __user *buffer,
			  unsigned long count, void *data) {
  int result = 0;
  CHEESYMAC_INST_PROC_DATA* procdata = data;
  if (procdata && procdata->inst) {
    CHEESYMAC_INSTANCE* inst = procdata->inst;
    static const int maxkdatalen = 256;
    char kdata[maxkdatalen];
    char* endp = 0;
    long intval = 0;

    /*
     * Drag the data over into kernel land
     */
    if (maxkdatalen <= count) {
      copy_from_user(kdata,buffer,(maxkdatalen-1));
      kdata[maxkdatalen-1] = 0;
      result = (maxkdatalen-1);
    }
    else {
      copy_from_user(kdata,buffer,count);
      result = count;
    }

    switch (procdata->entryid) {
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
      /*
       * CODEX STEP 5 BEGIN
       * Add a write handler for the proc entry
       */
      /*
       * Allow user turn encryption on or off
       */
    case CHEESYMAC_INST_PROC_USECRYPTO:
      intval = simple_strtol(kdata,&endp,10);
      write_lock(&(inst->mac_busy));
      inst->use_crypto = intval;
      write_unlock(&(inst->mac_busy));
      break;
      /*
       * CODEX STEP 5 END
       */
    default:
      break;
    }
  }
  else {
    result = count;
  }

  return result;
}

/*
 * CODEX STEP 7 BEGIN
 * Implement the "encryption" function
 */
/*
 * Simple "encryption/decryption" function
 */
static void cheesymac_rot128(struct sk_buff* skb)
{
	int i = 0;
	for (i=0;i<skb->len;i++)
	{
		skb->data[i] = ((skb->data[i] + 128) % 256);
	}
}
/*
 * CODEX STEP 7 END
 */

static int
cu_softmac_mac_attach_cheesymac(void* handle,
				    vmac_phy_t* phyinfo) {
  CHEESYMAC_INSTANCE* inst = handle;
  int result = 0;

  if (inst && phyinfo) {
      cu_softmac_mac_detach_cheesymac(handle);
      write_lock(&(inst->mac_busy));
      inst->myphy = vmac_get_phy(phyinfo);
      write_unlock(&(inst->mac_busy));
  }
  else {
      printk(KERN_ALERT "VMAC CheesyMAC: Invalid MAC/PHY data on attach!\n");
      result = -1;
  }

  return result;
}

static int
cu_softmac_mac_detach_cheesymac(void* handle) {
  CHEESYMAC_INSTANCE* inst = handle;
  int result = 0;

  if (inst) {
      write_lock(&(inst->mac_busy));
      if (inst->myphy != inst->defaultphy) {
	  vmac_free_phy(inst->myphy);
	  inst->myphy = inst->defaultphy;
      }
      write_unlock(&(inst->mac_busy));
  } else {
      printk(KERN_ALERT "VMAC CheesyMAC: Invalid MAC/PHY data on detach!\n");
      result = -1;
  }
  return result;
}

static int 
cu_softmac_mac_set_rx_func_cheesymac(void* mydata,
				     mac_rx_func_t rxfunc,
				     void* rxpriv) {
  int result = 0;
  CHEESYMAC_INSTANCE* inst = mydata;
  if (inst) {
    write_lock(&(inst->mac_busy));  
    inst->myrxfunc = rxfunc;
    inst->myrxfunc_priv = rxpriv;
    write_unlock(&(inst->mac_busy));  
  }

  return result;
}

static int
cu_softmac_mac_set_unload_notify_func_cheesymac(void* mydata,
						mac_unload_notify_func_t unloadfunc,
						void* unloadpriv) {
  int result = 0;
  CHEESYMAC_INSTANCE* inst = mydata;
  if (inst) {
    write_lock(&(inst->mac_busy));  
    inst->myunloadnotifyfunc = unloadfunc;
    inst->myunloadnotifyfunc_priv = unloadpriv;
    write_unlock(&(inst->mac_busy));  
  }

  return result;
}


/*
 * Get the default parameters used to initialize new CheesyMAC instances
 */
void
cu_softmac_cheesymac_get_default_params(CU_SOFTMAC_CHEESYMAC_PARAMETERS* params) {
  if (params) {
    spin_lock(&cheesymac_global_lock);
    params->txbitrate = cheesymac_txbitrate;
    params->defertx = cheesymac_defertx;
    params->defertxdone = cheesymac_defertxdone;
    params->deferrx = cheesymac_deferrx;
    params->maxinflight = cheesymac_maxinflight;
    spin_unlock(&cheesymac_global_lock);
  }
  else {
    printk(KERN_DEBUG "VMAC CheesyMAC: Called get_default_params with null parameters!\n");
  }
}

/*
 * Set the default parameters used to initialize new CheesyMAC instances
 */
void
cu_softmac_cheesymac_set_default_params(CU_SOFTMAC_CHEESYMAC_PARAMETERS* params) {
  if (params) {
    spin_lock(&cheesymac_global_lock);
    cheesymac_txbitrate = params->txbitrate;
    cheesymac_defertx = params->defertx;
    cheesymac_defertxdone = params->defertxdone;
    cheesymac_deferrx = params->deferrx;
    cheesymac_maxinflight = params->maxinflight;
    spin_unlock(&cheesymac_global_lock);
  }
  else {
    printk(KERN_DEBUG "VMAC CheesyMAC: Called set_default_params with null parameters!\n");
  }
}

/*
 * Get the parameters of a specific CheesyMAC instance
 */
void
cu_softmac_cheesymac_get_instance_params(void* macpriv,
					 CU_SOFTMAC_CHEESYMAC_PARAMETERS* params) {
  if (macpriv && params) {
    CHEESYMAC_INSTANCE* inst = macpriv;
    read_lock(&(inst->mac_busy));
    params->txbitrate = inst->txbitrate;
    params->defertx = inst->defertx;
    params->defertxdone = inst->defertxdone;
    params->deferrx = inst->deferrx;
    params->maxinflight = inst->maxinflight;    
    read_unlock(&(inst->mac_busy));
  }
  else {
    printk(KERN_DEBUG "VMAC CheesyMAC: Called get_instance_params with bad data!\n");
  }
}

/*
 * Set the parameters of a specific CheesyMAC instance
 */
void
cu_softmac_cheesymac_set_instance_params(void* macpriv,
					 CU_SOFTMAC_CHEESYMAC_PARAMETERS* params) {
  if (macpriv && params) {
    CHEESYMAC_INSTANCE* inst = macpriv;
    write_lock(&(inst->mac_busy));
    inst->txbitrate = params->txbitrate;
    inst->defertx = params->defertx;
    inst->defertxdone = params->defertxdone;
    inst->deferrx = params->deferrx;
    inst->maxinflight = params->maxinflight;
    write_unlock(&(inst->mac_busy));
  }
  else {
    printk(KERN_DEBUG "VMAC CheesyMAC: Called set_instance_params with bad data!\n");
  }
}

static int __init softmac_cheesymac_init(void)
{
	printk(KERN_DEBUG "Loading CheesyMAC\n");

	strncpy(the_cheesymac.name, "cheesymac", VMAC_NAME_SIZE);
	the_cheesymac.new_instance = cu_softmac_cheesymac_new_instance;
	the_cheesymac.destroy_instance = cu_softmac_cheesymac_free_instance;

	vmac_layer_register(&the_cheesymac);
	cheesymac_procfsroot_handle = the_cheesymac.proc;

	return 0;
}

static void __exit softmac_cheesymac_exit(void)
{
  printk(KERN_DEBUG "Unloading CheesyMAC\n");
  /*
   * Tell any/all softmac PHY layers that we're leaving
   */
  spin_lock(&cheesymac_global_lock);
  if (!list_empty(&cheesymac_instance_list)) {
    printk(KERN_DEBUG "CheesyMAC: Deleting instances\n");
    CHEESYMAC_INSTANCE* cheesy_instance = 0;
    struct list_head* tmp = 0;
    struct list_head* p = 0;
    
    /*
     * Walk through all instances, remove from the linked 
     * list and then dispose of them cleanly.
     */
    list_for_each_safe(p,tmp,&cheesymac_instance_list) {
      cheesy_instance = list_entry(p,CHEESYMAC_INSTANCE,list);
      printk(KERN_DEBUG "CheesyMAC: Detaching and destroying instance ID %d\n",cheesy_instance->instanceid);
      cheesymac_destroy_instance(cheesy_instance);
    }
  }
  else {
    printk(KERN_DEBUG "CheesyMAC: No instances found\n");
  }
  
  vmac_layer_unregister(&the_cheesymac);

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
