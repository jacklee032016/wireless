/*
* $Id: vmac.h,v 1.1.1.1 2006/11/30 17:01:32 lizhijie Exp $
*/

#ifndef __VMAC_H__
#define __VMAC_H__

/** 
 *  The main VMAC header file.
 *
 * This file includes definitions for the functions, types, and constants
 * used by MAC and PHY layers in the VMAC system.
 */

/** \mainpage VMAC
 * 
 * \section intro_sec What is VMAC?
 *
 * VMAC is an abstraction designed to facilitate the creation
 * of custom and experimental MAC layers when used with Software
 * Defined Radio hardware. A traditional network interface system
 * is structured with a tightly coupled MAC and PHY layer exporting
 * a standard API to the operating system.
 *

  Network interface,
     e.g. eth0
       |
       |
 ++++++++++++++
+              +
+  OS Network  + 
+  Interface   +
+    Layer     +
+              +
 ++++++++++++++
       | 
 ++++++++++++++
+              +
+   MAC/PHY    +
+   software   +
+  driver and  +
+ network card + 
+  hardware    +
+              +    
 ++++++++++++++
       |
       |
 Network Medium

 *
 * This structure permits the easy swapping of underlying network
 * hardware. Two different brands of ethernet card will both show up
 * as eth0 on a system, and as long as the drivers and hardware
 * both function properly an application doesn't have to concern
 * itself with which network card is being used.
 *

 *
 * The VMAC breaks out the integrated MAC and PHY layer
 * into separate (but still coupled) components.
 *

  Network interface,
     e.g. eth0
       |
       |
 ++++++++++++++
+              +
+  OS Network  + 
+  Interface   +
+    Layer     +
+              +
 ++++++++++++++
       |
 ++++++++++++++
+              +
+ VMAC MAC  + 
+    Layer     +
+              +
 ++++++++++++++
       |
 ++++++++++++++
+              +
+ VMAC PHY  + 
+    Layer     +
+              +
 ++++++++++++++
       |
       |
 Network Medium

 * 
 * This structure affords even greater flexibility than the traditional
 * network stack when used with appropriately capable hardware. Most MAC
 * layers are still implemented either wholly or partially by the underlying
 * hardware and are immutable. With such hardware the VMAC abstraction
 * makes no sense, and serves only as an extra layer of inefficiency.
 * However, recent advances in <i>Software Defined Radio</i> technology
 * have resulted in much more flexible network hardware. Fully exploiting
 * the capabilities of this hardware requires a more flexible network
 * stack. VMAC is designed to strike a balance between flexibility
 * and efficiency that will permit experimentation with this hardware.
 *
 * \section sdr_sec Software Defined Radio
 *
 * A Software Defined Radio, or <i>SDR</i>, ...
 */


#if __ARM__
	#define __user		
	
	#define MYLIST_HEAD							struct list_head
	#define MYLIST_NODE							struct list_head

	#define MYLIST_INIT_LIST_HEAD 				INIT_LIST_HEAD
	#define MYLIST_FOR_EACH					list_for_each
	#define MYLIST_FOR_EACH_SAFE				list_for_each_safe			/* 3 parameters */
	#define MYLIST_FOR_EACH_ENTRY_SAFE		list_for_each_entry_safe	/* 4 parameters */
	#define MYLIST_ENTRY						list_entry
	#define MYLIST_ADD_HEAD					list_add
	#define MYLIST_DEL							list_del

	#define MYLIST_CHECK_EMPTY					list_empty
#else
	#define MYLIST_HEAD							struct hlist_head
	#define MYLIST_NODE							struct hlist_node

	#define MYLIST_INIT_LIST_HEAD  				INIT_HLIST_HEAD
	#define MYLIST_FOR_EACH					hlist_for_each
	#define MYLIST_FOR_EACH_SAFE				hlist_for_each_safe
	#define MYLIST_FOR_EACH_ENTRY_SAFE		hlist_for_each_entry_safe
	#define MYLIST_ENTRY						hlist_entry
	#define MYLIST_ADD_HEAD					hlist_add_head
	#define MYLIST_DEL							hlist_del

	#define MYLIST_CHECK_EMPTY					hlist_empty
