/*
* $Id: wlan_node.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
/*
 * IEEE 802.11 node handling support.
 */

#include "_node.h"

static inline void copy_bss(struct ieee80211_node *nbss, const struct ieee80211_node *obss)
{
	/* propagate useful state */
	nbss->ni_authmode = obss->ni_authmode;
	nbss->ni_txpower = obss->ni_txpower;
	nbss->ni_vlan = obss->ni_vlan;
	nbss->ni_rsn = obss->ni_rsn;
	nbss->ni_chan = obss->ni_chan;
	/* XXX statistics? */
}

void ieee80211_create_ibss(struct ieee80211com* ic, struct ieee80211_channel *chan)
{
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "%s: creating ibss\n", __func__);

	/*
	 * Create the station/neighbor table.  Note that for adhoc
	 * mode we make the initial inactivity timer longer since
	 * we create nodes only through discovery and they typically
	 * are long-lived associations.
	 */
	nt = &ic->ic_sta;
	IEEE80211_NODE_LOCK(nt);
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
	{
		nt->nt_name = "station";
		nt->nt_inact_init = ic->ic_inact_init;
	}
	else
	{/* for adhoc and sta, it is 'neighbor' table */
		nt->nt_name = "neighbor";
		nt->nt_inact_init = ic->ic_inact_run;
	}
	IEEE80211_NODE_UNLOCK(nt);

	ni = ieee80211_alloc_node(nt, ic->ic_myaddr);
	if (ni == NULL)
	{
		/* XXX recovery? */
		return;
	}
	IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_myaddr);
	ni->ni_esslen = ic->ic_des_esslen;
	memcpy(ni->ni_essid, ic->ic_des_essid, ni->ni_esslen);
	copy_bss(ni, ic->ic_bss);
	ni->ni_intval = ic->ic_lintval;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if (ic->ic_phytype == IEEE80211_T_FH)
	{
		ni->ni_fhdwell = 200;	/* XXX */
		ni->ni_fhindex = 1;
	}
	if (ic->ic_opmode == IEEE80211_M_IBSS)
	{
		ic->ic_flags |= IEEE80211_F_SIBSS;
		ni->ni_capinfo |= IEEE80211_CAPINFO_IBSS;	/* XXX */
		if (ic->ic_flags & IEEE80211_F_DESBSSID)
			IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_des_bssid);
		else
			ni->ni_bssid[0] |= 0x02;        /* local bit for IBSS */
	}
	/* 
	 * Fix the channel and related attributes.
	 */
	ieee80211_set_chan(ic, ni, chan);
	ic->ic_curmode = ieee80211_chan2mode(ic, chan);
	/*
	 * Do mode-specific rate setup.
	 */
	if (ic->ic_curmode == IEEE80211_MODE_11G ||ic->ic_curmode == IEEE80211_MODE_TURBO_G)
	{/* Use a mixed 11b/11g rate set. */
		ieee80211_set11gbasicrates(&ni->ni_rates, IEEE80211_MODE_11G);
	}
	else if (ic->ic_curmode == IEEE80211_MODE_11B)
	{/* Force pure 11b rate set. */
		ieee80211_set11gbasicrates(&ni->ni_rates, IEEE80211_MODE_11B);
	}
	
	printk("%s: creating bss %s\n", ic->ic_dev->name, swjtu_mesh_mac_sprintf(ni->ni_bssid));
	
	(void) ieee80211_sta_join(ic, ieee80211_ref_node(ni));
}

