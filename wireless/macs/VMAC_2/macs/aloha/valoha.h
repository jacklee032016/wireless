/***/

#ifndef __VMAC_ALOHA_H__
#define __VMAC_ALOHA_H__

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

/**
 * @file softmac_cheesymac.h
 *  CheesyMAC: an example of an "Alohaesque" wireless MAC constructed
 * using the VMAC framework.
 *
 * It does not do packet ACKs as required by Aloha. Furthermore, 
 * this version uses Atheros-specific PHY layer API calls.
 */


/*  Basic properties of a CheesyMAC instance. */
typedef struct
{
	/***  802.11 bitrate to use, specified in units of 512 Kb/s.   *
	* These units may seem odd, but match what appears in the 802.11
	* PLCP header. Practically speaking this amounts to 2*(bitrate in Mb/s).
	* So, a "2" here means to use the 1 Mb/s rate, 11 the 5.5 Mb/s rate, 22
	* the 11 Mb/s rate.
	*/
	int32_t txbitrate;

	/***  Do not transmit packets immediately. Queue them and schedule
	* the transmit callback to run as a tasklet.   */
	int32_t defertx;

	/***  Do not delete the sk_buff associated with transmitted packets
	* immediately. Queue them and schedule the "transmit done" callback
	* to run as a tasklet. */
	int32_t defertxdone;

	/***  Do not handle packet reception immediately. Queue received packets
	* and schedule the "receive" callback to run as a tasklet.   */
	int32_t deferrx;

	int32_t maxinflight;
} aloha_param_t;


enum
{ /***  Maximum length of a proc filesystem entry   */
	CHEESYMAC_PROCDIRNAME_LEN = 64,
};

/*  This is the structure containing all of the state information
 * required for each CheesyMAC instance. */
typedef struct 
{
	struct list_head list;

	rwlock_t mac_busy;

	/* the currently attached PHY layer. */
	vmac_phy_t *myphy;
	vmac_phy_t *defaultphy;

	vmac_mac_t *mymac;

	vnet_handle *netif;

	mac_rx_func_t myrxfunc;
	void* myrxfunc_priv;

	mac_unload_notify_func_t myunloadnotifyfunc;
	void* myunloadnotifyfunc_priv;

	/**  We keep a unique ID for each instance we create in order to
	* do things like create separate proc directories for the settings on each one.  */
	int instanceid;

	/* the root procfs directory for this instance */
	struct proc_dir_entry* my_procfs_root;

	char procdirname[CHEESYMAC_PROCDIRNAME_LEN];

	struct proc_dir_entry* my_procfs_dir;

	struct list_head my_procfs_data;

	/* Transmit bitrate encoding to use. */
	unsigned char txbitrate;

	/**  Setting the "defertx" flag results in transmission always
	* being deferred to the "work" callback, even when the packet comes
	* down while in the bottom half. */
	int defertx;

	/*  Setting the "defertxdone" flag causes handling of the "txdone"
	* to always be deferred to the "work" callback.   */
	int defertxdone;

	/*  Setting the "deferrx" flag causes handling of the "txdone"
	* to always be deferred to the "work" callback. */
	int deferrx;

	/**  The "maxinflight" variable determines the maximum number
	* of packets that are allowed to be pending transmission at any given time.   */
	int maxinflight;

	/* The cheesymac uses Linux sk_buff queues when it needs
	* to keep packets around for deferred handling. */

	/** A Linux kernel sk_buff packet queue containing packets
	* whose transmission has been deferred. Generally not an issue
	* since packet transmission in Linux is handled in the bottom half inside of a softirq.
	*/
	struct sk_buff_head tx_skbqueue;

	/**  A Linux kernel sk_buff packet queue containing packets
	* that have been transmitted but not yet handled. Because notification
	* of "transmit complete" often occurs in the top half of an interrupt
	* it may frequently make sense to use this queue in order to limit the
	* amount of work performed with interrupts disabled.  */
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

	int use_crypto;

} valoha_inst_t;


/**
 *  Initial values for global MAC default parameter values.
 * Can override these upon module load.
 */
enum
{
	VALOHA_DEFAULT_DEFERTX = 0,
	VALOHA_DEFAULT_DEFERTXDONE = 1,
	VALOHA_DEFAULT_DEFERRX = 1,
	VALOHA_DEFAULT_MAXINFLIGHT = 256,
	VALOHA_DEFAULT_DEFERALLRX = 0,
	VALOHA_DEFAULT_DEFERALLTXDONE = 0,
	VALOHA_DEFAULT_TXBITRATE = 2,
};

/* **_init.c */
 void valoha_destroy_instance(void* mypriv);

/* **_proc.c */
int valoha_delete_procfs(valoha_inst_t* inst);
int valoha_make_procfs(valoha_inst_t* inst);
extern struct proc_dir_entry* aloha_proc_handle;


/* **_net.c */
void valoha_prep_skb(valoha_inst_t* inst, struct sk_buff* skb);
void valoha_mac_rot128(struct sk_buff* skb);
int valoha_netif(void *mypriv);

/* **_mac_funs.c */
int valoha_detach(void* handle);
int valoha_tx(void* mydata, struct sk_buff* packet,  int intop);
int valoha_set_rx_func(void* mydata,mac_rx_func_t rxfunc, void* rxpriv);
int valoha_set_unload_notify_func(void* mydata, mac_unload_notify_func_t unloadfunc,void* unloadpriv);
void valoha_set_mac_funcs(vmac_mac_t *macinfo);

#endif

