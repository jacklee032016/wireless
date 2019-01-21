/*
* $Id: ixp400_eth_adm.c,v 1.1.1.1 2006/11/29 08:55:24 lizhijie Exp $
* Add ADM6996 PHY support, Li Zhijie,2005.11.25
*/

/*
 * File: ixp400_eth.c
 *
 * Author: Intel Corporation
 *
 * IXP400 Ethernet Driver for Linux
 *
 * @par
 * -- Intel Copyright Notice --
 * 
 * Copyright 2004-2005  Intel Corporation. All Rights Reserved. 
 *  
 * This software program is licensed subject to the GNU
 * General Public License (GPL). Version 2, June 1991, available at
 * http://www.fsf.org/copyleft/gpl.html
 * 
 * -- End Intel Copyright Notice --
 * 
 */

/* 
 * DESIGN NOTES:
 * This driver is written and optimized for Intel Xscale technology.
 *
 * SETUP NOTES:
 * By default, this driver uses predefined MAC addresses.
 * These are set in global var 'default_mac_addr' in this file.
 * If required, these can be changed at run-time using
 * the 'ifconfig' tool.
 *
 * Example - to set ixp0 MAC address to 00:02:B3:66:88:AA, 
 * run ifconfig with the following arguments:
 *
 *   ifconfig ixp0 hw ether 0002B36688AA
 *
 * (more information about ifconfig is available thru ifconfig -h)
 *
 * Example - to set up the ixp1 IP address to 192.168.10.1
 * run ifconfig with the following arguments:
 * 
 * ifconfig ixp1 192.168.10.1 up
 *
 */


/*
 * System-defined header files
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/socket.h>
#include <linux/cache.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <net/pkt_sched.h>
#include <net/ip.h>
#include <linux/sysctl.h>
#include <linux/unistd.h>

/*
 * Intel IXP400 Software specific header files
 */
#include <IxQMgr.h>
#include <IxEthAcc.h>
#include <IxEthDB.h>
#include <IxEthMii.h>
#include <IxEthNpe.h>
#include <IxNpeDl.h>
#include <IxNpeMh.h>
#include <IxFeatureCtrl.h>
#include <IxVersionId.h>
#include <IxOsal.h>
#include <IxQueueAssignments.h>


#define WITH_ADM6996_SWITCH	1

#if WITH_ADM6996_SWITCH
	#include <linux/if_vlan.h>

	/* CLONE Device position, when 0 ,eth0 are WAN and eth1 are LAN, ie. the first one
	of these 2 are WAN */
	#define ADM_START_POSITION		0

	/* raw WAN device name : when 2 , device name is eth2 */
	#define RAW_WAN_POSITION			2

	#define VLAN_ID_OF_LAN				1
	#define VLAN_ID_OF_WAN				2
	
#endif

#define DEBUG

#ifdef CONFIG_XSCALE_PMU_TIMER
/* We want to use interrupts from the XScale PMU timer to
 * drive our NPE Queue Dispatcher loop.  But if this #define
 * is set, then it means the system is already using this timer
 * so we cannot.
 */
#error "XScale PMU Timer not available (CONFIG_XSCALE_PMU_TIMER is defined). Cannot continue"
#endif

/*
 * Module version information
 */
MODULE_DESCRIPTION("IXP400 NPE Ethernet driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Intel Corporation");
#define MODULE_NAME "ixp400_eth"
#define MODULE_VERSION "1.4"

/* Module parameters */
static int npe_learning = 1;      /* default : NPE learning & filtering enable */
static int datapath_poll = 1;     /* default : rx/tx polling, not interrupt driven*/
static int log_level = 0;         /* default : no log */
static int no_ixp400_sw_init = 0; /* default : init core components of the IXP400 Software */
static int no_phy_scan = 1;       /* default : no phy discovery */
static int phy_reset = 0;         /* default : mo phy reset */

/* maximum number of ports supported by this driver ixp0, ixp1 ....
 * The default is to configure all ports defined in EthAcc component
*/
#ifdef CONFIG_IXP400_ETH_NPEC_ONLY
static int dev_max_count = 1; /* only NPEC is used */
#elif defined (CONFIG_IXP400_ETH_NPEB_ONLY)
static int dev_max_count = 1; /* only NPEB is used */
#elif defined (CONFIG_ARCH_IXDP425)
static int dev_max_count = 2; /* only NPEB and NPEC */
#elif defined (CONFIG_ARCH_IXDP465)
static int dev_max_count = 3; /* all NPEs are used */
#endif

/* netdev_max_backlog: ideally /proc/sys/net/core/netdev_max_backlog, but any 
 * value > 46 looks to work. This is used to control the maximum number of 
 * skbuf to push into the linux stack, and avoid the performance degradations 
 * during overflow.
 */
static int netdev_max_backlog = 290;

MODULE_PARM(npe_learning, "i");
MODULE_PARM_DESC(npe_learning, "If non-zero, NPE MAC Address Learning & Filtering feature will be enabled");
MODULE_PARM(log_level, "i");
MODULE_PARM_DESC(log_level, "Set log level: 0 - None, 1 - Verbose, 2 - Debug");
MODULE_PARM(no_ixp400_sw_init, "i");
MODULE_PARM_DESC(no_ixp400_sw_init, "If non-zero, do not initialise Intel IXP400 Software Release core components");
MODULE_PARM(no_phy_scan, "i");
MODULE_PARM_DESC(no_phy_scan, "If non-zero, use hard-coded phy addresses");
MODULE_PARM(datapath_poll, "i");
MODULE_PARM_DESC(datapath_poll, "If non-zero, use polling method for datapath instead of interrupts");
MODULE_PARM(phy_reset, "i");
MODULE_PARM_DESC(phy_reset, "If non-zero, reset the phys");
MODULE_PARM(netdev_max_backlog, "i");
MODULE_PARM_DESC(netdev_max_backlog, "Should be set to the value of /proc/sys/net/core/netdev_max_backlog (perf affecting)");
MODULE_PARM(dev_max_count, "i");
MODULE_PARM_DESC(dev_max_count, "Number of devices to initialize");

/* devices will be called ixp0 and ixp1 */
#define DEVICE_NAME "eth"  /* change the name as eth0 and eth1 */

/* boolean values for PHY link speed, duplex, and autonegotiation */
#define PHY_SPEED_10    0
#define PHY_SPEED_100   1
#define PHY_DUPLEX_HALF 0
#define PHY_DUPLEX_FULL 1
#define PHY_AUTONEG_OFF 0
#define PHY_AUTONEG_ON  1

/* will clean skbufs from the sw queues when they are older
 * than this time (this mechanism is needed to prevent the driver 
 * holding skbuf and memory space for too long). Unit are in seconds
 * because the timestamp in buffers are in seconds.
 */
#define BUFFER_MAX_HOLD_TIME_S (3)

/* maintenance time (jiffies) */
#define DB_MAINTENANCE_TIME (IX_ETH_DB_MAINTENANCE_TIME*HZ)

/* Time before kernel will decide that the driver had stuck in transmit (jiffies) */
#define DEV_WATCHDOG_TIMEO (10*HZ)

/* Interval between media-sense/duplex checks (jiffies) */
#define MEDIA_CHECK_INTERVAL (3*HZ)

/* Dictates the rate (per second) at which the NPE queues are serviced */
/*   4000 times/sec = 37 mbufs/interrupt at line rate */
#define QUEUE_DISPATCH_TIMER_RATE (4000)

/* Tunable value, the highest helps for steady rate traffic, but too high
 * increases the packet drops. E.g. TCP or UDP traffic generated from a PC.
 * The lowest helps for well-timed traffic (e.g. smartApps). Also, the lowest
 * is sometimes better for extremely bursty traffic
 *
 * Value range from 5 up to 8. 6 is a "good enough" compromize.
 *
 * With this setting (6), small drops of traffic may be observed at 
 * high traffic rates. To measure the maximum throughput between the
 * ports of the driver,
 * - Modify /proc/sys/net/core/netdev_max_backlog value in the kernel 
 * - Adjust netdev_max_backlog=n in the driver's command line
 * in order to get the best rates depending on the testing tool 
 * and the OS load.
 *
 */
#define BACKLOG_TUNE (6)

/* number of packets to prealloc for the Rx pool (per driver instance) */
#define RX_MBUF_POOL_SIZE (80)

/* Maximum number of packets in Tx+TxDone queue */
#define TX_MBUF_POOL_SIZE (256)

#if WITH_ADM6996_SWITCH
	#define VLAN_HDR 4
#else
	/* VLAN header size definition. Only if VLAN 802.1Q frames are enabled */
	#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	#define VLAN_HDR 4
	#else
	#define VLAN_HDR 0
	#endif
#endif

/* Number of mbufs in sw queue */
#define MB_QSIZE  (256) /* must be a power of 2 and > TX_MBUF_POOL_SIZE */
#define SKB_QSIZE (256) /* must be a power of 2 and > RX_MBUF_POOL_SIZE */

/* Maximum number of addresses the EthAcc multicast filter can hold */
#define IX_ETH_ACC_MAX_MULTICAST_ADDRESSES (256)

/* Parameter passed to Linux in ISR registration (cannot be 0) */
#define IRQ_ANY_PARAMETER (1)

/* 
 * The size of the SKBs to allocate is more than the MSDU size.
 *
 * skb->head starts here
 * (a) 16 bytes reserved in dev_sk_alloc()
 * (b) 48 RNDIS optional reserved bytes
 * (c) 2 bytes for necessary alignment of the IP header on a word boundary
 * skb->data starts here, the NPE will store the payload at this address
 * (d)      14 bytes  (dev->dev_hard_len)
 * (e)      1500 bytes (dev->mtu, can grow up for Jumbo frames)
 * (f)      4 bytes fcs (stripped out by MAC core)
 * (g)      xxx round-up needed for a NPE 64 bytes boundary
 * skb->tail the NPE will not write more than these value
 * (h)      yyy round-up needed for a 32 bytes cache line boundary
 * (i) sizeof(struct sk_buff). there is some shared info sometimes
 *     used by the system (allocated in net/core/dev.c)
 *
 * The driver private structure stores the following fields
 *
 *  msdu_size = d+e : used to set the msdu size
 *  replenish_size = d+e+g : used for replenish
 *  pkt_size = b+c+d+e+g+h : used to allocate skbufs
 *  alloc_size = a+b+c+d+e+g+h+i, compare to skb->truesize
*/

#ifdef CONFIG_USB_RNDIS
#define HDR_SIZE (2 + 48)
#else
#define HDR_SIZE (2)
#endif

/* dev_alloc_skb reserves 16 bytes in the beginning of the skbuf
 * and sh_shared_info at the end of the skbuf. But the value used
 * used to set truesize in skbuff.c is struct sk_buff (???). This
 * behaviour is reproduced here for consistency.
*/
#define SKB_RESERVED_HEADER_SIZE (16)
#define SKB_RESERVED_TRAILER_SIZE sizeof(struct sk_buff)

/* NPE-A Functionality: Ethernet only */
#define IX_ETH_NPE_A_IMAGE_ID IX_NPEDL_NPEIMAGE_NPEA_ETH

/* NPE-B Functionality: Ethernet only */
#define IX_ETH_NPE_B_IMAGE_ID IX_NPEDL_NPEIMAGE_NPEB_ETH

/* NPE-C Functionality: Ethernet only  */
#define IX_ETH_NPE_C_IMAGE_ID IX_NPEDL_NPEIMAGE_NPEC_ETH


/*
 * Macros to turn on/off debug messages
 */
/* Print kernel error */
#define P_ERROR(args...) \
    printk(KERN_ERR MODULE_NAME ": " args)
/* Print kernel warning */
#define P_WARN(args...) \
    printk(KERN_WARNING MODULE_NAME ": " args)
/* Print kernel notice */
#define P_NOTICE(args...) \
    printk(KERN_NOTICE MODULE_NAME ": " args)
/* Print kernel info */
#define P_INFO(args...) \
    printk(KERN_INFO MODULE_NAME ": " args)
/* Print verbose message. Enabled/disabled by 'log_level' param */
#define P_VERBOSE(args...) \
    if (log_level >= 1) printk(MODULE_NAME ": " args)
/* Print debug message. Enabled/disabled by 'log_level' param  */
#define P_DEBUG(args...) \
    if (log_level >= 2) { \
        printk("%s: %s()\n", MODULE_NAME, __FUNCTION__); \
        printk(args); }

#ifdef DEBUG
/* Print trace message */
#define TRACE \
    if (log_level >= 2) printk("%s: %s(): line %d\n", MODULE_NAME, __FUNCTION__, __LINE__)
#else
/* no trace */
#define TRACE 
#endif

/* extern Linux kernel data */
extern struct softnet_data softnet_data[]; /* used to get the current queue level */
extern unsigned long loops_per_jiffy; /* used to calculate CPU clock speed */

/* internal Ethernet Access layer polling entry points */
extern void 
ixEthRxFrameQMCallback(IxQMgrQId qId, IxQMgrCallbackId callbackId);
extern void 
ixEthTxFrameDoneQMCallback(IxQMgrQId qId, IxQMgrCallbackId callbackId);

/* Private device data */
typedef struct {
    spinlock_t lock;  /* multicast management lock */
    
    unsigned int msdu_size;
    unsigned int replenish_size;
    unsigned int pkt_size;
    unsigned int alloc_size;

    struct net_device_stats stats; /* device statistics */
	
#if WITH_ADM6996_SWITCH
	struct net_device_stats clone_stats; 	/* device statistics for cloned device */

	struct net_device		*rawDev;
	struct net_device		*clonedDev;

	/* ref count for these 2 device, when any device is open, then refcount>0 */
	int					refCount;
#endif

    IxEthAccPortId port_id; /* EthAcc port_id */

#ifdef CONFIG_IXP400_ETH_QDISC_ENABLED
    /* private scheduling discipline */
	struct Qdisc *qdisc;

	#if WITH_ADM6996_SWITCH
		struct Qdisc *cloned_qdisc;
	#endif
#endif

    /* Implements a software queue for mbufs 
     * This queue is written in the tx done process and 
     * read during tx. Because there is 
     * 1 reader and 1 writer, there is no need for
     * a locking algorithm.
     */

    /* mbuf Tx queue indexes */
    unsigned int mbTxQueueHead;
    unsigned int mbTxQueueTail;

    /* mbuf Rx queue indexes */
    unsigned int mbRxQueueHead;
    unsigned int mbRxQueueTail;

    /* software queue containers */
    IX_OSAL_MBUF *mbTxQueue[MB_QSIZE];
    IX_OSAL_MBUF *mbRxQueue[MB_QSIZE];

    /* RX MBUF pool */
    IX_OSAL_MBUF_POOL *rx_pool;

    /* TX MBUF pool */
    IX_OSAL_MBUF_POOL *tx_pool;

    /* id of thread for the link duplex monitoring */
    int maintenanceCheckThreadId;

    /* mutex locked by thread, until the thread exits */
    struct semaphore *maintenanceCheckThreadComplete;

    /* Used to stop the kernel thread for link monitoring. */
    volatile BOOL maintenanceCheckStopped;

    /* used for tx timeout */
    struct tq_struct tq_timeout;
#if WITH_ADM6996_SWITCH
	struct tq_struct cloned_tq_timeout;
#endif
    /* used to control the message output */
    UINT32 devFlags;
} priv_data_t;

/* Collection of boolean PHY configuration parameters */
typedef struct {
    BOOL speed100;
    BOOL duplexFull;
    BOOL autoNegEnabled;
    BOOL linkMonitor;
} phy_cfg_t;

/*
 * STATIC VARIABLES
 *
 * This section sets several default values for each port.
 * These may be edited if required.
 */

/* values used inside the irq */
static unsigned long timer_countup_ticks;
static IxQMgrDispatcherFuncPtr dispatcherFunc;
static struct timeval  irq_stamp;  /* time of interrupt */
static unsigned int rx_queue_id = IX_QMGR_MAX_NUM_QUEUES;

/* Implements a software queue for skbufs 
 * This queue is written in the tx done process and 
 * read during rxfree. Because there is 
 * 1 reader and 1 writer, there is no need for
 * a locking algorithm.
 *
 * This software queue is shared by both ports.
 */

/* skbuf queue indexes */
static unsigned int skQueueHead;
static unsigned int skQueueTail;

/* software queue containers */
static struct sk_buff *skQueue[SKB_QSIZE];

