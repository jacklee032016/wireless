/*
 * $Id: vnetif.c,v 1.1.1.1 2006/11/30 17:01:32 lizhijie Exp $
 */

#include <linux/module.h>
#if __ARM__
#include <linux/random.h>
#else
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/rtnetlink.h>

#include "vmac.h"
#include "vnetif.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Li Zhijie");

typedef struct 
{
	MYLIST_HEAD					list;
	rwlock_t 						devLock;

	int 							devOpen;
	int 							devRegistered;
	int 							devRegPending;

	vnet_tx_func_t				txFunc;
	void							*txFuncPriv;
	
	vnet_notify_func_t 				unloadFunc;
	void							*unloadFuncPriv;

	struct net_device				netDev;
	struct net_device_stats 		devStats;
} vnet_t;

/*  Keep a reference to the head of our linked list of instances. */
static LIST_HEAD(_vnetif_instance_list);


static int _vnetif_test_on = 0;
#if __ARM__
MODULE_PARM(_vnetif_test_on, "1i");
#else
module_param(_vnetif_test_on, int, S_IRUSR | S_IRGRP | S_IROTH);
#endif
MODULE_PARM_DESC(_vnetif_test_on,"Set to non-zero to have the netif module create a fake network interface on load");


/** Initialize a netif instance */
static void __init_instance(vnet_t* inst)
{
	if (inst)
	{
		memset(inst,0,sizeof(vnet_t));
		MYLIST_INIT_LIST_HEAD(&inst->list);
		inst->devLock = RW_LOCK_UNLOCKED;
	}
}

static void __cleanup_instance(vnet_t* inst)
{
	if (inst)
	{
		printk(KERN_DEBUG "About to cleanup %p (%s) -- locking\n",inst, inst->netDev.name);
		write_lock(&(inst->devLock) );
		printk(KERN_DEBUG "About to cleanup %p (%s) -- got lock\n",inst,inst->netDev.name);
		if (inst->unloadFunc)
		{
			(inst->unloadFunc)(inst, inst->unloadFuncPriv);
			inst->unloadFunc = 0;
			inst->unloadFuncPriv = 0;
		}

		inst->txFunc = 0;
		inst->txFuncPriv = 0;
		
		if (inst->devRegistered)
		{
			int unregresult = 0;
			printk(KERN_DEBUG "About to unregister %p (%s) -- got lock\n",inst,inst->netDev.name);
			inst->devRegistered = 0;
			write_unlock(&(inst->devLock));

			rtnl_lock();
			unregresult = unregister_netdevice(&(inst->netDev));
			rtnl_unlock();

			if (unregresult)
			{
				printk(KERN_DEBUG "Unregister failed: result == %d\n",unregresult);
			}
			else
			{
				printk(KERN_DEBUG "Unregister succeeded\n");
			}
		}
		else
		{
			write_unlock(&(inst->devLock));
		}
	}
}

/* Function handed over as the "hard_start" element in the network device structure. */
static int __hard_start_xmit(struct sk_buff  *skb, struct net_device *dev)
{
	int txresult = 0;
	//printk(KERN_DEBUG "VMAC netif: hard_start\n");
	if (dev && dev->priv)
	{
		vnet_t* inst = dev->priv;
		read_lock(&(inst->devLock));
		if (inst->txFunc)
		{
			txresult = (inst->txFunc)(inst, inst->txFuncPriv, skb);
		}
		else
		{
		/** Just drop the packet on the floor if there's no callback set    */
			dev_kfree_skb(skb);
			skb = 0;
			txresult = 0;
		}
		read_unlock(&(inst->devLock));
	}
	else
	{ /** Returning a "1" from here indicates transmit failure.     */
		txresult = 1;
	}

	return txresult;
}

static int __dev_open(struct net_device *dev)
{
	int result = 0;
	printk(KERN_DEBUG  VMAC_MODULE_VETH ": open %s\n", dev->name );
	if (dev && dev->priv)
	{
		vnet_t* inst = dev->priv;
		
		/** Mark the device as "open"     */
		write_lock(&(inst->devLock));
		if (!inst->devRegistered)
		{
			/*  Someone is opening us -> we've been registered       */
			inst->devRegistered = 1;
		}

		if (!inst->devOpen)
		{
			netif_start_queue(dev);
			inst->devOpen = 1;
		}
		write_unlock(&(inst->devLock));
	}
	return result;
}

static int __dev_stop(struct net_device *dev)
{
	int result = 0;
	printk(KERN_DEBUG VMAC_MODULE_VETH": dev stop on %s\n", dev->name );
	if (dev && dev->priv)
	{
		vnet_t* inst = dev->priv;

		/** Mark the device as "closed"   */
		write_lock(&(inst->devLock));
		if (inst->devOpen)
		{
			printk(KERN_DEBUG  VMAC_MODULE_VETH": stopping %s\n",inst->netDev.name);
			netif_stop_queue(dev);
			inst->devOpen = 0;
		}
		write_unlock(&(inst->devLock));
	}
	return result;
}

