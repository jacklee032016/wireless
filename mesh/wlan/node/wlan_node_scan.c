/*
* $Id: wlan_node_scan.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

/*
 * IEEE 802.11 node handling support.
 */

#include "_node.h"

/*
 * AP scanning support.
 */
static __inline u_int8_t __maxrate(const struct ieee80211_node *ni)
{
	const struct ieee80211_rateset *rs = &ni->ni_rates;
	/* NB: assumes rate set is sorted (happens on frame receive) */
	return rs->rs_rates[rs->rs_nrates-1] & IEEE80211_RATE_VAL;
}

#ifdef IEEE80211_DEBUG
static void __dump_chanlist(const u_char chans[])
{
	const char *sep;
	int i;

	sep = " ";
	for (i = 0; i < IEEE80211_CHAN_MAX; i++)
		if (isset(chans, i))
		{
			printf("%s%u", sep, i);
			sep = ", ";
		}
}
#endif /* IEEE80211_DEBUG */


/*
 * Compare the capabilities of two nodes and decide which is
 * more desirable (return >0 if a is considered better).  Note
 * that we assume compatibility/usability has already been checked
 * so we don't need to (e.g. validate whether privacy is supported).
 * Used to select the best scan candidate for association in a BSS.
 */
static int __ieee80211_node_compare(struct ieee80211com *ic, const struct ieee80211_node *a, const struct ieee80211_node *b)
{
	u_int8_t maxa, maxb;
	u_int8_t rssia, rssib;

	/* privacy support preferred */
	if ((a->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) &&
	    (b->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
		return 1;
	
	if ((a->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0 &&
	    (b->ni_capinfo & IEEE80211_CAPINFO_PRIVACY))
		return -1;

	rssia = ic->ic_node_getrssi(a);
	rssib = ic->ic_node_getrssi(b);
	
	if (abs(rssib - rssia) < 5)
	{
		/* best/max rate preferred if signal level close enough XXX */
		maxa = __maxrate(a);
		maxb = __maxrate(b);
		if (maxa != maxb)
			return maxa - maxb;
		
		/* XXX use freq for channel preference */
		/* for now just prefer 5Ghz band to all other bands */
		if (IEEE80211_IS_CHAN_5GHZ(a->ni_chan) &&
		   !IEEE80211_IS_CHAN_5GHZ(b->ni_chan))
			return 1;

		if (!IEEE80211_IS_CHAN_5GHZ(a->ni_chan) &&
		     IEEE80211_IS_CHAN_5GHZ(b->ni_chan))
			return -1;
	}
	
	/* all things being equal, use signal level */
	return rssia - rssib;
}

/*
 * Initialize the channel set to scan based on the
 * of available channels and the current PHY mode.
 */
static void __ieee80211_reset_scan(struct ieee80211com *ic)
{

	/* XXX ic_des_chan should be handled with ic_chan_active */
	if (ic->ic_des_chan != IEEE80211_CHAN_ANYC)
	{
		memset(ic->ic_chan_scan, 0, sizeof(ic->ic_chan_scan));
		setbit(ic->ic_chan_scan, ieee80211_chan2ieee(ic, ic->ic_des_chan));
	}
	else
		memcpy(ic->ic_chan_scan, ic->ic_chan_active, sizeof(ic->ic_chan_active));

	/* NB: hack, setup so next_scan starts with the first channel */
	if (ic->ic_bss->ni_chan == IEEE80211_CHAN_ANYC)
		ieee80211_set_chan(ic, ic->ic_bss, &ic->ic_channels[IEEE80211_CHAN_MAX]);
	
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(ic))
	{
		printf("%s: scan set:", __func__);
		__dump_chanlist(ic->ic_chan_scan);
		printf(" start chan %u\n", ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan));
	}
#endif /* IEEE80211_DEBUG */
}

/* Complete a scan of potential channels. */
static void __ieee80211_end_scan(struct ieee80211com *ic)
{
	struct ieee80211_node_table *nt = &ic->ic_scan;
	struct ieee80211_node *ni, *selbs;
	ic->ic_flags &= ~IEEE80211_F_SSCAN;

	ic->ic_scan.nt_scangen++;
	ieee80211_cancel_scan(ic);

#if 0	
	ieee80211_notify_scan_done(ic);
#endif

	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
	{
		u_int8_t maxrssi[IEEE80211_CHAN_MAX];	/* XXX off stack? */
		int i, bestchan;
		u_int8_t rssi;

		/*
		 * The passive scan to look for existing AP's completed,
		 * select a channel to camp on.  Identify the channels
		 * that already have one or more AP's and try to locate
		 * an unoccupied one.  If that fails, pick a channel that
		 * looks to be quietest.
		 */
		memset(maxrssi, 0, sizeof(maxrssi));
		IEEE80211_NODE_LOCK(nt);
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
		{
			rssi = ic->ic_node_getrssi(ni);
			i = ieee80211_chan2ieee(ic, ni->ni_chan);
			if (rssi > maxrssi[i])
				maxrssi[i] = rssi;
		}
		
		IEEE80211_NODE_UNLOCK(nt);
		/* XXX select channel more intelligently */
		bestchan = -1;

		for (i = 0; i < IEEE80211_CHAN_MAX; i++)
		{
			if (isset(ic->ic_chan_active, i))
			{
				/*
				 * If the channel is unoccupied the max rssi
				 * should be zero; just take it.  Otherwise
				 * track the channel with the lowest rssi and
				 * use that when all channels appear occupied.
				 */
				if (maxrssi[i] == 0)
				{
					bestchan = i;
					break;
				}
				
				if (bestchan == -1 || maxrssi[i] < maxrssi[bestchan])
				    bestchan = i;
			}
		}
		
		if (bestchan != -1)
		{
			ieee80211_create_ibss(ic, &ic->ic_channels[bestchan]);
			return;
		}
		/* no suitable channel, should not happen */
	}

	/*
	 * When manually sequencing the state machine; scan just once
	 * regardless of whether we have a candidate or not.  The
	 * controlling application is expected to setup state and
	 * initiate an association.
	 */
	if (ic->ic_roaming == IEEE80211_ROAMING_MANUAL)
	{
		return;
	}
	/*
	 * Automatic sequencing; look for a candidate and
	 * if found join the network.
	 */
	/* NB: unlocked read should be ok */
	if (TAILQ_FIRST(&nt->nt_node) == NULL)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "%s: no scan candidate\n", __func__);
  notfound:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    (ic->ic_flags & IEEE80211_F_IBSSON) &&
		    ic->ic_des_esslen != 0)
		{
			ieee80211_create_ibss(ic, ic->ic_ibss_chan);
			return;
		}
		/*
		 * Reset the list of channels to scan and start again.
		 */
		__ieee80211_reset_scan(ic);
		ic->ic_flags |= IEEE80211_F_SCAN;
		ieee80211_next_scan(ic);
		return;
	}
	
	selbs = NULL;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "\t%s\n",  "macaddr          bssid         chan  rssi rate flag  wep  essid");
	IEEE80211_NODE_LOCK(nt);
	
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
	{
		if (ni->ni_fails)
		{
			/*
			 * The configuration of the access points may change
			 * during my scan.  So delete the entry for the AP
			 * and retry to associate if there is another beacon.
			 */
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "%s: skip scan candidate %s, fails %u\n",	__func__, swjtu_mesh_mac_sprintf(ni->ni_macaddr), ni->ni_fails);
			ni->ni_fails++;
#if 1
			if (ni->ni_fails++ > 2)
			{
				IEEE80211_NODE_LOCK(nt);
				_node_reclaim(nt, ni);
				IEEE80211_NODE_UNLOCK(nt);
			}
#endif
			continue;
		}
		
		if (_ieee80211_match_bss(ic, ni) == 0)
		{
			if (selbs == NULL)
				selbs = ni;
			else if (__ieee80211_node_compare(ic, ni, selbs) > 0)
				selbs = ni;
		}
	}
	
	if (selbs != NULL)		/* NB: grab ref while dropping lock */
		(void) ieee80211_ref_node(selbs);
	
	IEEE80211_NODE_UNLOCK(nt);
	if (selbs == NULL)
	{
		ic->ic_flags |= IEEE80211_F_SSCAN;
		goto notfound;
	}

	if (!ieee80211_sta_join(ic, selbs))
	{
		ieee80211_free_node(selbs);
		goto notfound;
	}
}