/* 
 * The PHY addresses mapped to Intel IXP400 Software EthAcc ports.
 *
 * These are hardcoded and ordered by increasing ports.
 * Overwriting these values by a PHY discovery is disabled by default but
 * can optionally be enabled if required
 * by passing module param no_phy_scan with a zero value
 *
 * Up to 32 PHYs may be discovered by a phy scan. Addresses
 * of all PHYs found will be stored here, but only the first
 * 2 will be used with the Intel IXP400 Software EthAcc ports.
 *
 * See also the function phy_init() in this file.
 *
 * NOTE: The hardcoded PHY addresses have been verified on
 * the IXDP425 and Coyote (IXP4XX RG) Development platforms.
 * However, they may differ on other platforms.
 */
static int phyAddresses[IXP425_ETH_ACC_MII_MAX_ADDR] =
{
#if defined(CONFIG_ARCH_IXDP425)
    /* 1 PHY per NPE port */
    0, /* Port 1 (IX_ETH_PORT_1 / NPE B) */
    1  /* Port 2 (IX_ETH_PORT_2 / NPE C) */

#elif defined(CONFIG_ARCH_IXDP465)
    /* 1 PHY per NPE port */
    0, /* Port 1 (IX_ETH_PORT_1 / NPE B) */
    1, /* Port 2 (IX_ETH_PORT_2 / NPE C) */
    2  /* Port 3 (IX_ETH_PORT_2 / NPE A) */

#elif defined(CONFIG_ARCH_COYOTE)
    4, /* Port 1 (IX_ETH_PORT_1) - Connected to PHYs 1-4      */
    5, /* Port 2 (IX_ETH_PORT_2) - Only connected to PHY 5    */

    3,  /******************************************************/
    2,  /* PHY addresses on Coyote platform (physical layout) */
    1   /* (4 LAN ports, switch)  (1 WAN port)                */
        /*       ixp0              ixp1                       */
        /*  ________________       ____                       */
        /* /_______________/|     /___/|                      */
	/* | 1 | 2 | 3 | 4 |      | 5 |                       */
        /* ----------------------------------------           */
#else
    /* other platforms : suppose 1 PHY per NPE port */
    0, /* PHY address for EthAcc Port 1 (IX_ETH_PORT_1 / NPE B) */
    1  /* PHY address for EthAcc Port 2 (IX_ETH_PORT_2 / NPE C) */

#endif
};

/* The default configuration of the phy on each Intel IXP400 Software EthAcc port.
 *
 * This configuration is loaded when the phy's are discovered.
 * More PHYs can optionally be configured here by adding more
 * configuration entries in the array below.  The PHYs will be
 * configured in the order that they are found, starting with the
 * lowest PHY address.
 *
 * See also function phy_init() in this file
 */
static phy_cfg_t default_phy_cfg[] =
{
#if defined(CONFIG_ARCH_IXDP425)
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,TRUE},/* Port 1: monitor the phy */
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,TRUE} /* Port 2: monitor the link */

#elif defined(CONFIG_ARCH_IXDP465)
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,TRUE},/* Port 1: monitor the phy */
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,TRUE},/* Port 2: monitor the link */
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,FALSE} /* Port 3: ignore the link */

#elif defined(CONFIG_ARCH_COYOTE)
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,FALSE},/* Port 1: NO link */
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,TRUE},/* Port 2: monitor the link */
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,FALSE},
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,FALSE},
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,FALSE}

#else
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,TRUE},/* Port 1: monitor the link*/
    {PHY_SPEED_100, PHY_DUPLEX_FULL, PHY_AUTONEG_ON,TRUE} /* Port 2: monitor the link*/

#endif
};

/* Default MAC addresses for EthAcc Ports 1 and 2 (using Intel MAC prefix) 
 * Default is 
 *   IX_ETH_PORT_1 -> MAC 00:02:b3:01:01:01
 *   IX_ETH_PORT_2 -> MAC 00:02:b3:02:02:02
 *   IX_ETH_PORT_3 -> MAC 00:02:b3:03:03:03
*/
static IxEthAccMacAddr default_mac_addr[] =
{
    {{0x00, 0x02, 0xB3, 0x01, 0x01, 0x01}}  /* EthAcc Port 1 */
    ,{{0x00, 0x02, 0xB3, 0x02, 0x02, 0x02}} /* EthAcc Port 2 */
#if defined (CONFIG_ARCH_IXDP465)
    ,{{0x00, 0x02, 0xB3, 0x03, 0x03, 0x03}} /* EthAcc Port 3 */
#endif
};

/* Default mapping of  NpeImageIds for EthAcc Ports 
 * Default is 
 *   IX_ETH_PORT_1 -> IX_ETH_NPE_B
 *   IX_ETH_PORT_2 -> IX_ETH_NPE_C
 *   IX_ETH_PORT_3 -> IX_ETH_NPE_A
 *
 * This mapping cannot be changed.
 * 
 */
static UINT32 default_npeImageId[] =
{
    IX_ETH_NPE_B_IMAGE_ID  /* Npe firmware for EthAcc Port 1  */
    ,IX_ETH_NPE_C_IMAGE_ID /* Npe firmware for EthAcc Port 2  */
#if defined (CONFIG_ARCH_IXDP465)
    ,IX_ETH_NPE_A_IMAGE_ID /* Npe firmware for EthAcc Port 3  */
#endif
};

/* Default mapping of ethAcc portId for devices ixp0 and ixp1 
 * Default is 
 *   device ixp0 ---> IX_ETH_PORT_1
 *   device ixp1 ---> IX_ETH_PORT_2
 *
 * On a system with NPE C, and a single PHY, 
 * - swap the entries from this array to assign ixp0 to IX_ETH_PORT_2
 * - set phyAddress[] with a single entry and the correct PHY address
 * - set default_phy_cfg[] with a single entry and the initialisation PHY setup
 * - starts the driver with the option dev_max_count=1 no_phy_scan=1 
 *      dev_max_count=1 will create 1 driver instance ixp0
 *      no_phy_scan=1 will bypass the default mapping set by phy_init
 *
*/
static IxEthAccPortId default_portId[] =
{
#ifdef CONFIG_IXP400_ETH_NPEC_ONLY
    /* configure port for NPE C first */
    IX_ETH_PORT_2, /* EthAcc Port 2 for ixp0 */
    IX_ETH_PORT_1
#elif defined (CONFIG_IXP400_ETH_NPEB_ONLY)
    /* configure port for NPE B first */
    IX_ETH_PORT_1, /* EthAcc Port 1 for ixp0 */
    IX_ETH_PORT_2
#elif defined (CONFIG_ARCH_IXDP465)
    /* configure port for NPE B first */
    IX_ETH_PORT_1, /* EthAcc Port 1 for ixp0 */
    IX_ETH_PORT_2, /* EthAcc Port 2 for ixp1 */
    IX_ETH_PORT_3  /* EthAcc Port 3 for ixp2 */
#else
    /* configure and use both ports */
    IX_ETH_PORT_1, /* EthAcc Port 1 for ixp0 */
    IX_ETH_PORT_2  /* EthAcc Port 2 for ixp1 */
#endif
};

/* Mutex lock used to coordinate access to IxEthAcc functions
 * which manipulate the MII registers on the PHYs
 */
static struct semaphore *miiAccessMutex;

/* mutex locked when maintenance is being performed */
static struct semaphore *maintenance_mutex;

/* Flags which is set when the corresponding IRQ is running,
 */
static int irq_pmu_used = 0;
static int irq_qm1_used = 0;

/*
 * ERROR COUNTERS
 */

static UINT32 skbAllocFailErrorCount = 0;
static UINT32 replenishErrorCount = 0;


#if WITH_ADM6996_SWITCH
	static struct net_device adm6996_clone_dev;
	static priv_data_t  *sharedPriv = NULL;	/* private shared by WAN1 and Cloned Device */

	static IxEthAccMacAddr cloned_mac_addr = 
		{{0x00, 0x02, 0xB3, 0x09, 0x09, 0x09}};

#endif


/*
 * ERROR NUMBER FUNCTIONS
 */

/* Convert IxEthAcc return codes to suitable Linux return codes */
static int convert_error_ethAcc (IxEthAccStatus error)
{
    switch (error)
    {
	case IX_ETH_ACC_SUCCESS:            return  0;
	case IX_ETH_ACC_FAIL:               return -1;
	case IX_ETH_ACC_INVALID_PORT:       return -ENODEV;
	case IX_ETH_ACC_PORT_UNINITIALIZED: return -EPERM;
	case IX_ETH_ACC_MAC_UNINITIALIZED:  return -EPERM;
	case IX_ETH_ACC_INVALID_ARG:        return -EINVAL;
	case IX_ETH_TX_Q_FULL:              return -EAGAIN;
	case IX_ETH_ACC_NO_SUCH_ADDR:       return -EFAULT;
	default:                            return -1;
    };
}

/*
 * DEBUG UTILITY FUNCTIONS
 */
#ifdef DEBUG_DUMP

static void hex_dump(void *buf, int len)
{
    int i;

    for (i = 0 ; i < len; i++)
    {
	printk("%02x", ((u8*)buf)[i]);
	if (i%2)
	    printk(" ");
	if (15 == i%16)
	    printk("\n");
    }
    printk("\n");
}

static void mbuf_dump(char *name, IX_OSAL_MBUF *mbuf)
{
    printk("+++++++++++++++++++++++++++\n"
	"%s MBUF dump mbuf=%p, m_data=%p, m_len=%d, len=%d\n",
	name, mbuf, IX_OSAL_MBUF_MDATA(mbuf), IX_OSAL_MBUF_MLEN(mbuf), IX_OSAL_MBUF_PKT_LEN(mbuf));
    printk(">> mbuf:\n");
    hex_dump(mbuf, sizeof(*mbuf));
    printk(">> m_data:\n");
    hex_dump(__va(IX_OSAL_MBUF_MDATA(mbuf)), IX_OSAL_MBUF_MLEN(mbuf));
    printk("\n-------------------------\n");
}

static void skb_dump(char *name, struct sk_buff *skb)
{
    printk("+++++++++++++++++++++++++++\n"
	"%s SKB dump skb=%p, data=%p, tail=%p, len=%d\n",
	name, skb, skb->data, skb->tail, skb->len);
    printk(">> data:\n");
    hex_dump(skb->data, skb->len);
    printk("\n-------------------------\n");
}
#endif

/*
 * MEMORY MANAGEMENT
 */

/* XScale specific : preload a cache line to memeory */
static inline void dev_preload(void *virtualPtr) 
{
   __asm__ (" pld [%0]\n" : : "r" (virtualPtr));
}

static inline void dev_skb_preload(struct sk_buff *skb)
{
    /* from include/linux/skbuff.h, skb->len field and skb->data filed 
     * don't share the same cache line (more than 32 bytes between the fields)
     */
    dev_preload(&skb->len);
    dev_preload(&skb->data);
}

/*
 * BUFFER MANAGEMENT
 */

/*
 * Utility functions to handle software queues
 * Each driver has a software queue of skbufs and 
 * a software queue of mbufs for tx and mbufs for rx
 */
#define MB_QINC(index) ((index)++ & (MB_QSIZE - 1))
#define SKB_QINC(index) ((index)++ & (SKB_QSIZE - 1))
#define SKB_QFETCH(index) ((index) & (SKB_QSIZE - 1))

/**
 * mbuf_swap_skb : links/unlinks mbuf to skb
 */
static inline struct sk_buff *mbuf_swap_skb(IX_OSAL_MBUF *mbuf, struct sk_buff *skb)
{
	struct sk_buff *res = IX_OSAL_MBUF_PRIV(mbuf);

	IX_OSAL_MBUF_PRIV(mbuf) = skb;
	if (!skb)
		return res;

	IX_OSAL_MBUF_MDATA(mbuf) = skb->data;
	IX_OSAL_MBUF_MLEN(mbuf) = IX_OSAL_MBUF_PKT_LEN(mbuf) = skb->len;

	return res;
}

/*
 * dev_skb_mbuf_free: relaese skb to the kernel and mbuf to the pool 
 * This function is called during port_disable and has no constraint
 * of performances.
*/
static inline void mbuf_free_skb(IX_OSAL_MBUF *mbuf)
{
    TRACE;
    /* chained buffers can be received during port
     * disable after a mtu size change.
     */
    do
    {
	/* unchain and free the buffers */
	IX_OSAL_MBUF *next = IX_OSAL_MBUF_NEXT_BUFFER_IN_PKT_PTR(mbuf);
	IX_OSAL_MBUF_NEXT_BUFFER_IN_PKT_PTR(mbuf) = NULL;
	if (IX_OSAL_MBUF_PRIV(mbuf) != NULL)
	{
	    /* this buffer has an skb attached, free both of them */	    
	    dev_kfree_skb_any(mbuf_swap_skb(mbuf, NULL));
	    IX_OSAL_MBUF_POOL_PUT(mbuf);
	}
	else
	{
	    /* this buffer has no skb attached, just ignore it */
	}
	mbuf = next;
    }
    while (mbuf != NULL);
    
    TRACE;
}


/* dev_skb_dequeue: remove a skb from the skb queue */
static inline struct sk_buff  *dev_skb_dequeue(priv_data_t *priv)
{
    struct sk_buff *skb;
    if (skQueueHead != skQueueTail)
    {
	/* get from queue (fast) packet is ready for use 
	 * because the fields are reset during the enqueue
	 * operations
	 */
	skb = skQueue[SKB_QINC(skQueueTail)];
	if (skQueueHead != skQueueTail)
	{
	    /* preload the next skbuf : this is an optimisation to 
	     * avoid stall cycles when acessing memory.
	     */
	    dev_skb_preload(skQueue[SKB_QFETCH(skQueueTail)]);
	}
	/* check the skb size fits the driver requirements 
	 */
	if (skb->truesize >= priv->alloc_size)
	{
	    return skb;
	}
	/* the skbuf is too small : put it to pool (slow)
	 * This may occur when the ports are configured
	 * with a different mtu size.
	 */
	dev_kfree_skb_any(skb);
    }
    
    /* get a skbuf from pool (slow) */
    skb = dev_alloc_skb(priv->pkt_size);
    if (skb != NULL)
    {
#if WITH_ADM6996_SWITCH
	skb_reserve(skb, HDR_SIZE + VLAN_HDR);
#else
	skb_reserve(skb, HDR_SIZE);
#endif
	skb->len = priv->replenish_size;
	IX_ACC_DATA_CACHE_INVALIDATE(skb->data, priv->replenish_size);
    }
    else
    {
	skbAllocFailErrorCount++;
    }
    
    return skb;
}

#ifdef CONFIG_IXP400_ETH_SKB_RECYCLE
/* dev_skb_enqueue: add a skb to the skb queue */
static inline void dev_skb_enqueue(priv_data_t *priv, struct sk_buff *skb)
{
    /* check for big-enough unshared skb, and check the queue 
     * is not full If the queue is full or the complete ownership of the
     * skb is not guaranteed, just free the skb.
     * (atomic counters are read on the fly, there is no need for lock)
     */

    if ((skb->truesize >= priv->alloc_size) && 
	(skb->users.counter == 1) && 
	(skb->cloned == 0)  &&
        (skb->destructor == NULL) && 
	(skb_shinfo(skb)->dataref.counter == 1) &&
        (skb_shinfo(skb)->nr_frags == 0) &&
        (skb_shinfo(skb)->frag_list == NULL) &&
	(skQueueHead - skQueueTail < SKB_QSIZE))
    {
 	/* put big unshared mbuf to queue (fast)
	 *
	 * The purpose of this part of code is to reset the skb fields
	 * so they will be ready for reuse within the driver.
	 * 
	 * The following fields are reset during a skb_free and 
	 * skb_alloc sequence (see dev/core/skbuf.c). We
	 * should call the function skb_headerinit(). Unfortunately,
	 * this function is static.
	 *
	 * This code resets the current skb fields to zero,
	 * resets the data pointer to the beginning of the 
	 * skb data area, then invalidates the all payload.
	 */

	dst_release(skb->dst);

#ifdef CONFIG_NETFILTER
	/* Some packets may get incorrectly process by netfilter firewall 
	 * software if CONFIG_NETFILTER is enabled and filtering is in use. 
	 * The solution is to reset the following fields in the skbuff 
	 * before re-using it on the Rx-path
	 */
        skb->nfmark = 0;
	skb->nfcache = 0;
        nf_conntrack_put(skb->nfct);
        skb->nfct = NULL;
#ifdef CONFIG_NETFILTER_DEBUG
        skb->nf_debug = 0;
#endif
#endif /* CONFIG_NETFILTER */
#ifdef CONFIG_NETFILTER
#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
/* We need to free the memory attached to the nf_bridge pointer to avoid a memory leak */
	 nf_bridge_put(skb->nf_bridge);
        skb->nf_bridge = NULL;

#endif
#endif /* CONFIG_NETFILTER */
	skb->sk = NULL;
        skb->dst = NULL;
	skb->pkt_type = PACKET_HOST;    /* Default type */
        skb->ip_summed = 0;
        skb->priority = 0;
        skb->security = 0;
#ifdef CONFIG_NET_SCHED
	skb->tc_index = 0;
#endif
#if defined(CONFIG_IMQ) || defined(CONFIG_IMQ_MODULE)
        skb->imq_flags = 0;
        skb->nf_info = NULL;
#endif

	/* reset the data pointer (skb_reserve is not used for efficiency) */
#if WITH_ADM6996_SWITCH
	skb->data = skb->head + SKB_RESERVED_HEADER_SIZE + HDR_SIZE + VLAN_HDR;
#else
	skb->data = skb->head + SKB_RESERVED_HEADER_SIZE + HDR_SIZE;
#endif
	skb->len = priv->replenish_size;

	/* invalidate the payload
	 * This buffer is now ready to be used for replenish.
	 */
	IX_ACC_DATA_CACHE_INVALIDATE(skb->data, skb->len);

	skQueue[SKB_QINC(skQueueHead)] = skb;
    }
    else
    {
	/* put to pool (slow) */
	dev_kfree_skb_any(skb);
    }
}
#endif /* ifdef CONFIG_IXP400_ETH_SKB_RECYCLE */


