
/*  CheesyMAC: an example of an "Alohaesque" wireless MAC constructed using the VMAC framework. */


#include "valoha.h"


/* VMAC Netif stuff */
static int __netif_txhelper(vnet_handle nif, void* priv, struct sk_buff* packet) 
{
	valoha_inst_t* inst = priv;
	if (inst)
	{
		/* Call the cheesymac tx function. The "intop" indicator
		* is always zero for the particular netif implementation
		* we're using -- in Linux the packet transmission is
		* instigated by a softirq in the bottom half.     */
		int txresult = valoha_tx(inst,packet,0);
		if (MAC_TX_OK != txresult)
		{
			dev_kfree_skb(packet);
		}
	}
	return 0;
}

static int __netif_unload_helper(vnet_handle netif, void* priv) 
{
	valoha_inst_t* inst = priv;
	if (inst)
	{  
		valoha_set_rx_func(inst, 0, 0);
	}
	return 0;
}

int valoha_netif(void *mypriv)
{
	valoha_inst_t *inst = mypriv;
	vmac_mac_t *macinfo = inst->mymac;
	int ret = 0;
	struct net_device *checknet = 0;

	checknet = dev_get_by_name(macinfo->name);
	if (checknet)
	{
		printk(KERN_DEBUG VMAC_MODULE_ALOHA": Attaching to %s\n", macinfo->name);
		inst->netif = vmac_get_vnetif(checknet);
		dev_put(checknet);
		vmac_vnetif_set_tx_callback(inst->netif, __netif_txhelper, (void *)inst);
	}
	else
	{
		printk(KERN_DEBUG VMAC_MODULE_ALOHA": Creating %s\n", macinfo->name);
		inst->netif = vmac_create_vnetif(macinfo->name, 0, __netif_txhelper,	inst);
	}

	if (inst->netif)
	{
		//printk(KERN_DEBUG "ALOHA: Setting mac unload notify func\n");
		valoha_set_unload_notify_func(inst, vmac_vnetif_detach, inst->netif);
		//printk(KERN_DEBUG "ALOHA: Setting netif unload callback func\n");
		vmac_netif_set_unload_callback(inst->netif, __netif_unload_helper, inst);
		valoha_set_rx_func(inst, vmac_vnetif_rx, inst->netif);
	}
	else
	{
		printk(KERN_ALERT VMAC_MODULE_ALOHA": Unable to attach to netif!\n");
		valoha_destroy_instance(inst);
		ret = -1;
	}

	return ret;
}

void valoha_prep_skb(valoha_inst_t* inst, struct sk_buff* skb)
{
	if (inst && (inst->myphy != inst->defaultphy))
	{  /** XXX use of atheros-specific PHY calls!!!*/
		vmac_ath_set_default_phy_props(inst->myphy->phy_private, skb);
		vmac_ath_set_tx_bitrate(inst->myphy->phy_private, skb, inst->txbitrate);
	}
}


/* Simple "encryption/decryption" function */
void valoha_mac_rot128(struct sk_buff* skb)
{
	int i = 0;
	for (i=0;i<skb->len;i++)
	{
		skb->data[i] = ((skb->data[i] + 128) % 256);
	}
}

