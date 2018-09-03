/*
* $Id: wlan_media.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#include "_phy.h"

//#include <ieee80211_dot1x.h>

const char *ieee80211_phymode_name[] =
{
	"auto",		/* IEEE80211_MODE_AUTO */
	"11a",		/* IEEE80211_MODE_11A */
	"11b",		/* IEEE80211_MODE_11B */
	"11g",		/* IEEE80211_MODE_11G */
	"FH",		/* IEEE80211_MODE_FH */
	"turboA",		/* IEEE80211_MODE_TURBO_A */
	"turboG",		/* IEEE80211_MODE_TURBO_G */
};

/* Convert MHz frequency to IEEE channel number (integer). */
u_int ieee80211_mhz2ieee(u_int freq, u_int flags)
{
	if (flags & IEEE80211_CHAN_2GHZ)
	{/* 2GHz band */
		if (freq == 2484)
			return 14;
		if (freq < 2484)
			return (freq - 2407) / 5;
		else
			return 15 + ((freq - 2512) / 20);
	}
	else if (flags & IEEE80211_CHAN_5GHZ)
	{/* 5Ghz band */
		return (freq - 5000) / 5;
	}
	else
	{/* either, guess */
		if (freq == 2484)
			return 14;
		if (freq < 2484)
			return (freq - 2407) / 5;
		if (freq < 5000)
			return 15 + ((freq - 2512) / 20);
		return (freq - 5000) / 5;
	}
}

/* Convert channel to IEEE channel number */
u_int ieee80211_chan2ieee(struct ieee80211com *ic, struct ieee80211_channel *c)
{
	if (ic->ic_channels <= c && c <= &ic->ic_channels[IEEE80211_CHAN_MAX])
		return c - ic->ic_channels;
	else if (c == IEEE80211_CHAN_ANYC)
		return IEEE80211_CHAN_ANY;
	else if (c != NULL)
	{
		mdev_printf(ic->ic_dev, "invalid channel freq %u flags %x\n", c->ic_freq, c->ic_flags);
		return 0;		/* XXX */
	}
	else
	{
		mdev_printf(ic->ic_dev, "invalid channel (NULL)\n");
		return 0;		/* XXX */
	}
}

/* Convert IEEE channel number to MHz frequency. */
u_int ieee80211_ieee2mhz(u_int chan, u_int flags)
{
	if (flags & IEEE80211_CHAN_2GHZ)
	{/* 2GHz band */
		if (chan == 14)
			return 2484;
		if (chan < 14)
			return 2407 + chan*5;
		else
			return 2512 + ((chan-15)*20);
	}
	else if (flags & IEEE80211_CHAN_5GHZ)
	{/* 5Ghz band */
		return 5000 + (chan*5);
	}
	else
	{/* either, guess */
		if (chan == 14)
			return 2484;
		if (chan < 14)			/* 0-13 */
			return 2407 + chan*5;
		if (chan < 27)			/* 15-26 */
			return 2512 + ((chan-15)*20);
		return 5000 + (chan*5);
	}
}


static int __findrate(struct ieee80211com *ic, enum ieee80211_phymode mode, int rate)
{
#define	IEEERATE(_ic,_m,_i) \
	((_ic)->ic_sup_rates[_m].rs_rates[_i] & IEEE80211_RATE_VAL)
	
	int i, nrates = ic->ic_sup_rates[mode].rs_nrates;
	for (i = 0; i < nrates; i++)
		if (IEEERATE(ic, mode, i) == rate)
			return i;
	return -1;
#undef IEEERATE
}

