
#include "if_ath_vmac.h"

// ath_cu_softmac_rx function is called when a packet comes in 
static int ath_cu_softmac_rx(struct net_device* dev,struct sk_buff* skb,int intop) 
{
	struct ath_softc* sc = dev->priv;
	int result = 0;
	void *macpriv;
	int rxresult;
    
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
	read_lock(&(sc->sc_vmac_mac_lock));
#else
	if (!read_trylock(&(sc->sc_vmac_mac_lock)))
	{
		dev_kfree_skb_any(skb);
		return 0;
	}
#endif

	/* if in raw mode, packets are passed to the MAC unchanged, eg. ath net_device 
	* otherwise, softmac packets get decapsulated and others are dropped     */
	if ( !(sc->sc_vmac_options & CU_SOFTMAC_ATH_RAW_MODE) )
	{
		if (vmac_ath_issoftmac(sc,skb))
		{/* remove vmac header and send to upper VMAC layer */
			//printk("%s got softmac packet!\n", __func__);
			skb = vmac_ath_decapsulate(sc, skb);
		}
		else
		{/* not in raw mode and not softmac, discard */
			dev_kfree_skb_any(skb);
			read_unlock(&(sc->sc_vmac_mac_lock));
			return 0;
		}
	}
	else
	{
		//printk("%s got non-softmac packet!\n", __func__);
	}

	/* rx by upper layer VMAC */
	macpriv = sc->sc_vmac_mac->mac_private;
	rxresult = (sc->sc_vmac_mac->rx_packet)(macpriv,skb,intop);
	if (VMAC_MAC_NOTIFY_OK == rxresult)
	{
		//printk(KERN_DEBUG "VMAC: packet handled -- not running again\n");
	}
	else if (VMAC_MAC_NOTIFY_RUNAGAIN == rxresult)
	{
		//printk(KERN_DEBUG "VMAC: packet not finished -- running again\n");
		result = 1;
	}
	else
	{
		/* Something went wrong -- free the packet and move on. */
		printk(KERN_DEBUG "VMAC: packet handling failed -- nuking\n");
		dev_kfree_skb_any(skb);
	}

	read_unlock(&(sc->sc_vmac_mac_lock));
	return result;
}

/* called in ISR or RX tasklet */
int ath_cu_softmac_handle_rx(struct net_device* dev,int intop)
{
	int result = 0;
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((caddr_t)(_sc)->sc_desc + \
		((_pa) - (_sc)->sc_desc_daddr)))

