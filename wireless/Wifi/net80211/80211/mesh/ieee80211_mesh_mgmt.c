/*
* $Id: ieee80211_mesh_mgmt.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
*/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/if_vlan.h>

#include "if_llc.h"
#include "if_ethersubr.h"
#include "if_media.h"

#include <ieee80211_var.h>
#include "ieee80211_mesh.h"

#include "mesh_route.h"


void action_frame_rx(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni,	int subtype, int rssi, u_int32_t rstamp)
{
	struct ieee80211_frame *wh;
	u_int8_t *frm, *efrm;
	u_int8_t *route;
//	int allocbs;
	struct ieee80211_action_header *actionheader;
	struct net_device *netdev = ic->ic_dev;

	route_dev_t *routeDev;
	route_op_type_t result;
	
	wh = (struct ieee80211_frame *) skb->data;
	frm = (u_int8_t *)&wh[1];
	efrm = skb->data + skb->len;

	actionheader =(struct ieee80211_action_header *) frm;

	if (ic->ic_opmode != IEEE80211_M_IBSS ||ic->ic_state != IEEE80211_S_RUN)
	{
		ic->ic_stats.is_rx_mgtdiscard++;
		return;
	}

#if 0	
	if (IEEE80211_IS_MULTICAST(wh->i_addr2))
	{ /* action frame can not be send in multicast source address */
		ic->ic_stats.is_rx_mgtdiscard++;	/* XXX stat */
		return;
	}
#endif

	if(!IEEE80211_IS_MESH_MGMT(actionheader->category) )
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_MESH, "Action Frame is not for Mesh Management(0x%x)\n",actionheader->category);
		ic->ic_stats.is_rx_mgtdiscard++;
		return;
	}

	/*
	 * action frame format for Mesh Mgmt
	 *	[1] category
	 *	[1] action
	 *	[tlv] route request/reply/error/reply_ack
	 */
	route = frm+sizeof(struct ieee80211_action_header);

#if 0
	IEEE80211_VERIFY_ELEMENT(route, IEEE80211_RATE_MAXSIZE);
	IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
	IEEE80211_VERIFY_SSID(ic->ic_bss, ssid);

	if (ni == ic->ic_bss)
	{
		if (ic->ic_opmode == IEEE80211_M_IBSS)
		{
			/*
			 * XXX Cannot tell if the sender is operating
			 * in ibss mode.  But we need a new node to
			 * send the response so blindly add them to the
			 * neighbor table.
			 */
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT,  "[%s] creating adhoc node from probe request\n",
			    ether_sprintf(wh->i_addr2));
			ni = ieee80211_fakeup_adhoc_node(&ic->ic_sta,
				wh->i_addr2);
		}
		else
			ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);

		if (ni == NULL)
			return;
		allocbs = 1;
	}
	else
		allocbs = 0;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_MESH,  "[%s] recv MESH Mgmt Frame\n", ether_sprintf(wh->i_addr2));
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	
	rate = ieee80211_setup_rates(ic, ni, rates, xrates,
		  IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE
		| IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (rate & IEEE80211_RATE_BASIC) {
		IEEE80211_DISCARD(ic, IEEE80211_MSG_XRATE,
		    wh, ieee80211_mgt_subtype_name[subtype >>
			IEEE80211_FC0_SUBTYPE_SHIFT],
		    "%s", "recv'd rate set invalid");
	}
	else
	{
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_PROBE_RESP, 0);
	}
	
	if (allocbs && ic->ic_opmode != IEEE80211_M_IBSS)
	{
		/* reclaim immediately */
		ieee80211_free_node(ni);
	}
#endif

	routeDev = route_dev_lookup( netdev );
	if(!routeDev)
	{/* AODV packet must for a route device */
		printk("Bug, No Route Device for Net Device(int) %s\n",netdev->name);
		return ;
	}

	result = routeDev->ops->input_packet(routeDev, skb, (void *)ni);
#if 0	
	switch(result)
	{
		case ROUTE_OP_FORWARD:
			return NF_ACCEPT;
		case ROUTE_OP_QUEUE:
			return NF_QUEUE;
		case ROUTE_OP_DENY:
		default:
			return NF_DROP;
	}
#endif

	return;
}