/* Handle a media change request. */
int ieee80211_media_change(MESH_DEVICE *dev)
{
	struct ieee80211com *ic;
	struct wlan_ifmedia_entry *ime;
	enum ieee80211_opmode newopmode;
	enum ieee80211_phymode newphymode;
	int i, j, newrate, error = 0;

	ic = _ieee80211_find_instance(dev);
	if (!ic)
	{
		mdev_printf(dev, "%s: no 802.11 instance!\n", __func__);
		return EINVAL;
	}
	ime = ic->ic_media.wlan_ifm_cur;

	ktrace;
	/* First, identify the phy mode. */
	switch (IFM_MODE(ime->wlan_ifm_media))
	{
		case IFM_IEEE80211_11A:
			newphymode = IEEE80211_MODE_11A;
			break;
		case IFM_IEEE80211_11B:
			newphymode = IEEE80211_MODE_11B;
			break;
		case IFM_IEEE80211_11G:
			newphymode = IEEE80211_MODE_11G;
			break;
		case IFM_IEEE80211_FH:
			newphymode = IEEE80211_MODE_FH;
			break;
		case IFM_AUTO:
			newphymode = IEEE80211_MODE_AUTO;
			break;
		default:
			return EINVAL;
	}
	/*
	 * Turbo mode is an ``option''.
	 * XXX does not apply to AUTO
	 */
	if (ime->wlan_ifm_media & IFM_IEEE80211_TURBO)
	{
		if (newphymode == IEEE80211_MODE_11A)
			newphymode = IEEE80211_MODE_TURBO_A;
		else if (newphymode == IEEE80211_MODE_11G)
			newphymode = IEEE80211_MODE_TURBO_G;
		else
			return EINVAL;
	}
	/*
	 * Validate requested mode is available.
	 */
	if ((ic->ic_modecaps & (1<<newphymode)) == 0)
		return EINVAL;

	/*
	 * Next, the fixed/variable rate.
	 */
	i = -1;
	if (IFM_SUBTYPE(ime->wlan_ifm_media) != IFM_AUTO)
	{
		/*
		 * Convert media subtype to rate.
		 */
		newrate = ieee80211_media2rate(ime->wlan_ifm_media);
		if (newrate == 0)
			return EINVAL;
		/*
		 * Check the rate table for the specified/current phy.
		 */
		if (newphymode == IEEE80211_MODE_AUTO)
		{
			/*
			 * In autoselect mode search for the rate.
			 */
			for (j = IEEE80211_MODE_11A; j < IEEE80211_MODE_MAX; j++)
			{
				if ((ic->ic_modecaps & (1<<j)) == 0)
					continue;
				i = __findrate(ic, j, newrate);
				if (i != -1)
				{
					/* lock mode too */
					newphymode = j;
					break;
				}
			}
		}
		else
		{
			i = __findrate(ic, newphymode, newrate);
		}

		if (i == -1)			/* mode/rate mismatch */
			return EINVAL;
	}
	/* NB: defer rate setting to later */

ktrace;
	/*
	 * Deduce new operating mode but don't install it just yet.
	 */
	if ((ime->wlan_ifm_media & (IFM_IEEE80211_ADHOC|IFM_FLAG0)) == (IFM_IEEE80211_ADHOC|IFM_FLAG0))
		newopmode = IEEE80211_M_AHDEMO;
	else if (ime->wlan_ifm_media & IFM_IEEE80211_HOSTAP)
		newopmode = IEEE80211_M_HOSTAP;
	else if (ime->wlan_ifm_media & IFM_IEEE80211_ADHOC)
		newopmode = IEEE80211_M_IBSS;
	else if (ime->wlan_ifm_media & IFM_IEEE80211_MONITOR)
		newopmode = IEEE80211_M_MONITOR;
	else
		newopmode = IEEE80211_M_STA;

ktrace;
	/*
	 * Autoselect doesn't make sense when operating as an AP.
	 * If no phy mode has been selected, pick one and lock it
	 * down so rate tables can be used in forming beacon frames
	 * and the like.
	 */
	if (newopmode == IEEE80211_M_HOSTAP && newphymode == IEEE80211_MODE_AUTO)
	{
		for (j = IEEE80211_MODE_11A; j < IEEE80211_MODE_MAX; j++)
		{
			if (ic->ic_modecaps & (1<<j))
			{
				newphymode = j;
				break;
			}
		}	
	}

ktrace;
	/* Handle phy mode change.	 */
	if (ic->ic_curmode != newphymode)
	{/* change phy mode */
		error = ieee80211_setmode(ic, newphymode);
		if (error != 0)
			return error;
		error = ENETRESET;
	}

	/*
	 * Committed to changes, install the rate setting.
	 */
	if (ic->ic_fixed_rate != i)
	{
		ic->ic_fixed_rate = i;			/* set fixed tx rate */
		error = ENETRESET;
	}

	/* Handle operating mode change. */
	if (ic->ic_opmode != newopmode)
	{
		MESH_WARN_INFO("OP Mode changed as %d from %d\n\n\n",newopmode, ic->ic_opmode );
		ic->ic_opmode = newopmode;
		switch (newopmode)
		{
			case IEEE80211_M_AHDEMO:
			case IEEE80211_M_HOSTAP:
			case IEEE80211_M_STA:
			case IEEE80211_M_MONITOR:
				ic->ic_flags &= ~IEEE80211_F_IBSSON;
				break;
			case IEEE80211_M_IBSS:
				ic->ic_flags |= IEEE80211_F_IBSSON;
				break;
		}
		/*
		 * Yech, slot time may change depending on the
		 * operating mode so reset it to be sure everything
		 * is setup appropriately.
		 */
		ieee80211_reset_erp(ic);
		ieee80211_wme_initparams(ic);	/* after opmode change */
		error = ENETRESET;
	}
	
#ifdef notdef
	if (error == 0)
		ifp->if_baudrate = ifmedia_baudrate(ime->ifm_media);
#endif
	return error;
}

