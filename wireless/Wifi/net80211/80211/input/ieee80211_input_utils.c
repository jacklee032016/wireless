/*
* $Id: ieee80211_input_utils.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
*/

#include "ieee80211_input.h"
/*
 * Install received rate set information in the node's state block.
 */
int ieee80211_setup_rates(struct ieee80211com *ic, struct ieee80211_node *ni,
	u_int8_t *rates, u_int8_t *xrates, int flags)
{
	struct ieee80211_rateset *rs = &ni->ni_rates;

	memset(rs, 0, sizeof(*rs));
	rs->rs_nrates = rates[1];
	memcpy(rs->rs_rates, rates + 2, rs->rs_nrates);
	if (xrates != NULL) {
		u_int8_t nxrates;
		/*
		 * Tack on 11g extended supported rate element.
		 */
		nxrates = xrates[1];
		if (rs->rs_nrates + nxrates > IEEE80211_RATE_MAXSIZE) {
			nxrates = IEEE80211_RATE_MAXSIZE - rs->rs_nrates;
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_XRATE,
			     "[%s] extended rate set too large;"
			     " only using %u of %u rates\n",
			     ether_sprintf(ni->ni_macaddr), nxrates, xrates[1]);
			ic->ic_stats.is_rx_rstoobig++;
		}
		memcpy(rs->rs_rates + rs->rs_nrates, xrates+2, nxrates);
		rs->rs_nrates += nxrates;
	}
	return ieee80211_fix_rate(ic, ni, flags);
}

#ifdef IEEE80211_DEBUG
void dump_probe_beacon(u_int8_t subtype, int isnew,
	const u_int8_t mac[IEEE80211_ADDR_LEN],
	u_int8_t chan, u_int8_t bchan, u_int16_t capinfo, u_int16_t bintval,
	u_int8_t erp, u_int8_t *ssid, u_int8_t *country)
{
	printf("[%s] %s%s on chan %u (bss chan %u) ",
	    ether_sprintf(mac), isnew ? "new " : "",
	    ieee80211_mgt_subtype_name[subtype >> IEEE80211_FC0_SUBTYPE_SHIFT],
	    chan, bchan);
	ieee80211_print_essid(ssid + 2, ssid[1]);
	printf("\n");

	if (isnew) {
		printf("[%s] caps 0x%x bintval %u erp 0x%x", 
			ether_sprintf(mac), capinfo, bintval, erp);
		if (country) {
#ifdef __FreeBSD__
			printf(" country info %*D", country[1], country+2, " ");
#else
			int i;
			printf(" country info");
			for (i = 0; i < country[1]; i++)
				printf(" %02x", country[i+2]);
#endif
		}
		printf("\n");
	}
}
#endif /* IEEE80211_DEBUG */

/*
 * Handle station power-save state change.
 */
void ieee80211_node_pwrsave(struct ieee80211_node *ni, int enable)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct sk_buff *skb;

	if (enable) {
		if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) == 0)
			ic->ic_ps_sta++;
		ni->ni_flags |= IEEE80211_NODE_PWR_MGT;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] power save mode on, %u sta's in ps mode\n",
		    ether_sprintf(ni->ni_macaddr), ic->ic_ps_sta);
		return;
	}

	if (ni->ni_flags & IEEE80211_NODE_PWR_MGT)
		ic->ic_ps_sta--;
	ni->ni_flags &= ~IEEE80211_NODE_PWR_MGT;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
	    "[%s] power save mode off, %u sta's in ps mode\n",
	    ether_sprintf(ni->ni_macaddr), ic->ic_ps_sta);
	/* XXX if no stations in ps mode, flush mc frames */

	/*
	 * Flush queued unicast frames.
	 */
	if (IEEE80211_NODE_SAVEQ_QLEN(ni) == 0) {
		ic->ic_set_tim(ic, ni, 0);	/* just in case */
		return;
	}
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
	    "[%s] flush ps queue, %u packets queued\n",
	    ether_sprintf(ni->ni_macaddr), IEEE80211_NODE_SAVEQ_QLEN(ni));
	for (;;) {
		int qlen;

		IEEE80211_NODE_SAVEQ_DEQUEUE(ni, skb, qlen);
		if (skb == NULL)
			break;
		/* 
		 * If this is the last packet, turn off the TIM bit.
		 * If there are more packets, set the more packets bit
		 * in the packet dispatched to the station.
		 */
		if (qlen != 0) {
			struct ieee80211_frame_min *wh =
				(struct ieee80211_frame_min *)skb->data;
			wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
		}
		/* XXX need different driver interface */
		/* XXX bypasses q max */
		(*(ic->ic_dev)->hard_start_xmit)(skb, ic->ic_dev);
		//IF_ENQUEUE(&ic->ic_dev->if_snd, skb);
	}
}

