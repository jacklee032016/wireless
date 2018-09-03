
#include "valoha.h"

int valoha_tx(void* mydata, struct sk_buff* packet,  int intop)
{
	valoha_inst_t* inst = mydata;
	int status = MAC_TX_OK;
	int txresult = VMAC_PHY_SENDPACKET_OK;

	if (!inst)
	{/* Could not get our instance handle -- let the caller know... */
		printk(KERN_ALERT VMAC_MODULE_ALOHA": packet_tx -- no instance handle!\n");
		return MAC_TX_FAIL;
	}

	trace;	
	read_lock(&(inst->mac_busy));
	
	/** Check to see if we're in the top half or bottom half, i.e. are
	* we taking an interrupt right now?  */
	if (intop)
	{
		/* As a general rule, try to keep the stuff you do in the top
		* half to a minimum. This is during the immediate handling
		* of an interrupt and the place for time-critical tasks to occur.*/
		if ((inst->defertx || inst->use_crypto) && packet)
		{
			/* Queue the packet in tx_skbqueue, schedule the "work" method to run.*/
			skb_queue_tail(&(inst->tx_skbqueue),packet);
			(inst->myphy->phy_schedule_work_asap)(inst->myphy->phy_private);
			status = MAC_TX_OK;
		}
		else if (packet)
		{
			/* Send the packet now */
			valoha_prep_skb(inst, packet);
			txresult = (inst->myphy->phy_sendpacket)(inst->myphy->phy_private, inst->maxinflight, packet);

			if (VMAC_PHY_SENDPACKET_OK != txresult)
			{
				printk(KERN_ALERT VMAC_MODULE_ALOHA": top half packet tx failed: %d\n",txresult);
			}
			status = MAC_TX_OK;
		}
	}
	else
	{/* NOT in ISR -- process our transmit queue */
		trace;
		
		struct sk_buff* skb = 0;
		if (packet)
		{
			skb_queue_tail(&(inst->tx_skbqueue),packet);
		}

		/* Walk our transmit queue, shovelling out packets as we go... */
		while ((skb = skb_dequeue(&(inst->tx_skbqueue))))
		{
			if (inst->use_crypto)
			{
				valoha_mac_rot128(skb);
			}

#if 0
			/* it ought to be skb */
			valoha_prep_skb(inst, packet);
			txresult = (inst->myphy->phy_sendpacket)(inst->myphy->phy_private, inst->maxinflight,  packet);
#endif

#if VMAC_DEBUG_ALOHA
			printk(KERN_DEBUG VMAC_MODULE_ALOHA ": My PHY is %s\n", inst->myphy?inst->myphy->name:"NULL");
#endif
			valoha_prep_skb(inst, skb);
			txresult = (inst->myphy->phy_sendpacket)(inst->myphy->phy_private, inst->maxinflight,  skb);
			if (VMAC_PHY_SENDPACKET_OK != txresult)
			{
				printk(KERN_ALERT VMAC_MODULE_ALOHA": tasklet packet tx failed: %d\n",txresult);
				/* N.B. we return an "OK" for the transmit because
				* we're handling the sk_buff freeing down here --
				* as far as the caller is concerned we were successful. */
			}
		}
	}
	read_unlock(&(inst->mac_busy));

	return status;
}

static int valoha_tx_done(void* mydata, struct sk_buff* packet, int intop) 
{
	int status = VMAC_MAC_NOTIFY_OK;
	valoha_inst_t* inst = mydata;

	if (inst)
	{
		read_lock(&(inst->mac_busy));

		/** Check to see if we're supposed to defer handling  */
		if (intop)
		{
			if (inst->defertxdone && packet)
			{
				/* Queue the packet in txdone_skbqueue, schedule the tasklet */
				skb_queue_tail(&(inst->txdone_skbqueue), packet);
				status = VMAC_MAC_NOTIFY_RUNAGAIN;
			}
			else if (packet)
			{
				/* Free the packet immediately, do not run again*/
				(inst->myphy->phy_free_skb)(inst->myphy->phy_private, packet);
				packet = 0;
				status = VMAC_MAC_NOTIFY_OK;
			}
		}
		else
		{
			struct sk_buff* skb = 0;
			/** In bottom half -- process any deferred packets */
			if (packet)
			{
				skb_queue_tail(&(inst->txdone_skbqueue), packet);
			}

			while ((skb = skb_dequeue(&(inst->txdone_skbqueue))))
			{
				(inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
			}
		}
		read_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_ALERT VMAC_MODULE_ALOHA": packet_tx_done -- no instance handle!\n");
		status = VMAC_MAC_NOTIFY_HOSED;
	}

	return status;
}


