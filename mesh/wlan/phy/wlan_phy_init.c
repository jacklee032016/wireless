/*
* $Id: wlan_phy_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_phy.h"

static void __ieee80211_proto_attach(struct ieee80211com *ic)
{

#ifdef notdef
	ic->ic_rtsthreshold = IEEE80211_RTS_DEFAULT;
#else
	ic->ic_rtsthreshold = IEEE80211_RTS_MAX;
#endif
	ic->ic_fragthreshold = 2346;		/* XXX not used yet */
	ic->ic_fixed_rate = -1;			/* no fixed rate */
	ic->ic_protmode = IEEE80211_PROT_CTSONLY;
	ic->ic_roaming = IEEE80211_ROAMING_AUTO;

	ic->ic_wme.wme_hipri_switch_hysteresis = AGGRESSIVE_MODE_SWITCH_HYSTERESIS;

	// mtx_init(&ic->ic_mgtq.ifq_mtx, ifp->if_xname, "mgmt send q", MTX_DEF); // TODO
	spin_lock_init(&ic->ic_mgtq.lock);

	/* protocol state change handler */
	ic->ic_newstate = ieee80211_newstate;

	/* initialize management frame handlers */
	ic->ic_recv_mgmt = ieee80211_recv_mgmt;
	ic->ic_send_mgmt = ieee80211_send_mgmt;
	
	ieee80211_auth_setup();
}

static void __ieee80211_proto_detach(struct ieee80211com *ic)
{
	/* This should not be needed as we detach when reseting
	 * the state but be conservative here since the
	 * authenticator may do things like spawn kernel threads.
	 */
	if (ic->ic_auth->ia_detach)
		ic->ic_auth->ia_detach(ic);

	IF_DRAIN(&ic->ic_mgtq);

#if WITH_WLAN_AUTHBACK_ACL
	/* Detach any ACL'ator */
	if (ic->ic_acl != NULL)
		ic->ic_acl->iac_detach(ic);
#endif

}


/*
 * Default reset method for use with the ioctl support.  This
 * method is invoked after any state change in the 802.11
 * layer that should be propagated to the hardware but not
 * require re-initialization of the 802.11 state machine (e.g
 * rescanning for an ap).  We always return ENETRESET which
 * should cause the driver to re-initialize the device. Drivers
 * can override this method to implement more optimized support.
 * TODO: unneccessary??
 */
static int __ieee80211_default_reset(MESH_DEVICE *dev)
{
	return ENETRESET;
}

static void __ieee80211_watchdog(unsigned long data)
{
	struct ieee80211com *ic = (struct ieee80211com *) data;
	struct ieee80211_node_table *nt;
	int need_inact_timer = 0;

	if (ic->ic_state != IEEE80211_S_INIT)
	{
		if (ic->ic_mgt_timer && --ic->ic_mgt_timer == 0)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, 0);

		nt = &ic->ic_scan;
		if (nt->nt_inact_timer)
		{
			if (--nt->nt_inact_timer == 0)
				nt->nt_timeout(nt);
			need_inact_timer += nt->nt_inact_timer;
		}

		nt = &ic->ic_sta;
		if (nt->nt_inact_timer)
		{
			if (--nt->nt_inact_timer == 0)
				nt->nt_timeout(nt);
			need_inact_timer += nt->nt_inact_timer;
		}
	}

	if (ic->ic_mgt_timer != 0 || need_inact_timer)
		mod_timer(&ic->ic_slowtimo, jiffies + HZ);	/* once a second */
}