/*
 * Process a received ps-poll frame.
 */
void ieee80211_recv_pspoll(struct ieee80211com *ic,
	struct ieee80211_node *ni, struct sk_buff *skb0)
{
	struct ieee80211_frame_min *wh;
	struct ieee80211_cb *cb = (struct ieee80211_cb*)skb0->cb;
	struct net_device *dev = ic->ic_dev;
	struct sk_buff *skb;
	u_int16_t aid;
	int qlen;

	wh = (struct ieee80211_frame_min *)skb0->data;
	if (ni->ni_associd == 0) {
		IEEE80211_DISCARD(ic, IEEE80211_MSG_POWER | IEEE80211_MSG_DEBUG,
		    (struct ieee80211_frame *) wh, "ps-poll",
		    "%s", "unassociated station");
		ic->ic_stats.is_ps_unassoc++;
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
			IEEE80211_REASON_NOT_ASSOCED);
		return;
	}

	aid = le16toh(*(u_int16_t *)wh->i_dur);
	if (aid != ni->ni_associd) {
		IEEE80211_DISCARD(ic, IEEE80211_MSG_POWER | IEEE80211_MSG_DEBUG,
		    (struct ieee80211_frame *) wh, "ps-poll",
		    "aid mismatch: sta aid 0x%x poll aid 0x%x",
		    ni->ni_associd, aid);
		ic->ic_stats.is_ps_badaid++;
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
			IEEE80211_REASON_NOT_ASSOCED);
		return;
	}

	/* Okay, take the first queued packet and put it out... */
	IEEE80211_NODE_SAVEQ_DEQUEUE(ni, skb, qlen);
	if (skb == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] recv ps-poll, but queue empty\n",
		    ether_sprintf(wh->i_addr2));
		ieee80211_send_nulldata(ic, ni);
		ic->ic_stats.is_ps_qempty++;	/* XXX node stat */
		ic->ic_set_tim(ic, ni, 0);	/* just in case */
		return;
	}
	/* 
	 * If there are more packets, set the more packets bit
	 * in the packet dispatched to the station; otherwise
	 * turn off the TIM bit.
	 */
	if (qlen != 0) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] recv ps-poll, send packet, %u still queued\n",
		    ether_sprintf(ni->ni_macaddr), qlen);
		wh = (struct ieee80211_frame_min *) skb->data;
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
	} else {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] recv ps-poll, send packet, queue empty\n",
		    ether_sprintf(ni->ni_macaddr));
		ic->ic_set_tim(ic, ni, 0);
	}
	cb->flags |= M_PWR_SAV;			/* bypass PS handling */
	(*dev->hard_start_xmit)(skb0, dev);
	//IF_ENQUEUE(&ic->ic_dev->if_snd, skb0);	
}

#ifdef IEEE80211_DEBUG
/* Return the bssid of a frame */
const u_int8_t * ieee80211_getbssid(struct ieee80211com *ic, const struct ieee80211_frame *wh)
{
	if (ic->ic_opmode == IEEE80211_M_STA)
		return wh->i_addr2;
	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) != IEEE80211_FC1_DIR_NODS)
		return wh->i_addr1;
	if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PS_POLL)
		return wh->i_addr1;
	return wh->i_addr3;
}

void ieee80211_discard_frame(struct ieee80211com *ic,	const struct ieee80211_frame *wh,
	const char *type, const char *fmt, ...)
{
	va_list ap;
	char buf[512];		// XXX

	/*
	 * first print variable arguments into buffer, 
	 * because ether_sprintf may be used there as well
	 * and we would overwrite its static buffer
	 */
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	printf("[%s] discard ", ether_sprintf(ieee80211_getbssid(ic, wh)));
	if (type != NULL)
		printf("%s frame, ", type);
	else
		printf("frame, ");
	
	printf("%s\n", buf);
}

void ieee80211_discard_ie(struct ieee80211com *ic, const struct ieee80211_frame *wh,
	const char *type, const char *fmt, ...)
{
	va_list ap;
	char buf[512];		// XXX

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	printf("[%s] discard ", ether_sprintf(ieee80211_getbssid(ic, wh)));
	if (type != NULL)
		printf("%s information element, ", type);
	else
		printf("information element, ");

	printf("%s\n", buf);
}

void ieee80211_discard_mac(struct ieee80211com *ic, const u_int8_t mac[IEEE80211_ADDR_LEN],
	const char *type, const char *fmt, ...)
{
	va_list ap;
	char buf[512];
	
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	printf("[%s] discard ", ether_sprintf(mac));
	if (type != NULL)
		printf("%s frame, ", type);
	else
		printf("frame, ");

	printf("%s\n", buf);
}
#endif /* IEEE80211_DEBUG */

