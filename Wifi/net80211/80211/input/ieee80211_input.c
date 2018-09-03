/*
* $Id: ieee80211_input.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
*/

#include "ieee80211_input.h"

/*
 * Process a received frame.  The node associated with the sender
 * should be supplied.  If nothing was found in the node table then
 * the caller is assumed to supply a reference to ic_bss instead.
 * The RSSI and a timestamp are also supplied.  The RSSI data is used
 * during AP scanning to select a AP to associate with; it can have
 * any units so long as values have consistent units and higher values
 * mean ``better signal''.  The receive timestamp is currently not used
 * by the 802.11 layer.
 */
int ieee80211_input(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
#define	SEQ_LEQ(a,b)	((int)((a)-(b)) <= 0)
#define	HAS_SEQ(type)	((type & 0x4) == 0)
	struct net_device *dev = ic->ic_dev;
	struct ieee80211_frame *wh;
	u_int8_t dir, type, subtype;
	u_int8_t *bssid;
	u_int16_t rxseq;
	struct ieee80211_cb *cb = (struct ieee80211_cb *)skb->cb;
	// TODO: where is cb->flags set?

	KASSERT(ni != NULL, ("null node"));
	ni->ni_inact = ni->ni_inact_reload;

	/* trim CRC here so WEP can find its own CRC at the end of packet. */
	if (cb->flags & M_HASFCS)
	{
		skb_trim(skb, IEEE80211_CRC_LEN);
		cb->flags &= ~M_HASFCS;
	}
	KASSERT(skb->len >= sizeof(struct ieee80211_frame_min), ("frame length too short: %u", skb->len));

	type = -1;	/* undefined */	

	/*
	 * In monitor mode, send everything directly to bpf.
	 * XXX may want to include the CRC
	 */
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		goto out;

	if (skb->len < sizeof(struct ieee80211_frame_min))
	{
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY, ni->ni_macaddr, NULL,  "too short (1): len %u", skb->len);
		ic->ic_stats.is_rx_tooshort++;
		goto out;
	}
	/*
	 * Bit of a cheat here, we use a pointer for a 3-address
	 * frame format but don't reference fields past outside
	 * ieee80211_frame_min w/o first validating the data is
	 * present.
	 */
	wh = (struct ieee80211_frame *)skb->data;

	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) != IEEE80211_FC0_VERSION_0)
	{
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,  ni->ni_macaddr, NULL, "wrong version %x", wh->i_fc[0]);
		ic->ic_stats.is_rx_badversion++;
		goto err;
	}

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	if ((ic->ic_flags & IEEE80211_F_SCAN) == 0)
	{
		switch (ic->ic_opmode)
		{
			case IEEE80211_M_STA:
				bssid = wh->i_addr2;
				if (!IEEE80211_ADDR_EQ(bssid, ni->ni_bssid))
				{
					/* not interested in */
					IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT, bssid, NULL, "%s", "not to bss");
					ic->ic_stats.is_rx_wrongbss++;
					goto out;
				}
				break;
			case IEEE80211_M_IBSS:
			case IEEE80211_M_AHDEMO:
			case IEEE80211_M_HOSTAP:
				if (dir != IEEE80211_FC1_DIR_NODS)
					bssid = wh->i_addr1;
				else if (type == IEEE80211_FC0_TYPE_CTL)
					bssid = wh->i_addr1;
				else
				{
					if (skb->len < sizeof(struct ieee80211_frame))
					{
						IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY, ni->ni_macaddr,   NULL, "too short (2): len %u", skb->len);
						ic->ic_stats.is_rx_tooshort++;
						goto out;
					}
					bssid = wh->i_addr3;
				}
				if (type != IEEE80211_FC0_TYPE_DATA)
					break;
				/*
				 * Data frame, validate the bssid.
				 */
				if (!IEEE80211_ADDR_EQ(bssid, ic->ic_bss->ni_bssid) &&
				    !IEEE80211_ADDR_EQ(bssid, dev->broadcast))
				{
					/* not interested in */
					IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
					    bssid, NULL, "%s", "not to bss");
					ic->ic_stats.is_rx_wrongbss++;
					goto out;
				}
				/*
				 * For adhoc mode we cons up a node when it doesn't
				 * exist. This should probably done after an ACL check.
				 */
				if (ni == ic->ic_bss &&
				    ic->ic_opmode != IEEE80211_M_HOSTAP) {
					/*
					 * Fake up a node for this newly
					 * discovered member of the IBSS.
					 */
					IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT,
					    "[%s] creating adhoc node for data frame\n",
					    ether_sprintf(wh->i_addr2));
					ni = ieee80211_fakeup_adhoc_node(&ic->ic_sta,
							    wh->i_addr2);
					if (ni == NULL) {
						/* NB: stat kept for alloc failure */
						goto err;
					}
				}
				break;
			default:
				goto out;
		}

		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;

		if (HAS_SEQ(type))
		{
			u_int8_t tid;
			if (IEEE80211_QOS_HAS_SEQ(wh)) {
				tid = ((struct ieee80211_qosframe *)wh)->
					i_qos[0] & IEEE80211_QOS_TID;
				if (tid >= WME_AC_VI)
					ic->ic_wme.wme_hipri_traffic++;
				tid++;
			}
			else
				tid = 0;

			rxseq = le16toh(*(u_int16_t *)wh->i_seq);
			if ((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
			    SEQ_LEQ(rxseq, ni->ni_rxseqs[tid]))
			{
				/* duplicate, discard */
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
				    bssid, "duplicate",
				    "seqno <%u,%u> fragno <%u,%u> tid %u",
				    rxseq >> IEEE80211_SEQ_SEQ_SHIFT,
				    ni->ni_rxseqs[tid] >>
					IEEE80211_SEQ_SEQ_SHIFT,
				    rxseq & IEEE80211_SEQ_FRAG_MASK,
				    ni->ni_rxseqs[tid] &
					IEEE80211_SEQ_FRAG_MASK,
				    tid);
				ic->ic_stats.is_rx_dup++;
				IEEE80211_NODE_STAT(ni, rx_dup);
				goto out;
			}
			ni->ni_rxseqs[tid] = rxseq;
		}
	}/* scan */

	switch (type)
	{
		case IEEE80211_FC0_TYPE_DATA:
			return ieee80211_input_data( ic, skb,  ni, rssi, rstamp);

		case IEEE80211_FC0_TYPE_MGT:
			return ieee80211_input_mgmt(ic, skb, ni, rssi, rstamp);

		case IEEE80211_FC0_TYPE_CTL:
			IEEE80211_NODE_STAT(ni, rx_ctrl);
			ic->ic_stats.is_rx_ctl++;
			if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			{
				switch (subtype)
				{
					case IEEE80211_FC0_SUBTYPE_PS_POLL:
						ieee80211_recv_pspoll(ic, ni, skb);
						break;
				}
			}
			goto out;
		default:
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY,
			    wh, NULL, "bad frame type 0x%x", type);
			/* should not come here */
			break;
	}
err:
	ic->ic_devstats->rx_errors++;
out:
	if (skb != NULL)
		dev_kfree_skb(skb);
	return type;
#undef HAS_SEQ
#undef SEQ_LEQ
}

EXPORT_SYMBOL(ieee80211_input);