#endif


/*
** VMAC PHY Layer Functions
 */

/**
 *  Forward declaration of the structs containing layer information
 */
struct vmac_layer;
struct vmac_mac;
struct vmac_phy;

typedef void* vmac_phy_handle;
typedef void* vmac_mac_handle;

/**
 *  Maximum size of a MAC,PHY,LAYER name in VMAC
 */
#define VMAC_NAME_SIZE 			64

/***  Functions and variables exported by ALL layer classes */
typedef struct vmac_layer
{
	/***  Layer name.  like "cheesymac" or "ath"     */
	char name[VMAC_NAME_SIZE];

	/***  Create a new instance of this layer type     */
	void *(*new_instance)(void*);

	/***  Destroy an existing instance of this layer type     */
	void (*destroy_instance)(void*, void *inst);

	/***  Private data     */
	void *layer_private;
	void *proc;

	/* softmac internal */
	MYLIST_HEAD 		name_hlist;
} vmac_layer_t;


/***  Functions/variables exported by PHY layer. */
typedef struct vmac_phy
{
	/***  PHY layer instance name.  like "ath0"     */
	char name[VMAC_NAME_SIZE];

	/***  Layer information     */
	vmac_layer_t *layer;

	/***  Attach a MAC layer to the PHY layer     */
	int (*attach_mac)(void *, struct vmac_mac *macinfo);

	/***  Detach a MAC layer from the PHY layer
	*
	* Tell the softmac PHY that we are leaving the building. The semantics
	* of this call are such that *after* it returns the VMAC PHY won't
	* make any new calls into the MAC layer.     */
	void (*detach_mac)(void*);

	/***  Get current time
	*
	* This function is grouped with the PHY layer, even though it may
	* appear to be more of an OS-related function. However, some PHY layers
	* may have their own high-precision clocks that the MAC layer should
	* be using. For example, the Atheros PHY layer has such a clock.
	*/
	u_int64_t (*phy_get_time)(void*);

	/***  Set current time     */
	void (*phy_set_time)(void*,u_int64_t time);

	/***  Schedule the MAC layer <i>work</i> callback to be run as soon as possible.     */
	void (*phy_schedule_work_asap)(void*);

	/***  Allocate space for a packet.
	*
	* This function allocates an sk_buff with sufficient space to hold 
	* <i>datalen </i> bytes. This also includes sufficient headroom for whatever
	* additional headers the PHY layer may need to add as well as handling any
	* specia alignment or location requirements for efficient transfer to
	* hardware. For example, the Atheros PHY layer requires five extra bytes at
	* the beginning of each packet to ensure data integrity and
	* cacheline alignment to ensure speedy DMA transfers.
	*/
	struct sk_buff* (*phy_alloc_skb)(void*, int datalen);

	/***  Free an sk_buff (packet) either allocated by a call to
	* CU_SOFTMAC_PHY_ALLOC_SKB_FUNC or passed in from the PHY layer.     */
	void (*phy_free_skb)(void*,struct sk_buff*);

	/***  Send a packet, only permitting max_packets_inflight to be pending     */
	int (*phy_sendpacket)(void*,int max_packets_inflight,struct sk_buff* skb);

	/***  Send a packet, only permitting max_packets_inflight to be pending,
	* do NOT free the sk_buff upon failure.
	*
	* This allows callers to do things
	* like requeue a packet if they care to make another attempt to send the
	* packet that failed to go out.	*/
	int (*phy_sendpacket_keepskbonfail)(void*,int max_packets_inflight,struct sk_buff* skb);

	/***  Ask the phy layer how long (in microseconds) it will take for this
	* packet to be transmitted, not including any initial transmit latency.     */
	u_int32_t (*phy_get_duration)(void*, struct sk_buff* skb);

	/***  Ask the phy layer how much "lead time" there is between a request
	* to send a packet and the time it hits the air.     */
	u_int32_t (*phy_get_txlatency)(void*);

	/***  PHY layer private data     */
	void 				*phy_private;
	void 				*proc;

	/* softmac internal */
	atomic_t 				refcnt;
	
	MYLIST_HEAD 		name_hlist;
} vmac_phy_t;