int _ieee80211_match_bss(struct ieee80211com *ic, struct ieee80211_node *ni)
{
        u_int8_t rate;
        int fail;

	fail = 0;
	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ni->ni_chan)))
		fail |= 0x01;
	
	if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&   ni->ni_chan != ic->ic_des_chan)
		fail |= 0x01;

	if (ic->ic_opmode == IEEE80211_M_IBSS)
	{
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			fail |= 0x02;
	}
	else
	{
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= 0x02;
	}

	if (ic->ic_flags & IEEE80211_F_PRIVACY)
	{
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			fail |= 0x04;
	}
	else
	{
		/* node requires privacy */
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			fail |= 0x04;
	}

	rate = ieee80211_fix_rate(ic, ni,IEEE80211_F_DONEGO | IEEE80211_F_DOFRATE);
	if (rate & IEEE80211_RATE_BASIC)
		fail |= 0x08;
	
	if (ic->ic_des_esslen != 0 &&  (ni->ni_esslen != ic->ic_des_esslen ||
	     memcmp(ni->ni_essid, ic->ic_des_essid, ic->ic_des_esslen) != 0))
		fail |= 0x10;
	if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, ni->ni_bssid))
		fail |= 0x20;
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(ic)) {
		printf(" %c %s", fail ? '-' : '+',
		    swjtu_mesh_mac_sprintf(ni->ni_macaddr));
		printf(" %s%c", swjtu_mesh_mac_sprintf(ni->ni_bssid),
		    fail & 0x20 ? '!' : ' ');
		printf(" %3d%c", ieee80211_chan2ieee(ic, ni->ni_chan),
			fail & 0x01 ? '!' : ' ');
		printf(" %+4d", ni->ni_rssi);
		printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
		    fail & 0x08 ? '!' : ' ');
		printf(" %4s%c",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
		    (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
		    "????",
		    fail & 0x02 ? '!' : ' ');
		printf(" %3s%c ",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ?
		    "wep" : "no",
		    fail & 0x04 ? '!' : ' ');
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		printf("%s\n", fail & 0x10 ? "!" : "");
	}