void ieee80211_reset_bss(struct ieee80211com *ic)
{
	struct ieee80211_node *ni, *obss;

	ieee80211_node_table_reset(&ic->ic_scan);
	ieee80211_node_table_reset(&ic->ic_sta);

	ni = ieee80211_alloc_node(&ic->ic_scan, ic->ic_myaddr);
	KASSERT(ni != NULL, ("unable to setup inital BSS node"));
	obss = ic->ic_bss;
	ic->ic_bss = ieee80211_ref_node(ni);
	if (obss != NULL)
	{
		copy_bss(ni, obss);
		ni->ni_intval = ic->ic_lintval;
		ieee80211_free_node(obss);
	}
}

 
/*
 * Handle 802.11 ad hoc network merge.  The
 * convention, set by the Wireless Ethernet Compatibility Alliance
 * (WECA), is that an 802.11 station will change its BSSID to match
 * the "oldest" 802.11 ad hoc network, on the same channel, that
 * has the station's desired SSID.  The "oldest" 802.11 network
 * sends beacons with the greatest TSF timestamp.
 *
 * The caller is assumed to validate TSF's before attempting a merge.
 *
 * Return !0 if the BSSID changed, 0 otherwise.
 */
int ieee80211_ibss_merge(struct ieee80211com *ic, struct ieee80211_node *ni)
{

	if (ni == ic->ic_bss ||IEEE80211_ADDR_EQ(ni->ni_bssid, ic->ic_bss->ni_bssid))
	{/* ni is myself, so no merge needed */
		/* unchanged, nothing to do */
		return 0;
	}
	if (_ieee80211_match_bss(ic, ni) != 0)
	{/* capabilities mismatch */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "%s: merge failed, capabilities mismatch\n", __func__);
		ic->ic_stats.is_ibss_capmismatch++;
		return 0;
	}
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		"%s: new bssid %s: %s preamble, %s slot time%s\n", __func__,
		swjtu_mesh_mac_sprintf(ni->ni_bssid),
		ic->ic_flags&IEEE80211_F_SHPREAMBLE ? "short" : "long",
		ic->ic_flags&IEEE80211_F_SHSLOT ? "short" : "long",
		ic->ic_flags&IEEE80211_F_USEPROT ? ", protection" : ""
	);
	
	printk("%s: ibss merge: %s", ic->ic_dev->name, swjtu_mesh_mac_sprintf(ic->ic_bss->ni_bssid));
	printk(" -> %s\n", swjtu_mesh_mac_sprintf(ni->ni_bssid));

	return ieee80211_sta_join(ic, ieee80211_ref_node(ni));
}

/*
 * Join the specified IBSS/BSS network.  The node is assumed to be passed in with a held reference.
 * when play as station and adhoc node(peer as the master ) ?? 
 */
int ieee80211_sta_join(struct ieee80211com *ic, struct ieee80211_node *selbs)
{
	struct ieee80211_node *obss;

	if (ic->ic_opmode == IEEE80211_M_IBSS)
	{
		struct ieee80211_node_table *nt;
		/*
		 * Delete unusable rates; we've already checked
		 * that the negotiated rate set is acceptable.
		 */
		ieee80211_fix_rate(ic, selbs, IEEE80211_F_DODEL);
		/*
		 * Fillin the neighbor table; it will already
		 * exist if we are simply switching mastership.
		 * XXX ic_sta always setup so this is unnecessary?
		 */
		nt = &ic->ic_sta;
		IEEE80211_NODE_LOCK(nt);
		nt->nt_name = "neighbor";
		nt->nt_inact_init = ic->ic_inact_run;
		IEEE80211_NODE_UNLOCK(nt);
	}

	/*
	 * Committed to selbs, setup state.
	 */
	obss = ic->ic_bss;
	ic->ic_bss = selbs;		/* NB: caller assumed to bump refcnt */
	if (obss != NULL)
		ieee80211_free_node(obss);
	/*
	 * Set the erp state (mostly the slot time) to deal with
	 * the auto-select case; this should be redundant if the
	 * mode is locked.
	 */ 
	ic->ic_curmode = ieee80211_chan2mode(ic, selbs->ni_chan);
	ieee80211_reset_erp(ic);
	ieee80211_wme_initparams(ic);

#if WITH_SUPPORT_STA
	if (ic->ic_opmode == IEEE80211_M_STA)
		ieee80211_new_state(ic, IEEE80211_S_AUTH, -1);
	else
#endif		
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	swjtu_mesh_fwd_db_insert(ic->ic_dev->fwdTable, ic->ic_dev, selbs->ni_macaddr, 0, 0);
	return 1;
}