/**
 *  Errors that might be returned from the send packet procedures
 */
enum
{
	VMAC_PHY_SENDPACKET_OK 						= 0,
	VMAC_PHY_SENDPACKET_ERR_TOOMANYPENDING 	= -1000,
	VMAC_PHY_SENDPACKET_ERR_NETDOWN 			= -1001,
	VMAC_PHY_SENDPACKET_ERR_NOBUFFERS 			= -1002,
};


/*
** VMAC MAC Layer Functions
*/

typedef int (*mac_rx_func_t)(void*,struct sk_buff* packet);
typedef void (*mac_unload_notify_func_t)(void*);

/**
 *  Functions/data exported by a MAC layer implementation.
 */
typedef struct vmac_mac
{
	char 					name[VMAC_NAME_SIZE];

	/**   Layer information     */
	vmac_layer_t 				*layer;

	/***  Called when an ethernet-encapsulated packet is ready to transmit.     */
	int (*mac_tx)(void*,struct sk_buff* thepacket,int intop);

	/***  Called when transmission of a packet is complete.     */
	int (*mac_tx_done)(void*,struct sk_buff* thepacket,int intop);

	/***  Called upon receipt of a packet.     */
	int (*rx_packet)(void*,struct sk_buff* thepacket,int intop);

	/***  Notify the MAC layer that it is time to do some work.     */
	int (*mac_work)(void*,int intop);

	/***  Tell the MAC layer to attach to the specified PHY layer.     */
	int (*attach_phy)(void*,vmac_phy_t*);

	/***  Tell the MAC layer to detach from the specified PHY layer.     */
	int (*detach_phy)(void*);

	int (*set_rx_func)(void*,mac_rx_func_t,void*);

	/***  Notify the MAC layer that it is being removed from the PHY.     */
	int (*set_unload_notify_func)(void*, mac_unload_notify_func_t,void*);

	void					*mac_private;
	void					*proc;

	/* softmac internal */
	atomic_t 				refcnt;

	MYLIST_HEAD 		name_hlist;
} vmac_mac_t;

/**
 *  Status codes returned by the MAC layer when receiving
 * a notification from the VMAC PHY
 */
enum
{
	/***  Finished running, all is well     */
	VMAC_MAC_NOTIFY_OK = 0,

	/***  Finished for now, but schedule task to run again ASAP     */
	VMAC_MAC_NOTIFY_RUNAGAIN = 1,

	/***  The MAC layer is busy and cannot take delivery of a packet.
	* The PHY layer should free the packet and continue.	*/
	VMAC_MAC_NOTIFY_BUSY = 2,

	/*  The MAC layer is hosed. Free the packet and continue.	*/
	VMAC_MAC_NOTIFY_HOSED = 3,
};

enum
{
	MAC_TX_OK = 0,
	MAC_TX_FAIL = -1,
};


/**** VMAC Core Services  */

/**
 *  Create a new instance of a VMAC PHY or MAC layer
 * The 'vmac_layer_new_instance' function corresponding to 'name' is called
 */
void *vmac_layer_new_instance(const char *name);

/**
 *  Call the 'vmac_layer_free_instance' function for the VMAC 
 * layer associatied with 'inst'
 */
void vmac_layer_free_instance(void *inst);

/**
 *  Register a PHY or MAC layer with VMAC
 */
void vmac_layer_register(vmac_layer_t *info);

/**
 *  Remove a PHY or MAC layer from VMAC
 */
void vmac_layer_unregister(vmac_layer_t *info);

/**
 *  Register a MAC layer instance with VMAC
 */
void vmac_register_mac(vmac_mac_t* macinfo);