int ieee80211_mesh_send_mgmt(struct ieee80211com *ic, unsigned char action_type, int length, void *route_pdu_data, int isbroadcast)
{
#define	senderr(_x, _v)	do { ic->ic_stats._v++; ret = _x; goto bad; } while (0)
	struct sk_buff *skb;
	u_int8_t *frm;
	
	int ret = 0, timer;
	struct net_device *dev = ic->ic_dev;
	struct ieee80211_frame *wh;
	struct ieee80211_cb *cb ;//= (struct ieee80211_cb *)skb->cb;
	struct ieee80211_node *ni;

	/*
	 * Hold a reference on the node so it doesn't go away until after
	 * the xmit is complete all the way in the driver.  On error we
	 * will remove our reference.
	 */
#if 0	 
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		"ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n",
		__func__, __LINE__, ni, ether_sprintf(ni->ni_macaddr),
		ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);
#endif

	timer = 0;
	
	skb = ieee80211_getmgtframe(&frm, sizeof(struct ieee80211_action_header ) +length);
	if (skb == NULL)
		senderr(ENOMEM, is_tx_nobuf);
	cb = (struct ieee80211_cb *)skb->cb;
	
	*frm++ = IEEE80211_ACTION_CATEGORY_MESH_MGMT;
	*frm++ = action_type;
	memcpy(frm, route_pdu_data, length);
	
#if 0
			frm = ieee80211_add_ssid(frm, ic->ic_des_essid, ic->ic_des_esslen);
			mode = ieee80211_chan2mode(ic, ni->ni_chan);
			frm = ieee80211_add_rates(frm, &ic->ic_sup_rates[mode]);
			frm = ieee80211_add_xrates(frm, &ic->ic_sup_rates[mode]);
			if (ic->ic_opt_ie != NULL) {
				memcpy(frm, ic->ic_opt_ie, ic->ic_opt_ie_len);
				frm += ic->ic_opt_ie_len;
			}


	IEEE80211_NODE_STAT(ni, tx_probereq);
	if (ic->ic_opmode == IEEE80211_M_STA)
		timer = IEEE80211_TRANS_WAIT;
#endif			
	skb_trim(skb, frm - skb->data);


	/*
	 * We want to pass the node down to the
	 * driver's start routine.  If we don't do so then the start
	 * routine must immediately look it up again and that can
	 * cause a lock order reversal if, for example, this frame
	 * is being sent because the station is being timedout and
	 * the frame being sent is a DEAUTH message. We stuff it in 
	 * the cb structure.
	 */

	wh = (struct ieee80211_frame *)skb_push(skb, sizeof(struct ieee80211_frame));
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_ACTION;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)wh->i_dur = 0;

	if(isbroadcast)
		ni = ieee80211_find_txnode(ic, wh->i_addr1);
	else
		ni = ieee80211_find_txnode(ic, wh->i_addr1);
		
	if (ni == NULL)
	{
		IEEE80211_DPRINTF(ic,IEEE80211_MSG_MESH,"No node for dest MAC %s\n", ether_sprintf(wh->i_addr1));
		goto bad;
	}
	if (isbroadcast )
	{
		IEEE80211_ADDR_COPY(wh->i_addr1, dev->broadcast);
		IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
		IEEE80211_ADDR_COPY(wh->i_addr3, dev->broadcast);
	}
	else
	{
		IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
		IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
		IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
	}
	
	
	cb->ni = ni;
	*(u_int16_t *)wh->i_seq = htole16(ni->ni_txseqs[0] << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseqs[0]++;


#ifdef IEEE80211_DEBUG
	if ( ieee80211_msg_mesh(ic) )
	{
		printf("[%s] send %s mgmt frame\n", ether_sprintf(wh->i_addr1),  ieee80211_mgt_subtype_name[(IEEE80211_FC0_SUBTYPE_ACTION & IEEE80211_FC0_SUBTYPE_MASK) >>IEEE80211_FC0_SUBTYPE_SHIFT]);
	}
#endif
	IEEE80211_NODE_STAT(ni, tx_mgmt);
	IF_ENQUEUE(&ic->ic_mgtq, skb);

	(*dev->hard_start_xmit)(NULL, dev);
	if (timer)
		ic->ic_mgt_timer = timer;

bad:
	ieee80211_free_node(ni);

	return ret;
#undef senderr
}