/*
 * Leave the specified IBSS/BSS network.  The node is assumed to
 * be passed in with a held reference.
 */
void ieee80211_sta_leave(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	ic->ic_node_cleanup(ni);
	
	ieee80211_notify_node_leave(ic, ni);
}


struct ieee80211_node *ieee80211_dup_bss(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;

	ni = ic->ic_node_alloc(nt);
	if (ni != NULL)
	{
		_ieee80211_setup_node(nt, ni, macaddr);
		/*
		 * Inherit from ic_bss.
		 */
		ni->ni_authmode = ic->ic_bss->ni_authmode;
		ni->ni_txpower = ic->ic_bss->ni_txpower;
		ni->ni_vlan = ic->ic_bss->ni_vlan;	/* XXX?? */
		IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss->ni_bssid);
		ieee80211_set_chan(ic, ni, ic->ic_bss->ni_chan);
		ni->ni_rsn = ic->ic_bss->ni_rsn;
	}
	else
		ic->ic_stats.is_rx_nodealloc++;

	return ni;
}


/*
 * Fake up a node; this handles node discovery in adhoc mode.
 * Note that for the driver's benefit we we treat this like
 * an association so the driver has an opportunity to setup
 * it's private state.
 */
struct ieee80211_node *ieee80211_fakeup_adhoc_node(struct ieee80211_node_table *nt, const u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;

	ni = ieee80211_dup_bss(nt, macaddr);
	if (ni != NULL) {
		/* XXX no rate negotiation; just dup */
		ni->ni_rates = ic->ic_bss->ni_rates;
		if (ic->ic_newassoc != NULL)
			ic->ic_newassoc(ic, ni, 1);
		/* XXX not right for 802.1x/WPA */
		ieee80211_node_authorize(ic, ni);
	}
	return ni;
}

/*
 * Locate the node for sender, track state, and then pass the
 * (referenced) node up to the 802.11 layer for its use.  We
 * are required to pass some node so we fall back to ic_bss
 * when this frame is from an unknown sender.  The 802.11 layer
 * knows this means the sender wasn't in the node table and
 * acts accordingly. 
 */
struct ieee80211_node *ieee80211_find_rxnode(struct ieee80211com *ic,	const struct ieee80211_frame_min *wh)
{
#define	IS_CTL(wh) \
	((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
#define	IS_PSPOLL(wh) \
	((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PS_POLL)
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;

	/* XXX may want scanned nodes in the neighbor table for adhoc */
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_MONITOR ||
	    (ic->ic_flags & IEEE80211_F_SCAN))
		nt = &ic->ic_scan;
	else
		nt = &ic->ic_sta;
	/* XXX check ic_bss first in station mode */
	/* XXX 4-address frames? */
	IEEE80211_NODE_LOCK(nt);
	if (IS_CTL(wh) && !IS_PSPOLL(wh) /*&& !IS_RTS(ah)*/)
		ni = _ieee80211_find_node(nt, wh->i_addr1);
	else
		ni = _ieee80211_find_node(nt, wh->i_addr2);
	IEEE80211_NODE_UNLOCK(nt);

	return (ni != NULL ? ni : ieee80211_ref_node(ic->ic_bss));
#undef IS_PSPOLL
#undef IS_CTL
}

/*
 * Return a reference to the appropriate node for sending
 * a data frame.  This handles node discovery in adhoc networks.
 */
struct ieee80211_node *ieee80211_find_txnode(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;

	/*
	 * The destination address should be in the node table
	 * unless we are operating in station mode or this is a
	 * multicast/broadcast frame.
	 */
#if WITH_SUPPORT_STA
	if (ic->ic_opmode == IEEE80211_M_STA || IEEE80211_IS_MULTICAST(macaddr))
#else
	if ( IEEE80211_IS_MULTICAST(macaddr) )
#endif
		return ieee80211_ref_node(ic->ic_bss);

	/* XXX can't hold lock across dup_bss 'cuz of recursive locking */
	IEEE80211_NODE_LOCK(nt);
	ni = _ieee80211_find_node(nt, macaddr);
	IEEE80211_NODE_UNLOCK(nt);

	if (ni == NULL) {
		if (ic->ic_opmode == IEEE80211_M_IBSS ||
		    ic->ic_opmode == IEEE80211_M_AHDEMO) {
			/*
			 * In adhoc mode cons up a node for the destination.
			 * Note that we need an additional reference for the
			 * caller to be consistent with _ieee80211_find_node.
			 */
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
				"%s faking txnode: %s\n", __func__,
				swjtu_mesh_mac_sprintf(macaddr));
			ni = ieee80211_fakeup_adhoc_node(nt, macaddr);
			if (ni != NULL)
				(void) ieee80211_ref_node(ni);
		} else {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT,
				"[%s] no node, discard frame (%s)\n",
				swjtu_mesh_mac_sprintf(macaddr), __func__);
			ic->ic_stats.is_tx_nonode++;
		}
	}
	return ni;
}

