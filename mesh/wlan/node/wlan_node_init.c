/*
*
*/
#include "_node.h"

static void __node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	ic->ic_node_cleanup(ni);

	if (ni->ni_wpa_ie != NULL)
		kfree(ni->ni_wpa_ie);
	if (ni->ni_wme_ie != NULL)
		kfree(ni->ni_wme_ie);
	
	IEEE80211_NODE_SAVEQ_DESTROY(ni);	// TODO: should drain?
	kfree(ni);
}

static struct ieee80211_node *__node_alloc(struct ieee80211_node_table *nt)
{
	struct ieee80211_node *ni;

	MALLOC(ni, struct ieee80211_node *, sizeof(struct ieee80211_node),	M_80211_NODE, M_NOWAIT | M_ZERO);
	return ni;
}

/*
 * Reclaim any resources in a node and reset any critical
 * state.  Typically nodes are free'd immediately after,
 * but in some cases the storage may be reused so we need
 * to insure consistent state (should probably fix that).
 */
static void __node_cleanup(struct ieee80211_node *ni)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
#define KEY_UNDEFINED(k)        ((k).wk_cipher == &ieee80211_cipher_none)

	struct ieee80211com *ic = ni->ni_ic;
	int qlen;
	u_int i;

	/* NB: preserve ni_table */
	if (ni->ni_flags & IEEE80211_NODE_PWR_MGT)
	{
		ic->ic_ps_sta--;
		ni->ni_flags &= ~IEEE80211_NODE_PWR_MGT;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,  "[%s] power save mode off, %u sta's in ps mode\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr), ic->ic_ps_sta);
	}

	/*
	 * Drain power save queue and, if needed, clear TIM.
	 */
	IEEE80211_NODE_SAVEQ_DRAIN(ni, qlen);
	if (qlen != 0 && ic->ic_set_tim != NULL)
		ic->ic_set_tim(ic, ni, 0);

	ni->ni_associd = 0;
	if (ni->ni_challenge != NULL)
	{
		kfree(ni->ni_challenge);
		ni->ni_challenge = NULL;
	}
	/*
	 * Preserve SSID, WPA, and WME ie's so the bss node is
	 * reusable during a re-auth/re-assoc state transition.
	 * If we remove these data they will not be recreated
	 * because they come from a probe-response or beacon frame
	 * which cannot be expected prior to the association-response.
	 * This should not be an issue when operating in other modes
	 * as stations leaving always go through a full state transition
	 * which will rebuild this state.
	 *
	 * XXX does this leave us open to inheriting old state?
	 */
	for (i = 0; i < N(ni->ni_rxfrag); i++)
	{
		if (ni->ni_rxfrag[i] != NULL)
		{
			kfree_skb(ni->ni_rxfrag[i]);
			ni->ni_rxfrag[i] = NULL;
		}
	}
	
	if(!KEY_UNDEFINED(ni->ni_ucastkey))
	    ieee80211_crypto_delkey(ic, &ni->ni_ucastkey);

#undef KEY_UNDEFINED
#undef N
}

static u_int8_t __node_getrssi(const struct ieee80211_node *ni)
{
	return ni->ni_rssi;
}

/*
 * Node table support.
 */
static void __ieee80211_node_table_init(struct ieee80211com *ic,
	struct ieee80211_node_table *nt, const char *name, int inact, void (*timeout)(struct ieee80211_node_table *))
{
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE, "%s %s table, inact %u\n", __func__, name, inact);

	nt->nt_ic = ic;
	/* XXX need unit */
	IEEE80211_NODE_LOCK_INIT(nt, ic->ic_ifp->if_xname);
	IEEE80211_SCAN_LOCK_INIT(nt, ic->ic_ifp->if_xname);
	TAILQ_INIT(&nt->nt_node);
	nt->nt_name = name;
	nt->nt_scangen = 1;
	nt->nt_inact_init = inact;
	nt->nt_timeout = timeout;
}