void ieee80211_media_status(MESH_DEVICE *dev, struct wlan_ifmediareq *imr)
{
	struct ieee80211com *ic;
	struct ieee80211_rateset *rs;

	ic = _ieee80211_find_instance(dev);
	if (!ic)
	{
		mdev_printf(dev, "%s: no 802.11 instance!\n", __func__);
		return;
	}
	
	imr->wlan_ifm_status = IFM_AVALID;
	imr->wlan_ifm_active = IFM_IEEE80211;
	
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->wlan_ifm_status |= IFM_ACTIVE;
	/*
	 * Calculate a current rate if possible.
	 */
	if (ic->ic_fixed_rate != -1)
	{
		/*
		 * A fixed rate is set, report that.
		 */
		rs = &ic->ic_sup_rates[ic->ic_curmode];
		imr->wlan_ifm_active |= ieee80211_rate2media(ic, rs->rs_rates[ic->ic_fixed_rate], ic->ic_curmode);
	}
	else if (ic->ic_opmode == IEEE80211_M_STA)
	{
		/*
		 * In station mode report the current transmit rate.
		 */
		rs = &ic->ic_bss->ni_rates;
		imr->wlan_ifm_active |= ieee80211_rate2media(ic, rs->rs_rates[ic->ic_bss->ni_txrate], ic->ic_curmode);
	}
	else
		imr->wlan_ifm_active |= IFM_AUTO;

	switch (ic->ic_opmode)
	{
		case IEEE80211_M_STA:
			break;
		case IEEE80211_M_IBSS:
			imr->wlan_ifm_active |= IFM_IEEE80211_ADHOC;
			break;
		case IEEE80211_M_AHDEMO:
			/* should not come here */
			break;
		case IEEE80211_M_HOSTAP:
			imr->wlan_ifm_active |= IFM_IEEE80211_HOSTAP;
			break;
		case IEEE80211_M_MONITOR:
			imr->wlan_ifm_active |= IFM_IEEE80211_MONITOR;
			break;
	}

	switch (ic->ic_curmode)
	{
		case IEEE80211_MODE_11A:
			imr->wlan_ifm_active |= IFM_IEEE80211_11A;
			break;
		case IEEE80211_MODE_11B:
			imr->wlan_ifm_active |= IFM_IEEE80211_11B;
			break;
		case IEEE80211_MODE_11G:
			imr->wlan_ifm_active |= IFM_IEEE80211_11G;
			break;
		case IEEE80211_MODE_FH:
			imr->wlan_ifm_active |= IFM_IEEE80211_FH;
			break;
		case IEEE80211_MODE_TURBO_A:
			imr->wlan_ifm_active |= IFM_IEEE80211_11A |IFM_IEEE80211_TURBO;
			break;
		case IEEE80211_MODE_TURBO_G:
			imr->wlan_ifm_active |= IFM_IEEE80211_11G |IFM_IEEE80211_TURBO;
			break;
	}
}