/* Handle a station joining an 11g network. */
static void ieee80211_node_join_11g(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	/*
	 * Station isn't capable of short slot time.  Bump
	 * the count of long slot time stations and disable
	 * use of short slot time.  Note that the actual switch
	 * over to long slot time use may not occur until the
	 * next beacon transmission (per sec. 7.3.1.4 of 11g).
	 */
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) == 0)
	{
		ic->ic_longslotsta++;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] station needs long slot time, count %d\n",
		    swjtu_mesh_mac_sprintf(ni->ni_macaddr), ic->ic_longslotsta);
		/* XXX vap's w/ conflicting needs won't work */
		ieee80211_set_shortslottime(ic, 0);
	}
	/*
	 * If the new station is not an ERP station
	 * then bump the counter and enable protection
	 * if configured.
	 * TODO: adopt for turbo (11g) mode, ieee80211_iserp_rateset
	 * is always false for turbo stations
	 */
	if (!ieee80211_iserp_rateset(ic, &ni->ni_rates)) {
		ic->ic_nonerpsta++;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] station is !ERP, %d non-ERP stations associated\n",
		    swjtu_mesh_mac_sprintf(ni->ni_macaddr), ic->ic_nonerpsta);
		/*
		 * If protection is configured, enable it.
		 */
		if (ic->ic_protmode != IEEE80211_PROT_NONE) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
			    "%s: enable use of protection\n", __func__);
			ic->ic_flags |= IEEE80211_F_USEPROT;
		}
		/*
		 * If station does not support short preamble
		 * then we must enable use of Barker preamble.
		 */
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE) == 0) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
			    "[%s] station needs long preamble\n",
			    swjtu_mesh_mac_sprintf(ni->ni_macaddr));
			ic->ic_flags |= IEEE80211_F_USEBARKER;
			ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
		}
	} else
		ni->ni_flags |= IEEE80211_NODE_ERP;
}