/* dev_skb_queue_drain: remove all entries from the skb queue */
static void dev_skb_queue_drain(priv_data_t *priv)
{
    struct sk_buff *skb;
    int key = ixOsalIrqLock();

    /* check for skbuf, then get it and release it */
    while (skQueueHead != skQueueTail)
    {
	skb = dev_skb_dequeue(priv);
	dev_kfree_skb_any(skb);
    }

    ixOsalIrqUnlock(key);
}

/* dev_rx_mb_dequeue: return one mbuf from the rx mbuf queue */
static inline IX_OSAL_MBUF * dev_rx_mb_dequeue(priv_data_t *priv)
{
    IX_OSAL_MBUF *mbuf;
    if (priv->mbRxQueueHead != priv->mbRxQueueTail)
    {
	/* get from queue (fast) */
	mbuf = priv->mbRxQueue[MB_QINC(priv->mbRxQueueTail)];
    }
    else
    {
	/* get from pool (slow) */
	mbuf = IX_OSAL_MBUF_POOL_GET(priv->rx_pool);
    }
    return mbuf;
}

/* dev_rx_mb_enqueue: add one mbuf to the rx mbuf queue */
static inline void dev_rx_mb_enqueue(priv_data_t *priv, IX_OSAL_MBUF *mbuf)
{
    /* check for queue not full */
    if (priv->mbRxQueueHead - priv->mbRxQueueTail < MB_QSIZE)
    {
	/* put to queue (fast) */
	priv->mbRxQueue[MB_QINC(priv->mbRxQueueHead)] = mbuf;
    }
    else
    {
	IX_OSAL_MBUF_POOL_PUT(mbuf);
    }
}

/* dev_rx_mb_queue_drain: remove all mbufs from the rx mbuf queue */
static void dev_rx_mb_queue_drain(priv_data_t *priv)
{
    IX_OSAL_MBUF *mbuf;
    int key = ixOsalIrqLock();

    /* free all queue entries */
    while(priv->mbRxQueueHead != priv->mbRxQueueTail)
    {
	mbuf = dev_rx_mb_dequeue(priv);
	IX_OSAL_MBUF_POOL_PUT(mbuf);
    }

    ixOsalIrqUnlock(key); 
}

/* dev_tx_mb_dequeue: remove one mbuf from the tx mbuf queue */
static inline IX_OSAL_MBUF * dev_tx_mb_dequeue(priv_data_t *priv)
{
    IX_OSAL_MBUF *mbuf;
    if (priv->mbTxQueueHead != priv->mbTxQueueTail)
    {
	/* get from queue (fast) */
	mbuf = priv->mbTxQueue[MB_QINC(priv->mbTxQueueTail)];
    }
    else
    {
	/* get from pool (slow) */
	mbuf = IX_OSAL_MBUF_POOL_GET(priv->tx_pool);
    }
    return mbuf;
}

/* dev_tx_mb_enqueue: add one mbuf to the tx mbuf queue */
static inline void dev_tx_mb_enqueue(priv_data_t *priv, IX_OSAL_MBUF *mbuf)
{
	TRACE;
    /* check for queue not full */
    if (priv->mbTxQueueHead - priv->mbTxQueueTail < MB_QSIZE)
    {
    	TRACE;
	/* put to queue (fast) */
	priv->mbTxQueue[MB_QINC(priv->mbTxQueueHead)] = mbuf;
    }
    else
    {
    	TRACE;
	IX_OSAL_MBUF_POOL_PUT(mbuf);
    }
	TRACE;
}

/* dev_tx_mb_queue_drain: remove all mbufs from the tx mbuf queue */
static void dev_tx_mb_queue_drain(priv_data_t *priv)
{
    IX_OSAL_MBUF *mbuf;
    int key = ixOsalIrqLock();

    /* free all queue entries */
    while(priv->mbTxQueueHead != priv->mbTxQueueTail)
    {
	mbuf = dev_tx_mb_dequeue(priv);
	IX_OSAL_MBUF_POOL_PUT(mbuf);
    }

    ixOsalIrqUnlock(key);
}

/* provides mbuf+skb pair to the NPEs.
 * In the case of an error, free skb and return mbuf to the pool
 */
static void dev_rx_buff_replenish(int port_id, IX_OSAL_MBUF *mbuf)
{
    IX_STATUS status;

    /* send mbuf to the NPEs */
    if ((status = ixEthAccPortRxFreeReplenish(port_id, mbuf)) != IX_SUCCESS)
    {
	replenishErrorCount++;

	P_ERROR("ixEthAccPortRxFreeReplenish failed for port %d, res = %d",
		port_id, status);
	
	/* detach the skb from the mbuf, free it, then free the mbuf */
	dev_kfree_skb_any(mbuf_swap_skb(mbuf, NULL));
	IX_OSAL_MBUF_POOL_PUT(mbuf);
    }
}

/* Allocate an skb for every mbuf in the rx_pool
 * and pass the pair to the NPEs
 */
static int dev_rx_buff_prealloc(priv_data_t *priv)
{
    int res = 0;
    IX_OSAL_MBUF *mbuf;
    struct sk_buff *skb;
    int key;

    while (NULL != (mbuf = IX_OSAL_MBUF_POOL_GET(priv->rx_pool)))
    {
	/* because this function may be called during monitoring steps
	 * interrupt are disabled so it won't conflict with the
	 * QMgr context.
	 */
	key = ixOsalIrqLock();
	/* get a skbuf (alloc from pool if necessary) */
	skb = dev_skb_dequeue(priv);

	if (skb == NULL)
	{
	    /* failed to alloc skb -> return mbuf to the pool, it'll be
	     * picked up later by the monitoring task
	     */
	    IX_OSAL_MBUF_POOL_PUT(mbuf);
	    res = -ENOMEM;
	}
	else
	{
	    /* attach a skb to the mbuf */
	    mbuf_swap_skb(mbuf, skb);
	    dev_rx_buff_replenish(priv->port_id, mbuf);
	}
	ixOsalIrqUnlock(key);

	if (res != 0)
	    return res;
    }

    return 0;
}


/* Replenish if necessary and slowly drain the sw queues
 * to limit the driver holding the memory space permanently.
 * This action should run at a low frequency, e.g during
 * the link maintenance.
 */
static int dev_buff_maintenance(struct net_device *dev)
{
    struct sk_buff *skb;
    int key;
    priv_data_t *priv = dev->priv;

    dev_rx_buff_prealloc(priv);

    /* Drain slowly the skb queue and free the skbufs to the 
     * system. This way, after a while, skbufs will be 
     * reused by other components, instead of being
     * parmanently held by this driver.
     */
    key = ixOsalIrqLock();
    
    /* check for queue not empty, then get the skbuff and release it */
    if (skQueueHead != skQueueTail)
    {
	skb = dev_skb_dequeue(priv);
	dev_kfree_skb_any(skb);
    }
    ixOsalIrqUnlock(key);

    return 0;
}


/* 
 * KERNEL THREADS
 */

/* flush the pending signals for a thread and 
 * check if a thread is killed  (e.g. system shutdown)
 */
static BOOL dev_thread_signal_killed(void)
{
    int killed = FALSE;
    if (signal_pending (current))
    {
	spin_lock_irq(&current->sigmask_lock);
	if (sigismember(&(current->pending.signal), SIGKILL)
	    || sigismember(&(current->pending.signal), SIGTERM))
	{
	    /* someone kills this thread */
	    killed = TRUE;
	}
	flush_signals(current);
	spin_unlock_irq(&current->sigmask_lock);
    }
    return killed;
}

/* This timer will check the PHY for the link duplex and
 * update the MAC accordingly. It also executes some buffer
 * maintenance to release mbuf in excess or replenish after
 * a severe starvation
 *
 * This function loops and wake up every 3 seconds.
 */
static int dev_media_check_thread (void* arg)
{
    struct net_device *dev = (struct net_device *) arg;
    priv_data_t *priv = dev->priv;
    int linkUp;
    int speed100;
    int fullDuplex = -1; /* unknown duplex mode */
    int newDuplex;
    int autonegotiate;
    unsigned phyNum = phyAddresses[priv->port_id];
    int res;

    TRACE;

    /* Lock the mutex for this thread.
       This mutex can be used to wait until the thread exits
    */
    down (priv->maintenanceCheckThreadComplete);

    daemonize();
    reparent_to_init();
    spin_lock_irq(&current->sigmask_lock);
    sigemptyset(&current->blocked);
    recalc_sigpending(current);
    spin_unlock_irq(&current->sigmask_lock);
    
    snprintf(current->comm, sizeof(current->comm), "ixp400 %s", dev->name);

    TRACE;
    
    while (1)
    {
	/* We may have been woken up by a signal. If so, we need to
	 * flush it out and check for thread termination 
	 */ 
	if (dev_thread_signal_killed())
	{
	    priv->maintenanceCheckStopped = TRUE;
	}

	/* If the interface is down, or the thread is killed,
	 * or gracefully aborted, we need to exit this loop
	 */
	if (priv->maintenanceCheckStopped)
	{
	    break;
	}
	
	/*
	 * Determine the link status
	 */

	TRACE;

	if (default_phy_cfg[priv->port_id].linkMonitor)
	{
	    /* lock the MII register access mutex */
	    down(miiAccessMutex);
	    
	    res = ixEthMiiLinkStatus(phyNum,
				     &linkUp,
				     &speed100,
				     &newDuplex, 
				     &autonegotiate);
	    /* release the MII register access mutex */
	    up(miiAccessMutex);

	    /* We may have been woken up by a signal. If so, we need to
	     * flush it out and check for thread termination 
	     */ 
	    if (dev_thread_signal_killed())
	    {
		priv->maintenanceCheckStopped = TRUE;
	    }
	    
	    /* If the interface is down, or the thread is killed,
	     * or gracefully aborted, we need to exit this loop
	     */
	    if (priv->maintenanceCheckStopped)
	    {
		break;
	    }
	
	    if (res != IX_ETH_ACC_SUCCESS)
	    {
		P_WARN("ixEthMiiLinkStatus failed on PHY%d.\n"
		       "\tCan't determine\nthe auto negotiated parameters. "
		       "Using default values.\n",
		       phyNum); 
		/* something is bad, gracefully stops the loop */
		priv->maintenanceCheckStopped = TRUE;
		break;
	    }
	    
	    if (linkUp)
	    {
		if (! netif_carrier_ok(dev))
		{
		    /* inform the kernel of a change in link state */
		    netif_carrier_on(dev);
		}

		/*
		 * Update the MAC mode to match the PHY mode if 
		 * there is a phy mode change.
		 */
		if (newDuplex != fullDuplex)
		{
		    fullDuplex = newDuplex;
		    if (fullDuplex)
		    {
			ixEthAccPortDuplexModeSet (priv->port_id, IX_ETH_ACC_FULL_DUPLEX);
		    }
		    else
		    {
			ixEthAccPortDuplexModeSet (priv->port_id, IX_ETH_ACC_HALF_DUPLEX);
		    }
		}
	    }
	    else
	    {
		fullDuplex = -1;
		if (netif_carrier_ok(dev))
		{
		    /* inform the kernel of a change in link state */
		    netif_carrier_off(dev);
		}
	    }
	}
	else
	{
	    /* if no link monitoring is set (because the PHY do not
	     * require (or support) a link-monitoring follow-up, then assume 
	     * the link is up 
	     */
	    if (!netif_carrier_ok(dev))
	    {
		netif_carrier_on(dev);
	    }
	}
    
	TRACE;
    
	/* this is to prevent the rx pool from emptying when
	 * there's not enough memory for a long time
	 * It prevents also from holding the memory for too
	 * long
	 */
	dev_buff_maintenance(dev);
    
	/* Now sleep for 3 seconds */
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(MEDIA_CHECK_INTERVAL);
    } /* while (1) ... */

    /* free the mutex for this thread. */
    up (priv->maintenanceCheckThreadComplete);

    return 0;
}

/*
 * TIMERS
 *
 * PMU Timer : This timer based on IRQ  will call the qmgr dispatcher 
 * function a few thousand times per second.
 *
 * Maintenance Timer : This timer run the maintanance action every 
 * 60 seconds approximatively.
 *
 */

/* PMU Timer reload : this should be done at each interrupt 
 *
 * Because the timer may overflow exactly between the time we
 * write the counter and the time we clear the overflow bit, all
 * irqs are disabled. Missing the everflow event and the timer 
 * will trigger only after a wrap-around.
*/
static void dev_pmu_timer_restart(void)
{
    unsigned long flags;
    save_flags_cli(flags);
     __asm__(" mcr p14,0,%0,c1,c1,0\n"  /* write current counter */
            : : "r" (timer_countup_ticks));

    __asm__(" mrc p14,0,r1,c4,c1,0; "  /* get int enable register */
            " orr r1,r1,#1; "
            " mcr p14,0,r1,c5,c1,0; "  /* clear overflow */
            " mcr p14,0,r1,c4,c1,0\n"  /* enable interrupts */
            : : : "r1");
    restore_flags(flags);
}

/* Internal ISR : run a few thousand times per second and calls 
 * the queue manager dispatcher entry point.
 */
static void dev_qmgr_os_isr(int irg, void *dev_id, struct pt_regs *regs)
{
    /* get the time of this interrupt : all buffers received during this
     * interrupt will be assigned the same time */
    do_gettimeofday(&irq_stamp);

    /* call the queue manager entry point */
    dispatcherFunc(IX_QMGR_QUELOW_GROUP);
}

/* Internal ISR : run a few thousand times per second and calls 
 * the ethernet entry point.
 */
static void dev_poll_os_isr(int irg, void *dev_id, struct pt_regs *regs)
{
    dev_pmu_timer_restart(); /* set up the timer for the next interrupt */

    /* get the time of this interrupt : all buffers received during this
     * interrupt will be assigned the same time */
    do_gettimeofday(&irq_stamp);

    ixEthRxFrameQMCallback(rx_queue_id,0);
    ixEthTxFrameDoneQMCallback(0,0);
   
}

/* initialize the PMU timer */
static int dev_pmu_timer_init(void)
{
    UINT32 controlRegisterMask =
        BIT(0) | /* enable counters */
        BIT(2);  /* reset clock counter; */
        
    /* 
    *   Compute the number of xscale cycles needed between each 
    *   PMU IRQ. This is done from the result of an OS calibration loop.
    *
    *   For 533MHz CPU, 533000000 tick/s / 4000 times/sec = 138250
    *   4000 times/sec = 37 mbufs/interrupt at line rate 
    *   The pmu timer is reset to -138250 = 0xfffde3f6, to trigger an IRQ
    *   when this up counter overflows.
    *
    *   The multiplication gives a number of instructions per second.
    *   which is close to the processor frequency, and then close to the
    *   PMU clock rate.
    *
    *      HZ : jiffies/second (global OS constant)
    *      loops/jiffy : global OS value cumputed at boot time
    *      2 is the number of instructions per loop
    *
    */
    UINT32 timer_countdown_ticks = (loops_per_jiffy * HZ * 2) / 
	QUEUE_DISPATCH_TIMER_RATE;

    timer_countup_ticks = -timer_countdown_ticks; 

    /* enable the CCNT (clock count) timer from the PMU */
    __asm__(" mcr p14,0,%0,c0,c1,0\n"  /* write control register */
            : : "r" (controlRegisterMask));
    
    return 0;
}

