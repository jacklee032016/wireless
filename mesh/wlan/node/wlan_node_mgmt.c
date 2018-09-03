/*
* $Id: wlan_node_mgmt.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */


#include "_node.h"

void _ieee80211_setup_node(struct ieee80211_node_table *nt,	struct ieee80211_node *ni, const u_int8_t *macaddr)
{
	struct ieee80211com *ic = nt->nt_ic;
	int hash;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,  "%s %p<%s> in %s table\n", __func__, ni,  swjtu_mesh_mac_sprintf(macaddr), nt->nt_name);

	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	hash = IEEE80211_NODE_HASH(macaddr);
	skb_queue_head_init(&ni->ni_savedq);
	ieee80211_node_initref(ni);		/* mark referenced */
	
	ni->ni_chan = IEEE80211_CHAN_ANYC;
	ni->ni_authmode = IEEE80211_AUTH_OPEN;
	ni->ni_txpower = ic->ic_txpowlimit;	/* max power */
	ieee80211_crypto_resetkey(ic, &ni->ni_ucastkey, IEEE80211_KEYIX_NONE);
	ni->ni_inact_reload = nt->nt_inact_init;
	ni->ni_inact = ni->ni_inact_reload;
	IEEE80211_NODE_SAVEQ_INIT(ni, "unknown");

	IEEE80211_NODE_LOCK_BH(nt);
	TAILQ_INSERT_TAIL(&nt->nt_node, ni, ni_list);
	LIST_INSERT_HEAD(&nt->nt_hash[hash], ni, ni_hash);
	ni->ni_table = nt;
	ni->ni_ic = ic;
	IEEE80211_NODE_UNLOCK_BH(nt);
}

struct ieee80211_node *ieee80211_alloc_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;

	ni = ic->ic_node_alloc(nt);
	if (ni != NULL)
		_ieee80211_setup_node(nt, ni, macaddr);
	else
		ic->ic_stats.is_rx_nodealloc++;
	return ni;
}


static void __ieee80211_free_node(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_node_table *nt = ni->ni_table;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE, "%s %p<%s> in %s table\n", __func__, ni, swjtu_mesh_mac_sprintf(ni->ni_macaddr), nt != NULL ? nt->nt_name : "<gone>");

	IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
	if (nt != NULL)
	{
		TAILQ_REMOVE(&nt->nt_node, ni, ni_list);
		LIST_REMOVE(ni, ni_hash);
	}
	ic->ic_node_free(ni);
}

void ieee80211_free_node(struct ieee80211_node *ni)
{
	struct ieee80211_node_table *nt = ni->ni_table;

	if (ieee80211_node_dectestref(ni))
	{
		/*
		 * Beware; if the node is marked gone then it's already
		 * been removed from the table and we cannot assume the
		 * table still exists.  Regardless, there's no need to lock
		 * the table.
		 */
		if (ni->ni_table != NULL)
		{
			IEEE80211_NODE_LOCK(nt);
			__ieee80211_free_node(ni);
			IEEE80211_NODE_UNLOCK(nt);
		}
		else
			__ieee80211_free_node(ni);
	}
}


/*
 * Reclaim a node.  If this is the last reference count then
 * do the normal free work.  Otherwise remove it from the node
 * table and mark it gone by clearing the back-reference.
 */
void _node_reclaim(struct ieee80211_node_table *nt, struct ieee80211_node *ni)
{
	IEEE80211_DPRINTF(ni->ni_ic, IEEE80211_MSG_NODE,"%s: remove %p<%s> from %s table, refcnt %d\n",	__func__, ni, swjtu_mesh_mac_sprintf(ni->ni_macaddr),	nt->nt_name, ieee80211_node_refcnt(ni)-1);
	if (!ieee80211_node_dectestref(ni))
	{
		/*
		 * Other references are present, just remove the
		 * node from the table so it cannot be found.  When
		 * the references are dropped storage will be
		 * reclaimed.
		 */
		TAILQ_REMOVE(&nt->nt_node, ni, ni_list);
		LIST_REMOVE(ni, ni_hash);
		ni->ni_table = NULL;		/* clear reference */
	}
	else
		__ieee80211_free_node(ni);
}

void _ieee80211_free_allnodes_locked(struct ieee80211_node_table *nt)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE, "%s: free all nodes in %s table\n", __func__, nt->nt_name);

	while ((ni = TAILQ_FIRST(&nt->nt_node)) != NULL)
	{
		if (ni->ni_associd != 0)
		{
			if (ic->ic_auth->ia_node_leave != NULL)
				ic->ic_auth->ia_node_leave(ic, ni);
			IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
		}
		_node_reclaim(nt, ni);
	}
	ieee80211_reset_erp(ic);
}

void _ieee80211_free_allnodes(struct ieee80211_node_table *nt)
{
	IEEE80211_NODE_LOCK(nt);
	_ieee80211_free_allnodes_locked(nt);
	IEEE80211_NODE_UNLOCK(nt);
}

void ieee80211_node_table_reset(struct ieee80211_node_table *nt)
{
	IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE, "%s %s table\n", __func__, nt->nt_name);

	IEEE80211_NODE_LOCK(nt);
	nt->nt_inact_timer = 0;
	_ieee80211_free_allnodes_locked(nt);
	IEEE80211_NODE_UNLOCK(nt);
}