/* a node join the BSSID or ESS when we play as AP or Ad Hoc Master 
* After A Associated Request has been rx, but this node not enter into neighbor table (has been scanned )
*/
void ieee80211_node_join(struct ieee80211com *ic, struct ieee80211_node *ni, int resp)
{
	int newassoc;

	if (ni->ni_associd == 0)
	{
		u_int16_t aid;

		/*
		 * It would be good to search the bitmap
		 * more efficiently, but this will do for now.
		 */
		for (aid = 1; aid < ic->ic_max_aid; aid++)
		{
			if (!IEEE80211_AID_ISSET(aid, ic->ic_aid_bitmap))
				break;
		}
		if (aid >= ic->ic_max_aid)
		{
			IEEE80211_SEND_MGMT(ic, ni, resp,IEEE80211_REASON_ASSOC_TOOMANY);
			ieee80211_node_leave(ic, ni);
			return;
		}
		ni->ni_associd = aid | 0xc000;
		IEEE80211_AID_SET(ni->ni_associd, ic->ic_aid_bitmap);
		ic->ic_sta_assoc++;
		newassoc = 1;
		if (ic->ic_curmode == IEEE80211_MODE_11G ||ic->ic_curmode == IEEE80211_MODE_TURBO_G)
			ieee80211_node_join_11g(ic, ni);
	} 
	else
		newassoc = 0;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC | IEEE80211_MSG_DEBUG,
	    "[%s] station %sassociated at aid %d: %s preamble, %s slot time%s%s\n",
	    swjtu_mesh_mac_sprintf(ni->ni_macaddr), newassoc ? "" : "re",
	    IEEE80211_NODE_AID(ni),
	    ic->ic_flags & IEEE80211_F_SHPREAMBLE ? "short" : "long",
	    ic->ic_flags & IEEE80211_F_SHSLOT ? "short" : "long",
	    ic->ic_flags & IEEE80211_F_USEPROT ? ", protection" : "",
	    ni->ni_flags & IEEE80211_NODE_QOS ? ", QoS" : ""
	);

	/* give driver a chance to setup state like ni_txrate */
	if (ic->ic_newassoc != NULL)
		ic->ic_newassoc(ic, ni, newassoc);

	ni->ni_inact_reload = ic->ic_inact_auth;
	ni->ni_inact = ni->ni_inact_reload;

	IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_SUCCESS);
	
	/* tell the authenticator about new station */
	if (ic->ic_auth->ia_node_join != NULL)
		ic->ic_auth->ia_node_join(ic, ni);

	ieee80211_notify_node_join(ic, ni, newassoc);

}

/* Handle a station leaving an 11g network. */
static void ieee80211_node_leave_11g(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	KASSERT(ic->ic_curmode == IEEE80211_MODE_11G ||	ic->ic_curmode == IEEE80211_MODE_TURBO_G, ("not in 11g, curmode %x", ic->ic_curmode));

	/* If a long slot station do the slot time bookkeeping.	 */
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) == 0)
	{
		KASSERT(ic->ic_longslotsta > 0,  ("bogus long slot station count %d", ic->ic_longslotsta));
		ic->ic_longslotsta--;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] long slot time station leaves, count now %d\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr), ic->ic_longslotsta);
		if (ic->ic_longslotsta == 0)
		{
			/*
			 * Re-enable use of short slot time if supported
			 * and not operating in IBSS mode (per spec).
			 */
			if ((ic->ic_caps & IEEE80211_C_SHSLOT) && ic->ic_opmode != IEEE80211_M_IBSS)
			{
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "%s: re-enable use of short slot time\n",   __func__);
				ieee80211_set_shortslottime(ic, 1);
			}
		}
	}
	/*
	 * If a non-ERP station do the protection-related bookkeeping.
	 */
	if ((ni->ni_flags & IEEE80211_NODE_ERP) == 0)
	{
		KASSERT(ic->ic_nonerpsta > 0,   ("bogus non-ERP station count %d", ic->ic_nonerpsta));
		ic->ic_nonerpsta--;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "[%s] non-ERP station leaves, count now %d\n",  swjtu_mesh_mac_sprintf(ni->ni_macaddr), ic->ic_nonerpsta);
		if (ic->ic_nonerpsta == 0)
		{
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,	"%s: disable use of protection\n", __func__);
			ic->ic_flags &= ~IEEE80211_F_USEPROT;
			/* XXX verify mode? */
			if (ic->ic_caps & IEEE80211_C_SHPREAMBLE)
			{
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC, "%s: re-enable use of short preamble\n",  __func__);
				ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
				ic->ic_flags &= ~IEEE80211_F_USEBARKER;
			}
		}
	}
}