/* stops the timer when the module terminates 
 *
 * This is protected from re-entrancy while the timeer is being restarted.
*/
static void dev_pmu_timer_disable(void)
{
    unsigned long flags;
    save_flags_cli(flags);
    __asm__(" mrc p14,0,r1,c4,c1,0; "  /* get int enable register */
            " and r1,r1,#0x1e; "
            " mcr p14,0,r1,c4,c1,0\n"  /* disable interrupts */
            : : : "r1");
    restore_flags(flags);
}

/* This timer will call ixEthDBDatabaseMaintenance every
 * IX_ETH_DB_MAINTENANCE_TIME jiffies
 */
static void maintenance_timer_cb(unsigned long data);

static struct timer_list maintenance_timer = {
    function:&maintenance_timer_cb
};

static void maintenance_timer_task(void *data);

/* task spawned by timer interrupt for EthDB maintenance */
static struct tq_struct tq_maintenance = {
  routine:maintenance_timer_task
};

static void maintenance_timer_set(void)
{
    maintenance_timer.expires = jiffies + DB_MAINTENANCE_TIME;
    add_timer(&maintenance_timer);
}

static void maintenance_timer_clear(void)
{
    del_timer_sync(&maintenance_timer);
}

static void maintenance_timer_task(void *data)
{
    down(maintenance_mutex);
    ixEthDBDatabaseMaintenance();
    up(maintenance_mutex);
}

static void maintenance_timer_cb(unsigned long data)
{
    schedule_task(&tq_maintenance);

    maintenance_timer_set();
}

/*
 *  DATAPLANE
 */

/* This callback is called when transmission of the packed is done, and
 * IxEthAcc does not need the buffer anymore. The port is down or
 * a portDisable is running. The action is to free the buffers
 * to the pools.
 */
static void tx_done_disable_cb(UINT32 callbackTag, IX_OSAL_MBUF *mbuf)
{
    struct net_device *dev = (struct net_device *)callbackTag;
    priv_data_t *priv = dev->priv;

    TRACE;

    priv->stats.tx_packets++; /* total packets transmitted */
    priv->stats.tx_bytes += IX_OSAL_MBUF_MLEN(mbuf); /* total bytes transmitted */

    /* extract skb from the mbuf, free skb and return the mbuf to the pool */
    mbuf_free_skb(mbuf);

    TRACE;
}


/* This callback is called when transmission of the packed is done, and
 * IxEthAcc does not need the buffer anymore. The buffers will be returned to
 * the software queues.
 */
static void tx_done_cb(UINT32 callbackTag, IX_OSAL_MBUF *mbuf)
{
#if WITH_ADM6996_SWITCH
	struct net_device_stats *stats;
#endif
	struct net_device *dev = (struct net_device *)callbackTag;
	priv_data_t *priv = dev->priv;

#if WITH_ADM6996_SWITCH
	if(dev==&(adm6996_clone_dev) )
		stats = &(priv->clone_stats);
	else
		stats = &(priv->stats);
	stats->tx_packets++; /* total packets transmitted */
	stats->tx_bytes += IX_OSAL_MBUF_MLEN(mbuf);/* total bytes transmitted*/
#else
	priv->stats.tx_packets++; /* total packets transmitted */
	priv->stats.tx_bytes += IX_OSAL_MBUF_MLEN(mbuf);/* total bytes transmitted*/
#endif		

    TRACE;
    /* extract skb from the mbuf, free skb */
#ifdef CONFIG_IXP400_ETH_SKB_RECYCLE
    /* recycle skb for later use in rx (fast) */
    dev_skb_enqueue(priv, mbuf_swap_skb(mbuf, NULL));
#else
    /* put to kernel pool (slow) */
    dev_kfree_skb_any(mbuf_swap_skb(mbuf, NULL));
#endif
    /* return the mbuf to the queue */
    dev_tx_mb_enqueue(priv, mbuf);
}

/* This callback is called when transmission of the packed is done, and
 * IxEthAcc does not need the buffer anymore. The buffers will be returned to
 * the software queues.  Also, it checks to see if the netif_queue has been
 * stopped (due to buffer starvation) and it restarts it because it now knows
 * that there is at least 1 mbuf available for Tx.
 */
static void tx_done_queue_stopped_cb(UINT32 callbackTag, IX_OSAL_MBUF *mbuf)
{
    struct net_device *dev = (struct net_device *)callbackTag;
    priv_data_t *priv = dev->priv;

    TRACE;

    tx_done_cb(callbackTag, mbuf);

    if (netif_queue_stopped(dev))
    {
        ixEthAccPortTxDoneCallbackRegister(priv->port_id, 
                                           tx_done_cb,
                                           (UINT32)dev);
        netif_wake_queue(dev);
    }
}

/* the following function performs the operations normally done
 * in eth_type_trans() (see net/ethernet/eth.c) , and takes care about 
 * the flags returned by the NPE, so a payload lookup is not needed
 * in most of the cases.
 */
#if WITH_ADM6996_SWITCH
static inline void dev_eth_type_trans(unsigned int mflags, struct sk_buff *skb, 
	struct net_device *dev, int header_len)
{
#else
static inline void dev_eth_type_trans(unsigned int mflags, 
				      struct sk_buff *skb, 
				      struct net_device *dev)
{
    unsigned header_len = dev->hard_header_len;
#endif

    skb->mac.raw=skb->data;
    /* skip the mac header : there is no need for length comparison since
     * the skb during a receive is always greater than the header size and 
     * runt frames are not enabled.
     */
    skb->data += header_len;
    skb->len -= header_len;
   
    /* fill the pkt arrival time (set at the irq callback entry) */
    skb->stamp = irq_stamp;
 
    /* fill the input device field */
    skb->dev = dev;
    
    /* set the protocol from the bits filled by the NPE */
    if (mflags & IX_ETHACC_NE_IPMASK)
    { 
	/* the type_length field is 0x0800 */
	skb->protocol = htons(ETH_P_IP); 
    }
    else
    {
	/* use linux algorithm to find the protocol 
	 * from the type-length field. This costs a
	 * a lookup inside the packet payload. The algorithm
	 * and its constants are taken from the eth_type_trans()
	 * function.
	 */
	struct ethhdr *eth = skb->mac.ethernet;
	unsigned short hproto = ntohs(eth->h_proto);
	
	if (hproto >= 1536)
	{
	    skb->protocol = eth->h_proto;
	}
	else
	{
	    unsigned short rawp = *(unsigned short *)skb->data;
	    if (rawp == 0xFFFF)
		skb->protocol = htons(ETH_P_802_3);
	    else
		skb->protocol = htons(ETH_P_802_2);
	}
    }

    /* set the packet type 
     * check mcast and bcast bits as filled by the NPE 
     */
    if (mflags & (IX_ETHACC_NE_MCASTMASK | IX_ETHACC_NE_BCASTMASK))
    {
	if (mflags & IX_ETHACC_NE_BCASTMASK)
	{
	    /* the packet is a broadcast one */
	    skb->pkt_type=PACKET_BROADCAST;
	}
	else
	{
	    /* the packet is a multicast one */
	    skb->pkt_type=PACKET_MULTICAST;
	    ((priv_data_t *)(dev->priv))->stats.multicast++;
	}
    }
    else
    {
	if (dev->flags & IFF_PROMISC)
	{
	    /* check dest mac address only if promiscuous
	     * mode is set This costs
	     * a lookup inside the packet payload.
	     */
	    struct ethhdr *eth = skb->mac.ethernet;
	    unsigned char *hdest = eth->h_dest;
	    
	    if (memcmp(hdest, dev->dev_addr, ETH_ALEN)!=0)
	    {
		skb->pkt_type = PACKET_OTHERHOST;
	    }
	}
	else
	{
	    /* promiscuous mode is not set, All packets are filtered
	     * by the NPE and the destination MAC address matches
	     * the driver setup. There is no need for a lookup in the 
	     * payload and skb->pkt_type is already set to PACKET_LOCALHOST;
	     */
	}
    }

    return;
}

/* This callback is called when new packet received from MAC
 * and not ready to be transfered up-stack. (portDisable
 * is in progress or device is down)
 *
 */
static void rx_disable_cb(UINT32 callbackTag, IX_OSAL_MBUF *mbuf, IxEthAccPortId portId)
{
    TRACE;

    /* this is a buffer returned by NPEs during a call to PortDisable: 
     * free the skb & return the mbuf to the pool 
     */
    mbuf_free_skb(mbuf);

    TRACE;
}


/* This callback is called when new packet received from MAC
 * and ready to be transfered up-stack.
 *
 * If this is a valid packet, then new skb is allocated and switched
 * with the one in mbuf, which is pushed upstack.
 *
 */
static void rx_cb(UINT32 callbackTag, IX_OSAL_MBUF *mbuf, IxEthAccPortId portId)
{
    struct net_device *dev;
    priv_data_t *priv;
    struct sk_buff *skb;
    int len;
    unsigned int mcastFlags;
    unsigned int qlevel;
   
    TRACE;
    dev = (struct net_device *)callbackTag;
    priv = dev->priv;

    qlevel = softnet_data[0].input_pkt_queue.qlen;
    /* check if the system accepts more traffic and
     * against chained mbufs 
     */
    if ((qlevel < netdev_max_backlog)
	&& (IX_OSAL_MBUF_NEXT_PKT_IN_CHAIN_PTR(mbuf) == NULL))
    {      
	/* the netif_rx queue is not overloaded */
	TRACE;
  
	len = IX_OSAL_MBUF_MLEN(mbuf);
	mcastFlags = IX_ETHACC_NE_FLAGS(mbuf);

	/* allocate new skb and "swap" it with the skb that is tied to the
	 * mbuf. then return the mbuf + new skb to the NPEs.
	 */
	skb = dev_skb_dequeue(priv);
	
	/* extract skb from mbuf and replace it with the new skbuf */
	skb = mbuf_swap_skb(mbuf, skb);

	if (IX_OSAL_MBUF_PRIV(mbuf))
	{
	    TRACE;

	    /* a skb is attached to the mbuf */
	    dev_rx_buff_replenish(priv->port_id, mbuf);
	}
	else
	{
	    /* failed to alloc skb -> return mbuf to the pool, it'll be
	     * picked up later by the monitoring task when skb will
	     * be available again
	     */
	    TRACE;

	    IX_OSAL_MBUF_POOL_PUT(mbuf);
	}

	/* set the length of the received skb from the mbuf length  */
	skb->tail = skb->data + len;
	skb->len = len;
	
#ifdef DEBUG_DUMP
	skb_dump("rx", skb);
#endif

#if WITH_ADM6996_SWITCH
	/* Set the skb protocol and set mcast/bcast flags */
	dev_eth_type_trans(mcastFlags, skb, dev, dev->hard_header_len);
#else
	/* Set the skb protocol and set mcast/bcast flags */
	dev_eth_type_trans(mcastFlags, skb, dev);
#endif
	/* update the stats */
	priv->stats.rx_packets++; /* total packets received */
	priv->stats.rx_bytes += len; /* total bytes received */
   
	TRACE;
	
	/* push upstack */
	netif_rx(skb);
    }
    else
    {
	/* netif_rx queue threshold reached, stop hammering the system
	 * or we received an unexpected unsupported chained mbuf
	 */
	TRACE;

	/* update the stats */
	priv->stats.rx_dropped++; 

	/* replenish with unchained mbufs */
	do
	{
	    IX_OSAL_MBUF *next = IX_OSAL_MBUF_NEXT_BUFFER_IN_PKT_PTR(mbuf);
	    IX_OSAL_MBUF_NEXT_BUFFER_IN_PKT_PTR(mbuf) = NULL;
	    IX_OSAL_MBUF_MLEN(mbuf) = priv->replenish_size;
	    dev_rx_buff_replenish(priv->port_id, mbuf);
	    mbuf = next;
	}
	while (mbuf != NULL);
    }
    
    TRACE;
}

/* Set promiscuous/multicast mode for the MAC */
static void dev_set_multicast_list(struct net_device *dev)
{
    int res;
    priv_data_t *priv = dev->priv;
    IxEthAccMacAddr addr1 = {};

/* 4 possible scenarios here
 *
 * scenarios:
 * #1 - promiscuous mode ON
 * #2 - promiscuous mode OFF, accept NO multicast addresses
 * #3 - promiscuous mode OFF, accept ALL multicast addresses
 * #4 - promiscuous mode OFF, accept LIST of multicast addresses 
 */

    TRACE;

    /* if called from irq handler, lock already acquired */
    if (!in_irq())
	spin_lock_irq(&priv->lock);

    /* clear multicast addresses that were set the last time (if exist) */
    ixEthAccPortMulticastAddressLeaveAll (priv->port_id);

/**** SCENARIO #1 ****/
    /* Set promiscuous mode */
    if (dev->flags & IFF_PROMISC)
    {
	if ((res = ixEthAccPortPromiscuousModeSet(priv->port_id)))
	{
	    P_ERROR("%s: ixEthAccPortPromiscuousModeSet failed on port %d\n",
		    dev->name, priv->port_id);
	}
	else
	{
	    /* avoid redundant messages */
	    if (!(priv->devFlags & IFF_PROMISC))
	    {
		P_VERBOSE("%s: Entering promiscuous mode\n", dev->name);
	    }
	    priv->devFlags = dev->flags;
	}

	goto Exit;
    }


/**** SCENARIO #2 ****/

    /* Clear promiscuous mode */
    if ((res = ixEthAccPortPromiscuousModeClear(priv->port_id)))
    {
	/* should not get here */
	P_ERROR("%s: ixEthAccPortPromiscuousModeClear failed for port %d\n",
		dev->name, priv->port_id);
    }
    else
    {
	/* avoid redundant messages */
	if (priv->devFlags & IFF_PROMISC)
	{
	    P_VERBOSE("%s: Leaving promiscuous mode\n", dev->name);
	}
	priv->devFlags = dev->flags;
    }


/**** SCENARIO #3 ****/
    /* If there's more addresses than we can handle, get all multicast
     * packets and sort the out in software
     */
    /* Set multicast mode */
    if ((dev->flags & IFF_ALLMULTI) || 
	(dev->mc_count > IX_ETH_ACC_MAX_MULTICAST_ADDRESSES))
    {
	/* ALL MULTICAST addresses will be accepted */
        ixEthAccPortMulticastAddressJoinAll(priv->port_id);

	P_VERBOSE("%s: Accepting ALL multicast packets\n", dev->name);
	goto Exit;
    }


/**** SCENARIO #4 ****/
    /* Store all of the multicast addresses in the hardware filter */
    if ((dev->mc_count))
    {
	/* now join the current address list */
	/* Get only multicasts from the list */
	struct dev_mc_list *mc_ptr;

	/* Rewrite each multicast address */
	for (mc_ptr = dev->mc_list; mc_ptr; mc_ptr = mc_ptr->next)
	{
	    memcpy (&addr1.macAddress[0], &mc_ptr->dmi_addr[0],
		    IX_IEEE803_MAC_ADDRESS_SIZE);
	    ixEthAccPortMulticastAddressJoin (priv->port_id, &addr1);
	}
    }

Exit:
    if (!in_irq())
	spin_unlock_irq(&priv->lock);
}

/* The QMgr dispatch entry point can be called from the 
 * IX_OSAL_IXP400_QM1_IRQ_LVL irq (which will trigger
 * an interrupt for every packet) or a timer (which will
 * trigger interrupts on a regular basis). The PMU
 * timer offers the greatest performances and flexibility.
 *
 * This function setup the datapath in polling mode 
 * and is protected against multiple invocations. It should be
 * called at initialisation time.
 */
static int ethAcc_datapath_poll_setup(void)
{
  static int poll_setup_done = 0;
  IxEthDBPropertyType property_type = 0;
  IxEthAccPortId portId;
  
  if (poll_setup_done == 0)
  {
      /* Determine what queue ID is mapped to the port of ixp0 and QOS class 0 for RX.
       * This will be the RX queue to poll (RX for all ports arrives on the same queue for 
       * a given priority class)  Higher priority queues will be handled by qmgr irq. 
       * Disable the queue ID and store for callback
       */ 
      portId = default_portId[0];

      if(IX_ETH_DB_SUCCESS == ixEthDBFeaturePropertyGet(portId,
			IX_ETH_DB_VLAN_QOS,
			IX_ETH_DB_QOS_TRAFFIC_CLASS_0_RX_QUEUE_PROPERTY,
                        &property_type,
                        (void *)&rx_queue_id))
      {
	  if (property_type == IX_ETH_DB_INTEGER_PROPERTY)
	  {
	      /* This port/class has an associated queue.*/
	      /* Disable interrupts on this queue and store ID for polling */
	      ixQMgrNotificationDisable(rx_queue_id);
	  }
      }

    if(rx_queue_id == IX_QMGR_MAX_NUM_QUEUES)
    {
        P_ERROR("Failed to find RX queue ID!\n");
        return -1;
    }
    
    /* remove txdone queue from qmgr dispatcher */
    ixQMgrNotificationDisable(IX_ETH_ACC_TX_FRAME_DONE_ETH_Q);

    /* poll the datapath from a timer IRQ */
    if (request_irq(IX_OSAL_IXP400_XSCALE_PMU_IRQ_LVL,
                    dev_poll_os_isr,
                    SA_SHIRQ,
                    MODULE_NAME,
                    (void *)IRQ_ANY_PARAMETER))
    {
        P_ERROR("Failed to reassign irq to PMU timer interrupt!\n");
        return -1;
    }
    irq_pmu_used = 1;

    if (dev_pmu_timer_init())
    {
        P_ERROR("Error initialising IXP400 PMU timer!\n");
        return -1;
    }

    TRACE;

    dev_pmu_timer_restart(); /* set up the timer for the next interrupt */
    
    /* set a static flag for re-entrancy */
    poll_setup_done = 1;
  }
  return 0;
}

/* Enable the MAC port.
 * Called on do_dev_open, dev_tx_timeout and mtu size changes
 */
static int port_enable(struct net_device *dev)
{
    int res;
    IxEthAccMacAddr npeMacAddr;
    priv_data_t *priv = dev->priv;

    P_DEBUG("port_enable(%s)\n", dev->name);

    /* Set MAC addr in h/w (ethAcc checks for MAC address to be valid) */
    memcpy(&npeMacAddr.macAddress, dev->dev_addr,IX_IEEE803_MAC_ADDRESS_SIZE);
    if ((res = ixEthAccPortUnicastMacAddressSet(priv->port_id, &npeMacAddr)))
    {
        P_VERBOSE("Failed to set MAC address %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x for port %d\n",
	       (unsigned)npeMacAddr.macAddress[0],
	       (unsigned)npeMacAddr.macAddress[1],
	       (unsigned)npeMacAddr.macAddress[2],
	       (unsigned)npeMacAddr.macAddress[3],
	       (unsigned)npeMacAddr.macAddress[4],
	       (unsigned)npeMacAddr.macAddress[5],
	       priv->port_id);
	return convert_error_ethAcc(res);	
    }

    /* restart the link-monitoring thread if necessary */
    if (priv->maintenanceCheckStopped)
    {
	/* Starts the driver monitoring thread, if configured */
	priv->maintenanceCheckStopped = FALSE;
	
	priv->maintenanceCheckThreadId = kernel_thread(dev_media_check_thread,
			  (void *) dev,
			  CLONE_FS | CLONE_FILES);
	if (priv->maintenanceCheckThreadId < 0)
	{
	    P_ERROR("%s: Failed to start thread for media checks\n", dev->name);
	    priv->maintenanceCheckStopped = TRUE;
	}
    }

    /* force replenish if necessary */
    dev_rx_buff_prealloc(priv);

    /* set the callback supporting the traffic */
    ixEthAccPortTxDoneCallbackRegister(priv->port_id, 
				       tx_done_cb,
				       (UINT32)dev);
    ixEthAccPortRxCallbackRegister(priv->port_id, 
				   rx_cb, 
				   (UINT32)dev);
    

    if ((res = ixEthAccPortEnable(priv->port_id)))
    {
	P_ERROR("%s: ixEthAccPortEnable failed for port %d, res = %d\n",
		dev->name, priv->port_id, res);
	return convert_error_ethAcc(res);
    }

    /* Do not enable aging unless learning is enabled (nothing to age otherwise) */
    if (npe_learning)
    {
        if ((res = ixEthDBPortAgingEnable(priv->port_id)))
        {
            P_ERROR("%s: ixEthDBPortAgingEnable failed for port %d, res = %d\n",
                    dev->name, priv->port_id, res);
            return -1;
        }
    }

    TRACE;

    /* reset the current time for the watchdog timer */
    dev->trans_start = jiffies;

    netif_start_queue(dev);

    TRACE;

#ifdef CONFIG_IXP400_ETH_QDISC_ENABLED
    /* restore the driver's own TX queueing discipline */
    dev->qdisc_sleeping = priv->qdisc;
    dev->qdisc = priv->qdisc;
#endif

    TRACE;

    return 0;
}

/* Disable the MAC port.
 * Called on do_dev_stop and dev_tx_timeout
 */
static void port_disable(struct net_device *dev)
{
    priv_data_t *priv = dev->priv;
    int res;
    IX_STATUS status;

    P_DEBUG("port_disable(%s)\n", dev->name);

    if (!netif_queue_stopped(dev))
    {
        dev->trans_start = jiffies;
        netif_stop_queue(dev);
    }

    if (priv->maintenanceCheckStopped)
    {
	/* thread is not running */
    }
    else
    {
	/* thread is running */
	priv->maintenanceCheckStopped = TRUE;
	/* Wake up the media-check thread with a signal.
	   It will check the 'running' flag and exit */
	if ((res = kill_proc (priv->maintenanceCheckThreadId, SIGKILL, 1)))
	{
	    P_ERROR("%s: unable to signal thread\n", dev->name);
	}
	else
	{
	    /* wait for the thread to exit. */
	    down (priv->maintenanceCheckThreadComplete);
	    up (priv->maintenanceCheckThreadComplete);
	}
    }

    /* Set callbacks when port is disabled */
    ixEthAccPortTxDoneCallbackRegister(priv->port_id, 
				       tx_done_disable_cb,
				       (UINT32)dev);
    ixEthAccPortRxCallbackRegister(priv->port_id, 
				   rx_disable_cb, 
				   (UINT32)dev);

    if ((status = ixEthAccPortDisable(priv->port_id)) != IX_SUCCESS)
    {
	/* should not get here */
	P_ERROR("%s: ixEthAccPortDisable(%d) failed\n",
		dev->name, priv->port_id);
    }

    /* remove all entries from the sw queues */
    dev_skb_queue_drain(priv);
    dev_tx_mb_queue_drain(priv);
    dev_rx_mb_queue_drain(priv);

    TRACE;
}


/* this function is called by the kernel to transmit packet 
 * It is expected to run in the context of the ksoftirq thread.
*/
static int dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    int res;
    priv_data_t *priv = dev->priv;
    IX_OSAL_MBUF *mbuf;

//	printf("dev hard start xmit\r\n" );