/**
 *  Remove a MAC layer instance from VMAC
 */
void vmac_unregister_mac(vmac_mac_t* macinfo);

/**
 *  Get a private copy of 'macinfo' (increment ref count)
 */
vmac_mac_t* vmac_get_mac(vmac_mac_t* macinfo); 

/*  Get a private copy of pointer to macinfo structure for 'name' (increment ref count) */
vmac_mac_t* vmac_get_mac_by_name(const char *name);

/*  Allocate a macinfo structure representing a MAC layer instance (ref count = 1) */
vmac_mac_t* vmac_alloc_mac(void);

/**
 *  Dereference a pointer to a macinfo structure (decrement ref count), possibly deallocate
 */
void vmac_free_mac(vmac_mac_t* macinfo);

/*  Initialize 'macinfo' to zero and set function pointers to "do nothing" functions */
void vmac_init_mac(vmac_mac_t* macinfo);

/***  Register a PHY layer instance with VMAC */
void vmac_register_phy(vmac_phy_t* phyinfo);

/**
 *  Remove a PHY layer instance from VMAC
 */
void vmac_unregister_phy(vmac_phy_t* phyinfo);

/**
 *  Get a private copy of 'phyinfo' (increment ref count)
 */
vmac_phy_t* vmac_get_phy(vmac_phy_t* phyinfo);

/**
 *  Get a private copy of pointer to phyinfo structure for 'name'
 */
vmac_phy_t* vmac_get_phy_by_name(const char *name);

/**
 *  Allocate a phyinfo structure representing a PHY layer instance (ref count = 1)
 */
vmac_phy_t* vmac_alloc_phy(void);

/*  Dereference a pointer to a phyinfo structure (decrement ref count), possibly deallocate */
void vmac_free_phy(vmac_phy_t* phyinfo); 

/*  Initialize 'phyinfo' to zero and set function pointers to "do nothing" functions */
void vmac_init_phy(vmac_phy_t* phyinfo);



/* old stuff */

/*
 * We're also defining guidelines for "composable" MAC modules.
 * MAC modules that are "composable" need to export a couple of
 * key functions:
 *
 * 1) A "create_instance" function that takes a pointer to a "client_info"
 * structure and a vmac_phy_t and fills in appropriate values
 * for its notification functions and private data
 *
 * 2) A "set_cu_softmac_phylayer_info" function that sets the
 * vmac_phy_t that should be used by the MAC module
 * when accessing PHY layer softmac services.
 * 
 * In the initial iteration the "composite" MAC module will be assumed
 * to just "know" the names of these functions in the relevant MAC
 * layer modules and phy layers. Someone with more time/industry than
 * I've got right now could certainly make a "MAC broker" service that allowed
 * MAC layers to register and be referenced dynamically by some sort
 * of naming scheme by a composite MAC object.
 */
 
/* following modules are mandidate modules */ 
#define	VMAC_MODULE_VETH					"VETH"		/* virtual ethernet view from protocol stack */

#define	VMAC_MODULE_ATH_PHY				"ATHPHY"
#define	VMAC_MODULE_ATH_MAC				"ATHMAC"

/* followings are optional */
#define	VMAC_MODULE_ALOHA				"ALOHA"


#define	VMAC_DEBUG_VETH					1
#define 	VMAC_DEBUG_ALOHA					1

#define	VMAC_DEBUG_ATH					1
#define	VMAC_DEBUG						1

#ifdef __KERNEL__
	#undef	KERN_DEBUG
	#define  	KERN_DEBUG		KERN_ERR

	#undef	KERN_INFO
	#define 	KERN_INFO			KERN_ERR

	#undef 	KERN_ALERT
	#define	KERN_ALERT			KERN_ERR
	
	#define trace					printk(KERN_ERR "%s[%d]\r\n", __FUNCTION__, __LINE__)
//	#define trace			
#else
	#define trace					printf("%s[%d]\r\n", __FUNCTION__, __LINE__)
#endif


#endif