/* attached PHY interface (ic is data member of ath_softc ), called in _phy_attach() */
int ieee80211_ifattach(struct ieee80211com *ic)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ieee80211_channel *c;
	u_int i;

	_MOD_INC_USE(THIS_MODULE, return ENODEV);

	ieee80211_crypto_attach(ic);

	/*
	 * Fill in 802.11 available channel set, mark
	 * all available channels as active, and pick
	 * a default channel if not already specified.
	 */
	memset(ic->ic_chan_avail, 0, sizeof(ic->ic_chan_avail));
	ic->ic_modecaps |= 1<<IEEE80211_MODE_AUTO;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
	{
		c = &ic->ic_channels[i];
		if (c->ic_flags)
		{
			/* Verify driver passed us valid data. */
			if (i != ieee80211_chan2ieee(ic, c))
			{
				mdev_printf(dev, "bad channel ignored; "
					"freq %u flags %x number %u\n",
					c->ic_freq, c->ic_flags, i);
				c->ic_flags = 0;	/* NB: remove */
				continue;
			}
			setbit(ic->ic_chan_avail, i);
			/*
			 * Identify mode capabilities.
			 */
			if (IEEE80211_IS_CHAN_A(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_11A;
			if (IEEE80211_IS_CHAN_B(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_11B;
			if (IEEE80211_IS_CHAN_PUREG(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_11G;
			if (IEEE80211_IS_CHAN_FHSS(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_FH;
			if (IEEE80211_IS_CHAN_T(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_TURBO_A;
			if (IEEE80211_IS_CHAN_108G(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_TURBO_G;
		}
	}
	
	/* validate ic->ic_curmode */
	if ((ic->ic_modecaps & (1<<ic->ic_curmode)) == 0)
		ic->ic_curmode = IEEE80211_MODE_AUTO;
	ic->ic_des_chan = IEEE80211_CHAN_ANYC;	/* any channel is ok */
#if 0
	/*
	 * Enable WME by default if we're capable.
	 */
	if (ic->ic_caps & IEEE80211_C_WME)
		ic->ic_flags |= IEEE80211_F_WME;
#endif
	(void) ieee80211_setmode(ic, ic->ic_curmode);

	if (ic->ic_lintval == 0)
		ic->ic_lintval = IEEE80211_BINTVAL_DEFAULT;
	ic->ic_bmisstimeout = 7*ic->ic_lintval;	/* default 7 beacons */
	ic->ic_dtim_period = IEEE80211_DTIM_DEFAULT;
	IEEE80211_BEACON_LOCK_INIT(ic, "beacon");

	ic->ic_txpowlimit = IEEE80211_TXPOWER_MAX;

	ieee80211_node_attach(ic);
	__ieee80211_proto_attach(ic);
	_ieee80211_add_vap(ic);
	/*
	 * Install a default reset method for the ioctl support.
	 * The driver is expected to fill this in before calling us.
	 * TODO: unneccessary?
	 */
	if (ic->ic_reset == NULL)
		ic->ic_reset = __ieee80211_default_reset;

	init_timer(&ic->ic_slowtimo);
	ic->ic_slowtimo.data = (unsigned long) ic;
	ic->ic_slowtimo.function = __ieee80211_watchdog;
	__ieee80211_watchdog((unsigned long) ic);		/* prime timer */
	/*
	 * NB: ieee80211_sysctl_register is called by the driver
	 *     since dev->name isn't setup at this point; yech!
	 */
	return 0;
}

void ieee80211_ifdetach(struct ieee80211com *ic)
{

	_ieee80211_remove_vap(ic);
	del_timer(&ic->ic_slowtimo);
	
	__ieee80211_proto_detach(ic);
	ieee80211_crypto_detach(ic);
	ieee80211_node_detach(ic);
	wlan_ifmedia_removeall(&ic->ic_media);
#ifdef CONFIG_SYSCTL
	ieee80211_sysctl_unregister(ic);
#endif
	IEEE80211_BEACON_LOCK_DESTROY(ic);
	_MOD_DEC_USE(THIS_MODULE);
}

/*
 * Setup the media data structures according to the channel and
 * rate tables.  This must be called by the driver after
 * ieee80211_attach and before most anything else.
 */
void ieee80211_media_init(struct ieee80211com *ic, wlan_ifm_change_cb_t media_change, wlan_ifm_stat_cb_t media_stat)
{
#define	ADD(_ic, _subtype, _optionType) \
	wlan_ifmedia_add(&(_ic)->ic_media, \
		IFM_MAKEWORD(IFM_IEEE80211, (_subtype), (_optionType), 0), 0, NULL)
		
	MESH_DEVICE *dev = ic->ic_dev;
	struct wlan_ifmediareq imr;
	int i, j, mode, rate, maxrate, mword, mopt, r;
	struct ieee80211_rateset *rs;
	struct ieee80211_rateset allrates;

	/*
	 * Do late attach work that must wait for any subclass
	 * (i.e. driver) work such as overriding methods.
	 */
	ieee80211_node_lateattach(ic);

	/* Fill in media characteristics. */
	wlan_ifmedia_init(&ic->ic_media, 0, media_change, media_stat);
	maxrate = 0;
	memset(&allrates, 0, sizeof(allrates));
	
	for (mode = IEEE80211_MODE_AUTO; mode < IEEE80211_MODE_MAX; mode++)
	{
		static const u_int mopts[] =
		{ 
			IFM_AUTO,
			IFM_IEEE80211_11A,
			IFM_IEEE80211_11B,
			IFM_IEEE80211_11G,
			IFM_IEEE80211_FH,
			IFM_IEEE80211_11A | IFM_IEEE80211_TURBO,
			IFM_IEEE80211_11G | IFM_IEEE80211_TURBO,
		};
		
		if ((ic->ic_modecaps & (1<<mode)) == 0)
			continue;

		mopt = mopts[mode];
		ADD(ic, IFM_AUTO, mopt);	/* e.g. 11a auto */
		if (ic->ic_caps & IEEE80211_C_IBSS)
			ADD(ic, IFM_AUTO, mopt | IFM_IEEE80211_ADHOC);
		if (ic->ic_caps & IEEE80211_C_HOSTAP)
			ADD(ic, IFM_AUTO, mopt | IFM_IEEE80211_HOSTAP);
		if (ic->ic_caps & IEEE80211_C_AHDEMO)
			ADD(ic, IFM_AUTO, mopt | IFM_IEEE80211_ADHOC | IFM_FLAG0);
		if (ic->ic_caps & IEEE80211_C_MONITOR)
			ADD(ic, IFM_AUTO, mopt | IFM_IEEE80211_MONITOR);
		if (mode == IEEE80211_MODE_AUTO)
			continue;
		rs = &ic->ic_sup_rates[mode];
		
		for (i = 0; i < rs->rs_nrates; i++) {
			rate = rs->rs_rates[i];
			mword = ieee80211_rate2media(ic, rate, mode);
			if (mword == 0)
				continue;
			ADD(ic, mword, mopt);
			if (ic->ic_caps & IEEE80211_C_IBSS)
				ADD(ic, mword, mopt | IFM_IEEE80211_ADHOC);
			if (ic->ic_caps & IEEE80211_C_HOSTAP)
				ADD(ic, mword, mopt | IFM_IEEE80211_HOSTAP);
			if (ic->ic_caps & IEEE80211_C_AHDEMO)
				ADD(ic, mword, mopt | IFM_IEEE80211_ADHOC | IFM_FLAG0);
			if (ic->ic_caps & IEEE80211_C_MONITOR)
				ADD(ic, mword, mopt | IFM_IEEE80211_MONITOR);
			/*
			 * Add rate to the collection of all rates.
			 */
			r = rate & IEEE80211_RATE_VAL;
			for (j = 0; j < allrates.rs_nrates; j++)
				if (allrates.rs_rates[j] == r)
					break;
			if (j == allrates.rs_nrates) {
				/* unique, add to the set */
				allrates.rs_rates[j] = r;
				allrates.rs_nrates++;
			}
			rate = (rate & IEEE80211_RATE_VAL) / 2;
			if (rate > maxrate)
				maxrate = rate;
		}
	}
	
	for (i = 0; i < allrates.rs_nrates; i++)
	{
		mword = ieee80211_rate2media(ic, allrates.rs_rates[i], IEEE80211_MODE_AUTO);
		if (mword == 0)
			continue;
		mword = IFM_SUBTYPE(mword);	/* remove media options */
		ADD(ic, mword, 0);
		if (ic->ic_caps & IEEE80211_C_IBSS)
			ADD(ic, mword, IFM_IEEE80211_ADHOC);
		if (ic->ic_caps & IEEE80211_C_HOSTAP)
			ADD(ic, mword, IFM_IEEE80211_HOSTAP);
		if (ic->ic_caps & IEEE80211_C_AHDEMO)
			ADD(ic, mword, IFM_IEEE80211_ADHOC | IFM_FLAG0);
		if (ic->ic_caps & IEEE80211_C_MONITOR)
			ADD(ic, mword, IFM_IEEE80211_MONITOR);
	}
	
	ieee80211_media_status(dev, &imr);
	wlan_ifmedia_set(&ic->ic_media, imr.wlan_ifm_active);
	
#ifndef __linux__
	if (maxrate)
		ifp->if_baudrate = IF_Mbps(maxrate);
#endif

#undef ADD
}


/* just print out some msg */
void ieee80211_announce(struct ieee80211com *ic)
{
	MESH_DEVICE *dev = ic->ic_dev;
	int i, mode, rate, mword;
	struct ieee80211_rateset *rs;

	printk("Build date: %s\n", __DATE__);
	
	for (mode = IEEE80211_MODE_11A; mode < IEEE80211_MODE_MAX; mode++)
	{
		if ((ic->ic_modecaps & (1<<mode)) == 0)
			continue;
		mdev_printf(dev, "%s rates: ", ieee80211_phymode_name[mode]);
		rs = &ic->ic_sup_rates[mode];
		for (i = 0; i < rs->rs_nrates; i++)
		{
			rate = rs->rs_rates[i];
			mword = ieee80211_rate2media(ic, rate, mode);
			if (mword == 0)
				continue;
			printf("%s%d%sMbps", (i != 0 ? " " : ""),
			    (rate & IEEE80211_RATE_VAL) / 2,
			    ((rate & 0x1) != 0 ? ".5" : ""));
		}
		printf("\n");
	}
	
	mdev_printf(dev, "H/W encryption support:");
	if (ic->ic_caps & IEEE80211_C_WEP)
		printk(" WEP");
	if (ic->ic_caps & IEEE80211_C_AES)
		printk(" AES");
	if (ic->ic_caps & IEEE80211_C_AES_CCM)
		printk(" AES_CCM");
	if (ic->ic_caps & IEEE80211_C_CKIP)
		printk(" CKIP");
	if (ic->ic_caps & IEEE80211_C_TKIP)
		printk(" TKIP");
	printk("\n");
}


EXPORT_SYMBOL(ieee80211_ifattach);
EXPORT_SYMBOL(ieee80211_ifdetach);
EXPORT_SYMBOL(ieee80211_media_init);
EXPORT_SYMBOL(ieee80211_announce);