    TRACE;

    /* get mbuf struct from tx software queue */
    mbuf = dev_tx_mb_dequeue(priv);

    if (mbuf == NULL)
    {
		/* No mbuf available, free the skbuf */
		dev_kfree_skb_any(skb);
		priv->stats.tx_dropped++;
		if (!netif_queue_stopped(dev))
		{
			ixEthAccPortTxDoneCallbackRegister(priv->port_id, 
                                               tx_done_queue_stopped_cb, (UINT32)dev);
			dev->trans_start = jiffies;
			netif_stop_queue (dev);
		}
		return 0;
	}

#ifdef DEBUG_DUMP
    skb_dump("tx", skb);
#endif

	mbuf_swap_skb(mbuf, skb); /* this should return NULL, as mbufs in the pool have no skb attached */

	/* set ethernet flags to zero */
	IX_ETHACC_NE_FLAGS(mbuf) = 0;

	/* flush the mbuf data from the cache */
	IX_OSAL_CACHE_FLUSH(IX_OSAL_MBUF_MDATA(mbuf), IX_OSAL_MBUF_MLEN(mbuf));

	if ((res = ixEthAccPortTxFrameSubmit(priv->port_id, mbuf, IX_ETH_ACC_TX_DEFAULT_PRIORITY)))
	{
		dev_kfree_skb_any(skb);
		priv->stats.tx_dropped++;
		P_ERROR("%s: ixEthAccPortTxFrameSubmit failed for port %d, res = %d\n",	dev->name, priv->port_id, res);
		return convert_error_ethAcc(res);
	}

	TRACE;
	return 0;
}

/* Open the device.
 * Request resources and start interrupts
 */
static int do_dev_open(struct net_device *dev)
{
    int res;

    /* prevent the maintenance task from running while bringing up port */
    down(maintenance_mutex);

    /* bring up the port */
    res = port_enable(dev);

    up(maintenance_mutex);

    return res;
}

/* Close the device.
 * Free resources acquired in dev_start
 */
static int do_dev_stop(struct net_device *dev)
{
    TRACE;

    /* prevent the maintenance task from running while bringing up port */
    down(maintenance_mutex);

    /* bring the port down */
    port_disable(dev);

    up(maintenance_mutex);

    return 0;
}

static void
dev_tx_timeout_task(void *dev_id)
{
    struct net_device *dev = (struct net_device *)dev_id;
    priv_data_t *priv = dev->priv;

    P_WARN("%s: Tx Timeout for port %d\n", dev->name, priv->port_id);

    down(maintenance_mutex);
    port_disable(dev);

    /* Note to user: Consider performing other reset operations here
     * (such as PHY reset), if it is known to help the Tx Flow to 
     * become "unstuck". This scenario is application/board-specific.
     * 
     * e.g.
     *
     *  if (netif_carrier_ok(dev))
     *  {
     *	down(miiAccessMutex);
     *	ixEthMiiPhyReset(phyAddresses[priv->port_id]);
     *	up(miiAccessMutex);
     *  }
     */
    
    /* enable traffic again if the port is up */
    if (dev->flags & IFF_UP)
    {
	port_enable(dev);
    }

    up(maintenance_mutex);
}


/* This function is called when kernel thinks that TX is stuck */
static void dev_tx_timeout(struct net_device *dev)
{
	priv_data_t *priv = dev->priv;
	TRACE;
#if WITH_ADM6996_SWITCH
	if(dev == &adm6996_clone_dev)
		schedule_task(&priv->cloned_tq_timeout);
	else
		schedule_task(&priv->tq_timeout);
#else
	schedule_task(&priv->tq_timeout);
#endif	
    
}

/* update the maximum msdu value for this device */
static void dev_change_msdu(struct net_device *dev, int new_msdu_size)
{
    priv_data_t *priv = dev->priv;
    unsigned int new_size = new_msdu_size;

    priv->msdu_size = new_size;

    /* ensure buffers are large enough (do not use too small buffers
     * even if it is possible to configure so. 
     */
    if (new_size < IX_ETHNPE_ACC_FRAME_LENGTH_DEFAULT)
    {
	new_size = IX_ETHNPE_ACC_FRAME_LENGTH_DEFAULT;
    }

    /* the NPE needs 64 bytes boundaries : round-up to the next
    * frame boundary. This size is used to invalidate and replenish.
    */
    new_size = IX_ETHNPE_ACC_RXFREE_BUFFER_ROUND_UP(new_size);
    priv->replenish_size = new_size;
    
    /* Xscale MMU needs a cache line boundary : round-up to the next
     * cache line boundary. This will be the size used to allocate
     * skbufs from the kernel.
     */
    new_size = HDR_SIZE + new_size;
    new_size = L1_CACHE_ALIGN(new_size);
    priv->pkt_size = new_size;

    /* Linux stack uses a reserved header.
     * skb contain shared info about fragment lists 
     * this will be the value stored in skb->truesize
     */
#if WITH_ADM6996_SWITCH
	/* VLAN_HDR has beed added as parameter of this function, so no add is needed here */
#endif
    priv->alloc_size = SKB_RESERVED_HEADER_SIZE + 
	SKB_DATA_ALIGN(new_size) + SKB_RESERVED_TRAILER_SIZE;
}

static int dev_change_mtu(struct net_device *dev, int new_mtu_size)
{
    priv_data_t *priv = dev->priv;
    /* the msdu size includes the ethernet header plus the 
     * mtu (IP payload), but does not include the FCS which is 
     * stripped out by the access layer.
     */
    unsigned int new_msdu_size = new_mtu_size + dev->hard_header_len + VLAN_HDR;

    if (new_msdu_size > IX_ETHNPE_ACC_FRAME_LENGTH_MAX)
    {
	/* Unsupported msdu size */
	return -EINVAL;
    }

    /* safer to stop maintenance task while bringing port down and up */
    down(maintenance_mutex);

    if (ixEthDBFilteringPortMaximumFrameSizeSet(priv->port_id, 
						new_msdu_size))
    {
	P_ERROR("%s: ixEthDBFilteringPortMaximumFrameSizeSet failed for port %d\n",
		dev->name, priv->port_id);
	up(maintenance_mutex);

	return -1;
    }

    /* update the packet sizes needed */
    dev_change_msdu(dev, new_msdu_size);
    /* update the driver mtu value */
    dev->mtu = new_mtu_size;

    up(maintenance_mutex);

    return 0;
}

static int do_dev_ioctl(struct net_device *dev, struct ifreq *req, int cmd)
{
    priv_data_t *priv = dev->priv;
    struct mii_ioctl_data *data = (struct mii_ioctl_data *) & req->ifr_data;
    int phy = phyAddresses[priv->port_id];
    int res = 0;

    TRACE;

    switch (cmd)
    {
	/*
	 * IOCTL's for mii-tool support
	 */

	/* Get address of MII PHY in use */
	case SIOCGMIIPHY:
	case SIOCDEVPRIVATE:
	    data->phy_id = phy;
	    return 0;

        /* Read MII PHY register */
	case SIOCGMIIREG:		
	case SIOCDEVPRIVATE+1:
	    down (miiAccessMutex);     /* lock the MII register access mutex */
	    if ((res = ixEthAccMiiReadRtn (data->phy_id, data->reg_num, &data->val_out)))
	    {
		P_ERROR("Error reading MII reg %d on phy %d\n",
		       data->reg_num, data->phy_id);
		res = -1;
	    }
	    up (miiAccessMutex);	/* release the MII register access mutex */
	    return res;

	/* Write MII PHY register */
	case SIOCSMIIREG:
	case SIOCDEVPRIVATE+2:
	    down (miiAccessMutex);     /* lock the MII register access mutex */
	    if ((res = ixEthAccMiiWriteRtn (data->phy_id, data->reg_num, data->val_in)))
	    {
		P_ERROR("Error writing MII reg %d on phy %d\n",
                        data->reg_num, data->phy_id);
		res = -1;
	    }
	    up (miiAccessMutex);	/* release the MII register access mutex */
	    return res;

	/* set the MTU size */
	case SIOCSIFMTU:
	    return dev_change_mtu(dev, req->ifr_mtu);

	default:
	    return -EOPNOTSUPP;
    }

    return -EOPNOTSUPP;
}

static struct net_device_stats *dev_get_stats(struct net_device *dev)
{
    int res;
    /* "stats" should be cache-safe.
     * we alligne "stats" and "priv" by 32 bytes, so that the cache
     * operations will not affect "res" and "priv"
     */
    IxEthEthObjStats ethStats __attribute__ ((aligned(32))) = {};
    priv_data_t *priv;

    TRACE;

    priv = dev->priv;

    /* Get HW stats and translate to the net_device_stats */
    if (!netif_running(dev))
    {
	TRACE;
	return &priv->stats;
    }

    TRACE;

    invalidate_dcache_range((unsigned int)&ethStats, sizeof(ethStats));
    if ((res = ixEthAccMibIIStatsGetClear(priv->port_id, &ethStats)))
    {
	P_ERROR("%s: ixEthAccMibIIStatsGet failed for port %d, res = %d\n",
		dev->name, priv->port_id, res);
	return &priv->stats;
    }

    TRACE;