void ieee80211_node_lateattach(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	struct ieee80211_rsnparms *rsn;

	ni = ieee80211_alloc_node(&ic->ic_scan, ic->ic_myaddr);
	KASSERT(ni != NULL, ("unable to setup inital BSS node"));
	/*
	 * Setup "global settings" in the bss node so that
	 * each new station automatically inherits them.
	 */
	rsn = &ni->ni_rsn;
	/* WEP, TKIP, and AES-CCM are always supported */
	rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_WEP;
	rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_TKIP;
	rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_AES_CCM;
	if (ic->ic_caps & IEEE80211_C_AES)
		rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_AES_OCB;
	if (ic->ic_caps & IEEE80211_C_CKIP)
		rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_CKIP;
	/*
	 * Default unicast cipher to WEP for 802.1x use.  If
	 * WPA is enabled the management code will set these
	 * values to reflect.
	 */
	rsn->rsn_ucastcipher = IEEE80211_CIPHER_WEP;
	rsn->rsn_ucastkeylen = 104 / NBBY;
	/*
	 * WPA says the multicast cipher is the lowest unicast
	 * cipher supported.  But we skip WEP which would
	 * otherwise be used based on this criteria.
	 */
	rsn->rsn_mcastcipher = IEEE80211_CIPHER_TKIP;
	rsn->rsn_mcastkeylen = 128 / NBBY;

	/*
	 * We support both WPA-PSK and 802.1x; the one used
	 * is determined by the authentication mode and the
	 * setting of the PSK state.
	 */
	rsn->rsn_keymgmtset = WPA_ASE_8021X_UNSPEC | WPA_ASE_8021X_PSK;
	rsn->rsn_keymgmt = WPA_ASE_8021X_PSK;

	ic->ic_bss = ieee80211_ref_node(ni);		/* hold reference */
	ic->ic_auth = ieee80211_authenticator_get(ni->ni_authmode);
}

/* Port authorize/unauthorize interfaces for use by an authenticator. */
void ieee80211_node_authorize(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	ni->ni_flags |= IEEE80211_NODE_AUTH;
	ni->ni_inact_reload = ic->ic_inact_run;
}

void ieee80211_node_unauthorize(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	ni->ni_flags &= ~IEEE80211_NODE_AUTH;
}

void ieee80211_iterate_nodes(struct ieee80211_node_table *nt, ieee80211_iter_func *f, void *arg)
{
	struct ieee80211_node *ni;
	u_int gen;

	IEEE80211_SCAN_LOCK(nt);
	gen = nt->nt_scangen++;
restart:
	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
	{
		if (ni->ni_scangen != gen)
		{
			ni->ni_scangen = gen;
			(void) ieee80211_ref_node(ni);
			IEEE80211_NODE_UNLOCK(nt);
			(*f)(arg, ni);
			ieee80211_free_node(ni);
			goto restart;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	IEEE80211_SCAN_UNLOCK(nt);
}

struct ieee80211_node *_ieee80211_find_node(struct ieee80211_node_table *nt,	const u_int8_t *macaddr)
{
	struct ieee80211_node *ni;
	int hash;

	IEEE80211_NODE_LOCK_ASSERT(nt);

	hash = IEEE80211_NODE_HASH(macaddr);
	LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash)
	{
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr))
		{
			ieee80211_ref_node(ni);	/* mark referenced */
			return ni;
		}
	}
	return NULL;
}

struct ieee80211_node *ieee80211_find_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni;

	IEEE80211_NODE_LOCK(nt);
	ni = _ieee80211_find_node(nt, macaddr);
	IEEE80211_NODE_UNLOCK(nt);
	return ni;
}

/*
 * Like find but search based on the channel too. both mac_addr and chan
 */
struct ieee80211_node *ieee80211_find_node_with_channel(struct ieee80211_node_table *nt,
	const u_int8_t *macaddr, struct ieee80211_channel *chan)
{
	struct ieee80211_node *ni;
	int hash;

	hash = IEEE80211_NODE_HASH(macaddr);
	IEEE80211_NODE_LOCK(nt);
	LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash)
	{
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr) &&
		    ni->ni_chan == chan)
		{
			ieee80211_ref_node(ni);		/* mark referenced */
			IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE,  "%s %p<%s> refcnt %d\n", __func__, ni, swjtu_mesh_mac_sprintf(ni->ni_macaddr),  ieee80211_node_refcnt(ni));

			break;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);
	return ni;
}

/*
 * Like find but search based on the ssid too. both mac_addr and ssid 
 */
struct ieee80211_node *ieee80211_find_node_with_ssid(struct ieee80211_node_table *nt,
	const u_int8_t *macaddr, u_int ssidlen, const u_int8_t *ssid)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;
	int hash;

	hash = IEEE80211_NODE_HASH(macaddr);
	IEEE80211_NODE_LOCK(nt);

	LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash)
	{
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr) &&
		    ni->ni_esslen == ic->ic_des_esslen &&
		    memcmp(ni->ni_essid, ic->ic_des_essid, ni->ni_esslen) == 0)
		{
			ieee80211_ref_node(ni);		/* mark referenced */
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE, "%s %p<%s> refcnt %d\n", __func__, ni, swjtu_mesh_mac_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni));

			break;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);
	return ni;
}


