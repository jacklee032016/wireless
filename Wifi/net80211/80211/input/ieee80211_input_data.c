/*
* $Id: ieee80211_input_data.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#include "ieee80211_input.h"

/* parse 802.11 header and construct ether net header */
static struct sk_buff *ieee80211_decap(struct ieee80211com *ic, struct sk_buff *skb)
{
	struct ieee80211_frame wh;	/* NB: QoS stripped above */
	struct ether_header *eh;
	struct llc *llc;
	u_short ether_type = 0;
	
	// TODO: why not using linux llc.h ..., same for ieee80211_encap()
	memcpy(&wh, skb->data, sizeof(struct ieee80211_frame));
	llc = (struct llc *) skb_pull(skb, sizeof(struct ieee80211_frame));
	if (skb->len >= sizeof(struct llc) &&
		llc->llc_dsap == LLC_SNAP_LSAP && llc->llc_ssap == LLC_SNAP_LSAP &&
		llc->llc_control == LLC_UI && llc->llc_snap.org_code[0] == 0 &&
		llc->llc_snap.org_code[1] == 0 && llc->llc_snap.org_code[2] == 0)
	{
		ether_type = llc->llc_un.type_snap.ether_type;
		skb_pull(skb, sizeof(struct llc));
		llc = NULL;
	}
	
	eh = (struct ether_header *) skb_push(skb, sizeof(struct ether_header));
	switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK)
	{
		case IEEE80211_FC1_DIR_NODS:
			IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
			IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
			break;
		case IEEE80211_FC1_DIR_TODS:
			IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
			IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
			break;
		case IEEE80211_FC1_DIR_FROMDS:
			IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
			IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr3);
			break;
		case IEEE80211_FC1_DIR_DSTODS:
			/* not yet supported */
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY,
			    &wh, "data", "%s", "DS to DS not supported");
			dev_kfree_skb(skb);
			return NULL;
	}
	/*
	 *  TODO: we should ensure alignment on specific arch like amd64, alpha 
	 *  and ia64 at least. BSD uses ALIGNED_POINTER macro for this.
	 *  If anyone gets problems on one of the mentioned archs, modify compat.h
	 */
#ifdef ALIGNED_POINTER
	if (!ALIGNED_POINTER(skb->data + sizeof(*eh), u_int32_t))
	{
		struct sk_buff *n;

		/* XXX does this always work? */
		n = skb_copy(skb, GFP_ATOMIC);
		dev_kfree_skb(skb);
		if (n == NULL)
			return NULL;
		skb = n;
		eh = (struct ether_header *) skb->data;
	}
#endif /* ALIGNED_POINTER */

	if (llc != NULL)
		eh->ether_type = htons(skb->len - sizeof(*eh));
	else
		eh->ether_type = ether_type;

	return skb;
}

/*
 * This function reassemble fragments using the skb of the 1st fragment,
 * if large enough. If not, a new skb is allocated to hold incoming
 * fragments.
 *
 * Fragments are copied at the end of the previous fragment.  A different
 * strategy could have been used, where a non-linear skb is allocated and
 * fragments attached to that skb.
 */
static struct sk_buff *ieee80211_defrag(struct ieee80211com *ic, struct ieee80211_node *ni, struct sk_buff *skb)
{
	struct ieee80211_frame *wh = (struct ieee80211_frame *) skb->data;
	struct ieee80211_frame *lwh;
	u_int16_t rxseq;
	u_int8_t fragno;
	u_int8_t more_frag = wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG;
	struct sk_buff *skbfrag;

	KASSERT(!IEEE80211_IS_MULTICAST(wh->i_addr1), ("multicast fragm?"));

	rxseq = le16toh(*(u_int16_t *)wh->i_seq);
	fragno = rxseq & IEEE80211_SEQ_FRAG_MASK;

	/* Quick way out, if there's nothing to defragment */
	if (!more_frag && fragno == 0 && ni->ni_rxfrag[0] == NULL)
		return skb;

	/* Remove frag to insure it doesn't get reaped by timer. */
	if (ni->ni_table == NULL)
	{
		/*
		 * Should never happen.  If the node is orphaned (not in
		 * the table) then input packets should not reach here.
		 * Otherwise, a concurrent request that yanks the table
		 * should be blocked by other interlocking and/or by first
		 * shutting the driver down.  Regardless, be defensive
		 * here and just bail
		 */
		/* XXX need msg+stat */
		dev_kfree_skb(skb);
		return NULL;
	}
	IEEE80211_NODE_LOCK(ni->ni_table);
	skbfrag = ni->ni_rxfrag[0];
	ni->ni_rxfrag[0] = NULL;
	IEEE80211_NODE_UNLOCK(ni->ni_table);