static void __dev_tx_timeout(struct net_device *dev)
{
	printk(KERN_DEBUG VMAC_MODULE_VETH": dev_tx timeout on %s!\n", dev->name );
}

static void __dev_mclist(struct net_device *dev)
{
	/* Nothing to do for multicast filters.  We always accept all frames.    */
	return;
}

static struct net_device_stats *__dev_stats(struct net_device *dev)
{
	struct net_device_stats *nds = 0;
	if (dev && dev->priv)
	{
		vnet_t* inst = dev->priv;
		nds = &inst->devStats;
	}
	return nds;
}

static int __change_mtu(struct net_device *dev, int new_mtu)
{
	int error=0;

	if (dev && dev->priv)
	{
		vnet_t* inst = dev->priv;
		if (!(VMAC_MIN_MTU <= new_mtu && new_mtu <= VMAC_MAX_MTU))
		{
			printk(KERN_DEBUG VMAC_MODULE_VETH" - %s: Invalid MTU size, %u < MTU < %u\n", inst->netDev.name, VMAC_MIN_MTU, VMAC_MAX_MTU);
			return -EINVAL;
		}

		write_lock(&(inst->devLock));
		dev->mtu = new_mtu;
		write_unlock(&(inst->devLock));
	}
	else
	{
		return -EINVAL;
	}

        return error;
}



/** This function creates an ethernet interface */
vnet_handle vmac_create_vnetif(char* name,unsigned char* macaddr, vnet_tx_func_t txfunc,void* txfunc_priv)
{
	vnet_t* newinst = 0;
	if (!name)
	{
		return 0;
	}

	printk(KERN_DEBUG VMAC_MODULE_VETH": create_eth %s\n",name);

	newinst = kmalloc(sizeof(vnet_t),GFP_ATOMIC);
	if (newinst)
	{
		struct net_device* dev = &(newinst->netDev);
		unsigned char randommacaddr[6];
		int regresult = 0;

		__init_instance(newinst);
		list_add_tail(&newinst->list, &_vnetif_instance_list);

		if (!macaddr)
		{
			/** No MAC address passed in? Just make one up. Code
			* liberated from the kernel tap/tun driver.       */
			macaddr = randommacaddr;
			*((u_int16_t *)macaddr) = htons(0x00FF);
			get_random_bytes(macaddr + 2, 4);
		}

		/** Fire up the instance...     */
		write_lock(&(newinst->devLock));
		ether_setup(dev);
		strncpy(dev->name, name, IFNAMSIZ);

		memcpy(dev->dev_addr,macaddr,6);
		dev->priv = newinst;
		
		dev->open = __dev_open;
		dev->stop = __dev_stop;
		dev->hard_start_xmit = __hard_start_xmit;
		dev->tx_timeout = __dev_tx_timeout;
		dev->watchdog_timeo = 5 * HZ;			/* XXX */
		dev->set_multicast_list = __dev_mclist;
		dev->get_stats = __dev_stats;
		dev->change_mtu = __change_mtu;
		
		newinst->txFunc = txfunc;
		newinst->txFuncPriv = txfunc_priv;
		
		write_unlock(&(newinst->devLock));
		printk(KERN_DEBUG VMAC_MODULE_VETH": create_eth registering netdev\n");

		rtnl_lock();
		regresult = register_netdevice(&(newinst->netDev));
		rtnl_unlock();

		if (!regresult)
		{
			printk(KERN_DEBUG VMAC_MODULE_VETH": create_eth registered netdev\n");
			write_lock(&(newinst->devLock));
			newinst->devRegistered = 1;
			write_unlock(&(newinst->devLock));
		}
		else
		{
			printk(KERN_DEBUG VMAC_MODULE_VETH": create_eth unable to register netdev!\n");
		}

	}
	return newinst;
}

/*
 * Get a netif handle from a dev. Dangerous if you aren't absolutely
 * sure that the device is of type netif... Should devise some way
 * of checking for this.
 */
vnet_handle vmac_get_vnetif(struct net_device *netdev)
{
	vnet_handle netifhandle = 0;
	if (netdev)
	{
		netifhandle = netdev->priv;
	}
	return netifhandle;
}

/** Get a dev from netif handle */
struct net_device *vmac_get_netdev(vnet_handle nif)
{
	vnet_t *inst = nif;  
	return (&(inst->netDev));
}

/* Detach the current client */
void vmac_vnetif_detach(vnet_handle nif)
{
	vnet_t* inst = nif;
	if (inst)
	{
		write_lock(&(inst->devLock));
		
		inst->unloadFunc = 0;
		inst->unloadFuncPriv = 0;
		inst->txFunc = 0;
		inst->txFuncPriv = 0;
		
		write_unlock(&(inst->devLock));
	}
}