#define	IS_CTL(wh) \
	((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)

	//struct net_device *dev = (struct net_device *)data;
	struct ath_buf *bf;
	struct ath_softc *sc = dev->priv;
	//struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds;
	struct sk_buff *skb;
	//struct ieee80211_node *ni;
	//struct ath_node *an;
	int len;
	//u_int phyerr;
	HAL_STATUS status;

	//printk(KERN_DEBUG "if_ath: in handle_rx %d\n",intop);
	DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s\n", __func__);

	do
	{
		bf = STAILQ_FIRST(&sc->sc_rxbuf);
		if (bf == NULL)
		{		/* XXX ??? can this happen */
			printk("%s: no buffer (%s)\n", dev->name, __func__);
			break;
		}
		ds = bf->bf_desc;
		if (ds->ds_link == bf->bf_daddr)
		{/* NB: never process the self-linked entry at the end */
			break;
		}
		skb = bf->bf_skb;
		if (skb == NULL)
		{		/* XXX ??? can this happen */
			printk("%s: no skbuff (%s)\n", dev->name, __func__);
			continue;
		}
		/* XXX sync descriptor memory */
		/* Must provide the virtual address of the current
		* descriptor, the physical address, and the virtual
		* address of the next descriptor in the h/w chain.
		* This allows the HAL to look ahead to see if the
		* hardware is done with a descriptor by checking the
		* done bit in the following descriptor and the address
		* of the current descriptor the DMA engine is working
		* on.  All this is necessary because of our use of
		* a self-linked list to avoid rx overruns.     */

		status = ath_hal_rxprocdesc(ah, ds, bf->bf_daddr, PA2DESC(sc, ds->ds_link));
#ifdef AR_DEBUG
		if (sc->sc_debug & ATH_DEBUG_RECV_DESC)
			ath_printrxbuf(bf, status == HAL_OK); 
#endif
		if (status == HAL_EINPROGRESS)
			break;

		STAILQ_REMOVE_HEAD(&sc->sc_rxbuf, bf_list);

		// XXX need to decide what to do with jumbograms
		if (ds->ds_rxstat.rs_status != 0)
		{
			//DPRINTF(sc,ATH_DEBUG_ANY,"%s: RXERR: %hx DATALEN %hd\n",__func__,(short int)ds->ds_rxstat.rs_status,ds->ds_rxstat.rs_datalen);
			if (ds->ds_rxstat.rs_status & HAL_RXERR_CRC)
			{
				//DPRINTF(sc,ATH_DEBUG_ANY,"%s: RXERR: %hx HAL_RXERR_CRC\n",__func__,(short int)ds->ds_rxstat.rs_status);
			}
			if (ds->ds_rxstat.rs_status & HAL_RXERR_FIFO)
			{
				//DPRINTF(sc,ATH_DEBUG_ANY,"%s: RXERR: %hx HAL_RXERR_FIFO\n",__func__,(short int)ds->ds_rxstat.rs_status);
			}
			if (ds->ds_rxstat.rs_status & HAL_RXERR_PHY)
			{
				//sc->sc_stats.ast_rx_phyerr++;
				//phyerr = ds->ds_rxstat.rs_phyerr & 0x1f;
				//sc->sc_stats.ast_rx_phy[phyerr]++;
				//DPRINTF(sc,ATH_DEBUG_ANY,"%s: RXERR: %hx HAL_RXERR_PHY %hx\n",__func__,(short int)ds->ds_rxstat.rs_status,(short int)ds->ds_rxstat.rs_phyerr);
			}
		}

		//rx_accept:
		/*
		* Sync and unmap the frame.  At this point we're
		* committed to passing the sk_buff somewhere so
		* clear buf_skb; this means a new sk_buff must be
		* allocated when the rx descriptor is setup again
		* to receive another frame.     */

		len = ds->ds_rxstat.rs_datalen;
		bus_dma_sync_single(sc->sc_bdev, bf->bf_skbaddr, len, BUS_DMA_FROMDEVICE);

		if (0 < len)
		{
			ath_cu_softmac_tag_cb(sc,ds,skb);
		}

		bus_unmap_single(sc->sc_bdev, bf->bf_skbaddr, sc->sc_rxbufsize, BUS_DMA_FROMDEVICE);
		bf->bf_skb = NULL;

		/* Normal receive. */

		if (0 < len)
		{
			// Handoff to the softmac
			if ((sc->sc_vmac_options & CU_SOFTMAC_ATH_ALLOW_CRCERR) || !(skb->cb[ATH_VMAC_CB_RX_CRCERR]))
			{
				skb_put(skb, len);
				*((struct ath_buf **)(skb->cb+ATH_VMAC_CB_RX_BF0)) = bf;
				if (ath_cu_softmac_rx(dev,skb,intop))
				{
					// Returning non-zero from here means that
					// the softmac layer has not finished handling
					// the packet. The softmac tasklet should execute.
					result = 1;
				}
			}
			else
			{
				// Free packets with CRC errors
				dev_kfree_skb_any(skb);
			}
		}
		else
		{
			dev_kfree_skb_any(skb);
		}

		//rx_next:
		STAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	}while (ath_rxbuf_init(sc, bf) == 0);

#undef IS_CTL
#undef PA2DESC
	return result;
}

/* called in RX tasklet */
void ath_cu_softmac_rx_tasklet(TQUEUE_ARG data)
{
	struct net_device *dev = (struct net_device *)data;
	struct ath_softc* sc = dev->priv;
	int goagain = 0; 
	//printk(KERN_DEBUG "if_ath: in rx_tasklet\n");
	goagain = ath_cu_softmac_handle_rx(dev,0);
	if (goagain)
	{
		int needmark = 0;
		ATH_SCHEDULE_TQUEUE(&sc->sc_vmac_worktq, &needmark);
		if (needmark)
		{
			mark_bh(IMMEDIATE_BH);
		}
	}
}