/* Timeout entries in the scan cache. */
static void __ieee80211_timeout_scan_candidates(struct ieee80211_node_table *nt)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni, *tni;

	IEEE80211_NODE_LOCK(nt);
	ni = ic->ic_bss;
	/* XXX belongs elsewhere */
	if (ni->ni_rxfrag[0] != NULL && jiffies > ni->ni_rxfragstamp + HZ)
	{
		kfree_skb(ni->ni_rxfrag[0]);
		ni->ni_rxfrag[0] = NULL;
	}

	ni = NULL;
	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, tni)
	{
		if (ni->ni_inact && --ni->ni_inact == 0)
		{
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE, "[%s] scan candidate purged from cache (refcnt %u)\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni));
			_node_reclaim(nt, ni);
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	nt->nt_inact_timer = IEEE80211_INACT_WAIT;
}

/*
 * Timeout inactive stations and do related housekeeping.
 * Note that we cannot hold the node lock while sending a
 * frame as this would lead to a LOR.  Instead we use a
 * generation number to mark nodes that we've scanned and
 * drop the lock and restart a scan if we have to time out
 * a node.  Since we are single-threaded by virtue of
 * controlling the inactivity timer we can be sure this will
 * process each node only once.
 */
static void __ieee80211_timeout_stations(struct ieee80211_node_table *nt)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;
	u_int gen;

	IEEE80211_SCAN_LOCK(nt);
	gen = nt->nt_scangen++;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE, "%s: %s scangen %u\n", __func__, nt->nt_name, gen);
restart:
	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
	{
		if (ni->ni_scangen == gen)	/* previously handled */
			continue;
		ni->ni_scangen = gen;
		/*
		 * Free fragment if not needed anymore
		 * (last fragment older than 1s).
		 * XXX doesn't belong here
		 */
		if (ni->ni_rxfrag[0] != NULL &&
		    jiffies > ni->ni_rxfragstamp + HZ)
		{
			kfree_skb(ni->ni_rxfrag[0]);
			ni->ni_rxfrag[0] = NULL;
		}
		/*
		 * Special case ourself; we may be idle for extended periods
		 * of time and regardless reclaiming our state is wrong.
		 */
		if (ni == ic->ic_bss)
			continue;
		ni->ni_inact--;
		if (ni->ni_associd != 0)
		{
			/*
			 * Age frames on the power save queue. The
			 * aging interval is 4 times the listen
			 * interval specified by the station.  This
			 * number is factored into the age calculations
			 * when the frame is placed on the queue.  We
			 * store ages as time differences we can check
			 * and/or adjust only the head of the list.
			 */
			if (IEEE80211_NODE_SAVEQ_QLEN(ni) != 0)
			{
				struct sk_buff *skb;
				int discard = 0;

				IEEE80211_NODE_SAVEQ_LOCK(ni);
				while (IF_POLL(&ni->ni_savedq, skb) != NULL &&
				     M_AGE_GET(skb) < IEEE80211_INACT_WAIT)
				{
					IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER, "[%s] discard frame, age %u\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr), M_AGE_GET(skb));/*XXX*/
					_IEEE80211_NODE_SAVEQ_DEQUEUE_HEAD(ni, skb);
					kfree_skb(skb);
					discard++;
				}

				if (skb != NULL)
					M_AGE_SUB(skb, IEEE80211_INACT_WAIT);
				IEEE80211_NODE_SAVEQ_UNLOCK(ni);

				if (discard != 0)
				{
					IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER, "[%s] discard %u frames for age\n",
					    swjtu_mesh_mac_sprintf(ni->ni_macaddr),   discard);
					
					IEEE80211_NODE_STAT_ADD(ni, ps_discard, discard);
					if (IEEE80211_NODE_SAVEQ_QLEN(ni) == 0)
						ic->ic_set_tim(ic, ni, 0);
				}
			}
			/*
			 * Probe the station before time it out.  We
			 * send a null data frame which may not be
			 * universally supported by drivers (need it
			 * for ps-poll support so it should be...).
			 */
			if (0 < ni->ni_inact &&
			    ni->ni_inact <= ic->ic_inact_probe)
			{
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE, "[%s] probe station due to inactivity\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr));
				IEEE80211_NODE_UNLOCK(nt);
				ieee80211_send_nulldata(ic, ni);
				/* XXX stat? */
				goto restart;
			}
		}

		if (ni->ni_inact <= 0)
		{
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,   "[%s] station timed out due to inactivity (refcnt %u)\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni));
			/*
			 * Send a deauthenticate frame and drop the station.
			 * This is somewhat complicated due to reference counts
			 * and locking.  At this point a station will typically
			 * have a reference count of 1.  ieee80211_node_leave
			 * will do a "free" of the node which will drop the
			 * reference count.  But in the meantime a reference
			 * wil be held by the deauth frame.  The actual reclaim
			 * of the node will happen either after the tx is
			 * completed or by ieee80211_node_leave.
			 *
			 * Separately we must drop the node lock before sending
			 * in case the driver takes a lock, as this will result
			 * in  LOR between the node lock and the driver lock.
			 */
			IEEE80211_NODE_UNLOCK(nt);
			if (ni->ni_associd != 0)
			{
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_EXPIRE);
			}

			ieee80211_node_leave(ic, ni);
			ic->ic_stats.is_node_timeout++;
			goto restart;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	IEEE80211_SCAN_UNLOCK(nt);

	nt->nt_inact_timer = IEEE80211_INACT_WAIT;
}


