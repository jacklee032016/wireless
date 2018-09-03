/*
* $Id: wlan_output_utils.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_output.h"

#include "if_llc.h"
#include "if_ethersubr.h"

#include <linux/ip.h>
/* 
 * Assign priority to a frame based on any vlan tag assigned
 * to the station and/or any Diffserv setting in an IP header.
 * Finally, if an ACM policy is setup (in station mode) it's
 * applied.
 */
/* this function must be enhanced in future, lizhijie 2006.10.05 */
int ieee80211_classify(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni)
{
	int v_wme_ac, d_wme_ac, ac;
	struct ether_header *eh;
	const struct iphdr *ip;

	struct llc *llc;
	int hdrsize;
	hdrsize = sizeof(struct ieee80211_frame);
//	if (ic->ic_flags & IEEE80211_F_DATAPAD)
	hdrsize = roundup(hdrsize, sizeof(u_int32_t));

	if ((ni->ni_flags & IEEE80211_NODE_QOS) == 0)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME, "[%s] no QOS/WME for this node (%s)\n", __func__, swjtu_mesh_mac_sprintf(ni->ni_macaddr));
		ac = WME_AC_BE;
		goto done;
	}

	/* 
	 * If node has a vlan tag then all traffic
	 * to it must have a matching tag.
	 */
	v_wme_ac = 0;
	
#if WITH_VLAN_ENABLED	
	if (ni->ni_vlan != 0)
	{
		 if (ic->ic_vlgrp == NULL || !vlan_tx_tag_present(skb))
		 {
			IEEE80211_NODE_STAT(ni, tx_novlantag);
			return 1;
		}

		if (vlan_tx_tag_get(skb) != ni->ni_vlan)
		{
			IEEE80211_NODE_STAT(ni, tx_vlanmismatch);
			return 1;
		}
		/* map vlan priority to AC */
		switch (vlan_get_ingress_priority(skb->dev, ni->ni_vlan))
		{
			case 1:
			case 2:
				v_wme_ac = WME_AC_BK;
				break;
			case 0:
			case 3:
				v_wme_ac = WME_AC_BE;
				break;
			case 4:
			case 5:
				v_wme_ac = WME_AC_VI;
				break;
			case 6:
			case 7:
				v_wme_ac = WME_AC_VO;
				break;
		}
	}
#endif
	
//	eh = (struct ether_header *) skb->data;
//	if (eh->ether_type == __constant_htons(ETHERTYPE_IP))
	llc = (struct llc *) (skb->data + hdrsize) ;
	
	if (llc->llc_snap.ether_type == __constant_htons(ETHERTYPE_IP))
	{
		ip = skb->nh.iph;

		/*
		 * IP frame, map the DSCP field (corrected version, <<2).
		 */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME, "ip=%p,ip->version=%d, ip->length=%d, &(ip->saddr)=%p, &(ip->daddr)=%p\n",
			ip, ip->version, ip->ihl, &(ip->saddr), &(ip->daddr));
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME, "[%s (%s)] src: 0x%x dst: 0x%x paket tos: 0x%x\n", 
				__func__, ic->ic_dev->name, ip->saddr, ip->daddr, ip->tos);
		switch (ip->tos)
		{
		case 0x20:
		case 0x40:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME, "[%s (%s)] sorting packet in WME_AC_BK queue (node %s)\n", 
				__func__, ic->ic_dev->name, swjtu_mesh_mac_sprintf(ni->ni_macaddr));
			d_wme_ac = WME_AC_BK;	/* background */
			break;
		case 0x80:
		case 0xa0:
			d_wme_ac = WME_AC_VI;	/* video */
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME, "[%s (%s)] sorting packet in WME_AC_VI queue (node %s)\n", 
				__func__, ic->ic_dev->name, swjtu_mesh_mac_sprintf(ni->ni_macaddr));
			break;
		case 0xc0:			/* voice */
		case 0xe0:
		case 0x88:			/* XXX UPSD */
		case 0xb8:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME, "[%s (%s)] sorting packet in WME_AC_VO queue (node %s)\n", 
				__func__, ic->ic_dev->name, swjtu_mesh_mac_sprintf(ni->ni_macaddr));
			d_wme_ac = WME_AC_VO;
			break;
		default:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME, "[%s (%s)] sorting packet in WME_AC_BE queue (node %s)\n", 
				__func__, ic->ic_dev->name, swjtu_mesh_mac_sprintf(ni->ni_macaddr));
			d_wme_ac = WME_AC_BE;
			break;
		}
	}
	else
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME, "[%s (%s)] no IP packet, sorting packet in WME_AC_BE queue (node %s)\n", 
			__func__, ic->ic_dev->name, swjtu_mesh_mac_sprintf(ni->ni_macaddr));
		d_wme_ac = WME_AC_BE;
	}
	/*
	 * Use highest priority AC.
	 */
	if (v_wme_ac > d_wme_ac)
		ac = v_wme_ac;
	else
		ac = d_wme_ac;

	/*
	 * Apply ACM policy.
	 */
	if (ic->ic_opmode == IEEE80211_M_STA) {
		static const int acmap[4] = {
			WME_AC_BK,	/* WME_AC_BE */
			WME_AC_BK,	/* WME_AC_BK */
			WME_AC_BE,	/* WME_AC_VI */
			WME_AC_VI,	/* WME_AC_VO */
		};
		while (ac != WME_AC_BK &&
		    ic->ic_wme.wme_wmeBssChanParams.cap_wmeParams[ac].wmep_acm)
			ac = acmap[ac];
	}
done:
	M_WME_SETAC(skb, ac);
	return 0;
}
EXPORT_SYMBOL(ieee80211_classify);



/*
 * Save an outbound packet for a node in power-save sleep state.
 * The new packet is placed on the node's saved queue, and the TIM
 * is changed, if necessary.
 */
void ieee80211_pwrsave(struct ieee80211com *ic, struct ieee80211_node *ni, struct sk_buff *skb)
{
	int qlen, age;

	IEEE80211_NODE_SAVEQ_LOCK(ni);
	if (_IF_QFULL(&ni->ni_savedq))
	{
		_IF_DROP(&ni->ni_savedq);
		IEEE80211_NODE_SAVEQ_UNLOCK(ni);
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"[%s] pwr save q overflow (max size %d)\n",
			swjtu_mesh_mac_sprintf(ni->ni_macaddr), IEEE80211_PS_MAX_QUEUE);
#ifdef IEEE80211_DEBUG
		if (ieee80211_msg_dumppkts(ic))
			ieee80211_dump_pkt((caddr_t) skb->data, skb->len, -1, -1);
#endif
		dev_kfree_skb(skb);
		return;
	}
	/*
	 * Tag the frame with it's expiry time and insert
	 * it in the queue.  The aging interval is 4 times
	 * the listen interval specified by the station. 
	 * Frames that sit around too long are reclaimed
	 * using this information.
	 */
	/* XXX handle overflow? */
	age = ((ni->ni_intval * ic->ic_lintval) << 2) / 1024; /* TU -> secs */
	// TODO: required? cb->ni = ni;
	_IEEE80211_NODE_SAVEQ_ENQUEUE(ni, skb, qlen, age);
	IEEE80211_NODE_SAVEQ_UNLOCK(ni);

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		"[%s] save frame, %u now queued\n",
		swjtu_mesh_mac_sprintf(ni->ni_macaddr), qlen);

	if (qlen == 1)
		ic->ic_set_tim(ic, ni, 1);
}
EXPORT_SYMBOL(ieee80211_pwrsave);