	/*
	 * Validate new fragment is in order and
	 * related to the previous ones.
	 */
	if (skbfrag != NULL)
	{
		u_int16_t last_rxseq;

		lwh = (struct ieee80211_frame *) skbfrag->data;
		last_rxseq = le16toh(*(u_int16_t *)lwh->i_seq);
		/* NB: check seq # and frag together */
		if (rxseq != last_rxseq+1 ||
		    !IEEE80211_ADDR_EQ(wh->i_addr1, lwh->i_addr1) ||
		    !IEEE80211_ADDR_EQ(wh->i_addr2, lwh->i_addr2))
		{
			/*
			 * Unrelated fragment or no space for it,
			 * clear current fragments.
			 */
			dev_kfree_skb(skbfrag);
			skbfrag = NULL;
		}
	}

	/* If this is the first fragment */
 	if (skbfrag == NULL)
	{
		if (fragno != 0)
		{/* !first fragment, discard */
			IEEE80211_NODE_STAT(ni, rx_defrag);
			dev_kfree_skb(skb);
			return NULL;
		}
		skbfrag = skb;
		if (more_frag)
		{
			/*
			 * Check that we have enough space to hold
			 * all remaining incoming fragments
			 * 
			 * ath always has enough space, since we always
			 * allocate the largest possible buffer
			 * (IEEE80211_MAX_LEN + cacheline size)
			 * but maybe it's better to double check here
			 */
			if (skb->end - skb->head < ic->ic_dev->mtu +
				sizeof(struct ieee80211_frame))
			{
				/*
				 * This should also linearize the skbuff
				 * TODO: not 100% sure
				 */
				skbfrag = skb_copy_expand(skb, 0,
					(ic->ic_dev->mtu +
					 sizeof(struct ieee80211_frame))
				         - (skb->end - skb->head), GFP_ATOMIC);
				dev_kfree_skb(skb);
			}
		}
	}
	else
	{/* concatenate */
		/*
		 * We know we have enough space to copy,
		 * we've verified that before
		 */
		/* Copy current fragment at end of previous one */
		memcpy(skbfrag->tail, skb->data + sizeof(struct ieee80211_frame),
		       skb->len - sizeof(struct ieee80211_frame) );
		/* Update tail and length */
		skb_put(skbfrag, skb->len - sizeof(struct ieee80211_frame));
		/* track last seqnum and fragno */
		lwh = (struct ieee80211_frame *) skbfrag->data;
		*(u_int16_t *) lwh->i_seq = *(u_int16_t *) wh->i_seq;
		/* we're done with the fragment */
		dev_kfree_skb(skb);
	}
	
	if (more_frag)
	{/* more to come, save */
		ni->ni_rxfragstamp = jiffies;
		ni->ni_rxfrag[0] = skbfrag;
		skbfrag = NULL;
	}
	return skbfrag;
}


