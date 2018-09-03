/*
* $Id: wlan_handlers.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "_wlan_ioctl.h"

static u_int encode_ie(void *, size_t, const u_int8_t *, size_t, const char *, size_t);
static void set_quality(struct meshw_quality *, u_int);

struct read_ap_args
{
	int i;
	int mode;
	struct meshw_event *iwe;
	char *start;
	char *current_ev;
};

static void read_ap_result(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct read_ap_args *sa = arg;
	struct meshw_event *iwe = sa->iwe;
	char *end_buf = sa->start + MESHW_SCAN_MAX_DATA;
	char *current_val;
	int j;
	char buf[64*2 + 30];

	/* Translate data to WE format. */
	if (sa->current_ev >= end_buf)
	{
		return;
	}

	if ((sa->mode != 0) ^ (ni->ni_wpa_ie != NULL))
		return;

	memset(iwe, 0, sizeof(iwe));
	iwe->cmd = MESHW_IO_R_AP_MAC;
	iwe->u.ap_addr.sa_family = ARPHRD_ETHER;
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		IEEE80211_ADDR_COPY(iwe->u.ap_addr.sa_data, ni->ni_macaddr);
	else
		IEEE80211_ADDR_COPY(iwe->u.ap_addr.sa_data, ni->ni_bssid);
	sa->current_ev = iwe_stream_add_event(sa->current_ev,
		end_buf, iwe, MESHW_EV_ADDR_LEN);

	memset(iwe, 0, sizeof(iwe));
	iwe->cmd = MESHW_IO_R_ESSID;
	iwe->u.data.flags = 1;
	
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
	{
		iwe->u.data.length = ic->ic_des_esslen;
		sa->current_ev = iwe_stream_add_point(sa->current_ev,
				end_buf, iwe, ic->ic_des_essid);
	}
	else
	{
		iwe->u.data.length = ni->ni_esslen;
		sa->current_ev = iwe_stream_add_point(sa->current_ev, end_buf, iwe, ni->ni_essid);
	}

	if (ni->ni_capinfo & (IEEE80211_CAPINFO_ESS|IEEE80211_CAPINFO_IBSS))
	{
		memset(iwe, 0, sizeof(iwe));
		iwe->cmd = MESHW_IO_R_MODE;
		iwe->u.mode = ni->ni_capinfo & IEEE80211_CAPINFO_ESS ?
			MESHW_MODE_MASTER : MESHW_MODE_ADHOC;
		sa->current_ev = iwe_stream_add_event(sa->current_ev,	end_buf, iwe, MESHW_EV_UINT_LEN);
	}

	memset(iwe, 0, sizeof(iwe));
	iwe->cmd = MESHW_IO_R_FREQ;
	iwe->u.freq.m = ni->ni_chan->ic_freq * 100000;
	iwe->u.freq.e = 1;
	sa->current_ev = iwe_stream_add_event(sa->current_ev,	end_buf, iwe, MESHW_EV_FREQ_LEN);

	memset(iwe, 0, sizeof(iwe));
	iwe->cmd = MESHW_EV_QUAL;
	set_quality(&iwe->u.qual, (*ic->ic_node_getrssi)(ni));
	sa->current_ev = iwe_stream_add_event(sa->current_ev,
		end_buf, iwe, MESHW_EV_QUAL_LEN);

	memset(iwe, 0, sizeof(iwe));
	iwe->cmd = MESHW_IO_R_ENCODE;
	if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
		iwe->u.data.flags = MESHW_ENCODE_ENABLED | MESHW_ENCODE_NOKEY;
	else
		iwe->u.data.flags = MESHW_ENCODE_DISABLED;
	iwe->u.data.length = 0;
	sa->current_ev = iwe_stream_add_point(sa->current_ev, end_buf, iwe, "");

	memset(iwe, 0, sizeof(iwe));
	iwe->cmd = MESHW_IO_R_RATE;
	current_val = sa->current_ev + MESHW_EV_LCP_LEN;
	for (j = 0; j < ni->ni_rates.rs_nrates; j++)
	{
		if (ni->ni_rates.rs_rates[j])
		{
			iwe->u.bitrate.value = ((ni->ni_rates.rs_rates[j] & IEEE80211_RATE_VAL) / 2) * 1000000;
			current_val = iwe_stream_add_value(sa->current_ev,
				current_val, end_buf, iwe, MESHW_EV_PARAM_LEN);
		}
	}
	/* remove fixed header if no rates were added */
	if ((u_int)(current_val - sa->current_ev) > MESHW_EV_LCP_LEN)
		sa->current_ev = current_val;

	memset(iwe, 0, sizeof(iwe));
	iwe->cmd = MESHW_EV_CUSTOM;
	snprintf(buf, sizeof(buf), "bcn_int=%d", ni->ni_intval);
	iwe->u.data.length = strlen(buf);
	sa->current_ev = iwe_stream_add_point(sa->current_ev, end_buf, iwe, buf);

	if (ni->ni_wpa_ie != NULL)
	{
		static const char rsn_leader[] = "rsn_ie=";
		static const char wpa_leader[] = "wpa_ie=";
			memset(iwe, 0, sizeof(iwe));
		iwe->cmd = MESHW_EV_CUSTOM;
		if (ni->ni_wpa_ie[0] == IEEE80211_ELEMID_RSN)
			iwe->u.data.length = encode_ie(buf, sizeof(buf),
				ni->ni_wpa_ie, ni->ni_wpa_ie[1]+2,
				rsn_leader, sizeof(rsn_leader)-1);
		else
			iwe->u.data.length = encode_ie(buf, sizeof(buf),
				ni->ni_wpa_ie, ni->ni_wpa_ie[1]+2,
				wpa_leader, sizeof(wpa_leader)-1);
		if (iwe->u.data.length != 0)
			sa->current_ev = iwe_stream_add_point(sa->current_ev, end_buf,
				iwe, buf);
	}
	return;
}

/*
 * Units are in db above the noise floor. That means the
 * rssi values reported in the tx/rx descriptors in the
 * driver are the SNR expressed in db.
 *
 * If you assume that the noise floor is -95, which is an
 * excellent assumption 99.5 % of the time, then you can
 * derive the absolute signal level (i.e. -95 + rssi). 
 * There are some other slight factors to take into account
 * depending on whether the rssi measurement is from 11b,
 * 11g, or 11a.   These differences are at most 2db and
 * can be documented.
 *
 * NB: various calculations are based on the orinoco/wavelan
 *     drivers for compatibility
 */