static int valoha_rx(void* mydata, struct sk_buff* packet,  int intop) 
{
	valoha_inst_t* inst = mydata;
	int status = VMAC_MAC_NOTIFY_OK;

	// XXX check to see if MAC is active...
	if (inst)
	{
		read_lock(&(inst->mac_busy));
		//printk(KERN_DEBUG "cheesymac: packet rx\n");
		if (intop)
		{
			/* We defer handling to the bottom half if we're either
			* told to explicitly, or if we're using encryption. */
			if ((inst->deferrx || inst->use_crypto) && packet)
			{
				/* Queue packet for later processing*/
				//printk(KERN_DEBUG "cheesymac: packet rx deferring\n");
				skb_queue_tail(&(inst->rx_skbqueue), packet);
				status = VMAC_MAC_NOTIFY_RUNAGAIN;
			}
			else if (packet)
			{
				/* Just send the packet up the network stack if we've got
				* an attached rxfunc, otherwise whack the packet.*/
				if (inst->myrxfunc)
				{
					//printk(KERN_DEBUG "cheesymac: packet rx -- calling receive\n");
					(inst->myrxfunc)(inst->myrxfunc_priv, packet);
				}
				else
				{ //if (inst->myphy->cu_softmac_free_skb) {
					//printk(KERN_DEBUG "cheesymac: packet rx -- freeing skb\n");
					(inst->myphy->phy_free_skb)(inst->myphy->phy_private, packet);
				}
				//else {
				//printk(KERN_DEBUG "cheesymac: packet rx -- mac hosed\n");
				//status = VMAC_MAC_NOTIFY_HOSED;
				//}
			}
		}
		else
		{
			struct sk_buff* skb = 0;
			/** Not in top half - walk our rx queue and send packets
			* up the stack */
			//printk(KERN_DEBUG "cheesymac: packet rx -- bottom half\n");
			if (packet)
			{
				skb_queue_tail(&(inst->rx_skbqueue), packet);
			}

			while ((skb = skb_dequeue(&(inst->rx_skbqueue))))
			{
				if (inst->myrxfunc)
				{
					if (inst->use_crypto)
					{
						valoha_mac_rot128(skb);
					}
					(inst->myrxfunc)(inst->myrxfunc_priv, packet);
				}
				else
				{
					/* No rx function available? Ask phy layer to free the packet. */
					(inst->myphy->phy_free_skb)(inst->myphy->phy_private, packet);
				}
			}
		}
		read_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_ALERT VMAC_MODULE_ALOHA": packet_rx -- no instance handle!\n");
		status = VMAC_MAC_NOTIFY_HOSED;
	}

	return status;
}

static int valoha_work(void* mydata, int intop) 
{
	valoha_inst_t* inst = mydata;
	int status = VMAC_MAC_NOTIFY_OK;

	if (inst)
	{
		int txresult;
		struct sk_buff* skb = 0;

		/* Walk our receive queue, pumping packets up to the system as we go... */
		while ((skb = skb_dequeue(&(inst->rx_skbqueue))))
		{
			if (inst->myrxfunc)
			{
				//printk(KERN_DEBUG "cheesymac: have rxfunc -- receiving\n");
				if (inst->use_crypto)
				{
					valoha_mac_rot128(skb);
				}
				(inst->myrxfunc)(inst->myrxfunc_priv,skb);
			}
			else
			{
				printk(KERN_DEBUG VMAC_MODULE_ALOHA": packet rx -- no rxfunc freeing skb\n");
				(inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
			}
		}

		/* Walk our transmit queue, shovelling out packets as we go... */
		while ((skb = skb_dequeue(&(inst->tx_skbqueue))))
		{
			valoha_prep_skb(inst, skb);
			if (inst->use_crypto)
			{
				valoha_mac_rot128(skb);
			}

			txresult = (inst->myphy->phy_sendpacket)(inst->myphy->phy_private, 
							  inst->maxinflight,  skb);

			if (VMAC_PHY_SENDPACKET_OK != txresult)
			{
				printk(KERN_ALERT VMAC_MODULE_ALOHA": work packet tx failed: %d\n",txresult);
			}
		}

		/* Walk through the "transmit done" queue, freeing packets as we go... */
		while ((skb = skb_dequeue(&(inst->txdone_skbqueue))))
		{
			(inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
		}
	}
	else
	{
		status = VMAC_MAC_NOTIFY_HOSED;
	}

	return status;
}

int valoha_detach(void* handle)
{
	valoha_inst_t* inst = handle;
	int result = 0;

	if (inst)
	{
		write_lock(&(inst->mac_busy));
		if (inst->myphy != inst->defaultphy)
		{
			vmac_free_phy(inst->myphy);
			inst->myphy = inst->defaultphy;
		}
		write_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_ALERT VMAC_MODULE_ALOHA": Invalid MAC/PHY data on detach!\n");
		result = -1;
	}

	return result;
}

static int valoha_attach(void* handle, vmac_phy_t* phyinfo)
{
	valoha_inst_t* inst = handle;
	int result = 0;

	if (inst && phyinfo)
	{
		valoha_detach(handle);
		write_lock(&(inst->mac_busy));
		inst->myphy = vmac_get_phy(phyinfo);
		write_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_ALERT VMAC_MODULE_ALOHA": Invalid MAC/PHY data on attach!\n");
		result = -1;
	}

	return result;
}


int valoha_set_rx_func(void* mydata,mac_rx_func_t rxfunc, void* rxpriv)
{
	int result = 0;
	valoha_inst_t* inst = mydata;
	if (inst)
	{
		write_lock(&(inst->mac_busy));
		inst->myrxfunc = rxfunc;
		inst->myrxfunc_priv = rxpriv;
		write_unlock(&(inst->mac_busy));
	}

	return result;
}

int valoha_set_unload_notify_func(void* mydata, mac_unload_notify_func_t unloadfunc,void* unloadpriv)
{
	int result = 0;
	valoha_inst_t* inst = mydata;
	if (inst)
	{
		write_lock(&(inst->mac_busy));
		inst->myunloadnotifyfunc = unloadfunc;
		inst->myunloadnotifyfunc_priv = unloadpriv;
		write_unlock(&(inst->mac_busy));
	}

	return result;
}


void valoha_set_mac_funcs(vmac_mac_t *macinfo) 
{
	// XXX lock
	macinfo->mac_tx = valoha_tx;
	macinfo->mac_tx_done = valoha_tx_done;
	macinfo->rx_packet = valoha_rx;
	macinfo->mac_work = valoha_work;
	macinfo->attach_phy = valoha_attach;
	macinfo->detach_phy = valoha_detach;
	macinfo->set_rx_func = valoha_set_rx_func;
	macinfo->set_unload_notify_func = valoha_set_unload_notify_func;
}