    /* bad packets received */
    priv->stats.rx_errors += 
	ethStats.dot3StatsAlignmentErrors +
	ethStats.dot3StatsFCSErrors +
	ethStats.dot3StatsInternalMacReceiveErrors;

    /* packet transmit problems */
    priv->stats.tx_errors += 
	ethStats.dot3StatsLateCollisions +
	ethStats.dot3StatsExcessiveCollsions +
	ethStats.dot3StatsInternalMacTransmitErrors +
	ethStats.dot3StatsCarrierSenseErrors;

    priv->stats.collisions +=
	ethStats.dot3StatsSingleCollisionFrames +
	ethStats.dot3StatsMultipleCollisionFrames;

    /* recved pkt with crc error */
    priv->stats.rx_crc_errors +=
	ethStats.dot3StatsFCSErrors;

    /* recv'd frame alignment error */
    priv->stats.rx_frame_errors += 
	ethStats.dot3StatsAlignmentErrors;

    /* detailed tx_errors */
    priv->stats.tx_carrier_errors +=
	ethStats.dot3StatsCarrierSenseErrors;

    /* Rx traffic dropped at the NPE level */
    priv->stats.rx_dropped +=
	ethStats.RxOverrunDiscards +
	ethStats.RxLearnedEntryDiscards +
	ethStats.RxLargeFramesDiscards +
	ethStats.RxSTPBlockedDiscards +
	ethStats.RxVLANTypeFilterDiscards +
	ethStats.RxVLANIdFilterDiscards +
	ethStats.RxInvalidSourceDiscards +
	ethStats.RxBlackListDiscards +
	ethStats.RxWhiteListDiscards +
	ethStats.RxUnderflowEntryDiscards;

    /* Tx traffic dropped at the NPE level */
    priv->stats.tx_dropped += 
	ethStats.TxLargeFrameDiscards + 
	ethStats.TxVLANIdFilterDiscards;

    return &priv->stats;
}


/* Initialize QMgr and bind it's interrupts */
static int qmgr_init(void)
{
    int res;

    /* Initialise Queue Manager */
    P_VERBOSE("Initialising Queue Manager...\n");
    if ((res = ixQMgrInit()))
    {
	P_ERROR("Error initialising queue manager!\n");
	return -1;
    }

    TRACE;

    /* Get the dispatcher entrypoint */
    ixQMgrDispatcherLoopGet (&dispatcherFunc);

    TRACE;

    /* The QMgr dispatch entry point can be called from the 
     * IX_OSAL_IXP400_QM1_IRQ_LVL irq (which will trigger
     * an interrupt for every packet) or a timer (which will
     * trigger interrupts on a regular basis). The PMU
     * timer offers the greatest performances and flexibility.
     */
    if (request_irq(IX_OSAL_IXP400_QM1_IRQ_LVL,
                    dev_qmgr_os_isr,
                    SA_SHIRQ,
                    MODULE_NAME,
                    (void *)IRQ_ANY_PARAMETER))
    {
        P_ERROR("Failed to request_irq to Queue Manager interrupt!\n");
        return -1;
    }
    irq_qm1_used = 1;

    TRACE;
    return 0;
}

static int ethacc_uninit(void)
{ 
    int res;

    TRACE;

    /* we should uninitialize the components here */
    if ((res=ixEthDBUnload()))
    {
        P_ERROR("ixEthDBUnload Failed!\n");
    }
    TRACE;

    ixEthAccUnload();

    /* best effort, always succeed and return 0 */
    return 0;
}

static int ethacc_init(void)
{
    int res = 0;
    IxEthAccPortId portId;
    int dev_count;

    /* start all NPEs before starting the access layer */
    TRACE;
    for (dev_count = 0; 
	 dev_count < dev_max_count;  /* module parameter */
	 dev_count++)
    {
	portId = default_portId[dev_count];

        TRACE;
	/* Initialise and Start NPE */
	if ((res = ixNpeDlNpeInitAndStart(default_npeImageId[portId])))
	{
	    P_ERROR("Error starting NPE for Ethernet port %d!\n", portId);
	    return -1;
	}
    }

    /* initialize the Ethernet Access layer */
    TRACE;
    if ((res = ixEthAccInit()))
    {
	P_ERROR("ixEthAccInit failed with res=%d\n", res);
	return convert_error_ethAcc(res);
    }

    TRACE;
    /* Always initialize the ethAcc ports  which access the MII registers */
    if ((res = ixEthAccPortInit(IX_ETH_PORT_1)))
    {
	P_ERROR("Failed to initialize Eth port %u res = %d\n", IX_ETH_PORT_1, res);
	return convert_error_ethAcc(res);
    }


    for (dev_count = 0; 
	 dev_count < dev_max_count;  /* module parameter */
	 dev_count++)
    {
        TRACE;

	portId = default_portId[dev_count];

        /* check if the port is not already initialized */
        if (portId != IX_ETH_PORT_1)
        {
	    /* Initialize the ethAcc ports */
	    if ((res = ixEthAccPortInit(portId)))
	    {
		TRACE;
		P_ERROR("Failed to initialize Eth port %u res = %d\n", portId, res);
		return convert_error_ethAcc(res);
	    }
        }
	
	/* Set Tx scheduling discipline */
	if ((res = ixEthAccTxSchedulingDisciplineSet(portId,
						     FIFO_NO_PRIORITY)))
	{
	    TRACE;
	    return convert_error_ethAcc(res);
	}
	if ((res = ixEthAccPortTxFrameAppendFCSEnable(portId)))
	{
	    TRACE;
	    return convert_error_ethAcc(res);
	}

	if ((res = ixEthAccPortRxFrameAppendFCSDisable(portId)))
	{
	    TRACE;
	    return convert_error_ethAcc(res);
	}
    }
    return 0;
}

static int phy_init(void)
{
    int res;
    BOOL physcan[IXP425_ETH_ACC_MII_MAX_ADDR];
    int i, phy_found, num_phys_to_set, dev_count;

    /* initialise the MII register access mutex */
    miiAccessMutex = (struct semaphore *) kmalloc(sizeof(struct semaphore), GFP_KERNEL);
    if (!miiAccessMutex)
	return -ENOMEM;

    init_MUTEX(miiAccessMutex);

    TRACE;

    /* detect the PHYs (ethMii requires the PHYs to be detected) 
     * and provides a maximum number of PHYs to search for.
     */
    res = ixEthMiiPhyScan(physcan, 
			  sizeof(default_phy_cfg) / sizeof(phy_cfg_t));
    if (res != IX_SUCCESS)
    {
	P_ERROR("PHY scan failed\n");
	return convert_error_ethAcc(res);
    }

    /* Module parameter */
    if (no_phy_scan) 
    { 
	/* Use hardcoded phy addresses */
	num_phys_to_set = (sizeof(default_phy_cfg) / sizeof(phy_cfg_t));
    }
    else
    {
	/* Update the hardcoded values with discovered parameters 
	 *
	 * This set the following mapping
	 *  ixp0 --> first PHY discovered  (lowest address)
	 *  ixp1 --> second PHY discovered (next address)
	 *  .... and so on
	 *
	 * If the Phy address and the wiring on the board do not
	 * match this mapping, then hardcode the values in the
	 * phyAddresses array and use no_phy_scan=1 parameter on 
	 * the command line.
	 */
	for (i=0, phy_found=0; i < IXP425_ETH_ACC_MII_MAX_ADDR; i++)
	{
	    if (physcan[i])
	    {
		P_INFO("Found PHY %d at address %d\n", phy_found, i);
                if (phy_found < dev_max_count)
                {
                    phyAddresses[default_portId[phy_found]] = i;
                }
                else
                {
                    phyAddresses[phy_found] = i;
                }
		
		if (++phy_found == IXP425_ETH_ACC_MII_MAX_ADDR)
		    break;
	    }
	}

	num_phys_to_set = phy_found;
    }

    /* Reset and Set each phy properties */
    for (i=0; i < num_phys_to_set; i++) 
    {
	P_VERBOSE("Configuring PHY %d\n", i);
	P_VERBOSE("\tSpeed %s\tDuplex %s\tAutonegotiation %s\n",
		  (default_phy_cfg[i].speed100) ? "100" : "10",   
		  (default_phy_cfg[i].duplexFull) ? "FULL" : "HALF",  
		  (default_phy_cfg[i].autoNegEnabled) ? "ON" : "OFF");

	if (phy_reset) /* module parameter */
	{
	    ixEthMiiPhyReset(phyAddresses[i]);
	}

	ixEthMiiPhyConfig(phyAddresses[i],
	    default_phy_cfg[i].speed100,   
	    default_phy_cfg[i].duplexFull,  
	    default_phy_cfg[i].autoNegEnabled);
    }

    /* for each device, display the mapping between the ixp device,
     * the IxEthAcc port, the NPE and the PHY address on MII bus. 
     * Also set the duplex mode of the MAC core depending
     * on the default configuration.
     */
    for (dev_count = 0; 
	 dev_count < dev_max_count  /* module parameter */
	     && dev_count <  num_phys_to_set;
	 dev_count++)
    {
	IxEthAccPortId port_id = default_portId[dev_count];
	char *npe_id = "?";

	if (port_id == IX_ETH_PORT_1) npe_id = "B";
	if (port_id == IX_ETH_PORT_2) npe_id = "C";
	if (port_id == IX_ETH_PORT_3) npe_id = "A";

	P_INFO("%s%d is using NPE%s and the PHY at address %d\n",
	       DEVICE_NAME, dev_count, npe_id, phyAddresses[port_id]);

	/* Set the MAC to the same duplex mode as the phy */
	ixEthAccPortDuplexModeSet(port_id,
            (default_phy_cfg[port_id].duplexFull) ?
                 IX_ETH_ACC_FULL_DUPLEX : IX_ETH_ACC_HALF_DUPLEX);
    }

    return 0;
}

/* set port MAC addr and update the dev struct if successfull */
int dev_set_mac_address(struct net_device *dev, void *addr)
{
    int res;
    priv_data_t *priv = dev->priv;
    IxEthAccMacAddr npeMacAddr;
    struct sockaddr *saddr = (struct sockaddr *)addr;

    /* Get MAC addr from parameter */
    memcpy(&npeMacAddr.macAddress,
	   &saddr->sa_data[0],
	   IX_IEEE803_MAC_ADDRESS_SIZE);

    /* Set MAC addr in h/w (ethAcc checks for MAC address to be valid) */
    if ((res = ixEthAccPortUnicastMacAddressSet(priv->port_id, &npeMacAddr)))
    {
        P_VERBOSE("Failed to set MAC address %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x for port %d\n",
	       (unsigned)npeMacAddr.macAddress[0],
	       (unsigned)npeMacAddr.macAddress[1],
	       (unsigned)npeMacAddr.macAddress[2],
	       (unsigned)npeMacAddr.macAddress[3],
	       (unsigned)npeMacAddr.macAddress[4],
	       (unsigned)npeMacAddr.macAddress[5],
	       priv->port_id);
        return convert_error_ethAcc(res);
    }

    /* update dev struct */
    memcpy(dev->dev_addr, 
	   &saddr->sa_data[0],
	   IX_IEEE803_MAC_ADDRESS_SIZE);

    return 0;
}


/* 
 *  TX QDISC
 */

#ifdef CONFIG_IXP400_ETH_QDISC_ENABLED

/* new tx scheduling discipline : the algorithm is based on a 
 * efficient JBI technology : Just Blast It. There is no need for
 * software queueing where the hardware provides this feature
 * and makes the internal transmission non-blocking.
 *
 * because of this reason, there is no need for throttling using
 * netif_stop_queue() and netif_start_queue() (there is no sw queue
 * that linux may restart)
 *
 * This tx queueing scheduling mechanism can be enabled
 * by defining CONFIG_IXP400_ETH_QDISC_ENABLED at compile time
 */
static int dev_qdisc_no_enqueue(struct sk_buff *skb, struct Qdisc * qdisc)
{
	TRACE;
        return dev_hard_start_xmit(skb, qdisc->dev);     
}

static struct sk_buff *dev_qdisc_no_dequeue(struct Qdisc * qdisc)
{
	return NULL;
}

static struct Qdisc_ops dev_qdisc_ops =
{
	NULL, NULL, "ixp400_eth", 0,
	dev_qdisc_no_enqueue, 
	dev_qdisc_no_dequeue,
	dev_qdisc_no_enqueue, 
	NULL, 
	NULL, NULL, NULL, NULL, NULL
};

#endif

/* Initialize device structs.
 * Resource allocation is deffered until do_dev_open
 */
/* WAN-0 */ 
static int __devinit dev_eth_probe(struct net_device *dev)
{
	priv_data_t *priv;

#if MAC_BOOTLOADER
/* add conditional compile for MAC address, lizhijie 2005.01.25 */
	unsigned char assist_mac[IX_IEEE803_MAC_ADDRESS_SIZE];
    TRACE;
#endif

    /* there is a limited number of devices */
//    if (found_devices >= dev_max_count) /* module parameter */
//	return -ENODEV;

    SET_MODULE_OWNER(dev);

    /* set device name */
#if WITH_ADM6996_SWITCH
	sprintf(dev->name, DEVICE_NAME "%d", RAW_WAN_POSITION );
#else
	sprintf(dev->name, DEVICE_NAME "%d", 0);
#endif

    /* allocate and initialize priv struct */
    priv = dev->priv = kmalloc(sizeof(priv_data_t), GFP_KERNEL);
    if (dev->priv == NULL)
	return -ENOMEM;

    memset(dev->priv, 0, sizeof(priv_data_t));

    TRACE;

    /* set the mapping between port ID and devices 
     * 
     */
#if WITH_ADM6996_SWITCH
/* this item must be modified when WAN as eth0 and eth1, lizhijie, 2005.11.23 */
	priv->port_id  = default_portId[RAW_WAN_POSITION-1];
#else
	priv->port_id  = default_portId[0];
#endif

    TRACE;

    /* initialize RX pool */
    priv->rx_pool = IX_OSAL_MBUF_POOL_INIT(RX_MBUF_POOL_SIZE, 0,
				           "IXP400 Ethernet driver Rx Pool");
    if(priv->rx_pool == NULL)
    {
	P_ERROR("%s: Buffer RX Pool init failed on port %d\n",
		dev->name, priv->port_id);
	kfree(dev->priv);
	return -ENOMEM;
    }

    TRACE;

    /* initialize TX pool */
    priv->tx_pool = IX_OSAL_MBUF_POOL_INIT(TX_MBUF_POOL_SIZE, 0, 
				           "IXP400 Ethernet driver Tx Pool");
    if(priv->tx_pool == NULL)
    {
	P_ERROR("%s: Buffer TX Pool init failed on port %d\n",
		dev->name, priv->port_id);
	kfree(dev->priv);
	return -ENOMEM;
    }

     TRACE;

   /* initialise the MII register access mutex */
    priv->maintenanceCheckThreadComplete = (struct semaphore *)
	kmalloc(sizeof(struct semaphore), GFP_KERNEL);
    if (!priv->maintenanceCheckThreadComplete)
    {
	kfree(dev->priv);
	return -ENOMEM;
    }
    priv->lock = SPIN_LOCK_UNLOCKED;
    init_MUTEX(priv->maintenanceCheckThreadComplete);
    priv->maintenanceCheckStopped = TRUE;

    /* initialize ethernet device (default handlers) */
    ether_setup(dev);

    TRACE;

     /* fill in dev struct callbacks with customized handlers */
    dev->open = do_dev_open;
    dev->stop = do_dev_stop;

    dev->hard_start_xmit = dev_hard_start_xmit;

    dev->watchdog_timeo = DEV_WATCHDOG_TIMEO;
    dev->tx_timeout = dev_tx_timeout;
    dev->change_mtu = dev_change_mtu;
    dev->do_ioctl = do_dev_ioctl;
    dev->get_stats = dev_get_stats;
    dev->set_multicast_list = dev_set_multicast_list;
    dev->flags |= IFF_MULTICAST;

    dev->set_mac_address = dev_set_mac_address;

    TRACE;

    /* Defines the unicast MAC address
     *
     * Here is a good place to read a board-specific MAC address
     * from a non-volatile memory, e.g. an external eeprom.
     * 
     * This memcpy uses a default MAC address from this
     * source code.
     *
     * This can be overriden later by the (optional) command
     *
     *     ifconfig ixp0 ether 0002b3010101
     *
     */

/* added by lizhijie 2004.10.07 */
#if MAC_BOOTLOADER
/* add conditional compile, lizhijie 2005.01.25 */
	if(assist_get_hardware_address(priv->port_id, assist_mac) )
		memcpy(assist_mac,  &default_mac_addr[priv->port_id].macAddress, IX_IEEE803_MAC_ADDRESS_SIZE);
	memcpy(dev->dev_addr, assist_mac, IX_IEEE803_MAC_ADDRESS_SIZE);
#else
	memcpy(dev->dev_addr, &default_mac_addr[priv->port_id].macAddress, IX_IEEE803_MAC_ADDRESS_SIZE);
#endif	

    /* possibly remove this test and the message when a valid MAC address 
     * is not hardcoded in the driver source code. 
     */
    if (is_valid_ether_addr(dev->dev_addr))
    {
	P_WARN("Use default MAC address %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x for port %d\n",
	       (unsigned)dev->dev_addr[0],
	       (unsigned)dev->dev_addr[1],
	       (unsigned)dev->dev_addr[2],
	       (unsigned)dev->dev_addr[3],
	       (unsigned)dev->dev_addr[4],
	       (unsigned)dev->dev_addr[5],
	       priv->port_id);
    }
    
    /* Set/update the internal packet size 
     * This can be overriden later by the command
     *                      ifconfig ixp0 mtu 1504
     */
    TRACE;

    dev_change_msdu(dev, dev->mtu + dev->hard_header_len + VLAN_HDR);

    priv->tq_timeout.routine = dev_tx_timeout_task;
    priv->tq_timeout.data = (void *)dev;

#ifdef CONFIG_IXP400_ETH_QDISC_ENABLED
    /* configure and enable a fast TX queuing discipline */
    TRACE;

    priv->qdisc = qdisc_create_dflt(dev, &dev_qdisc_ops);
    dev->qdisc_sleeping = priv->qdisc;
    dev->qdisc = priv->qdisc;
    
    if (!dev->qdisc_sleeping)
    {
	P_ERROR("%s: qdisc_create_dflt failed on port %d\n",
		dev->name, priv->port_id);
	kfree(dev->priv);
	return -ENOMEM;
    }
#endif

    /* set the internal maximum queueing capabilities */
    dev->tx_queue_len = TX_MBUF_POOL_SIZE;

    if (!netif_queue_stopped(dev))
    {
	TRACE;

        dev->trans_start = jiffies;
        netif_stop_queue(dev);
    }

//    found_devices++;

    TRACE;

    return 0;
}