#endif
	return fail;
}

/* Begin an active scan. */
void ieee80211_begin_scan(struct ieee80211com *ic, int reset)
{
	/*
	 * In all but hostap mode scanning starts off in
	 * an active mode before switching to passive.
	 */
	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
	{
		ic->ic_flags |= IEEE80211_F_ASCAN;	/* XXX */
		ic->ic_stats.is_scan_active++;
	}
	else
		ic->ic_stats.is_scan_passive++;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "begin %s scan in %s mode, scangen %u\n",
		(ic->ic_flags & IEEE80211_F_ASCAN) ?  "active" : "passive", ieee80211_phymode_name[ic->ic_curmode], ic->ic_scan.nt_scangen);
	/*
	 * Clear scan state and flush any previously seen AP's.
	 */
	__ieee80211_reset_scan(ic);
	if (reset)
		_ieee80211_free_allnodes(&ic->ic_scan);

	ic->ic_flags |= IEEE80211_F_SCAN;

	/* Scan the next channel. */
	ieee80211_next_scan(ic);
}

/*
 * Switch to the next channel marked for scanning.
 */
int ieee80211_next_scan(struct ieee80211com *ic)
{
	struct ieee80211_channel *chan;

	/*
	 * Insure any previous mgt frame timeouts don't fire.
	 * This assumes the driver does the right thing in
	 * flushing anything queued in the driver and below.
	 */
	ic->ic_mgt_timer = 0;

	chan = ic->ic_bss->ni_chan;
	do
	{
	    /* disable channel scanning for VMAC */
#if HAS_VMAC 	    
		if (++chan > &ic->ic_channels[IEEE80211_CHAN_MAX])
			chan = &ic->ic_channels[0];
#endif
		if (isset(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan)))
		{
			clrbit(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan));
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,  "%s: chan %d->%d\n", __func__,
			    ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan),   ieee80211_chan2ieee(ic, chan));
			ieee80211_set_chan(ic, ic->ic_bss, chan);
#ifdef notyet
			/* XXX driver state change */
			/*
			 * Scan next channel. If doing an active scan
			 * and the channel is not marked passive-only
			 * then send a probe request.  Otherwise just
			 * listen for beacons on the channel.
			 */
			if ((ic->ic_flags & IEEE80211_F_ASCAN) &&
			    (ni->ni_chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0)
			{
				IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
			}
#else
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
#endif
			return 1;
		}
	} while (chan != ic->ic_bss->ni_chan);
	
	__ieee80211_end_scan(ic);
	return 0;
}

/* Mark an ongoing scan stopped. */
void ieee80211_cancel_scan(struct ieee80211com *ic)
{
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "%s: end %s scan\n", __func__, (ic->ic_flags & IEEE80211_F_ASCAN) ?  "active" : "passive");

	ic->ic_flags &= ~(IEEE80211_F_SCAN | IEEE80211_F_ASCAN);
}

