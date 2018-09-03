/*
* $Id: wlan_handlers_private.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_wlan_ioctl.h"

int ieee80211_ioctl_setkey(struct ieee80211com *ic, struct meshw_request_info *info,
		   	 void *w, char *extra)
{
	struct ieee80211req_key *ik = (struct ieee80211req_key *)extra;
	struct ieee80211_node *ni;
	struct ieee80211_key *wk;
	u_int16_t kid;
	int error;

	/* NB: cipher support is verified by ieee80211_crypt_newkey */
	/* NB: this also checks ik->ik_keylen > sizeof(wk->wk_key) */
	if (ik->ik_keylen > sizeof(ik->ik_keydata))
		return -E2BIG;
	kid = ik->ik_keyix;
	if (kid == IEEE80211_KEYIX_NONE) 
	{
		/* XXX unicast keys currently must be tx/rx */
		if (ik->ik_flags != (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV))
			return -EINVAL;
		if (ic->ic_opmode == IEEE80211_M_STA)
		{
			ni = ic->ic_bss;
			if (!IEEE80211_ADDR_EQ(ik->ik_macaddr, ni->ni_bssid))
				return -EADDRNOTAVAIL;
		}
		else
			ni = ieee80211_find_node(&ic->ic_sta, ik->ik_macaddr);
		
		if (ni == NULL)
			return -ENOENT;
		wk = &ni->ni_ucastkey;

	}
	else
	{
		if (kid >= IEEE80211_WEP_NKID)
			return -EINVAL;
		wk = &ic->ic_nw_keys[kid];
		ni = NULL;
		/* XXX auto-add group key flag until applications are updated */
		if ((ik->ik_flags & IEEE80211_KEY_XMIT) == 0)    /* XXX */
			ik->ik_flags |= IEEE80211_KEY_GROUP;     /* XXX */
	}
	error = 0;
	ieee80211_key_update_begin(ic);
	
	if (ieee80211_crypto_newkey(ic, ik->ik_type, ik->ik_flags, wk))
	{
		wk->wk_keylen = ik->ik_keylen;
		/* NB: MIC presence is implied by cipher type */
		if (wk->wk_keylen > IEEE80211_KEYBUF_SIZE)
			wk->wk_keylen = IEEE80211_KEYBUF_SIZE;
		wk->wk_keyrsc = ik->ik_keyrsc;
		wk->wk_keytsc = 0;			/* new key, reset */
		wk->wk_flags |=
			ik->ik_flags & (IEEE80211_KEY_XMIT|IEEE80211_KEY_RECV);
		memset(wk->wk_key, 0, sizeof(wk->wk_key));
		memcpy(wk->wk_key, ik->ik_keydata, ik->ik_keylen);
		if (!ieee80211_crypto_setkey(ic, wk,
		    ni != NULL ? ni->ni_macaddr : ik->ik_macaddr))
			error = -EIO;
		else if ((ik->ik_flags & IEEE80211_KEY_DEFAULT))
			ic->ic_def_txkey = kid;
	}
	else
		error = -ENXIO;
	
	ieee80211_key_update_end(ic);
	if (ni != NULL)
		ieee80211_free_node(ni);
	return error;
}


int ieee80211_ioctl_delkey(struct ieee80211com *ic, struct meshw_request_info *info,
		   	 void *w, char *extra)
{
	struct ieee80211req_del_key *dk = (struct ieee80211req_del_key *)extra;
	int kid;

	kid = dk->idk_keyix;
	/* XXX u_int8_t -> u_int16_t */
	if (dk->idk_keyix == (u_int8_t) IEEE80211_KEYIX_NONE)
	{
		struct ieee80211_node *ni =
			ieee80211_find_node(&ic->ic_sta, dk->idk_macaddr);
		if (ni == NULL)
			return -EINVAL;		/* XXX */
		/* XXX error return */
		ieee80211_crypto_delkey(ic, &ni->ni_ucastkey);
		ieee80211_free_node(ni);
	}
	else
	{
		if (kid >= IEEE80211_WEP_NKID)
			return -EINVAL;
		/* XXX error return */
		ieee80211_crypto_delkey(ic, &ic->ic_nw_keys[kid]);
	}
	return 0;
}