#if  WITH_ADM6996_SWITCH


/* WAN1 and LAN device */
static int cloned_dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats;
	unsigned short vlanId;
	priv_data_t  *priv ;

	if(!dev)
		return -ENODEV;
	priv = dev->priv;

	if(dev==(&adm6996_clone_dev)  )
	{
		vlanId = VLAN_ID_OF_LAN;
		stats = &(sharedPriv->clone_stats);
	}
	else //if(vlanId = VLAN_ID_OF_WAN)
	{
		vlanId = VLAN_ID_OF_WAN;
		stats = &(sharedPriv->stats);
	}

	struct vlan_ethhdr *veth = (struct vlan_ethhdr *)(skb->data);

	if (veth->h_vlan_proto != __constant_htons(ETH_P_8021Q))
	{/* if packet with VLAN ID ,the send it to device directly */

		if (skb_headroom(skb) < VLAN_HLEN)
		{
#if 0		 
			TRACE;
			struct sk_buff *sk_tmp = skb;
			skb = skb_realloc_headroom(sk_tmp, VLAN_HLEN);
			TRACE;

		dev_kfree_skb_any(skb);
			
			kfree_skb(sk_tmp);
			if (skb == NULL)
			{
				stats->tx_dropped++;
				return 0;
			}
#else
			/* after modify dev_change_msdu and dev_skb_dequeu, following code */
			/* only in the case of after change mtu  */
		 	P_ERROR("No space is left for VLAN, so this packet is drop\n");
			/* return this skb to recycle queue */
			dev_skb_enqueue( priv, skb);
			stats->tx_dropped++;
#endif
		}

		/* move skb->header point 4 bytes to left */
		veth = (struct vlan_ethhdr *)skb_push(skb, VLAN_HLEN);

		/* Move the mac addresses to the beginning of the new header. */
		memmove(skb->data, skb->data + VLAN_HLEN, 12);

		TRACE;

		veth->h_vlan_proto = __constant_htons(ETH_P_8021Q);
		veth->h_vlan_TCI = htons(vlanId);
	}

#if 0
	skb_dump(skb->dev->name,  skb);
#endif

	stats->tx_packets++; 
	stats->tx_bytes += skb->len;

	TRACE;

	return dev_hard_start_xmit( skb,  dev);
}


#ifdef CONFIG_IXP400_ETH_QDISC_ENABLED
#if 0
static int cloned_dev_qdisc_no_enqueue(struct sk_buff *skb, struct Qdisc * qdisc)
{
	printf("cloned Qdisc of device \r\n"  );
//	printf("cloned Qdisc of device %s\r\n" ,qdisc->dev->name );
	return cloned_dev_hard_start_xmit(skb, qdisc->dev);     
}
#endif


/* Qdisc and ops must be created in every net_device.
* ops set to null, so no ops are used and then dev_hard_start_xmit are called directly.
*/
static struct Qdisc_ops cloned_dev_qdisc_ops =
{
	NULL, NULL, "ixp400_eth_adm", 0,
	0/*cloned_dev_qdisc_no_enqueue*/, 
	0/*dev_qdisc_no_dequeue*/,
	0/*cloned_dev_qdisc_no_enqueue*/, 
	NULL, 
	NULL, NULL, NULL, NULL, NULL
};

#endif


/*
* return 0 : send to upper layer , 1 : error 
*/ 
static inline int cloned_dev_eth_type_trans(unsigned int mflags, struct sk_buff *skb)
{
	unsigned short vid;
	unsigned short vlan_TCI;
	struct vlan_ethhdr *veth;
	struct net_device *dev = NULL;
    	struct net_device_stats *stats;
		
	veth = (struct vlan_ethhdr *)(skb->data);
	
	vlan_TCI = ntohs(veth->h_vlan_TCI);
	vid =   (vlan_TCI & VLAN_VID_MASK);
#if 0	
	printf("[rx_cb] vlan TCI = 0x%x\r\n",vlan_TCI);
	printf("[rx_cb] vid  0x%x\r\n",vid);
#endif

	if(vid == VLAN_ID_OF_WAN )
	{
		dev = sharedPriv->rawDev;
		stats = &(sharedPriv->stats);
	}
	else if(vid == VLAN_ID_OF_LAN )
	{
		dev = sharedPriv->clonedDev;
		stats = &(sharedPriv->clone_stats);
	}
	else
	{
		P_ERROR("No device is attached to VLAN with ID %d\n", vid);
		sharedPriv->stats.rx_dropped++; 		/* total packets received */
		return -ENOMEDIUM;	/**/
	}
	stats->rx_packets++; /* total packets received */
	stats->rx_bytes += skb->len; /* total bytes received */

#ifdef DEBUG_DUMP
	skb_dump("before", skb);
#endif

#if 1	
	/* first 12 bytes are shift right 4 position */
	/* for optimize performance, this case is in used*/
	memcpy(skb->data+VLAN_HDR, skb->data, ETH_ALEN*2);
	skb_pull(skb, VLAN_HDR);
#else
	memcpy(skb->data+ETH_ALEN*2,  skb->data+VLAN_HDR+ETH_ALEN*2, skb->len-ETH_ALEN*2);
#endif

#ifdef DEBUG_DUMP
	skb_dump("after", skb);
#endif

	/* Important Note  
	* here, hard_header_length must be ETH_HLEN for reorder to frame
	*/
	dev_eth_type_trans( mflags, skb,  dev, ETH_HLEN);
#ifdef DEBUG_DUMP
	skb_dump("finally", skb);
#endif

	return 0;
}


/*  CallBack for packet received */
static void cloned_rx_cb(UINT32 callbackTag, IX_OSAL_MBUF *mbuf, IxEthAccPortId portId)
{
	priv_data_t *priv;
	struct sk_buff *skb;
	int len;
	unsigned int mcastFlags;
	unsigned int qlevel;

	TRACE;
	priv = (priv_data_t *)callbackTag;

	qlevel = softnet_data[0].input_pkt_queue.qlen;

	/* check if the system accepts more traffic and against chained mbufs 
	*/

	if ((qlevel < netdev_max_backlog)
		&& (IX_OSAL_MBUF_NEXT_PKT_IN_CHAIN_PTR(mbuf) == NULL))
	{
		/* the netif_rx queue is not overloaded */
		TRACE;
		len = IX_OSAL_MBUF_MLEN(mbuf);
		mcastFlags = IX_ETHACC_NE_FLAGS(mbuf);

		/* allocate new skb and "swap" it with the skb that is tied to the
		*   mbuf. then return the mbuf + new skb to the NPEs.
		*/
		skb = dev_skb_dequeue(priv);

		/* extract skb from mbuf and replace it with the new skbuf */
		skb = mbuf_swap_skb(mbuf, skb);

		if (IX_OSAL_MBUF_PRIV(mbuf))
		{
			TRACE;
			/* a skb is attached to the mbuf */
			dev_rx_buff_replenish(priv->port_id, mbuf);
		}
		else
		{
			/* failed to alloc skb -> return mbuf to the pool, it'll be
			* picked up later by the monitoring task when skb will be available again
			*/
			TRACE;
			IX_OSAL_MBUF_POOL_PUT(mbuf);
		}

		/* set the length of the received skb from the mbuf length  */
		skb->tail = skb->data + len;
		skb->len = len;

#ifdef DEBUG_DUMP
		skb_dump("rx", skb);
#endif

		if( cloned_dev_eth_type_trans( mcastFlags,  skb) )
		{/* Now this skb is orphan, so can be drop */
			dev_kfree_skb_any(skb);
			return;
		}
		TRACE;

		/* push upstack */
		netif_rx(skb);
    }
    else
    {
		TRACE;
		/* update the stats */
		priv->stats.rx_dropped++;

		/* replenish with unchained mbufs */
		do
		{
			IX_OSAL_MBUF *next = IX_OSAL_MBUF_NEXT_BUFFER_IN_PKT_PTR(mbuf);
			IX_OSAL_MBUF_NEXT_BUFFER_IN_PKT_PTR(mbuf) = NULL;
			IX_OSAL_MBUF_MLEN(mbuf) = priv->replenish_size;
			dev_rx_buff_replenish(priv->port_id, mbuf);
			mbuf = next;
		}
		while (mbuf != NULL);
	}
    
	TRACE;
}

/* Enable the MAC port.
 * Called on do_dev_open, dev_tx_timeout and mtu size changes
 */
static int cloned_port_enable(struct net_device *dev)
{
	int res;
	IxEthAccMacAddr npeMacAddr;
	priv_data_t *priv = dev->priv;

	/* Set MAC addr in h/w (ethAcc checks for MAC address to be valid) */
	memcpy(&npeMacAddr.macAddress, dev->dev_addr,IX_IEEE803_MAC_ADDRESS_SIZE);
	if ((res = ixEthAccPortUnicastMacAddressSet(priv->port_id, &npeMacAddr)))
	{
		P_VERBOSE("Failed to set MAC address %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x for port %d\n",
			(unsigned)npeMacAddr.macAddress[0], (unsigned)npeMacAddr.macAddress[1], (unsigned)npeMacAddr.macAddress[2],
			(unsigned)npeMacAddr.macAddress[3], (unsigned)npeMacAddr.macAddress[4], (unsigned)npeMacAddr.macAddress[5], priv->port_id);
		return convert_error_ethAcc(res);	
	}

	if( priv->refCount ==0 )
	{
		/* restart the link-monitoring thread if necessary */
		if (priv->maintenanceCheckStopped)
		{
			/* Starts the driver monitoring thread, if configured */
			priv->maintenanceCheckStopped = FALSE;
			priv->maintenanceCheckThreadId = kernel_thread(dev_media_check_thread,
				  (void *) dev, CLONE_FS | CLONE_FILES);
			if (priv->maintenanceCheckThreadId < 0)
			{
				P_ERROR("%s: Failed to start thread for media checks\n", dev->name);
				priv->maintenanceCheckStopped = TRUE;
			}
		}

		/* force replenish if necessary */
		dev_rx_buff_prealloc(priv);

		/* set the callback supporting the traffic */
		ixEthAccPortTxDoneCallbackRegister(priv->port_id, tx_done_cb, (UINT32)dev);
		ixEthAccPortRxCallbackRegister(priv->port_id, cloned_rx_cb,  (UINT32)priv);

		if ((res = ixEthAccPortEnable(priv->port_id)))
		{
			P_ERROR("%s: ixEthAccPortEnable failed for port %d, res = %d\n", dev->name, priv->port_id, res);
			return convert_error_ethAcc(res);
		}

		/* Do not enable aging unless learning is enabled (nothing to age otherwise) */
		if (npe_learning)
		{
			if ((res = ixEthDBPortAgingEnable(priv->port_id)))
			{
				P_ERROR("%s: ixEthDBPortAgingEnable failed for port %d, res = %d\n", dev->name, priv->port_id, res);
				return -1;
			}
		}
	}
	
	TRACE;
	/* reset the current time for the watchdog timer */
	dev->trans_start = jiffies;
	netif_start_queue(dev);

	TRACE;
#ifdef CONFIG_IXP400_ETH_QDISC_ENABLED
	/* restore the driver's own TX queueing discipline */
	if(dev==&(adm6996_clone_dev) )
	{
		dev->qdisc_sleeping = priv->cloned_qdisc;
		dev->qdisc = priv->cloned_qdisc;
	}
	else
	{
		dev->qdisc_sleeping = priv->qdisc;
		dev->qdisc = priv->qdisc;
	}
#endif

	priv->refCount ++;

	TRACE;
	return 0;
}

/* Disable the MAC port.
 * Called on do_dev_stop and dev_tx_timeout
 */
static void cloned_port_disable(struct net_device *dev)
{
	priv_data_t *priv = dev->priv;
	int res;
	IX_STATUS status;

	if (!netif_queue_stopped(dev))
	{
		dev->trans_start = jiffies;
		netif_stop_queue(dev);
	}

	priv->refCount--;

	if( priv->refCount<=0)
	{
		if (priv->maintenanceCheckStopped)
		{/* thread is not running */
		}
		else
		{
			/* thread is running */
			priv->maintenanceCheckStopped = TRUE;
			/* Wake up the media-check thread with a signal.
			It will check the 'running' flag and exit */
			if ((res = kill_proc (priv->maintenanceCheckThreadId, SIGKILL, 1)))
			{
				P_ERROR("%s: unable to signal thread\n", dev->name);
			}
			else
			{
				/* wait for the thread to exit. */
				down (priv->maintenanceCheckThreadComplete);
				up (priv->maintenanceCheckThreadComplete);
			}
		}

		/* Set callbacks when port is disabled */
		ixEthAccPortTxDoneCallbackRegister(priv->port_id, tx_done_disable_cb, (UINT32)dev);
		ixEthAccPortRxCallbackRegister(priv->port_id,  rx_disable_cb,  (UINT32)dev);

	    if ((status = ixEthAccPortDisable(priv->port_id)) != IX_SUCCESS)
	    {/* should not get here */
			P_ERROR("%s: ixEthAccPortDisable(%d) failed\n", dev->name, priv->port_id);
	    }

	    /* remove all entries from the sw queues */
	    dev_skb_queue_drain(priv);
	    dev_tx_mb_queue_drain(priv);
	    dev_rx_mb_queue_drain(priv);

	}
	TRACE;
}


/* Open the device. Request resources and start interrupts
 */
static int cloned_do_dev_open(struct net_device *dev)
{
	int res;

	/* prevent the maintenance task from running while bringing up port */
	down(maintenance_mutex);
	/* bring up the port */
	res = cloned_port_enable(dev);
	up(maintenance_mutex);
	return res;
}

static int cloned_do_dev_stop(struct net_device *dev)
{
	TRACE;

	/* prevent the maintenance task from running while bringing up port */
	down(maintenance_mutex);
	/* bring the port down */
	cloned_port_disable(dev);
	up(maintenance_mutex);

	return 0;
}