static void set_quality(struct meshw_quality *iq, u_int rssi)
{
	iq->qual = rssi;
	/* NB: max is 94 because noise is hardcoded to 161 */
	if (iq->qual > 94)
		iq->qual = 94;

	iq->noise = 161;		/* -95dBm */
	iq->level = iq->noise + iq->qual;
	iq->updated = 7;
}

void ieee80211_iw_getstats(struct ieee80211com *ic, struct meshw_statistics *is)
{
#define	NZ(x)	((x) ? (x) : 1)

	switch (ic->ic_opmode)
	{
		case IEEE80211_M_STA:
			/* use stats from associated ap */
			if (ic->ic_bss)
				set_quality(&is->qual,
					(*ic->ic_node_getrssi)(ic->ic_bss));
			else
				set_quality(&is->qual, 0);
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
		{
			struct ieee80211_node_table *nt = &ic->ic_sta;
			struct ieee80211_node* ni;
			u_int32_t rssi_samples = 0;
			u_int32_t rssi_total = 0;

			/* average stats from all nodes */
			TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			{
				rssi_samples++;
				rssi_total += (*ic->ic_node_getrssi)(ni);
			}
			set_quality(&is->qual, rssi_total / NZ(rssi_samples));
			break;
		}
		case IEEE80211_M_MONITOR:
		default:
			/* no stats */
			set_quality(&is->qual, 0);
			break;
	}
	
	is->status = ic->ic_state;
	is->discard.nwid = ic->ic_stats.is_rx_wrongbss	 + ic->ic_stats.is_rx_ssidmismatch;
	is->discard.code = ic->ic_stats.is_rx_wepfail
			 + ic->ic_stats.is_rx_decryptcrc;
	is->discard.fragment = 0;
	is->discard.retries = 0;
	is->discard.misc = 0;

	is->miss.beacon = 0;
#undef NZ
}

int ieee80211_ioctl_giwname(struct ieee80211com *ic, struct meshw_request_info *info, char *name, char *extra)
{

	/* XXX should use media status but IFM_AUTO case gets tricky */
	switch (ic->ic_curmode)
	{
		case IEEE80211_MODE_11A:
			strncpy(name, "IEEE 802.11a", MESHNAMSIZ);
			break;
		case IEEE80211_MODE_11B:
			strncpy(name, "IEEE 802.11b", MESHNAMSIZ);
			break;
		case IEEE80211_MODE_11G:
			strncpy(name, "IEEE 802.11g", MESHNAMSIZ);
			break;
		case IEEE80211_MODE_TURBO_A:
			strncpy(name, "Turbo-A", MESHNAMSIZ);
			break;
		case IEEE80211_MODE_TURBO_G:
			strncpy(name, "Turbo-G", MESHNAMSIZ);
			break;
		default:
			strncpy(name, "IEEE 802.11", MESHNAMSIZ);
			break;
	}
	return 0;
}

/*
 * Get a key index from a request.  If nothing is
 * specified in the request we use the current xmit
 * key index.  Otherwise we just convert the index
 * to be base zero.
 */
static int getiwkeyix(struct ieee80211com *ic, const struct meshw_point* erq, int *kix)
{
	int kid;

	kid = erq->flags & MESHW_ENCODE_INDEX;
	if (kid < 1 || kid > IEEE80211_WEP_NKID)
	{
		kid = ic->ic_def_txkey;
		if (kid == IEEE80211_KEYIX_NONE)
			kid = 0;
	}
	else
		--kid;

	if (0 <= kid && kid < IEEE80211_WEP_NKID)
	{
		*kix = kid;
		return 0;
	}
	else
		return EINVAL;
}

int ieee80211_ioctl_siwencode(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_point *erq, char *keybuf)
{
	int kid, error = 0;
	int wepchange = 0;
	struct ieee80211_key *k;
	/* 
	 * set key
	 *
	 * New version of iwconfig set the MESHW_ENCODE_NOKEY flag
	 * when no key is given, but older versions don't,
	 * so we have to check the length too.
	 */
	if (erq->length > 0 && !(erq->flags & MESHW_ENCODE_NOKEY))
	{
		/* set key contents, set the default transmit key and enable crypto.	*/
		error = getiwkeyix(ic, erq, &kid);
		if (error)
			return -error;
		if (erq->length > IEEE80211_KEYBUF_SIZE)
			return -EINVAL;
		/* set key contents */
		k = &ic->ic_nw_keys[kid];
		ieee80211_key_update_begin(ic);
		
		k->wk_keyix = kid;      /* NB: force fixed key id */
		if (ieee80211_crypto_newkey(ic, IEEE80211_CIPHER_WEP,
			IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV, k))
		{
			k->wk_keylen = erq->length;
			/* NB: preserve flags set by newkey */
			k->wk_flags |= IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV;
			memcpy(k->wk_key, keybuf, erq->length);
			memset(k->wk_key + erq->length, 0, IEEE80211_KEYBUF_SIZE - erq->length);
			if (!ieee80211_crypto_setkey(ic, k, ic->ic_myaddr))
				error = -EINVAL;
		}
		else
		{
			error = -EINVAL;
		}
		ieee80211_key_update_end(ic);
		
		if (error == 0)
		{
			/* set default key & enable privacy */
			ic->ic_def_txkey = kid;
			wepchange = (ic->ic_flags & IEEE80211_F_PRIVACY) == 0;
			ic->ic_flags |= IEEE80211_F_PRIVACY;
			ic->ic_flags |= IEEE80211_F_DROPUNENC;
		}
	}
	
	/* set key index only */
	else if ( (erq->flags & MESHW_ENCODE_INDEX) > 0)
	{
		/* 
		 * verify the new key has a non-zero length
		 * and change the default transmit key.
		 */
		error = getiwkeyix(ic, erq, &kid);
		if (error)
			return -error;
		
		if (ic->ic_nw_keys[kid].wk_keylen == 0)
			return -EINVAL;
		
		ic->ic_def_txkey = kid;
	}
	
	/* disable crypto & make sure we allow unencrypted packets again */
	if (erq->flags & MESHW_ENCODE_DISABLED)
	{
		wepchange = (ic->ic_flags & IEEE80211_F_PRIVACY) != 0;
		ic->ic_flags &= ~IEEE80211_F_PRIVACY;
		ic->ic_flags &= ~IEEE80211_F_DROPUNENC;
	}

	/* allow unencrypted packets */
	if (erq->flags & MESHW_ENCODE_OPEN)
	{
		ic->ic_flags &= ~IEEE80211_F_DROPUNENC;
	}

	/* dont allow unencrypted packets & make sure privacy is enabled */
	if (erq->flags & MESHW_ENCODE_RESTRICTED)
	{
		wepchange = (ic->ic_flags & IEEE80211_F_PRIVACY) == 0;
		ic->ic_flags |= IEEE80211_F_PRIVACY;
		ic->ic_flags |= IEEE80211_F_DROPUNENC;
	}
	
	if (error == 0 && IS_UP(ic->ic_dev))
	{
		/*
		 * Device is up and running; we must kick it to
		 * effect the change.  If we're enabling/disabling
		 * crypto use then we must re-initialize the device
		 * so the 802.11 state machine is reset.  Otherwise
		 * the key state should have been updated above.
		 */
		if (wepchange && ic->ic_roaming == IEEE80211_ROAMING_AUTO)
			//error = -(*ic->ic_init)(ic->ic_dev);
			error = -(*ic->ic_reset)(ic->ic_dev);
	}
	return error;
}