/*
 * Set the current phy mode and recalculate the active channel
 * set based on the available channels for this mode.  Also
 * select a new default/current channel if the current one is
 * inappropriate for this mode.
 */
int ieee80211_setmode(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const u_int chanflags[] = {
		0,			/* IEEE80211_MODE_AUTO */
		IEEE80211_CHAN_A,	/* IEEE80211_MODE_11A */
		IEEE80211_CHAN_B,	/* IEEE80211_MODE_11B */
		IEEE80211_CHAN_PUREG,	/* IEEE80211_MODE_11G */
		IEEE80211_CHAN_FHSS,	/* IEEE80211_MODE_FH */
		IEEE80211_CHAN_T,	/* IEEE80211_MODE_TURBO_A */
		IEEE80211_CHAN_108G,	/* IEEE80211_MODE_TURBO_G */
	};
	struct ieee80211_channel *c;
	u_int modeflags;
	int i;

	/* validate new mode */
	if ((ic->ic_modecaps & (1<<mode)) == 0)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY, "%s: mode %u not supported (caps 0x%x)\n", __func__, mode, ic->ic_modecaps);
		return EINVAL;
	}

	/*
	 * Verify at least one channel is present in the available
	 * channel list before committing to the new mode.
	 */
	KASSERT(mode < N(chanflags), ("Unexpected mode %u", mode));
	modeflags = chanflags[mode];
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
	{
		c = &ic->ic_channels[i];
		if (mode == IEEE80211_MODE_AUTO)
		{
			/* ignore turbo channels for autoselect */
			if ((c->ic_flags &~ IEEE80211_CHAN_TURBO) != 0)
			{
				break;
			}
		}
		else
		{
			if ((c->ic_flags & modeflags) == modeflags)
				break;
		}
	}
	
	if (i > IEEE80211_CHAN_MAX)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY, "%s: no channels found for mode %u\n", __func__, mode);
		return EINVAL;
	}

	/*
	 * Calculate the active channel set.
	 */
	memset(ic->ic_chan_active, 0, sizeof(ic->ic_chan_active));
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
	{
		c = &ic->ic_channels[i];
		if (mode == IEEE80211_MODE_AUTO)
		{
			/* take anything but pure turbo channels */
			if ((c->ic_flags &~ IEEE80211_CHAN_TURBO) != 0)
				setbit(ic->ic_chan_active, i);
		}
		else
		{
			if ((c->ic_flags & modeflags) == modeflags)
				setbit(ic->ic_chan_active, i);
		}
	}
	/*
	 * If no current/default channel is setup or the current
	 * channel is wrong for the mode then pick the first
	 * available channel from the active list.  This is likely
	 * not the right one.
	 */
	if (ic->ic_ibss_chan == NULL || isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ic->ic_ibss_chan)))
	{
		for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
		{
			if (isset(ic->ic_chan_active, i))
			{
				ic->ic_ibss_chan = &ic->ic_channels[i];
				break;
			}
		}	
		KASSERT(ic->ic_ibss_chan != NULL &&  isset(ic->ic_chan_active,ieee80211_chan2ieee(ic, ic->ic_ibss_chan)),  ("Bad IBSS channel %u", ieee80211_chan2ieee(ic, ic->ic_ibss_chan)));
	}
	/*
	 * If the desired channel is set but no longer valid then reset it.
	 */
	if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
	    isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ic->ic_des_chan)))
		ic->ic_des_chan = IEEE80211_CHAN_ANYC;

	/*
	 * Do mode-specific rate setup.
	 */
	if (mode == IEEE80211_MODE_11G || mode == IEEE80211_MODE_TURBO_G)
	{
		/*
		 * Use a mixed 11b/11g rate set.
		 */
		ieee80211_set11gbasicrates(&ic->ic_sup_rates[mode],
			IEEE80211_MODE_11G);
	}
	else if (mode == IEEE80211_MODE_11B)
	{
		/*
		 * Force pure 11b rate set.
		 */
		ieee80211_set11gbasicrates(&ic->ic_sup_rates[mode], IEEE80211_MODE_11B);
	}
	/*
	 * Setup an initial rate set according to the
	 * current/default channel selected above.  This
	 * will be changed when scanning but must exist
	 * now so driver have a consistent state of ic_ibss_chan.
	 */
	if (ic->ic_bss)		/* NB: can be called before lateattach */
		ic->ic_bss->ni_rates = ic->ic_sup_rates[mode];

	ic->ic_curmode = mode;
	ieee80211_reset_erp(ic);	/* reset ERP state */
	ieee80211_wme_initparams(ic);	/* reset WME stat */

	return 0;
