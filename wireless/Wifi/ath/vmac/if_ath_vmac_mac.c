/*
* $Id: if_ath_vmac_mac.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#include "if_ath_vmac.h"

static int ath_vmac_mac_packet_tx(void *me, struct sk_buff* skb, int intop)
{
	struct ath_softc* sc = (struct ath_softc*)me;
	//struct ath_vmac_instance *inst = sc->sc_vmac_mac_inst;
	ath_start(skb, &sc->sc_dev);
	return VMAC_MAC_NOTIFY_OK;
}

int ath_vmac_mac_start(struct sk_buff* skb, struct net_device *dev)
{
	int ret;
	struct ath_softc *sc = dev->priv;
	ret = ath_vmac_mac_packet_tx(sc, skb, 0);

	return ret;
}

static int ath_vmac_mac_packet_rx(void *me, struct sk_buff* skb, int intop)
{
	struct ath_softc* sc = (struct ath_softc*)me;
	//struct ath_vmac_instance *inst = sc->sc_vmac_mac_inst;
	struct ath_buf *bf = *((struct ath_buf **)(skb->cb+ATH_VMAC_CB_RX_BF0));

	if (!bf)
	{
		printk("%s error bf == 0\n", __func__);
		dev_kfree_skb(skb);
		return VMAC_MAC_NOTIFY_OK;
	}

	struct ieee80211_node *ni;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_node *an;
	struct ath_desc *ds = bf->bf_desc;
	int type;

	sc->sc_stats.ast_ant_rx[ds->ds_rxstat.rs_antenna]++;

	if (skb->len < IEEE80211_ACK_LEN)
	{
		DPRINTF(sc, ATH_DEBUG_RECV,	"%s: runt packet %d\n", __func__, skb->len);
		sc->sc_stats.ast_rx_tooshort++;
		dev_kfree_skb(skb);
		return VMAC_MAC_NOTIFY_OK;
	}

	skb->protocol = ETH_P_CONTROL;		/* XXX */

	/* At this point we have no need for error frames
	* that aren't crypto problems since we're done with capture. */
	if (ds->ds_rxstat.rs_status &~ (HAL_RXERR_DECRYPT|HAL_RXERR_MIC))
	{
		dev_kfree_skb(skb);
		return VMAC_MAC_NOTIFY_OK;
	}

	/* From this point on we assume the frame is at least
	* as large as ieee80211_frame_min; verify that.  */

	if (skb->len < IEEE80211_MIN_LEN)
	{
		DPRINTF(sc, ATH_DEBUG_RECV, "%s: short packet %d\n", __func__, skb->len);
		sc->sc_stats.ast_rx_tooshort++;
		dev_kfree_skb(skb);
		return VMAC_MAC_NOTIFY_OK;
	}

	/* Normal receive. */
	if (IFF_DUMPPKTS(sc, ATH_DEBUG_RECV))
	{
		ieee80211_dump_pkt(skb->data, skb->len,
			sc->sc_hwmap[ds->ds_rxstat.rs_rate].ieeerate, ds->ds_rxstat.rs_rssi);
	}

	skb_trim(skb, skb->len - IEEE80211_CRC_LEN);

	/* Locate the node for sender, track state, and then
	* pass the (referenced) node up to the 802.11 layer for its use. */
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)skb->data);

	/* Track rx rssi and do any rx antenna management. */
	an = ATH_NODE(ni);
	ATH_RSSI_LPF(an->an_avgrssi, ds->ds_rxstat.rs_rssi);
	if (sc->sc_diversity)
	{
		/* When using fast diversity, change the default rx
		* antenna if diversity chooses the other antenna 3 times in a row. */
		if (sc->sc_defant != ds->ds_rxstat.rs_antenna)
		{
			if (++sc->sc_rxotherant >= 3)
				ath_setdefantenna(sc, ds->ds_rxstat.rs_antenna);
		}
		else
			sc->sc_rxotherant = 0;
	}

	/** Send frame up for processing. */
	type = ieee80211_input(ic, skb, ni, ds->ds_rxstat.rs_rssi, ds->ds_rxstat.rs_tstamp);

	/* Reclaim node reference. */
	ieee80211_free_node(ni);

	return VMAC_MAC_NOTIFY_OK;
}