int ieee80211_ioctl_giwencode(struct ieee80211com *ic,
			  struct meshw_request_info *info,
			  struct meshw_point *erq, char *key)
{
	struct ieee80211_key *k;
	int error, kid;

	if (ic->ic_flags & IEEE80211_F_PRIVACY)
	{
		error = getiwkeyix(ic, erq, &kid);
		if (error != 0)
			return -error;
		k = &ic->ic_nw_keys[kid];
		/* XXX no way to return cipher/key type */

		erq->flags = kid + 1;			/* NB: base 1 */
		if (erq->length > k->wk_keylen)
			erq->length = k->wk_keylen;
		memcpy(key, k->wk_key, erq->length);
		erq->flags |= MESHW_ENCODE_ENABLED;
	}
	else
	{
		erq->length = 0;
		erq->flags = MESHW_ENCODE_DISABLED;
	}
	if (ic->ic_flags & IEEE80211_F_DROPUNENC)
		erq->flags |= MESHW_ENCODE_RESTRICTED;
	else
		erq->flags |= MESHW_ENCODE_OPEN;
	return 0;
}

#ifndef ifr_media
#define	ifr_media	ifr_ifru.ifru_ivalue
#endif

int ieee80211_ioctl_siwrate(struct ieee80211com *ic,
			struct meshw_request_info *info,
			struct meshw_param *rrq, char *extra)
{
//	struct ifreq ifr;
	struct wlan_ifmediareq ifr;
	int rate;

	if (!ic->ic_media.wlan_ifm_cur)
		return -EINVAL;
	memset(&ifr, 0, sizeof(ifr));
#if 0	
	ifr.ifr_media = ic->ic_media.wlan_ifm_cur->wlan_ifm_media &~ IFM_TMASK;
#else
	ifr.wlan_ifm_current = ic->ic_media.wlan_ifm_cur->wlan_ifm_media &~ IFM_TMASK;
#endif
	if (rrq->fixed)
	{
		/* XXX fudge checking rates */
		rate = ieee80211_rate2media(ic, 2 * rrq->value / 1000000,
				ic->ic_curmode);
		if (rate == IFM_AUTO)		/* NB: unknown rate */
			return -EINVAL;
	}
	else
		rate = IFM_AUTO;
#if 0	
	ifr.ifr_media |= IFM_SUBTYPE(rate);
#else
	ifr.wlan_ifm_current |= IFM_SUBTYPE(rate);
#endif
	return -wlan_ifmedia_ioctl(ic->ic_dev, &ifr, &ic->ic_media, SIOCSIFMEDIA);
}

int ieee80211_ioctl_giwrate(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *rrq, char *extra)
{
	struct wlan_ifmediareq imr;
	int rate;

	memset(&imr, 0, sizeof(imr));
	(*ic->ic_media.wlan_ifm_status)(ic->ic_dev, &imr);

	rrq->fixed = IFM_SUBTYPE(ic->ic_media.wlan_ifm_media) != IFM_AUTO;
	/* media status will have the current xmit rate if available */
#if 0	
	rate = ieee80211_media2rate(imr.ifm_active);
#else
	rate = ieee80211_media2rate(imr.wlan_ifm_active);
#endif
	if (rate == -1)		/* IFM_AUTO */
		rate = 0;
	rrq->value = 1000000 * (rate / 2);

	return 0;
}

int ieee80211_ioctl_siwsens(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *sens, char *extra)
{
	return 0;
}

int ieee80211_ioctl_giwsens(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *sens, char *extra)
{
	sens->value = 0;
	sens->fixed = 1;
	return 0;
}

int ieee80211_ioctl_siwrts(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *rts, char *extra)
{
	u16 val;

	if (rts->disabled)
		val = IEEE80211_RTS_MAX;
	else if (IEEE80211_RTS_MIN <= rts->value &&
	    rts->value <= IEEE80211_RTS_MAX)
		val = rts->value;
	else
		return -EINVAL;
	if (val != ic->ic_rtsthreshold)
	{
		ic->ic_rtsthreshold = val;
		if (IS_UP(ic->ic_dev))
			return -(*ic->ic_reset)(ic->ic_dev);
	}
	return 0;
}

int ieee80211_ioctl_giwrts(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *rts, char *extra)
{
	rts->value = ic->ic_rtsthreshold;
	rts->disabled = (rts->value == IEEE80211_RTS_MAX);
	rts->fixed = 1;

	return 0;
}

int ieee80211_ioctl_siwfrag(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *rts, char *extra)
{
	u16 val;

	if (rts->disabled)
		val = __constant_cpu_to_le16(2346);
	else if (rts->value < 256 || rts->value > 2346)
		return -EINVAL;
	else
		val = __cpu_to_le16(rts->value & ~0x1); /* even numbers only */
	if (val != ic->ic_fragthreshold)
	{
		ic->ic_fragthreshold = val;
		if (IS_UP(ic->ic_dev))
			return -(*ic->ic_reset)(ic->ic_dev);
	}
	return 0;
}