#undef N
}

/*
 * Return the phy mode for with the specified channel so the
 * caller can select a rate set.  This is problematic for channels
 * where multiple operating modes are possible (e.g. 11g+11b).
 * In those cases we defer to the current operating mode when set.
 */
enum ieee80211_phymode ieee80211_chan2mode(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
	if (IEEE80211_IS_CHAN_5GHZ(chan))
	{
		/*
		 * This assumes all 11a turbo channels are also
		 * usable withut turbo, which is currently true.
		 */
		if (ic->ic_curmode == IEEE80211_MODE_TURBO_A)
			return IEEE80211_MODE_TURBO_A;
		return IEEE80211_MODE_11A;
	}
	else if (IEEE80211_IS_CHAN_FHSS(chan))
		return IEEE80211_MODE_FH;
	else if (chan->ic_flags & (IEEE80211_CHAN_OFDM|IEEE80211_CHAN_DYN))
	{
		/*
		 * This assumes all 11g channels are also usable
		 * for 11b, which is currently true.
		 */
		if (ic->ic_curmode == IEEE80211_MODE_TURBO_G)
			return IEEE80211_MODE_TURBO_G;
		if (ic->ic_curmode == IEEE80211_MODE_11B)
			return IEEE80211_MODE_11B;
		return IEEE80211_MODE_11G;
	}
	else
		return IEEE80211_MODE_11B;
}

/*
 * convert IEEE80211 rate value to ifmedia subtype.
 * ieee80211 rate is in unit of 0.5Mbps.
 */