/* Handle bookkeeping for station deauthentication/disassociation when operating as an ap. */
void ieee80211_node_leave(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_node_table *nt = ni->ni_table;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC | IEEE80211_MSG_DEBUG,   "[%s] station with aid %d leaves\n",   swjtu_mesh_mac_sprintf(ni->ni_macaddr), IEEE80211_NODE_AID(ni));

	KASSERT(ic->ic_opmode == IEEE80211_M_HOSTAP ||ic->ic_opmode == IEEE80211_M_IBSS ||	ic->ic_opmode == IEEE80211_M_AHDEMO, ("unexpected operating mode %u", ic->ic_opmode));
	/*
	 * If node wasn't previously associated all
	 * we need to do is reclaim the reference.
	 */
	/* XXX ibss mode bypasses 11g and notification */
	if (ni->ni_associd == 0)
		goto done;
	/*
	 * Tell the authenticator the station is leaving.
	 * Note that we must do this before yanking the
	 * association id as the authenticator uses the
	 * associd to locate it's state block.
	 */
	if (ic->ic_auth->ia_node_leave != NULL)
		ic->ic_auth->ia_node_leave(ic, ni);
	IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
	ni->ni_associd = 0;
	ic->ic_sta_assoc--;

	if (ic->ic_curmode == IEEE80211_MODE_11G || ic->ic_curmode == IEEE80211_MODE_TURBO_G)
		ieee80211_node_leave_11g(ic, ni);
	/*
	 * Cleanup station state.  In particular clear various
	 * state that might otherwise be reused if the node
	 * is reused before the reference count goes to zero
	 * (and memory is reclaimed).
	 */
	ieee80211_sta_leave(ic, ni);
done:
	/*
	 * Remove the node from any table it's recorded in and
	 * drop the caller's reference.  Removal from the table
	 * is important to insure the node is not reprocessed
	 * for inactivity.
	 */
	if (nt != NULL)
	{
		IEEE80211_NODE_LOCK(nt);
		_node_reclaim(nt, ni);
		IEEE80211_NODE_UNLOCK(nt);
	}
	else
		ieee80211_free_node(ni);
}

u_int8_t ieee80211_getrssi(struct ieee80211com *ic)
{
#define	NZ(x)	((x) == 0 ? 1 : (x))
	struct ieee80211_node_table *nt = &ic->ic_sta;
	u_int32_t rssi_samples, rssi_total;
	struct ieee80211_node *ni;

	rssi_total = 0;
	rssi_samples = 0;
	
	switch (ic->ic_opmode)
	{
		case IEEE80211_M_IBSS:		/* average of all ibss neighbors */
			/* XXX locking */
			TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			{
				if (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS)
				{
					rssi_samples++;
					rssi_total += ic->ic_node_getrssi(ni);
				}
			}	
			break;
#if WITH_MISC_MODE			
		case IEEE80211_M_AHDEMO:	/* average of all neighbors */
			/* XXX locking */
			TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			{
				rssi_samples++;
				rssi_total += ic->ic_node_getrssi(ni);
			}
			break;
#endif			
		case IEEE80211_M_HOSTAP:	/* average of all associated stations */
			/* XXX locking */
			TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			{
				if (IEEE80211_AID(ni->ni_associd) != 0)
				{
					rssi_samples++;
					rssi_total += ic->ic_node_getrssi(ni);
				}
			}	
			break;
#if WITH_MISC_MODE			
		case IEEE80211_M_MONITOR:	/* XXX */
		case IEEE80211_M_STA:		/* use stats from associated ap */
#endif
		default:
			if (ic->ic_bss != NULL)
				rssi_total = ic->ic_node_getrssi(ic->ic_bss);
			rssi_samples = 1;
			break;
	}
	
	return rssi_total / NZ(rssi_samples);
#undef NZ
}


EXPORT_SYMBOL(ieee80211_getrssi);
EXPORT_SYMBOL(ieee80211_node_leave);
EXPORT_SYMBOL(ieee80211_find_txnode);
EXPORT_SYMBOL(ieee80211_find_rxnode);
EXPORT_SYMBOL(ieee80211_ibss_merge);
EXPORT_SYMBOL(ieee80211_next_scan);
EXPORT_SYMBOL(ieee80211_find_node);
EXPORT_SYMBOL(ieee80211_iterate_nodes);
EXPORT_SYMBOL(ieee80211_node_unauthorize);
EXPORT_SYMBOL(ieee80211_node_authorize);
EXPORT_SYMBOL(ieee80211_free_node);