int ieee80211_ioctl_giwfrag(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *rts, char *extra)
{
	rts->value = ic->ic_fragthreshold;
	rts->disabled = (rts->value == 2346);
	rts->fixed = 1;

	return 0;
}

int ieee80211_ioctl_siwap(struct ieee80211com *ic,
		      struct meshw_request_info *info,
		      struct sockaddr *ap_addr, char *extra)
{
	static const u_int8_t zero_bssid[IEEE80211_ADDR_LEN];

	/* NB: should only be set when in STA mode */
        /* dh: changed line to also include ad-hoc modes */
	if (ic->ic_opmode != IEEE80211_M_STA &&
		ic->ic_opmode != IEEE80211_M_AHDEMO &&
		ic->ic_opmode != IEEE80211_M_IBSS)
		return -EINVAL;

	IEEE80211_ADDR_COPY(ic->ic_des_bssid, &ap_addr->sa_data);
	/* looks like a zero address disables */
	if (IEEE80211_ADDR_EQ(ic->ic_des_bssid, zero_bssid))
		ic->ic_flags &= ~IEEE80211_F_DESBSSID;
	else
		ic->ic_flags |= IEEE80211_F_DESBSSID;
	return IS_UP_AUTO(ic) ? -(*ic->ic_init)(ic->ic_dev) : 0;
}

int ieee80211_ioctl_giwap(struct ieee80211com *ic, struct meshw_request_info *info, struct sockaddr *ap_addr, char *extra)
{
	if (ic->ic_flags & IEEE80211_F_DESBSSID)
		IEEE80211_ADDR_COPY(&ap_addr->sa_data, ic->ic_des_bssid);
	else
		IEEE80211_ADDR_COPY(&ap_addr->sa_data, ic->ic_bss->ni_bssid);
	ap_addr->sa_family = ARPHRD_ETHER;
	return 0;
}

int ieee80211_ioctl_siwnickn(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_point *data, char *nickname)
{
	if (data->length > IEEE80211_NWID_LEN)
		return -EINVAL;

	memset(ic->ic_nickname, 0, IEEE80211_NWID_LEN);
	memcpy(ic->ic_nickname, nickname, data->length);
	ic->ic_nicknamelen = data->length;

	return 0;
}

int ieee80211_ioctl_giwnickn(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_point *data, char *nickname)
{
	if (data->length > ic->ic_nicknamelen + 1)
		data->length = ic->ic_nicknamelen + 1;
	if (data->length > 0)
	{
		memcpy(nickname, ic->ic_nickname, data->length-1);
		nickname[data->length-1] = '\0';
	}
	return 0;
}

static struct ieee80211_channel *getcurchan(struct ieee80211com *ic)
{
	switch (ic->ic_state)
	{
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			return ic->ic_des_chan;
		default:
			return ic->ic_ibss_chan;
	}
}

int ieee80211_ioctl_siwfreq(struct ieee80211com *ic,
			struct meshw_request_info *info,
			struct meshw_freq *freq, char *extra)
{
	struct ieee80211_channel *c;
	int i;
	
	if (freq->e > 1)
		return -EINVAL;
	if (freq->e == 1)
		i = ieee80211_mhz2ieee(freq->m / 100000, 0);
	else
		i = freq->m;
	if (i != 0)
	{
		if (i > IEEE80211_CHAN_MAX || isclr(ic->ic_chan_active, i))
			return -EINVAL;
		c = &ic->ic_channels[i];
		if (c == getcurchan(ic))
		{	/* no change, just return */
			ic->ic_des_chan = c;	/* XXX */
			return 0;
		}
		ic->ic_des_chan = c;
		if (c != IEEE80211_CHAN_ANYC)
			ic->ic_ibss_chan = c;
	}
	else
	{
		/*
		 * Intepret channel 0 to mean "no desired channel";
		 * otherwise there's no way to undo fixing the desired
		 * channel.
		 */
		if (ic->ic_des_chan == IEEE80211_CHAN_ANYC)
			return 0;
		ic->ic_des_chan = IEEE80211_CHAN_ANYC;
	}
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		return IS_UP(ic->ic_dev) ? -(*ic->ic_reset)(ic->ic_dev) : 0;
	else
		return IS_UP_AUTO(ic) ? -(*ic->ic_init)(ic->ic_dev) : 0;
}

int ieee80211_ioctl_giwfreq(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_freq *freq, char *extra)
{
	if (!ic->ic_ibss_chan)
		return -EINVAL;

	freq->m = ic->ic_ibss_chan->ic_freq * 100000;
	freq->e = 1;

	return 0;
}

int ieee80211_ioctl_siwessid(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_point *data, char *ssid)
{
	if (data->flags == 0)
	{		/* ANY */
		memset(ic->ic_des_essid, 0, sizeof(ic->ic_des_essid));
		ic->ic_des_esslen = 0;
	}
	else
	{
		if (data->length > sizeof(ic->ic_des_essid) )
			data->length = sizeof(ic->ic_des_essid);
		memcpy(ic->ic_des_essid, ssid, data->length);
		ic->ic_des_esslen = data->length;
		/*
		 * Deduct a trailing \0 since iwconfig passes a string
		 * length that includes this.  Unfortunately this means
		 * that specifying a string with multiple trailing \0's
		 * won't be handled correctly.  Not sure there's a good
		 * solution; the API is botched (the length should be
		 * exactly those bytes that are meaningful and not include
		 * extraneous stuff).
		 */
		if (ic->ic_des_esslen > 0 &&
		    ic->ic_des_essid[ic->ic_des_esslen-1] == '\0')
			ic->ic_des_esslen--;
	}
	return IS_UP_AUTO(ic) ? -(*ic->ic_init)(ic->ic_dev) : 0;
}

int ieee80211_ioctl_giwessid(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_point *data, char *essid)
{
	data->flags = 1;		/* active */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
	{
		if (data->length > ic->ic_des_esslen)
			data->length = ic->ic_des_esslen;
		memcpy(essid, ic->ic_des_essid, data->length);
	}
	else
	{
		if (ic->ic_des_esslen == 0)
		{
			if (data->length > ic->ic_bss->ni_esslen)
				data->length = ic->ic_bss->ni_esslen;
			memcpy(essid, ic->ic_bss->ni_essid, data->length);
		}
		else
		{
			if (data->length > ic->ic_des_esslen)
				data->length = ic->ic_des_esslen;
			memcpy(essid, ic->ic_des_essid, data->length);
		}
	}
	return 0;
}