int ieee80211_rate2media(struct ieee80211com *ic, int rate, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const struct {
		u_int	m;	/* rate + mode */
		u_int	r;	/* if_media rate */
	} rates[] = {
		{   2 | IFM_IEEE80211_FH, IFM_IEEE80211_FH1 },
		{   4 | IFM_IEEE80211_FH, IFM_IEEE80211_FH2 },
		{   2 | IFM_IEEE80211_11B, IFM_IEEE80211_DS1 },
		{   4 | IFM_IEEE80211_11B, IFM_IEEE80211_DS2 },
		{  11 | IFM_IEEE80211_11B, IFM_IEEE80211_DS5 },
		{  22 | IFM_IEEE80211_11B, IFM_IEEE80211_DS11 },
		{  44 | IFM_IEEE80211_11B, IFM_IEEE80211_DS22 },
		{  12 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM6 },
		{  18 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM9 },
		{  24 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM12 },
		{  36 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM18 },
		{  48 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM24 },
		{  72 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM36 },
		{  96 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM54 },
		{   2 | IFM_IEEE80211_11G, IFM_IEEE80211_DS1 },
		{   4 | IFM_IEEE80211_11G, IFM_IEEE80211_DS2 },
		{  11 | IFM_IEEE80211_11G, IFM_IEEE80211_DS5 },
		{  22 | IFM_IEEE80211_11G, IFM_IEEE80211_DS11 },
		{  12 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM6 },
		{  18 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM9 },
		{  24 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM12 },
		{  36 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM18 },
		{  48 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM24 },
		{  72 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM36 },
		{  96 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM54 },
		/* NB: OFDM72 doesn't realy exist so we don't handle it */
	};
	u_int mask, i;

	mask = rate & IEEE80211_RATE_VAL;

	switch (mode)
	{
		case IEEE80211_MODE_11A:
		case IEEE80211_MODE_TURBO_A:
			mask |= IFM_IEEE80211_11A;
			break;
		case IEEE80211_MODE_11B:
			mask |= IFM_IEEE80211_11B;
			break;
		case IEEE80211_MODE_FH:
			mask |= IFM_IEEE80211_FH;
			break;
		case IEEE80211_MODE_AUTO:
			/* NB: ic may be NULL for some drivers */
			if (ic && ic->ic_phytype == IEEE80211_T_FH)
			{
				mask |= IFM_IEEE80211_FH;
				break;
			}
			/* NB: hack, 11g matches both 11b+11a rates */
			/* fall thru... */
		case IEEE80211_MODE_11G:
		case IEEE80211_MODE_TURBO_G:
			mask |= IFM_IEEE80211_11G;
			break;
	}
	
	for (i = 0; i < N(rates); i++)
		if (rates[i].m == mask)
			return rates[i].r;

	return IFM_AUTO;
#undef N
}

int ieee80211_media2rate(int mword)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))

	static const int ieeerates[] = {
		-1,		/* IFM_AUTO */
		0,		/* IFM_MANUAL */
		0,		/* IFM_NONE */
		2,		/* IFM_IEEE80211_FH1 */
		4,		/* IFM_IEEE80211_FH2 */
		2,		/* IFM_IEEE80211_DS1 */
		4,		/* IFM_IEEE80211_DS2 */
		11,		/* IFM_IEEE80211_DS5 */
		22,		/* IFM_IEEE80211_DS11 */
		44,		/* IFM_IEEE80211_DS22 */
		12,		/* IFM_IEEE80211_OFDM6 */
		18,		/* IFM_IEEE80211_OFDM9 */
		24,		/* IFM_IEEE80211_OFDM12 */
		36,		/* IFM_IEEE80211_OFDM18 */
		48,		/* IFM_IEEE80211_OFDM24 */
		72,		/* IFM_IEEE80211_OFDM36 */
		96,		/* IFM_IEEE80211_OFDM48 */
		108,		/* IFM_IEEE80211_OFDM54 */
		144,		/* IFM_IEEE80211_OFDM72 */
	};
	return IFM_SUBTYPE(mword) < (int)N(ieeerates) ?
		(int)ieeerates[IFM_SUBTYPE(mword)] : 0;
#undef N
}

EXPORT_SYMBOL(ieee80211_mhz2ieee);
EXPORT_SYMBOL(ieee80211_chan2ieee);
EXPORT_SYMBOL(ieee80211_ieee2mhz);
EXPORT_SYMBOL(ieee80211_media_change);
EXPORT_SYMBOL(ieee80211_media_status);
EXPORT_SYMBOL(ieee80211_setmode);
EXPORT_SYMBOL(ieee80211_chan2mode);
EXPORT_SYMBOL(ieee80211_rate2media);
EXPORT_SYMBOL(ieee80211_media2rate);