static void __domlme(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211req_mlme *mlme = arg;

	if (ni->ni_associd != 0)
	{
		IEEE80211_SEND_MGMT(ic, ni,
			mlme->im_op == IEEE80211_MLME_DEAUTH ?
				IEEE80211_FC0_SUBTYPE_DEAUTH :
				IEEE80211_FC0_SUBTYPE_DISASSOC,
			mlme->im_reason);
	}
	ieee80211_node_leave(ic, ni);
}

int ieee80211_ioctl_setmlme(struct ieee80211com *ic, struct meshw_request_info *info,
		   	 void *w, char *extra)
{
	struct ieee80211req_mlme *mlme = (struct ieee80211req_mlme *)extra;
	struct ieee80211_node *ni;

	switch (mlme->im_op)
	{
		case IEEE80211_MLME_ASSOC:
	                if (ic->ic_opmode != IEEE80211_M_STA)
	                        return EINVAL;
	                /* XXX must be in S_SCAN state? */

			if (ic->ic_des_esslen != 0)
			{
				/*
				 * Desired ssid specified; must match both bssid and
				 * ssid to distinguish ap advertising multiple ssid's.
				 */
				ni = ieee80211_find_node_with_ssid(&ic->ic_scan,
					mlme->im_macaddr,
					ic->ic_des_esslen, ic->ic_des_essid);
			}
			else
			{
				/** Normal case; just match bssid.*/
				ni = ieee80211_find_node(&ic->ic_scan, mlme->im_macaddr);
			}
			if (ni == NULL)
				return EINVAL;
			
			if (!ieee80211_sta_join(ic, ni))
			{
				ieee80211_free_node(ni);
				return EINVAL;
			}
			break;
			
		case IEEE80211_MLME_DISASSOC:
		case IEEE80211_MLME_DEAUTH:
			switch (ic->ic_opmode)
			{
				case IEEE80211_M_STA:
					/* XXX not quite right */
					ieee80211_new_state(ic, IEEE80211_S_INIT, mlme->im_reason);
					break;
				case IEEE80211_M_HOSTAP:
					/* NB: the broadcast address means do 'em all */
					if (!IEEE80211_ADDR_EQ(mlme->im_macaddr, ic->ic_dev->broadcast)) {
						if ((ni = ieee80211_find_node(&ic->ic_sta, mlme->im_macaddr)) == NULL)
							return EINVAL;
						__domlme(mlme, ni);
						ieee80211_free_node(ni);
					}
					else
					{
						ieee80211_iterate_nodes(&ic->ic_sta,__domlme, mlme);
					}
					break;
				default:
					return EINVAL;
			}
			break;
		case IEEE80211_MLME_AUTHORIZE:
		case IEEE80211_MLME_UNAUTHORIZE:
			if (ic->ic_opmode != IEEE80211_M_HOSTAP)
				return -EINVAL;
			ni = ieee80211_find_node(&ic->ic_sta, mlme->im_macaddr);
			if (ni == NULL)
				return -EINVAL;
			if (mlme->im_op == IEEE80211_MLME_AUTHORIZE)
				ieee80211_node_authorize(ic, ni);
			else
				ieee80211_node_unauthorize(ic, ni);
			ieee80211_free_node(ni);
			break;
		case IEEE80211_MLME_CLEAR_STATS:
			if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			    return -EINVAL;
			ni = ieee80211_find_node(&ic->ic_sta, mlme->im_macaddr);
			if (ni == NULL)
			    return -EINVAL;
			/* clear statistics */
			memset(&ni->ni_stats, 0, sizeof(struct ieee80211_nodestats));
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

int ieee80211_ioctl_setoptie(struct ieee80211com *ic, struct meshw_request_info *info,
		   	 void *w, char *extra)
{
	union meshw_req_data *u = w;
	void *ie;

	/*
	 * NB: Doing this for ap operation could be useful (e.g. for
	 *     WPA and/or WME) except that it typically is worthless
	 *     without being able to intervene when processing
	 *     association response frames--so disallow it for now.
	 */
	if (ic->ic_opmode != IEEE80211_M_STA)
		return -EINVAL;
	/* NB: data.length is validated by the wireless extensions code */
	MALLOC(ie, void *, u->data.length, M_DEVBUF, M_WAITOK);
	if (ie == NULL)
		return -ENOMEM;
	memcpy(ie, extra, u->data.length);
	/* XXX sanity check data? */
	if (ic->ic_opt_ie != NULL)
		FREE(ic->ic_opt_ie, M_DEVBUF);
	ic->ic_opt_ie = ie;
	ic->ic_opt_ie_len = u->data.length;
	return 0;
}


int ieee80211_ioctl_getoptie(struct ieee80211com *ic, struct meshw_request_info *info,
		   	 void *w, char *extra)
{
	union meshw_req_data *u = w;

	if (ic->ic_opt_ie == NULL)
		return -EINVAL;
	if (u->data.length < ic->ic_opt_ie_len)
		return -EINVAL;
	u->data.length = ic->ic_opt_ie_len;
	memcpy(extra, ic->ic_opt_ie, u->data.length);
	return 0;
}


int ieee80211_ioctl_addmac(struct ieee80211com *ic, struct meshw_request_info *info,
		   	 void *w, char *extra)
{
	struct sockaddr *sa = (struct sockaddr *)extra;
	
#if WITH_WLAN_AUTHBACK_ACL
	const struct ieee80211_aclator *acl = ic->ic_acl;
	if (acl == NULL)
	{
		acl = ieee80211_aclator_get("mac");
		if (acl == NULL || !acl->iac_attach(ic))
			return -EINVAL;
		ic->ic_acl = acl;
	}
	acl->iac_add(ic, sa->sa_data);
#endif

	return 0;
}

int ieee80211_ioctl_delmac(struct ieee80211com *ic, struct meshw_request_info *info,
		   	 void *w, char *extra)
{
	struct sockaddr *sa = (struct sockaddr *)extra;
	
#if WITH_WLAN_AUTHBACK_ACL
	const struct ieee80211_aclator *acl = ic->ic_acl;
	if (acl == NULL)
	{
		acl = ieee80211_aclator_get("mac");
		if (acl == NULL || !acl->iac_attach(ic))
			return -EINVAL;
		ic->ic_acl = acl;
	}
	acl->iac_remove(ic, sa->sa_data);
#endif
	return 0;
}

int ieee80211_ioctl_chanlist(struct ieee80211com *ic, struct meshw_request_info *info, void *w, char *extra)
{
	struct ieee80211req_chanlist *list = (struct ieee80211req_chanlist *)extra;
	u_char chanlist[roundup(IEEE80211_CHAN_MAX, NBBY)];
	int i, j;

	memset(chanlist, 0, sizeof(chanlist));
	/*
	 * Since channel 0 is not available for DS, channel 1
	 * is assigned to LSB on WaveLAN.
	 */
	if (ic->ic_phytype == IEEE80211_T_DS)
		i = 1;
	else
		i = 0;

	for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++)
	{
		/* NB: silently discard unavailable channels so users
		 *  can specify 1-255 to get all available channels.
		 */
		if (isset(list->ic_channels, j) && isset(ic->ic_chan_avail, i))
			setbit(chanlist, i);
	}
	
	if (ic->ic_ibss_chan == NULL ||isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_ibss_chan)))
	{
		for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
		{
			if (isset(chanlist, i))
			{
				ic->ic_ibss_chan = &ic->ic_channels[i];
				goto found;
			}
		}	
		return EINVAL;			/* no active channels */
found:
		;
	}
	
	memcpy(ic->ic_chan_active, chanlist, sizeof(ic->ic_chan_active));
	if (ic->ic_bss->ni_chan == IEEE80211_CHAN_ANYC ||
	    isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan)))
		ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	
	return IS_UP_AUTO(ic) ? -(*ic->ic_init)(ic->ic_dev) : 0;
}