int ieee80211_ioctl_giwrange(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_point *data, char *extra)
{
	struct ieee80211_node *ni = ic->ic_bss;
	struct meshw_range *range = (struct meshw_range *) extra;
	struct ieee80211_rateset *rs;
	int i, r;

	data->length = sizeof(struct meshw_range);
	memset(range, 0, sizeof(struct meshw_range));

	/* TODO: could fill num_txpower and txpower array with
	 * something; however, there are 128 different values.. */

	range->txpower_capa = MESHW_TXPOW_DBM;

	if (ic->ic_opmode == IEEE80211_M_STA ||ic->ic_opmode == IEEE80211_M_IBSS)
	{
		range->min_pmp = 1 * 1024;
		range->max_pmp = 65535 * 1024;
		range->min_pmt = 1 * 1024;
		range->max_pmt = 1000 * 1024;
		range->pmp_flags = MESHW_POWER_PERIOD;
		range->pmt_flags = MESHW_POWER_TIMEOUT;
		range->pm_capa = MESHW_POWER_PERIOD | MESHW_POWER_TIMEOUT |
			MESHW_POWER_UNICAST_R | MESHW_POWER_ALL_R;
	}

	range->we_version_compiled = MESHW_MAIN_VERSION;
	range->we_version_source = MESHW_MINOR_VERSION;

	range->retry_capa = MESHW_RETRY_LIMIT;
	range->retry_flags = MESHW_RETRY_LIMIT;
	range->min_retry = 0;
	range->max_retry = 255;

	range->num_channels = IEEE80211_CHAN_MAX;	/* XXX */

	range->num_frequency = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
	{
		if (isset(ic->ic_chan_active, i))
		{
			range->freq[range->num_frequency].i = i;
			range->freq[range->num_frequency].m =	ic->ic_channels[i].ic_freq * 100000;
			range->freq[range->num_frequency].e = 1;
			if (++range->num_frequency == MESHW_MAX_FREQUENCIES)
				break;
		}
	}
	
	/* Max quality is max field value minus noise floor */
	range->max_qual.qual  = 0xff - 161;

	/*
	 * In order to use dBm measurements, 'level' must be lower
	 * than any possible measurement (see meshwu_print_stats() in
	 * wireless tools).  It's unclear how this is meant to be
	 * done, but setting zero in these values forces dBm and
	 * the actual numbers are not used.
	 */
	range->max_qual.level = 0;
	range->max_qual.noise = 0;

	range->sensitivity = 3;

	range->max_encoding_tokens = IEEE80211_WEP_NKID;
	/* XXX query driver to find out supported key sizes */
	range->num_encoding_sizes = 3;
	range->encoding_size[0] = 5;		/* 40-bit */
	range->encoding_size[1] = 13;		/* 104-bit */
	range->encoding_size[2] = 16;		/* 128-bit */

	/* XXX this only works for station mode */
	rs = &ni->ni_rates;
	range->num_bitrates = rs->rs_nrates;
	if (range->num_bitrates > MESHW_MAX_BITRATES)
		range->num_bitrates = MESHW_MAX_BITRATES;
	for (i = 0; i < range->num_bitrates; i++)
	{
		r = rs->rs_rates[i] & IEEE80211_RATE_VAL;
		range->bitrate[i] = (r * 1000000) / 2;
	}

	/* estimated maximum TCP throughput values (bps) */
	range->throughput = 5500000;

	range->min_rts = 0;
	range->max_rts = 2347;
	range->min_frag = 256;
	range->max_frag = 2346;

	return 0;
}

int ieee80211_ioctl_siwmode(struct ieee80211com *ic, struct meshw_request_info *info, __u32 *mode, char *extra)
{
//	struct ifreq ifr;
	struct wlan_ifmediareq ifr;
	if (!ic->ic_media.wlan_ifm_cur)
		return -EINVAL;
	memset(&ifr, 0, sizeof(ifr));
	/* NB: remove any fixed-rate at the same time */
#if 0	
	ifr.ifr_media = ic->ic_media.ifm_cur->ifm_media &~ (IFM_OMASK | IFM_TMASK);
#else
	ifr.wlan_ifm_current = ic->ic_media.wlan_ifm_cur->wlan_ifm_media &~ (IFM_OMASK | IFM_TMASK);
#endif

	MESH_WARN_INFO("MAC mode is %d\n", *mode);
	switch (*mode)
	{
		case MESHW_MODE_INFRA:
			/* NB: this is the default */
			ic->ic_des_chan = IEEE80211_CHAN_ANYC;
			break;
		case MESHW_MODE_ADHOC:
		{
#if 0			
			ifr.ifr_media |= IFM_IEEE80211_ADHOC;
#else
			ifr.wlan_ifm_current |= IFM_IEEE80211_ADHOC;
#endif
			break;
		}
		case MESHW_MODE_MASTER:
#if 0			
			ifr.ifr_media |= IFM_IEEE80211_HOSTAP;
#else
			ifr.wlan_ifm_current |= IFM_IEEE80211_HOSTAP;
#endif
			break;

		case MESHW_MODE_MONITOR:
#if 0			
			ifr.ifr_media |= IFM_IEEE80211_MONITOR;
#else
			ifr.wlan_ifm_current |= IFM_IEEE80211_MONITOR;
#endif
			break;

		default:
			return -EINVAL;
	}
	
	if (ic->ic_curmode == IEEE80211_MODE_TURBO_G ||ic->ic_curmode == IEEE80211_MODE_TURBO_A)
	{
#if 0	
		ifr.ifr_media |= IFM_IEEE80211_TURBO;
#else
		ifr.wlan_ifm_current |= IFM_IEEE80211_TURBO;
#endif
	}

	return -wlan_ifmedia_ioctl(ic->ic_dev, &ifr, &ic->ic_media, SIOCSIFMEDIA);
}

int ieee80211_ioctl_giwmode(struct ieee80211com *ic, struct meshw_request_info *info, __u32 *mode, char *extra)
{
	struct wlan_ifmediareq imr;

	memset(&imr, 0, sizeof(imr));
	(*ic->ic_media.wlan_ifm_status)(ic->ic_dev, &imr);

	if (imr.wlan_ifm_active & IFM_IEEE80211_HOSTAP)
		*mode = MESHW_MODE_MASTER;
	else if (imr.wlan_ifm_active & IFM_IEEE80211_MONITOR)
		*mode = MESHW_MODE_MONITOR;
	else if (imr.wlan_ifm_active & IFM_IEEE80211_ADHOC)
		*mode = MESHW_MODE_ADHOC;
	else
		*mode = MESHW_MODE_INFRA;
	return 0;
}