static int ath_vmac_mac_detach(void *me)
{
	struct ath_softc *sc = (struct ath_softc*)me;
	struct ath_vmac_instance *inst = sc->sc_vmac_inst;

	if (inst->phyinfo == inst->defaultphy)
		return 0;

	write_lock(&(inst->lock));
	vmac_free_phy(inst->phyinfo);
	inst->phyinfo = inst->defaultphy;
	write_unlock(&(inst->lock));
	return 0;
}

static int ath_vmac_mac_attach(void *me, vmac_phy_t* phyinfo)
{
	struct ath_softc* sc = (struct ath_softc*)me;
	struct ath_vmac_instance *inst = sc->sc_vmac_inst;
	int result = 0;

	ath_vmac_mac_detach(sc);

	write_lock(&(inst->lock));
	/* get reference to PHY */
	inst->phyinfo = vmac_get_phy(phyinfo);
	write_unlock(&(inst->lock));
	
	return result;
}

static int ath_vmac_mac_set_rx_func(void *me, mac_rx_func_t rxfunc, void *rxpriv)
{
	struct ath_softc* sc = (struct ath_softc*)me;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_vmac_instance *inst = sc->sc_vmac_inst;
	int result = 0;
	
	write_lock(&(inst->lock));
	ic->ic_softmac_rxfunc = rxfunc;
	ic->ic_softmac_rxpriv = rxpriv;
	write_unlock(&(inst->lock));
	return result;
}

static void ath_vmac_set_macinfo_functions(vmac_mac_t *macinfo)
{
	macinfo->mac_tx = ath_vmac_mac_packet_tx;
	//macinfo->mac_tx_done = cu_softmac_mac_packet_tx_done_ath;
	macinfo->rx_packet = ath_vmac_mac_packet_rx;
	//macinfo->mac_work = cu_softmac_mac_work_ath;
	macinfo->attach_phy = ath_vmac_mac_attach;
	macinfo->detach_phy = ath_vmac_mac_detach;
	macinfo->set_rx_func = ath_vmac_mac_set_rx_func;
	//macinfo->set_unload_notify_func = cu_softmac_mac_set_unload_notify_func_ath;
}

void* ath_vmac_new_mac(void *layer_private)
{
	struct list_head *list = layer_private;
	struct list_head *p;
	struct ath_softc *sc;
	struct ath_vmac_instance *inst;

	list_for_each(p, list)
	{
		sc = list_entry(p, struct ath_softc, sc_vmac_mac_list);
		inst = sc->sc_vmac_inst;
		if (!inst->macinfo)
		{/* default do nothing mac to avoid null checks */
			inst->defaultphy = vmac_alloc_phy();
			inst->phyinfo = inst->defaultphy;

			/* create a public interface and register it with softmac */
			inst->macinfo = vmac_alloc_mac();
			snprintf(inst->macinfo->name, VMAC_NAME_SIZE, VMAC_MODULE_ATH_MAC"%d", inst->id);
			inst->macinfo->layer = &the_athmac;
			inst->macinfo->mac_private = sc;
			ath_vmac_set_macinfo_functions(inst->macinfo);
			vmac_register_mac(inst->macinfo);
			/* why ???, lizhijie */
			vmac_free_mac(inst->macinfo);
			return inst->macinfo;
		}
	}
	return 0;
}

/* Called when a Atheros vmac_mac_t structure is being deallocated */
void ath_vmac_free_mac(void *layer_private, void *mac)
{
	vmac_mac_t *macinfo = mac;
	struct ath_softc *sc = (struct ath_softc *)macinfo->mac_private;
	struct ath_vmac_instance *inst = sc->sc_vmac_inst;

	ath_vmac_mac_detach(sc);
	vmac_free_phy(inst->defaultphy);
	inst->phyinfo = 0;
	inst->defaultphy = 0;
	vmac_free_mac(inst->macinfo);
	inst->macinfo = 0;
}

