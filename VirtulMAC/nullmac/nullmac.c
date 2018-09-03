
/* This is the NullMAC example module */

#include <linux/module.h>
#if __ARM__
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

MODULE_LICENSE("GPL");

/* private instance data */
struct nullmac_t
{
	vmac_mac_t *macinfo;
	vmac_phy_t *phyinfo;

	mac_rx_func_t netif_rx;
	void *netif_rx_priv;

	struct tasklet_struct rxq_tasklet;
	struct sk_buff_head   rxq;

	int id;
	rwlock_t lock;
};

static int nullmac_next_id;
static const char *nullmac_name = "nullmac";

/* Every VMAC MAC or PHY layer provides a vmac_layer_t interface */
static vmac_layer_t the_nullmac;

static int nullmac_attach_phy(void *me, vmac_phy_t *phyinfo)
{
	struct nullmac_t *inst = me;
	int ret = -1;
	unsigned long flags;

	write_lock_irqsave(&(inst->lock), flags);
	if (!inst->phyinfo)
	{
		inst->phyinfo = vmac_get_phy(phyinfo);
		ret = 0;
	}

	write_unlock_irqrestore(&(inst->lock), flags);
	return ret;
}

static int nullmac_detach_phy(void *me)
{
	struct nullmac_t *inst = me;
	unsigned long flags;

	write_lock_irqsave(&(inst->lock), flags);
	if (inst->phyinfo)
	{
		vmac_free_phy(inst->phyinfo);
		inst->phyinfo = 0;
	}

	write_unlock_irqrestore(&(inst->lock), flags);
	return 0;
}

static int nullmac_set_netif_rx_func(void *me, mac_rx_func_t rxfunc, void *rxpriv) 
{
	struct nullmac_t *inst = me;
	unsigned long flags;

	write_lock_irqsave(&(inst->lock), flags);
	inst->netif_rx = rxfunc;
	inst->netif_rx_priv = rxpriv;

	write_unlock_irqrestore(&(inst->lock), flags);
	return 0;
}

static int nullmac_tx_packet(void *me, struct sk_buff *skb, int intop)
{
	struct nullmac_t *inst = me; 

	BUG_ON(intop);
	read_lock(&(inst->lock));
	if (inst->phyinfo)
	{
		inst->phyinfo->phy_sendpacket(inst->phyinfo->phy_private, 1, skb);
	}
	else
	{ 
		kfree_skb(skb);
	}

	read_unlock(&(inst->lock));
	return VMAC_MAC_NOTIFY_OK;
}

static void nullmac_rx_tasklet(unsigned long data)
{
	struct nullmac_t *inst = (struct nullmac_t *)data;
	struct sk_buff *skb;

	read_lock(&(inst->lock));
	while ( (skb = skb_dequeue(&(inst->rxq))) )
	{
		if (inst->netif_rx)
			inst->netif_rx(inst->netif_rx_priv, skb);
		else if (inst->phyinfo)
			inst->phyinfo->phy_free_skb(inst->phyinfo->phy_private, skb);
		else
			kfree_skb(skb);
	}
	read_unlock(&(inst->lock));
}

static int nullmac_rx_packet(void *me, struct sk_buff *skb, int intop)
{
	struct nullmac_t *inst = me;

	skb_queue_tail(&(inst->rxq), skb);
	if (intop)
	{
		tasklet_schedule(&(inst->rxq_tasklet));
	}
	else
	{
		nullmac_rx_tasklet((unsigned long)me);
	}
	return VMAC_MAC_NOTIFY_OK;
} 

/* create and return a new nullmac instance */
static void *nullmac_new_instance (void *layer_priv)
{
	struct nullmac_t *inst;
	void *ret = 0;

	inst = kmalloc(sizeof(struct nullmac_t), GFP_ATOMIC);
	if (inst)
	{
		memset(inst, 0, sizeof(struct nullmac_t));
		inst->lock = RW_LOCK_UNLOCKED;
		inst->id = nullmac_next_id++;

		/* setup the macinfo structure */
		inst->macinfo = vmac_alloc_mac();
		inst->macinfo->mac_private = inst;
		inst->macinfo->layer = &the_nullmac;
		snprintf(inst->macinfo->name, VMAC_NAME_SIZE, "%s%d", the_nullmac.name, inst->id);

		/* override some macinfo functions */
		/* the rest remain pointed at the default "do nothing" functions */
		inst->macinfo->mac_tx = nullmac_tx_packet;
		inst->macinfo->rx_packet = nullmac_rx_packet;
		inst->macinfo->attach_phy = nullmac_attach_phy;
		inst->macinfo->detach_phy = nullmac_detach_phy;
		inst->macinfo->set_rx_func = nullmac_set_netif_rx_func;

		/* setup rx tasklet */
		tasklet_init(&(inst->rxq_tasklet), nullmac_rx_tasklet, (unsigned long)inst);
		skb_queue_head_init(&(inst->rxq));

		/* register with softmac */
		vmac_register_mac(inst->macinfo);
		/* note: an instance shouldn't hold a reference to it's own macinfo because
		* they are really just public and private parts of the same object.*/

		/* we've registered with softmac, decrement the ref count */
		vmac_free_mac(inst->macinfo);

		ret = inst->macinfo;
	}

	return ret;
}

static void nullmac_free_instance (void *layer_priv, void *info)
{
	vmac_mac_t *macinfo = info;
	struct nullmac_t *inst = macinfo->mac_private;
	if (inst->phyinfo)
	{
		vmac_free_phy(inst->phyinfo);
	}
	kfree(inst);
}

static int __init vmac_nullmac_init(void)
{
	strncpy(the_nullmac.name, nullmac_name, VMAC_NAME_SIZE);
	the_nullmac.new_instance = nullmac_new_instance;
	the_nullmac.destroy_instance = nullmac_free_instance;
	vmac_layer_register(&the_nullmac);

	return 0;
}

static void __exit  vmac_nullmac_exit(void)
{
	vmac_layer_unregister((vmac_layer_t *)&the_nullmac);
}

module_init(vmac_nullmac_init);
module_exit(vmac_nullmac_exit);