int ieee80211_ioctl_siwpower(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *wrq, char *extra)
{

	if (wrq->disabled)
	{
		if (ic->ic_flags & IEEE80211_F_PMGTON)
		{
			ic->ic_flags &= ~IEEE80211_F_PMGTON;
			goto done;
		}
		return 0;
	}

	if ((ic->ic_caps & IEEE80211_C_PMGT) == 0)
		return -EOPNOTSUPP;
	switch (wrq->flags & MESHW_POWER_MODE)
	{
		case MESHW_POWER_UNICAST_R:
		case MESHW_POWER_ALL_R:
		case MESHW_POWER_ON:
			ic->ic_flags |= IEEE80211_F_PMGTON;
			break;
		default:
			return -EINVAL;
	}

	if (wrq->flags & MESHW_POWER_TIMEOUT)
	{
		ic->ic_holdover = wrq->value / 1024;
		ic->ic_flags |= IEEE80211_F_PMGTON;
	}

	if (wrq->flags & MESHW_POWER_PERIOD)
	{
		ic->ic_lintval = wrq->value / 1024;
		ic->ic_flags |= IEEE80211_F_PMGTON;
	}
done:
	return IS_UP(ic->ic_dev) ? -(*ic->ic_reset)(ic->ic_dev) : 0;
}

int ieee80211_ioctl_giwpower(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *rrq, char *extra)
{

	rrq->disabled = (ic->ic_flags & IEEE80211_F_PMGTON) == 0;
	if (!rrq->disabled)
	{
		switch (rrq->flags & MESHW_POWER_TYPE)
		{
			case MESHW_POWER_TIMEOUT:
				rrq->flags = MESHW_POWER_TIMEOUT;
				rrq->value = ic->ic_holdover * 1024;
				break;
			case MESHW_POWER_PERIOD:
				rrq->flags = MESHW_POWER_PERIOD;
				rrq->value = ic->ic_lintval * 1024;
				break;
		}
		rrq->flags |= MESHW_POWER_ALL_R;
	}
	return 0;
}

int ieee80211_ioctl_siwretry(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *rrq, char *extra)
{

	if (rrq->disabled)
	{
		if (ic->ic_flags & IEEE80211_F_SWRETRY)
		{
			ic->ic_flags &= ~IEEE80211_F_SWRETRY;
			goto done;
		}
		return 0;
	}

	if ((ic->ic_caps & IEEE80211_C_SWRETRY) == 0)
		return -EOPNOTSUPP;
	if (rrq->flags == MESHW_RETRY_LIMIT)
	{
		if (rrq->value >= 0)
		{
			ic->ic_txmin = rrq->value;
			ic->ic_txmax = rrq->value;	/* XXX */
			ic->ic_txlifetime = 0;		/* XXX */
			ic->ic_flags |= IEEE80211_F_SWRETRY;
		}
		else
		{
			ic->ic_flags &= ~IEEE80211_F_SWRETRY;
		}
		return 0;
	}
done:
	return IS_UP(ic->ic_dev) ? -(*ic->ic_reset)(ic->ic_dev) : 0;
}

int ieee80211_ioctl_giwretry(struct ieee80211com *ic,	 struct meshw_request_info *info, struct meshw_param *rrq, char *extra)
{
	rrq->disabled = (ic->ic_flags & IEEE80211_F_SWRETRY) == 0;

	if (!rrq->disabled)
	{
		switch (rrq->flags & MESHW_RETRY_TYPE)
		{
			case MESHW_RETRY_LIFETIME:
				rrq->flags = MESHW_RETRY_LIFETIME;
				rrq->value = ic->ic_txlifetime * 1024;
				break;
			case MESHW_RETRY_LIMIT:
				rrq->flags = MESHW_RETRY_LIMIT;
				switch (rrq->flags & MESHW_RETRY_MODIFIER)
				{
					case MESHW_RETRY_MIN:
						rrq->flags |= MESHW_RETRY_MAX;
						rrq->value = ic->ic_txmin;
						break;
					case MESHW_RETRY_MAX:
						rrq->flags |= MESHW_RETRY_MAX;
						rrq->value = ic->ic_txmax;
						break;
				}
				break;
		}
	}
	return 0;
}

int ieee80211_ioctl_siwtxpow(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *rrq, char *extra)
{
	int fixed, disabled;

        fixed = (ic->ic_flags & IEEE80211_F_TXPOW_FIXED);
        disabled = (fixed && ic->ic_bss->ni_txpower == 0);

	if (rrq->fixed)
	{
		if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
			return -EOPNOTSUPP;
                if (!(IEEE80211_TXPOWER_MIN < rrq->value &&
                      rrq->value < IEEE80211_TXPOWER_MAX))
                        return -EINVAL;
		ic->ic_txpowlimit = rrq->value;
		ic->ic_flags |= IEEE80211_F_TXPOW_FIXED;
	}
	else
	{
		if (!fixed)		/* no change */
			return 0;
		ic->ic_flags &= ~IEEE80211_F_TXPOW_FIXED;
	}
	return IS_UP(ic->ic_dev) ? -(*ic->ic_reset)(ic->ic_dev) : 0;
}

int ieee80211_ioctl_giwtxpow(struct ieee80211com *ic, struct meshw_request_info *info, struct meshw_param *rrq, char *extra)
{

	rrq->value = ic->ic_txpowlimit;
        if (!(IEEE80211_TXPOWER_MIN < rrq->value &&
              rrq->value < IEEE80211_TXPOWER_MAX))
    		rrq->value = IEEE80211_TXPOWER_MAX;
	rrq->fixed = (ic->ic_flags & IEEE80211_F_TXPOW_FIXED) != 0;
	rrq->disabled = (rrq->fixed && rrq->value == 0);
	rrq->flags = MESHW_TXPOW_MWATT;
	return 0;
}