/* Detach the current client */
void vmac_netif_set_unload_callback(vnet_handle nif,vnet_notify_func_t unloadfunc,void* unloadpriv)
{
	vnet_t* inst = nif; 

	if (inst)
	{
		write_lock(&(inst->devLock));
		
		inst->unloadFunc = unloadfunc;
		inst->unloadFuncPriv = unloadpriv;

		write_unlock(&(inst->devLock));
	}
}

/* Destroy a previously created network interface */
void vmac_destroy_vnetif(vnet_handle nif)
{
	vnet_t* inst = nif;

	if (inst)
	{
		printk(KERN_DEBUG VMAC_MODULE_VETH": deactivating netif\n");
		__cleanup_instance(inst);
	}
}

/** A client should call this function when it has a packet ready
 * to send up to higher layers of the network stack. */
int vmac_vnetif_rx(vnet_handle nif, struct sk_buff* packet)
{
	int result = VNETIF_RX_PACKET_OK;
	vnet_t  *inst = nif;
	struct net_device  *dev = 0;

	if (inst)
	{
		read_lock(&(inst->devLock));
		dev = &(inst->netDev);
		packet->dev = dev;
		packet->mac.raw = packet->data;

		/*
		* XXX this assumes ethernet packets -- make more generic!
		*/
		//packet->nh.raw = packet->data + sizeof(struct ether_header);
		packet->protocol = eth_type_trans(packet,dev);
		if (inst->devOpen)
		{
			//printk(KERN_DEBUG "softmac_netif: receiving packet!\n");
			netif_rx(packet);
		}
		else
		{
			dev_kfree_skb_any(packet);
			result = VNETIF_RX_PACKET_ERROR;
		}
		read_unlock(&(inst->devLock));
	}
	else
	{
		result = VNETIF_RX_PACKET_ERROR;
	}

	return result;
}

/** Set the function to call when a packet is ready for transmit */
void vmac_vnetif_set_tx_callback(vnet_handle nif, vnet_tx_func_t txfunc, void* txfunc_priv)
{
	vnet_t* inst = nif;

	if (inst)
	{
		write_lock(&(inst->devLock));

		inst->txFunc = txfunc;
		inst->txFuncPriv = txfunc_priv;
		
		write_unlock(&(inst->devLock));
	}
}

static int testtxfunc(vnet_handle nif, void *priv, struct sk_buff* skb)
{
	printk(KERN_DEBUG "Got packet for transmit, length %d bytes\n",skb->len);
	dev_kfree_skb(skb);
	return 0;
}

static int __init __vnetif_init(void)
{
	printk(KERN_ALERT "Loading " VMAC_MODULE_VETH " module\n");
	if (_vnetif_test_on)
	{
		printk(KERN_ALERT "Creating test interface foo0\n");
		vmac_create_vnetif("foo0","\0\1\2\3\4\5",testtxfunc,0);
	}
	return 0;
}

static void __exit __vnetif_exit(void)
{
	printk(KERN_DEBUG "Unloading "VMAC_MODULE_VETH " module\n");

	if (!MYLIST_CHECK_EMPTY(&_vnetif_instance_list))
	{
		printk(KERN_DEBUG VMAC_MODULE_VETH": Deleting instances\n");
		vnet_t* netif_instance = 0;
		MYLIST_HEAD	*tmp = 0;
		MYLIST_HEAD	*p = 0;

		/** Walk through all instances, remove from the linked 
		* list and then dispose of them cleanly.     */
		MYLIST_FOR_EACH_SAFE(p, tmp, &_vnetif_instance_list)
		{
			netif_instance = MYLIST_ENTRY(p, vnet_t, list);
			printk(KERN_DEBUG VMAC_MODULE_VETH": Detaching and destroying instance %p\n",netif_instance);
			write_lock(&(netif_instance->devLock));
			MYLIST_DEL(&(netif_instance->list));
			write_unlock(&(netif_instance->devLock));
			
			vmac_destroy_vnetif(netif_instance);
			kfree(netif_instance);
			netif_instance = 0;
		}
	}
	else
	{
		printk(KERN_DEBUG VMAC_MODULE_VETH": No instances found\n");
	}
}

EXPORT_SYMBOL(vmac_create_vnetif);
EXPORT_SYMBOL(vmac_get_vnetif);
EXPORT_SYMBOL(vmac_get_netdev);
EXPORT_SYMBOL(vmac_destroy_vnetif);
EXPORT_SYMBOL(vmac_vnetif_rx);
EXPORT_SYMBOL(vmac_vnetif_set_tx_callback);
EXPORT_SYMBOL(vmac_vnetif_detach);
EXPORT_SYMBOL(vmac_netif_set_unload_callback);

module_init(__vnetif_init);
module_exit(__vnetif_exit);