static int  __devinit  _adm_init_priv_dat(int portId)
{
	priv_data_t *priv;
	
	/* allocate and initialize priv struct */
	priv =  kmalloc(sizeof(priv_data_t), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	memset( priv, 0, sizeof(priv_data_t));

	TRACE;

	/* set the mapping between port ID and devices   */
	priv->port_id  = default_portId[portId];
	TRACE;

	/* initialize RX pool */
	priv->rx_pool = IX_OSAL_MBUF_POOL_INIT(RX_MBUF_POOL_SIZE, 0, "IXP400 Ethernet driver Rx Pool");
	if(priv->rx_pool == NULL)
	{
		P_ERROR("ADM Clone Net Device : Buffer RX Pool init failed on port %d\n", priv->port_id);
		kfree( priv);
		return -ENOMEM;
	}

	TRACE;

	/* initialize TX pool */
	priv->tx_pool = IX_OSAL_MBUF_POOL_INIT(TX_MBUF_POOL_SIZE, 0,  "IXP400 Ethernet driver Tx Pool");
	if(priv->tx_pool == NULL)
	{
		P_ERROR("ADM Clone Net Device : Buffer TX Pool init failed on port %d\n", priv->port_id);
		kfree( priv);
		return -ENOMEM;
	}

	TRACE;

	/* initialise the MII register access mutex */
	priv->maintenanceCheckThreadComplete = (struct semaphore *)	kmalloc(sizeof(struct semaphore), GFP_KERNEL);
	if (!priv->maintenanceCheckThreadComplete)
	{
		kfree( priv);
		return -ENOMEM;
	}
	priv->lock = SPIN_LOCK_UNLOCKED;
	init_MUTEX(priv->maintenanceCheckThreadComplete);
	priv->maintenanceCheckStopped = TRUE;

	sharedPriv = priv;

	return 0;
}

static int __devinit  _cloned_dev_probe(struct net_device *dev )
{
#if MAC_BOOTLOADER
	unsigned char assist_mac[IX_IEEE803_MAC_ADDRESS_SIZE];
#endif
	int isCloned = 0;
	int devIndex = ADM_START_POSITION;
	struct Qdisc *qdisc;
	
    /* there is a limited number of devices */
	if (ADM_START_POSITION >= dev_max_count ) /* module parameter */
		return -ENODEV;
	SET_MODULE_OWNER(dev);

	if(dev== &(adm6996_clone_dev) )
	{
		isCloned = 1;
		devIndex ++;
	}	
	
	/* set device name */
	sprintf(dev->name, DEVICE_NAME "%d", devIndex );

	/* initialize ethernet device (default handlers) */
	ether_setup(dev);

	TRACE;

	/* fill in dev struct callbacks with customized handlers */
	dev->open = cloned_do_dev_open;
	dev->stop = cloned_do_dev_stop;
	dev->hard_start_xmit = cloned_dev_hard_start_xmit;

	dev->watchdog_timeo = DEV_WATCHDOG_TIMEO;
	dev->tx_timeout = dev_tx_timeout;
	dev->change_mtu = dev_change_mtu;
	dev->do_ioctl = do_dev_ioctl;
	dev->get_stats = dev_get_stats;
	dev->set_multicast_list = dev_set_multicast_list;
	dev->flags |= IFF_MULTICAST|IFF_PROMISC;
	dev->set_mac_address = dev_set_mac_address;

	/* Important Note : 
	* allocate extra space for vlan header of TX frame , 
	* it must be coordinate to the header length of RX frmae
	*/
	dev->hard_header_len = ETH_HLEN + VLAN_HDR;

#if MAC_BOOTLOADER
	if(assist_get_hardware_address(devIndex, assist_mac) )
	{
		if(isCloned)
			memcpy(dev->dev_addr, &cloned_mac_addr.macAddress, IX_IEEE803_MAC_ADDRESS_SIZE);
		else
			memcpy(dev->dev_addr,  &default_mac_addr[sharedPriv->port_id].macAddress, IX_IEEE803_MAC_ADDRESS_SIZE);
	}
#else
	if(isCloned)
		memcpy(dev->dev_addr, &cloned_mac_addr.macAddress, IX_IEEE803_MAC_ADDRESS_SIZE);
	else
		memcpy(dev->dev_addr,  &default_mac_addr[sharedPriv->port_id].macAddress, IX_IEEE803_MAC_ADDRESS_SIZE);
#endif	

	if (is_valid_ether_addr(dev->dev_addr))
	{
		P_WARN("Use default MAC address %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x for port %d(dev %s)\n",
			(unsigned)dev->dev_addr[0], (unsigned)dev->dev_addr[1], (unsigned)dev->dev_addr[2],
			(unsigned)dev->dev_addr[3], (unsigned)dev->dev_addr[4], (unsigned)dev->dev_addr[5], sharedPriv->port_id, dev->name);
	}
    
	TRACE;

	dev_change_msdu(dev, dev->mtu + dev->hard_header_len + VLAN_HDR);
	if( isCloned )
	{
		sharedPriv->cloned_tq_timeout.routine = dev_tx_timeout_task;
		sharedPriv->cloned_tq_timeout.data = (void *)dev;
	}
	else
	{
		sharedPriv->tq_timeout.routine = dev_tx_timeout_task;
		sharedPriv->tq_timeout.data = (void *)dev;
	}

#ifdef CONFIG_IXP400_ETH_QDISC_ENABLED
	TRACE;
	if( isCloned )
	{
		sharedPriv->cloned_qdisc = qdisc_create_dflt(dev, &cloned_dev_qdisc_ops);
		qdisc = sharedPriv->cloned_qdisc;
	}
	else
	{
		sharedPriv->qdisc = qdisc_create_dflt(dev, &cloned_dev_qdisc_ops);
		qdisc = sharedPriv->qdisc;
	}

	if (!qdisc )
	{
		P_ERROR("%s: qdisc_create_dflt failed on port %d\n", dev->name, sharedPriv->port_id);
		kfree(sharedPriv);
		return -ENOMEM;
	}
	
	dev->qdisc_sleeping = qdisc;
	dev->qdisc = qdisc;

#endif

	/* set the internal maximum queueing capabilities */
	dev->tx_queue_len = TX_MBUF_POOL_SIZE/2; /*shared by 2 eth */
	if (!netif_queue_stopped(dev))
	{
		TRACE;
		dev->trans_start = jiffies;
		netif_stop_queue(dev);
	}

	return 0;
}

#endif


/* Module initialization and cleanup */

#ifdef MODULE

static struct net_device ixp400_devices[IX_ETH_ACC_NUMBER_OF_PORTS];


int init_module(void)
{
    int res, dev_count;
    IxEthAccPortId portId;
    struct net_device *dev;

    TRACE;

    P_INFO("Initializing IXP400 NPE Ethernet driver software v. " MODULE_VERSION " \n");

    TRACE;

    /* check module parameter range */
    if (dev_max_count == 0 || dev_max_count > IX_ETH_ACC_NUMBER_OF_PORTS)
    {
	P_ERROR("Number of ports supported is dev_max_count <= %d\n", IX_ETH_ACC_NUMBER_OF_PORTS);
	return -1;
    }

    TRACE;

#ifndef DEBUG
    /* check module parameter range */
    if (log_level >= 2)  /* module parameter */
    {
	printk("Warning : log_level == %d and TRACE is disabled\n", log_level);
    }
#endif

    TRACE;

    /* display an approximate CPU clock information */
	P_INFO("CPU clock speed (approx) = %lu MHz\n",  loops_per_jiffy * 2 * HZ / 1000000);

    TRACE;
 
#if defined (CONFIG_ARCH_IXDP465)
    /* Set the expansion bus fuse register to enable MUX for NPEA MII */
    {
        UINT32 expbusCtrlReg;
        expbusCtrlReg = ixFeatureCtrlRead ();
        expbusCtrlReg |= ((unsigned long)1<<8);
        ixFeatureCtrlWrite (expbusCtrlReg);
    }
#endif

    /* Enable/disable the EthDB MAC Learning & Filtering feature.
     * This is a half-bridge feature, and should be disabled if this interface 
     * is used on a bridge with other non-NPE ethernet interfaces.
     * This is because the NPE's are not aware of the other interfaces and thus
     * may incorrectly filter (drop) incoming traffic correctly bound for another
     * interface on the bridge.
     */
    if (npe_learning) /* module parameter */
    {
        ixFeatureCtrlSwConfigurationWrite (IX_FEATURECTRL_ETH_LEARNING, TRUE);
    }
    else
    {
        ixFeatureCtrlSwConfigurationWrite (IX_FEATURECTRL_ETH_LEARNING, FALSE);
    }

    TRACE;

    /* Do not initialise core components if no_ixp400_sw_init is set */
    if (no_ixp400_sw_init) /* module parameter */
    {
	P_WARN("no_ixp400_sw_init != 0, no IXP400 SW core component initialisation performed\n");
    }
    else
    {
	/* initialize the required components for this driver */
	if ((res = qmgr_init()))
	    return res;
        TRACE;

	if ((res = ixNpeMhInitialize(IX_NPEMH_NPEINTERRUPTS_YES)))
	    return -1;
        TRACE;
    }

    /* Initialise the NPEs and access layer */
    TRACE;

    if ((res = ethacc_init()))
	return res;

    TRACE;

    /* Initialise the PHYs */
    if ((res = phy_init()))
	return res;

    TRACE;
#if WITH_ADM6996_SWITCH
	/* allocate memory for sharedPriv  */
	res = _adm_init_priv_dat(ADM_START_POSITION );
	if(res)
		 return res;

#endif

    /* Initialise the driver structure */
#if WITH_ADM6996_SWITCH
	for (dev_count = 0; dev_count < 2; dev_count++)
#else
	for (dev_count = 0; dev_count < dev_max_count;  /* module parameter */
			dev_count++)
#endif
	{
		portId = default_portId[dev_count];
		dev = &ixp400_devices[dev_count];

		dev->init = dev_eth_probe;
#if WITH_ADM6996_SWITCH
		if(dev_count == ADM_START_POSITION ) /* eth1 */
		{/* following code only be used one time, so sharedPriv is inited here  */
	
			dev->init 				= _cloned_dev_probe;
			adm6996_clone_dev.init 	= _cloned_dev_probe;
			
	
			sharedPriv->rawDev		= dev;
			sharedPriv->clonedDev 	= &adm6996_clone_dev;
			dev->priv 				= sharedPriv;
			adm6996_clone_dev.priv 	= sharedPriv;
		}
		else
		{
			dev->init = dev_eth_probe;
		}
#else
		dev->init = dev_eth_probe;
#endif
		
		TRACE;
		if ((res = register_netdev(dev)))
		{
			TRACE;

			P_ERROR("Failed to register netdev. res = %d\n", res);
			return res;
		}
#if WITH_ADM6996_SWITCH
		if(dev_count == ADM_START_POSITION ) /* eth1 */
		{
			if ((res = register_netdev( &adm6996_clone_dev )))
			{/* eth2 */
				TRACE;
				P_ERROR("Failed to register cloned netdev. res = %d\n", res);
				return res;
			}

		}
#endif

		TRACE;
		/* register "safe" callbacks. This ensure that no traffic will be 
		* sent to the stack until the port is brought up (ifconfig up)
		*/
		if ((res = ixEthAccPortTxDoneCallbackRegister(portId, tx_done_disable_cb, (UINT32)dev)))
		{
			TRACE;
			return convert_error_ethAcc(res);
		}
		if ((res = ixEthAccPortRxCallbackRegister(portId, rx_disable_cb,  (UINT32)dev)))
		{
			TRACE;
			return convert_error_ethAcc(res);
		}
	}

    TRACE;

    /*
     * If feature control indicates that the livelock dispatcher is
     * used, it is not valid to also use datapath polling
     */
    if ((IX_FEATURE_CTRL_SWCONFIG_DISABLED ==
         ixFeatureCtrlSwConfigurationCheck(IX_FEATURECTRL_ORIGB0_DISPATCHER)) &&
         (datapath_poll == 1))
    {
        datapath_poll = 0;
        printk(KERN_NOTICE "\nInvalid to have datapath_poll=1 when the\n");
        printk(KERN_NOTICE "livelock dispatcher is being used.\n");
        printk(KERN_NOTICE "Datapath polling turned off.\n\n");
    }

    TRACE;

    if (no_ixp400_sw_init == 0 && datapath_poll != 0 ) /* module parameter */
    {
      /* The QMgr dispatch entry point is called from the 
       * IX_OSAL_IXP400_QM1_IRQ_LVL irq (which will trigger
       * an interrupt for every packet)
       * This function setup the datapath in polling mode
       * for better performances.
       */

        if ((res = ethAcc_datapath_poll_setup()))
	{
	    TRACE;
	    return res;
	}
    }

    TRACE;

    /* initialise the DB Maintenance task mutex */
    maintenance_mutex = (struct semaphore *) kmalloc(sizeof(struct semaphore), GFP_KERNEL);
    if (!maintenance_mutex)
	return -ENOMEM;

    init_MUTEX(maintenance_mutex);

    TRACE;

    /* Do not start the EthDB maintenance thread if learning & filtering feature is disabled */
    if (npe_learning) /* module parameter */
    {
        maintenance_timer_set();
    }

    TRACE;

    /* set the softirq rx queue thresholds 
     * (These numbers are based on tuning experiments)
     * maxbacklog =  (netdev_max_backlog * 10) / 63;
    */
    if (netdev_max_backlog == 0)
    {
	netdev_max_backlog = 290; /* system default */
    }
    netdev_max_backlog /= BACKLOG_TUNE;

    TRACE;

    return 0;
}

void cleanup_module(void)
{
    int dev_count;

    TRACE;

    /* We can only get here when the module use count is 0,
     * so there's no need to stop devices.
     */

    /* stop the hardware that was configured at initialisation */
    if (no_ixp400_sw_init == 0 && datapath_poll != 0 ) /* module parameter */
    {
	TRACE;

	dev_pmu_timer_disable(); /* stop the timer */
    
	if (irq_pmu_used) 
        {
	    free_irq(IX_OSAL_IXP400_XSCALE_PMU_IRQ_LVL,(void *)IRQ_ANY_PARAMETER);
	    irq_pmu_used = 0;
	}
    }

    TRACE;

    if (no_ixp400_sw_init == 0)
    {
	if (irq_qm1_used) 
	{
	    free_irq(IX_OSAL_IXP400_QM1_IRQ_LVL,(void *)IRQ_ANY_PARAMETER);
	    irq_qm1_used = 0;
	}
    }

    TRACE;

    /* stop the maintenance timer */
    maintenance_timer_clear();

    TRACE;

    /* Wait for maintenance task to complete (if started) */
    if (npe_learning) /* module parameter */
    {
	TRACE;

	down(maintenance_mutex);
	up(maintenance_mutex);
    }

    TRACE;

    /* uninitialize the access layers */
    ethacc_uninit();

    TRACE;

    for (dev_count = 0; 
	 dev_count < dev_max_count;  /* module parameter */
	 dev_count++)
    {
	struct net_device *dev = &ixp400_devices[dev_count];
	priv_data_t *priv = dev->priv;
	if (priv != NULL)
	{
	    IxEthAccPortId portId = default_portId[dev_count];

/* comment by lizhijie, 2005.07.11 */
#if 0
	    if (default_npeImageId[portId] == IX_ETH_NPE_A_IMAGE_ID)
	    {
		if (IX_SUCCESS != ixNpeDlNpeStopAndReset(IX_NPEDL_NPEID_NPEA))
		{
		    P_NOTICE("Error Halting NPE for Ethernet port %d!\n", portId);
		}
	    }
#endif
	    
	    if (default_npeImageId[portId] == IX_ETH_NPE_B_IMAGE_ID)
	    {
		if (IX_SUCCESS != ixNpeDlNpeStopAndReset(IX_NPEDL_NPEID_NPEB))
		{
		    P_NOTICE("Error Halting NPE for Ethernet port %d!\n", portId);
		}
	    }
	    if (default_npeImageId[portId] == IX_ETH_NPE_C_IMAGE_ID)
	    {
		if (IX_SUCCESS != ixNpeDlNpeStopAndReset(IX_NPEDL_NPEID_NPEC))
		{
		    P_NOTICE("Error Halting NPE for Ethernet port %d!\n", portId);
		}
	    }
	    unregister_netdev(dev);
	    kfree(dev->priv);
	    dev->priv = NULL;
	}
    }

    TRACE;

    P_VERBOSE("IXP400 NPE Ethernet driver software uninstalled\n");
}

#endif /* MODULE */