/* Indicate whether there are frames queued for a station in power-save mode. */
static void __ieee80211_set_tim(struct ieee80211com *ic, struct ieee80211_node *ni, int set)
{
	u_int16_t aid;

	KASSERT(ic->ic_opmode == IEEE80211_M_HOSTAP ||ic->ic_opmode == IEEE80211_M_IBSS, ("operating mode %u", ic->ic_opmode));

	aid = IEEE80211_AID(ni->ni_associd);
	KASSERT(aid < ic->ic_max_aid, ("bogus aid %u, max %u", aid, ic->ic_max_aid));

	IEEE80211_BEACON_LOCK(ic);
	if (set != (isset(ic->ic_tim_bitmap, aid) != 0))
	{
		if (set)
		{
			setbit(ic->ic_tim_bitmap, aid);
			ic->ic_ps_pending++;
		}
		else
		{
			clrbit(ic->ic_tim_bitmap, aid);
			ic->ic_ps_pending--;
		}
		ic->ic_flags |= IEEE80211_F_TIMUPDATE;
	}
	IEEE80211_BEACON_UNLOCK(ic);
}

void ieee80211_node_attach(struct ieee80211com *ic)
{

	__ieee80211_node_table_init(ic, &ic->ic_sta, "station",	IEEE80211_INACT_INIT, __ieee80211_timeout_stations);
	__ieee80211_node_table_init(ic, &ic->ic_scan, "scan",	IEEE80211_INACT_SCAN, __ieee80211_timeout_scan_candidates);

	ic->ic_node_alloc = __node_alloc;
	ic->ic_node_free = __node_free;
	ic->ic_node_cleanup = __node_cleanup;
	ic->ic_node_getrssi = __node_getrssi;

	/* default station inactivity timer setings */
	ic->ic_inact_init = IEEE80211_INACT_INIT;
	ic->ic_inact_auth = IEEE80211_INACT_AUTH;
	ic->ic_inact_run = IEEE80211_INACT_RUN;
	ic->ic_inact_probe = IEEE80211_INACT_PROBE;

	/* XXX defer */
	if (ic->ic_max_aid == 0)
		ic->ic_max_aid = IEEE80211_AID_DEF;
	else if (ic->ic_max_aid > IEEE80211_AID_MAX)
		ic->ic_max_aid = IEEE80211_AID_MAX;
	
	MALLOC(ic->ic_aid_bitmap, u_int32_t *,howmany(ic->ic_max_aid, 32) * sizeof(u_int32_t),M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ic->ic_aid_bitmap == NULL)
	{
		/* XXX no way to recover */
		printf("%s: no memory for AID bitmap!\n", __func__);
		ic->ic_max_aid = 0;
	}

	/* XXX defer until using hostap/ibss mode */
	ic->ic_tim_len = howmany(ic->ic_max_aid, 8) * sizeof(u_int8_t);
	MALLOC(ic->ic_tim_bitmap, u_int8_t *, ic->ic_tim_len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ic->ic_tim_bitmap == NULL)
	{/* XXX no way to recover */
		printf("%s: no memory for TIM bitmap!\n", __func__);
	}
	
	ic->ic_set_tim = __ieee80211_set_tim;	/* NB: driver should override */
	
}




static void __ieee80211_node_table_cleanup(struct ieee80211_node_table *nt)
{
	IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE, "%s %s table\n", __func__, nt->nt_name);

	_ieee80211_free_allnodes_locked(nt);
	IEEE80211_SCAN_LOCK_DESTROY(nt);
	IEEE80211_NODE_LOCK_DESTROY(nt);
}

void ieee80211_node_detach(struct ieee80211com *ic)
{

	if (ic->ic_bss != NULL)
	{
		ieee80211_free_node(ic->ic_bss);
		ic->ic_bss = NULL;
	}
	
	__ieee80211_node_table_cleanup(&ic->ic_scan);
	__ieee80211_node_table_cleanup(&ic->ic_sta);
	if (ic->ic_aid_bitmap != NULL)
	{
		kfree(ic->ic_aid_bitmap);
		ic->ic_aid_bitmap = NULL;
	}
	if (ic->ic_tim_bitmap != NULL)
	{
		kfree(ic->ic_tim_bitmap);
		ic->ic_tim_bitmap = NULL;
	}
}


