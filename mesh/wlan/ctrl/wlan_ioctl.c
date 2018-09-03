/*
* $Id: wlan_ioctl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_wlan_ioctl.h"

static int __getkey(struct ieee80211com *ic, struct meshw_req *iwr)
{
	struct ieee80211_node *ni;
	struct ieee80211req_key ik;
	struct ieee80211_key *wk;
	const struct ieee80211_cipher *cip;
	u_int kid;

	if (iwr->u.data.length != sizeof(ik))
		return -EINVAL;
	if (copy_from_user(&ik, iwr->u.data.pointer, sizeof(ik)))
		return -EFAULT;
	kid = ik.ik_keyix;
	if (kid == IEEE80211_KEYIX_NONE)
	{
		ni = ieee80211_find_node(&ic->ic_sta, ik.ik_macaddr);
		if (ni == NULL)
			return -EINVAL;		/* XXX */
		wk = &ni->ni_ucastkey;
	}
	else
	{
		if (kid >= IEEE80211_WEP_NKID)
			return -EINVAL;
		wk = &ic->ic_nw_keys[kid];
		IEEE80211_ADDR_COPY(&ik.ik_macaddr, ic->ic_bss->ni_macaddr);
		ni = NULL;
	}
	cip = wk->wk_cipher;
	ik.ik_type = cip->ic_cipher;
	ik.ik_keylen = wk->wk_keylen;
	ik.ik_flags = wk->wk_flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV);
	if (wk->wk_keyix == ic->ic_def_txkey)
		ik.ik_flags |= IEEE80211_KEY_DEFAULT;
	
	if (capable(CAP_NET_ADMIN))
	{
		/* NB: only root can read key data */
		ik.ik_keyrsc = wk->wk_keyrsc;
		ik.ik_keytsc = wk->wk_keytsc;
		memcpy(ik.ik_keydata, wk->wk_key, wk->wk_keylen);
		if (cip->ic_cipher == IEEE80211_CIPHER_TKIP)
		{
			memcpy(ik.ik_keydata+wk->wk_keylen,wk->wk_key + IEEE80211_KEYBUF_SIZE, IEEE80211_MICBUF_SIZE);
			ik.ik_keylen += IEEE80211_MICBUF_SIZE;
		}
	}
	else
	{
		ik.ik_keyrsc = 0;
		ik.ik_keytsc = 0;
		memset(ik.ik_keydata, 0, sizeof(ik.ik_keydata));
	}
	
	if (ni != NULL)
		ieee80211_free_node(ni);
	
	return (copy_to_user(iwr->u.data.pointer, &ik, sizeof(ik)) ?-EFAULT : 0);
}

static int __getwpaie(struct ieee80211com *ic, struct meshw_req *iwr)
{
	struct ieee80211_node *ni;
	struct ieee80211req_wpaie wpaie;

	if (iwr->u.data.length != sizeof(wpaie))
		return -EINVAL;
	if (copy_from_user(&wpaie, iwr->u.data.pointer, IEEE80211_ADDR_LEN))
		return -EFAULT;
	
	ni = ieee80211_find_node(&ic->ic_sta, wpaie.wpa_macaddr);
	if (ni == NULL)
		return -EINVAL;		/* XXX */

	memset(wpaie.wpa_ie, 0, sizeof(wpaie.wpa_ie));
	if (ni->ni_wpa_ie != NULL)
	{
		int ielen = ni->ni_wpa_ie[1] + 2;
		if (ielen > (int)sizeof(wpaie.wpa_ie))
			ielen = sizeof(wpaie.wpa_ie);
		memcpy(wpaie.wpa_ie, ni->ni_wpa_ie, ielen);
	}

	ieee80211_free_node(ni);
	return (copy_to_user(iwr->u.data.pointer, &wpaie, sizeof(wpaie)) ? -EFAULT : 0);
}

static int __getstastats(struct ieee80211com *ic, struct meshw_req *iwr)
{
	struct ieee80211_node *ni;
	u_int8_t macaddr[IEEE80211_ADDR_LEN];
	const int off = offsetof(struct ieee80211req_sta_stats, is_stats);
	int error;

	if (iwr->u.data.length < off)
		return -EINVAL;
	if (copy_from_user(&macaddr, iwr->u.data.pointer, IEEE80211_ADDR_LEN))
		return -EFAULT;
	
	ni = ieee80211_find_node(&ic->ic_sta, macaddr);
	if (ni == NULL)
		return -EINVAL;		/* XXX */
	
	/* NB: copy out only the statistics */
	error = copy_to_user((u_int8_t *) iwr->u.data.pointer + off, &ni->ni_stats, iwr->u.data.length - off);

	ieee80211_free_node(ni);
	
	return (error ? -EFAULT : 0);
}