/* is obsolete iwlist ap */
int ieee80211_ioctl_iwaplist(struct ieee80211com *ic,	struct meshw_request_info *info, struct meshw_point *data, char *extra)
{
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;
	struct sockaddr addr[MESHW_MAX_AP];
	struct meshw_quality qual[MESHW_MAX_AP];
	int i;

	i = 0;
	
	if (ic->ic_opmode == IEEE80211_M_STA)
		nt = &ic->ic_scan;
	else
		nt = &ic->ic_sta;
	
	/*
	 * TODO: use ieee80211_iterate_nodes(&ic->ic_scan,func,&args)
	 * also create a func like bsd does: wi_read_ap_result()
	 */
	
	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
	{
		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS)
		{
			if (ni == ic->ic_bss) /* don't include BSS node */
				continue;
			IEEE80211_ADDR_COPY(addr[i].sa_data, ni->ni_macaddr);
		}
		else
			IEEE80211_ADDR_COPY(addr[i].sa_data, ni->ni_bssid);

		addr[i].sa_family = ARPHRD_ETHER;
		set_quality(&qual[i], (*ic->ic_node_getrssi)(ni));
		if (++i >= MESHW_MAX_AP)
			break;
	}
	IEEE80211_NODE_UNLOCK(nt);
	data->length = i;
	memcpy(extra, &addr, i*sizeof(addr[0]));
	data->flags = 1;		/* signal quality present (sort of) */
	memcpy(extra + i*sizeof(addr[0]), &qual, i*sizeof(qual[i]));

	return 0;

}

#ifdef MESHW_IO_R_SCAN
int ieee80211_ioctl_siwscan(struct ieee80211com *ic, 	struct meshw_request_info *info, struct meshw_point *data, char *extra)
{
	u_char *chanlist = ic->ic_chan_active;
	//u_char *chanlist = ic->ic_chan_avail; /* TODO: check */
	int i;

	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		return -EINVAL;

	/*
	 * XXX don't permit a scan to be started unless we
	 * know the device is ready.  For the moment this means
	 * the device is marked up as this is the required to
	 * initialize the hardware.  It would be better to permit
	 * scanning prior to being up but that'll require some
	 * changes to the infrastructure.
	 */
	if (!IS_UP(ic->ic_dev))
		return -ENETDOWN;
	if (ic->ic_state == IEEE80211_S_SCAN && (ic->ic_flags & (IEEE80211_F_SCAN|IEEE80211_F_ASCAN)))
	{
		return -EINPROGRESS;
	}

	if (ic->ic_ibss_chan == NULL ||
	    isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_ibss_chan)))
	{
		for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
			if (isset(chanlist, i))
			{
				ic->ic_ibss_chan = &ic->ic_channels[i];
				goto found;
			}
		return -EINVAL;			/* no active channels */
found:
		;
	}
	
	if (ic->ic_bss->ni_chan == IEEE80211_CHAN_ANYC ||
	    isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan)))
		ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	memcpy(ic->ic_chan_active, chanlist, sizeof(ic->ic_chan_active));
 	/*
	 * We force the state to INIT before calling ieee80211_new_state
	 * to get ieee80211_begin_scan called.  We really want to scan w/o
	 * altering the current state but that's not possible right now.
	 */
	/* XXX handle proberequest case */
	if (ic->ic_state != IEEE80211_S_INIT)
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	ieee80211_new_state(ic, IEEE80211_S_SCAN, 1);
	return 0;
}

/*
 * Encode a WPA or RSN information element as a custom
 * element using the hostap format.
 */
static u_int encode_ie(void *buf, size_t bufsize,
	const u_int8_t *ie, size_t ielen,
	const char *leader, size_t leader_len)
{
	u_int8_t *p;
	u_int i;

	if (bufsize < leader_len)
		return 0;
	p = buf;
	memcpy(p, leader, leader_len);
	bufsize -= leader_len;
	p += leader_len;
	for (i = 0; i < ielen && bufsize > 2; i++)
		p += sprintf(p, "%02x", ie[i]);
	return (i == ielen ? p - (u_int8_t *)buf : 0);
}

int ieee80211_ioctl_giwscan(struct ieee80211com *ic,	struct meshw_request_info *info,struct meshw_point *data, char *extra)
{
	char *current_ev = extra;
	struct meshw_event iwe;
	struct read_ap_args args;

	/* XXX use generation number and always return current results */
	if ((ic->ic_flags & (IEEE80211_F_SCAN|IEEE80211_F_ASCAN)) &&
	    !(ic->ic_flags & IEEE80211_F_SSCAN))
	{
		/*
		 * Still scanning, indicate the caller should try again.
		 */
		return -EAGAIN;
	}
	/*
	 * Do two passes to insure WPA/non-WPA scan candidates
	 * are sorted to the front.  This is a hack to deal with
	 * the wireless extensions capping scan results at
	 * MESHW_SCAN_MAX_DATA bytes.  In densely populated environments
	 * it's easy to overflow this buffer (especially with WPA/RSN
	 * information elements).  Note this sorting hack does not
	 * guarantee we won't overflow anyway.
	 */
	args.i = 0;
	args.iwe = &iwe;
	args.start = extra;
	args.current_ev = current_ev;
	args.mode = ic->ic_flags & IEEE80211_F_WPA;
	ieee80211_iterate_nodes(&ic->ic_scan, read_ap_result, &args);
	args.mode = args.mode ? 0 : IEEE80211_F_WPA;
	ieee80211_iterate_nodes(&ic->ic_scan, read_ap_result, &args);

	data->length = args.current_ev - extra;
	return 0;
}
#endif /* MESHW_IO_R_SCAN */


#define	MESHW_PRIV_TYPE_OPTIE	MESHW_PRIV_TYPE_BYTE | IEEE80211_MAX_OPT_IE
#define	MESHW_PRIV_TYPE_KEY \
	MESHW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_key)
#define	MESHW_PRIV_TYPE_DELKEY \
	MESHW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_del_key)
#define	MESHW_PRIV_TYPE_MLME \
	MESHW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_mlme)
#define	MESHW_PRIV_TYPE_CHANLIST \
	MESHW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_chanlist)