int ieee80211_input_data(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
	struct net_device *dev = ic->ic_dev;
	struct ieee80211_frame *wh;
	struct ieee80211_key *key;
	struct ether_header *eh;
	int len, hdrsize, off;
	u_int8_t dir, type, subtype;
	struct ieee80211_cb *cb = (struct ieee80211_cb *)skb->cb;

	wh = (struct ieee80211_frame *)skb->data;
	hdrsize = ieee80211_hdrsize(wh);
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	if (ic->ic_flags & IEEE80211_F_DATAPAD)
		hdrsize = roundup(hdrsize, sizeof(u_int32_t));

	if (skb->len < hdrsize)
	{
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
			     ni->ni_macaddr, NULL, "data too short: len %u, expecting %u", skb->len, hdrsize);
		ic->ic_stats.is_rx_tooshort++;
		goto out;		/* XXX */
	}

	if (subtype & IEEE80211_FC0_SUBTYPE_QOS)
	{/* XXX discard if node w/o IEEE80211_NODE_QOS? */
	/* Strip QoS control and any padding so only a stock 802.11 header is at the front. */

		/* XXX 4-address QoS frame */
		off = hdrsize - sizeof(struct ieee80211_frame);
		memmove(skb->data + off, skb->data, hdrsize - off);
		skb_pull(skb, off);
		wh = (struct ieee80211_frame *)skb->data;
		wh->i_fc[0] &= ~IEEE80211_FC0_SUBTYPE_QOS;
	}
	else
	{
			/* XXX copy up for 4-address frames w/ padding */
	}

	switch (ic->ic_opmode)
	{
		case IEEE80211_M_STA:
		{
			if (dir != IEEE80211_FC1_DIR_FROMDS)
			{
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}

			if ((dev->flags & IFF_MULTICAST) &&
				    IEEE80211_IS_MULTICAST(wh->i_addr1) &&
				    IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_myaddr))
			{
				/*
				 * In IEEE802.11 network, multicast packet
				 * sent from me is broadcasted from AP.
				 * It should be silently discarded for
				 * SIMPLEX interface.
				 *
				 * NB: Linux has no IFF_ flag to indicate
				 *     if an interface is SIMPLEX or not;
				 *     so we always assume it to be true.
				 */
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT, wh, NULL, "%s", "multicast echo");
				ic->ic_stats.is_rx_mcastecho++;
				goto out;
			}
			break;
		}	

		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
		{
			if (dir != IEEE80211_FC1_DIR_NODS)
			{
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			/* XXX no power-save support */
			break;
		}		

		case IEEE80211_M_HOSTAP:
		{
			if (dir != IEEE80211_FC1_DIR_TODS)
			{
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}

			/* check if source STA is associated */
			if (ni == ic->ic_bss)
			{
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT, wh, "data", "%s", "unknown src");

				/* NB: caller deals with reference */
				ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);
				if (ni != NULL)
				{
					IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
						    IEEE80211_REASON_NOT_AUTHED);
					ieee80211_free_node(ni);
				}
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}

			if (ni->ni_associd == 0)
			{
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT, wh, "data", "%s", "unassoc src");
				IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DISASSOC,
					    IEEE80211_REASON_NOT_ASSOCED);
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}

			/* Check for power save state change.*/
			if (((wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) ^(ni->ni_flags & IEEE80211_NODE_PWR_MGT)))
				ieee80211_node_pwrsave(ni,	wh->i_fc[1] & IEEE80211_FC1_PWR_MGT);

			break;
		}

		default:
			/* XXX here to keep compiler happy */
			goto out;
	}

	/*
	 * Handle privacy requirements.  Note that we
	 * must not be preempted from here until after
	 * we (potentially) call ieee80211_crypto_demic;
	 * otherwise we may violate assumptions in the
	 * crypto cipher modules used to do delayed update
	 * of replay sequence numbers.
	 */
	if (wh->i_fc[1] & IEEE80211_FC1_WEP)
	{
		if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0)
		{
			/*
			 * Discard encrypted frames when privacy is off.
			 */
			IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
			    wh, "WEP", "%s", "PRIVACY off");
			ic->ic_stats.is_rx_noprivacy++;
			IEEE80211_NODE_STAT(ni, rx_noprivacy);
			goto out;
		}

		key = ieee80211_crypto_decap(ic, ni, skb);
		if (key == NULL)
		{/* NB: stats+msgs handled in crypto_decap */
			IEEE80211_NODE_STAT(ni, rx_wepfail);
			goto out;
		}

		wh = (struct ieee80211_frame *)skb->data;
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
	}
	else
	{
		key = NULL;
	}

	/* Next up, any fragmentation. */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1))
	{
		skb = ieee80211_defrag(ic, ni, skb);
		if (skb == NULL)
		{
				/* Fragment dropped or frame not complete yet */
			goto out;
		}
	}
	wh = NULL;		/* no longer valid, catch any uses */

	/* Next strip any MSDU crypto bits. */
	if (key != NULL && !ieee80211_crypto_demic(ic, key, skb))
	{
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
			    ni->ni_macaddr, "data", "%s", "demic error");
		IEEE80211_NODE_STAT(ni, rx_demicfail);
		goto out;
	}

	/* Finally, strip the 802.11 header. */
	skb = ieee80211_decap(ic, skb);
	if (skb == NULL)
	{
		/* don't count Null data frames as errors */
		if (subtype == IEEE80211_FC0_SUBTYPE_NODATA)
			goto out;
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT, ni->ni_macaddr, "data", "%s", "decap error");
		ic->ic_stats.is_rx_decap++;
		IEEE80211_NODE_STAT(ni, rx_decap);
		goto err;
	}
	eh = (struct ether_header *) skb->data;

	if (!ieee80211_node_is_authorized(ni))
	{
		/*
		 * Deny any non-PAE frames received prior to
		 * authorization.  For open/shared-key
			 * authentication the port is mark authorized
			 * after authentication completes.  For 802.1x
			 * the port is not marked authorized by the
			 * authenticator until the handshake has completed.
			 */
		if (eh->ether_type != __constant_htons(ETHERTYPE_PAE))
		{
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
				    eh->ether_shost, "data",
				    "unauthorized port: ether type 0x%x len %u",
				    eh->ether_type, skb->len);
				ic->ic_stats.is_rx_unauth++;
			IEEE80211_NODE_STAT(ni, rx_unauth);
			goto err;
		}
	}
	else
	{
		/* When denying unencrypted frames, discard
		 * any non-PAE frames received without encryption.*/
		if ((ic->ic_flags & IEEE80211_F_DROPUNENC) && key == NULL &&
			    eh->ether_type != __constant_htons(ETHERTYPE_PAE))
		{
			/* Drop unencrypted frames. */
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
				    eh->ether_shost, "data", "unencrypted: ether type 0x%x len %u",
				    eh->ether_type, skb->len);
			ic->ic_stats.is_rx_unencrypted++;
			IEEE80211_NODE_STAT(ni, rx_unencrypted);
			goto out;
		}
	}

	ic->ic_devstats->rx_packets++;
	ic->ic_devstats->rx_bytes += skb->len;
	IEEE80211_NODE_STAT(ni, rx_data);
	IEEE80211_NODE_STAT_ADD(ni, rx_bytes, skb->len);

		/* perform as a bridge within the AP */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
		    (ic->ic_flags & IEEE80211_F_NOBRIDGE) == 0)
	{
		struct sk_buff *skb1 = NULL;

		if (ETHER_IS_MULTICAST(eh->ether_dhost))
		{
			skb1 = skb_copy(skb, GFP_ATOMIC);
			if (skb1 != NULL)
				cb->flags |= M_MCAST;
		}
		else
		{
			/* XXX this dups work done in ieee80211_encap */
			/* check if destination is associated */
			struct ieee80211_node *ni1 = ieee80211_find_node(&ic->ic_sta, eh->ether_dhost);
			if (ni1 != NULL && ni1 != ic->ic_bss)
			{
				if (ni1->ni_associd != 0 && ieee80211_node_is_authorized(ni1))
				{
					skb1 = skb;
					skb = NULL;
				}
				else
				{
					IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
						eh->ether_shost, "data",
						"bridge: node %s (aid: %d) not authorized",
						ether_sprintf(ni1->ni_macaddr), ni1->ni_associd);
				}
				/* XXX statistic? */
			}
			else if (!IEEE80211_ADDR_EQ(eh->ether_dhost, ic->ic_bss->ni_bssid))
			{
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
						eh->ether_shost, "data",
						"bridge: dest node %s not found", ether_sprintf(eh->ether_dhost));
			}

			if(ni1 != NULL)
				ieee80211_free_node(ni1);

		}

		if (skb1 != NULL)
		{
			len = skb1->len;
			skb1->dev = dev;
			skb1->mac.raw = skb1->data;
			skb1->nh.raw = skb1->data + 
				sizeof(struct ether_header);
			skb1->protocol = __constant_htons(ETH_P_802_2);
			dev_queue_xmit(skb1);			// NB: send directly to iface
		}
	}

	/* send this skb to net_device */
	if (skb != NULL)
	{
	        skb->dev = dev;
		if (ni->ni_vlan != 0 && ic->ic_vlgrp != NULL)
		{
			/* attach vlan tag */
		       skb->protocol = eth_type_trans(skb, dev);
		       vlan_hwaccel_receive_skb(skb, ic->ic_vlgrp, ni->ni_vlan);
		}
		else
		{
#ifdef HAS_VMAC
		        if (ic->ic_softmac_rxfunc)
			{
			        ic->ic_softmac_rxfunc(ic->ic_softmac_rxpriv, skb);
			}
			else
#endif
			/* debug VMAC for rx, lizhijie, 2006,04,03 */
			{
			        skb->protocol = eth_type_trans(skb, dev);
		                netif_rx(skb);
			}
		}
		dev->last_rx = jiffies;
	}
	return IEEE80211_FC0_TYPE_DATA;

err:
	ic->ic_devstats->rx_errors++;
out:
	if (skb != NULL)
		dev_kfree_skb(skb);
	return type;
}