/* Handle private ioctl requests. called by net_device ioctl  */
int wlan_ioctl(struct ieee80211com *ic, unsigned long data, int cmd)
{
	struct meshw_req *mreq = (struct meshw_req *)data;
	switch (cmd)
	{
		case SIOCG80211STATS:
		{
			return copy_to_user( mreq->u.data.pointer, /*ifr->ifr_data*/&ic->ic_stats, sizeof (ic->ic_stats)) ? -EFAULT : 0;
		}
		case IEEE80211_IOCTL_GETKEY:
			return __getkey(ic, mreq);
		case IEEE80211_IOCTL_GETWPAIE:
			return __getwpaie(ic, mreq);//(struct meshw_req *)data);
		case IEEE80211_IOCTL_GETSTASTATS:
			return __getstastats(ic, mreq);//(struct meshw_req *)data);
	}
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wlan_ioctl);


/* functions of standard handlers */
EXPORT_SYMBOL(ieee80211_iw_getstats);
EXPORT_SYMBOL(ieee80211_ioctl_giwname);
EXPORT_SYMBOL(ieee80211_ioctl_siwencode);
EXPORT_SYMBOL(ieee80211_ioctl_giwencode);
EXPORT_SYMBOL(ieee80211_ioctl_siwrate);
EXPORT_SYMBOL(ieee80211_ioctl_giwrate);
EXPORT_SYMBOL(ieee80211_ioctl_siwsens);
EXPORT_SYMBOL(ieee80211_ioctl_giwsens);
EXPORT_SYMBOL(ieee80211_ioctl_siwrts);
EXPORT_SYMBOL(ieee80211_ioctl_giwrts);
EXPORT_SYMBOL(ieee80211_ioctl_siwfrag);
EXPORT_SYMBOL(ieee80211_ioctl_giwfrag);
EXPORT_SYMBOL(ieee80211_ioctl_siwap);
EXPORT_SYMBOL(ieee80211_ioctl_giwap);
EXPORT_SYMBOL(ieee80211_ioctl_siwnickn);
EXPORT_SYMBOL(ieee80211_ioctl_giwnickn);
EXPORT_SYMBOL(ieee80211_ioctl_siwfreq);
EXPORT_SYMBOL(ieee80211_ioctl_giwfreq);
EXPORT_SYMBOL(ieee80211_ioctl_siwessid);
EXPORT_SYMBOL(ieee80211_ioctl_giwessid);

EXPORT_SYMBOL(ieee80211_ioctl_giwrange);
EXPORT_SYMBOL(ieee80211_ioctl_siwmode);
EXPORT_SYMBOL(ieee80211_ioctl_giwmode);
EXPORT_SYMBOL(ieee80211_ioctl_siwpower);
EXPORT_SYMBOL(ieee80211_ioctl_giwpower);
EXPORT_SYMBOL(ieee80211_ioctl_siwretry);
EXPORT_SYMBOL(ieee80211_ioctl_giwretry);
EXPORT_SYMBOL(ieee80211_ioctl_giwtxpow);
EXPORT_SYMBOL(ieee80211_ioctl_siwtxpow);

EXPORT_SYMBOL(ieee80211_ioctl_iwaplist);
#ifdef MESHW_IO_R_SCAN
EXPORT_SYMBOL(ieee80211_ioctl_siwscan);
EXPORT_SYMBOL(ieee80211_ioctl_giwscan);
#endif /* MESHW_IO_R_SCAN */


/* functions of private handlers */
EXPORT_SYMBOL(ieee80211_ioctl_getparam);
EXPORT_SYMBOL(ieee80211_ioctl_setparam);
EXPORT_SYMBOL(ieee80211_ioctl_setoptie);
EXPORT_SYMBOL(ieee80211_ioctl_getoptie);
EXPORT_SYMBOL(ieee80211_ioctl_setkey);
EXPORT_SYMBOL(ieee80211_ioctl_delkey);

EXPORT_SYMBOL(ieee80211_ioctl_setmlme);
EXPORT_SYMBOL(ieee80211_ioctl_addmac);
EXPORT_SYMBOL(ieee80211_ioctl_delmac);
EXPORT_SYMBOL(ieee80211_ioctl_chanlist);

/* events handlers */
EXPORT_SYMBOL(ieee80211_notify_replay_failure);
EXPORT_SYMBOL(ieee80211_notify_michael_failure);