static const struct meshw_priv_args ieee80211_priv_args[] = {
	/* NB: setoptie & getoptie are !MESHW_PRIV_SIZE_FIXED */
	{ IEEE80211_IOCTL_SETOPTIE,
	  MESHW_PRIV_TYPE_OPTIE, 0,			"setoptie" },
	{ IEEE80211_IOCTL_GETOPTIE,
	  0, MESHW_PRIV_TYPE_OPTIE,			"getoptie" },
	{ IEEE80211_IOCTL_SETKEY,
	  MESHW_PRIV_TYPE_KEY | MESHW_PRIV_SIZE_FIXED, 0,	"setkey" },
	{ IEEE80211_IOCTL_DELKEY,
	  MESHW_PRIV_TYPE_DELKEY | MESHW_PRIV_SIZE_FIXED, 0,	"delkey" },
	{ IEEE80211_IOCTL_SETMLME,
	  MESHW_PRIV_TYPE_MLME | MESHW_PRIV_SIZE_FIXED, 0,	"setmlme" },
	{ IEEE80211_IOCTL_ADDMAC,
	  MESHW_PRIV_TYPE_ADDR | MESHW_PRIV_SIZE_FIXED | 1, 0,"addmac" },
	{ IEEE80211_IOCTL_DELMAC,
	  MESHW_PRIV_TYPE_ADDR | MESHW_PRIV_SIZE_FIXED | 1, 0,"delmac" },
	{ IEEE80211_IOCTL_CHANLIST,
	  MESHW_PRIV_TYPE_CHANLIST | MESHW_PRIV_SIZE_FIXED, 0,"chanlist" },
#ifdef HAS_VMAC
	{ IEEE80211_IOCTL_SETREG,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 2, 0, "setreg" },
	{ IEEE80211_IOCTL_GETREG,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "getreg" },
#endif

	{ IEEE80211_IOCTL_SETPARAM,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 2, 0, "setparam" },
	/*
	 * These depends on sub-ioctl support which added in version 12.
	 */
	{ IEEE80211_IOCTL_GETPARAM,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1,	"getparam" },

	/* sub-ioctl handlers */
	{ IEEE80211_IOCTL_SETPARAM,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "" },
	{ IEEE80211_IOCTL_GETPARAM,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "" },

	/* sub-ioctl definitions */
	{ IEEE80211_PARAM_TURBO,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "turbo" },
	{ IEEE80211_PARAM_TURBO,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_turbo" },
	{ IEEE80211_PARAM_MODE,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "mode" },
	{ IEEE80211_PARAM_MODE,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_mode" },
	{ IEEE80211_PARAM_AUTHMODE,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "authmode" },
	{ IEEE80211_PARAM_AUTHMODE,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_authmode" },
	{ IEEE80211_PARAM_PROTMODE,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "protmode" },
	{ IEEE80211_PARAM_PROTMODE,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_protmode" },
	{ IEEE80211_PARAM_MCASTCIPHER,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "mcastcipher" },
	{ IEEE80211_PARAM_MCASTCIPHER,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_mcastcipher" },
	{ IEEE80211_PARAM_MCASTKEYLEN,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "mcastkeylen" },
	{ IEEE80211_PARAM_MCASTKEYLEN,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_mcastkeylen" },
	{ IEEE80211_PARAM_UCASTCIPHERS,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "ucastciphers" },
	{ IEEE80211_PARAM_UCASTCIPHERS,
	/*
	 * NB: can't use "get_ucastciphers" 'cuz iwpriv command names
	 *     must be <IFNAMESIZ which is 16.
	 */
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_uciphers" },
	{ IEEE80211_PARAM_UCASTCIPHER,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "ucastcipher" },
	{ IEEE80211_PARAM_UCASTCIPHER,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_ucastcipher" },
	{ IEEE80211_PARAM_UCASTKEYLEN,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "ucastkeylen" },
	{ IEEE80211_PARAM_UCASTKEYLEN,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_ucastkeylen" },
	{ IEEE80211_PARAM_KEYMGTALGS,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "keymgtalgs" },
	{ IEEE80211_PARAM_KEYMGTALGS,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_keymgtalgs" },
	{ IEEE80211_PARAM_RSNCAPS,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "rsncaps" },
	{ IEEE80211_PARAM_RSNCAPS,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_rsncaps" },
	{ IEEE80211_PARAM_ROAMING,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "roaming" },
	{ IEEE80211_PARAM_ROAMING,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_roaming" },
	{ IEEE80211_PARAM_PRIVACY,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "privacy" },
	{ IEEE80211_PARAM_PRIVACY,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_privacy" },
	{ IEEE80211_PARAM_COUNTERMEASURES,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "countermeasures" },
	{ IEEE80211_PARAM_COUNTERMEASURES,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_countermeas" },
	{ IEEE80211_PARAM_DROPUNENCRYPTED,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "dropunencrypted" },
	{ IEEE80211_PARAM_DROPUNENCRYPTED,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_dropunencry" },
	{ IEEE80211_PARAM_WPA,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "wpa" },
	{ IEEE80211_PARAM_WPA,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_wpa" },
	{ IEEE80211_PARAM_DRIVER_CAPS,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "driver_caps" },
	{ IEEE80211_PARAM_DRIVER_CAPS,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_driver_caps" },
	{ IEEE80211_PARAM_MACCMD,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "maccmd" },
	{ IEEE80211_PARAM_WME,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "wme" },
	{ IEEE80211_PARAM_WME,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_wme" },
	{ IEEE80211_PARAM_HIDESSID,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "hide_ssid" },
	{ IEEE80211_PARAM_HIDESSID,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_hide_ssid" },
	{ IEEE80211_PARAM_APBRIDGE,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "ap_bridge" },
	{ IEEE80211_PARAM_APBRIDGE,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_ap_bridge" },
	{ IEEE80211_PARAM_INACT,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "inact" },
	{ IEEE80211_PARAM_INACT,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_inact" },
	{ IEEE80211_PARAM_INACT_AUTH,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "inact_auth" },
	{ IEEE80211_PARAM_INACT_AUTH,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_inact_auth" },
	{ IEEE80211_PARAM_INACT_INIT,
	  MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, 0, "inact_init" },
	{ IEEE80211_PARAM_INACT_INIT,
	  0, MESHW_PRIV_TYPE_INT | MESHW_PRIV_SIZE_FIXED | 1, "get_inact_init" },

};

void ieee80211_ioctl_iwsetup(struct meshw_handler_def *def)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	def->private_args = (struct meshw_priv_args *) ieee80211_priv_args;
	def->num_private_args = N(ieee80211_priv_args);
#undef N
}
EXPORT_SYMBOL(ieee80211_ioctl_iwsetup);

