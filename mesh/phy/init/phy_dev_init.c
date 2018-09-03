/*
* $Id: phy_dev_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_phy_init.h"
#include "radar.h"

#define	PHY_DEBUG_OPTIONS		ATH_DEBUG_ANY


static const char *hal_status_desc[] =
{
	"No error",
	"No hardware present or device not yet supported",
	"Memory allocation failed",
	"Hardware didn't respond as expected",
	"EEPROM magic number invalid",
	"EEPROM version invalid",
	"EEPROM unreadable",
	"EEPROM checksum invalid",
	"EEPROM read problem",
	"EEPROM mac address invalid",
	"EEPROM size not supported",
	"Attempt to change write-locked EEPROM",
	"Invalid parameter to function",
	"Hardware revision not supported",
	"Hardware self-test failed",
	"Operation incomplete"
};

int _phy_detach(MESH_DEVICE *mdev)
{
        struct ath_softc *sc = (struct ath_softc *)mdev->priv;
        struct ieee80211com *ic = &sc->sc_ic;
    
	DPRINTF(sc, ATH_DEBUG_ANY, "%s: flags %x\n", __func__, mdev->flags);
	_phy_stop(mdev);
	sc->sc_invalid = 1;
	/* 
	 * NB: the order of these is important:
	 * o call the 802.11 layer before detaching the hal to
	 *   insure callbacks into the driver to delete global
	 *   key cache entries can be handled
	 * o reclaim the tx queue data structures after calling
	 *   the 802.11 layer as we'll get called back to reclaim
	 *   node state and potentially want to use them
	 * o to cleanup the tx queues the hal is called, so detach
	 *   it last
	 * Other than that, it's straightforward...
	 */
	ieee80211_ifdetach(ic);
	phy_rate_detach(sc->sc_rc);
	_phy_desc_free(sc);
	ath_tx_cleanup(sc);
	hw_hal_detach(sc->sc_ah);

#ifdef HAS_VMAC
	ath_vmac_unregister(sc);

	vmac_free_mac(sc->sc_vmac_defaultmac);
	sc->sc_vmac_defaultmac = 0;

	vmac_free_phy(sc->sc_vmac_inst->defaultphy);
	sc->sc_vmac_inst->defaultphy = 0;
	kfree(sc->sc_vmac_inst);
	sc->sc_vmac_inst = 0;
#endif

	/*
	 * NB: can't reclaim these until after ieee80211_ifdetach
	 * returns because we'll get called back to reclaim node
	 * state and potentially want to use them.
	 */
#ifdef CONFIG_SYSCTL
	ath_dynamic_sysctl_unregister(sc);
#endif /* CONFIG_SYSCTL */

#if 0
	ath_rawdev_detach(sc);
#endif

	swjtu_mesh_unregister_port(mdev);
	
	return 0;
}


int _phy_attach(u_int16_t devid, MESH_DEVICE *mdev)
{
	struct ath_softc 		*sc = (struct ath_softc *)mdev->priv;
	struct ieee80211com 	*ic = &sc->sc_ic;
	struct ath_hal 		*ah;
	HAL_STATUS 			status;
	int 					error = 0, i;
	u_int8_t 				csz;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: devid 0x%x\n", __func__, devid);

	/*
	 * Cache line size is used to size and align various
	 * structures used to communicate with the hardware.
	 */
	/* return bus cachesize in 4B word units */
//	bus_read_cachesize(sc, &csz);
	pci_read_config_byte(sc->sc_bdev, PCI_CACHE_LINE_SIZE, &csz);

	/* XXX assert csz is non-zero */
	sc->sc_cachelsz = csz << 2;		/* convert to bytes */

	ATH_LOCK_INIT(sc);
	ATH_TXBUF_LOCK_INIT(sc);

	ATH_INIT_TQUEUE(&sc->sc_fataltq,		_phy_fatal_tasklet,	mdev);
	ATH_INIT_TQUEUE(&sc->sc_radartq,	_phy_radar_tasklet,	mdev);
	ATH_INIT_TQUEUE(&sc->sc_rxorntq,	_phy_rxorn_tasklet,	mdev);
	ATH_INIT_TQUEUE(&sc->sc_bmisstq,	_phy_bmiss_tasklet,	mdev);
	ATH_INIT_TQUEUE(&sc->sc_bstuckq,	_phy_bstuck_tasklet,	mdev);

	ATH_INIT_TQUEUE(&sc->sc_rxtq,		ath_rx_tasklet,		mdev);

#ifdef HAS_VMAC
	/*
	 * softmac specific initialization
	 */
	ATH_INIT_TQUEUE(&sc->sc_vmac_worktq,ath_cu_softmac_work_tasklet,mdev);

	atomic_set(&(sc->sc_vmac_tx_packets_inflight),0);
	sc->sc_vmac_mac_lock = RW_LOCK_UNLOCKED;
	sc->sc_vmac_defaultmac = 0;
	sc->sc_vmac_mac = 0;
	sc->sc_vmac_phy = 0;
	sc->sc_vmac_txlatency = ATH_VMAC_DEFAULT_TXLATENCY;
	sc->sc_vmac_phocus_settletime = ATH_VMAC_DEFAULT_PHOCUS_SETTLETIME;

	sc->sc_vmac_wifictl0 = CU_SOFTMAC_WIFICTL0;
	sc->sc_vmac_wifictl1 = CU_SOFTMAC_WIFICTL1;

	sc->sc_vmac_inst = kmalloc(sizeof(struct ath_vmac_instance),GFP_ATOMIC);
	memset(sc->sc_vmac_inst, 0, sizeof(struct ath_vmac_instance));
	sc->sc_vmac_inst->lock = RW_LOCK_UNLOCKED;
	sc->sc_vmac_inst->defaultphy = 0;
	sc->sc_vmac_inst->phyinfo = 0;

	ath_vmac_register(sc);
#endif

	/*
	 * Attach the hal and verify ABI compatibility by checking
	 * the hal's ABI signature against the one the driver was
	 * compiled with.  A mismatch indicates the driver was
	 * built with an ah.h that does not correspond to the hal
	 * module loaded in the kernel.
	 */
	ah = hw_hal_attach(devid, sc, 0, (void *) mdev->mem_start, &status);
	if (ah == NULL)
	{
		MESH_ERR_INFO("%s: unable to attach hardware: '%s' (HAL status %u)\n",__func__, hal_status_desc[status], status);
		error = ENXIO;
		goto bad;
	}
	
	if (ah->ah_abi != HAL_ABI_VERSION)
	{
		MESH_ERR_INFO( "%s: HAL ABI mismatch; driver expects 0x%x, HAL reports 0x%x\n", __func__, HAL_ABI_VERSION, ah->ah_abi);
		error = ENXIO;          /* XXX */
		goto bad;
	}
	sc->sc_ah = ah;

	/*
	 * Check if the MAC has multi-rate retry support.
	 * We do this by trying to setup a fake extended
	 * descriptor.  MAC's that don't have support will
	 * return false w/o doing anything.  MAC's that do
	 * support it will return true w/o doing anything.
	 */
	sc->sc_mrretry = ath_hal_setupxtxdesc(ah, NULL, 0,0, 0,0, 0,0);

	/*
	 * Check if the device has hardware counters for PHY
	 * errors.  If so we need to enable the MIB interrupt
	 * so we can act on stat triggers.
	 */
	if (ath_hal_hwphycounters(ah))
		sc->sc_needmib = 1;

	/*
	 * Get the hardware key cache size.
	 */
	sc->sc_keymax = ath_hal_keycachesize(ah);
	if (sc->sc_keymax > sizeof(sc->sc_keymap) * NBBY)
	{
		mdev_printf(mdev,"Warning, using only %zu of %u key cache slots\n", sizeof(sc->sc_keymap) * NBBY, sc->sc_keymax);
		sc->sc_keymax = sizeof(sc->sc_keymap) * NBBY;
	}
	/*
	 * Reset the key cache since some parts do not
	 * reset the contents on initial power up.
	 */
	for (i = 0; i < sc->sc_keymax; i++)
		ath_hal_keyreset(ah, i);
	/*
	 * Mark key cache slots associated with global keys
	 * as in use.  If we knew TKIP was not to be used we
	 * could leave the +32, +64, and +32+64 slots free.
	 * XXX only for splitmic.
	 */
	for (i = 0; i < IEEE80211_WEP_NKID; i++)
	{
		setbit(sc->sc_keymap, i);
		setbit(sc->sc_keymap, i+32);
		setbit(sc->sc_keymap, i+64);
		setbit(sc->sc_keymap, i+32+64);
	}

	/*
	 * Collect the channel list using the default country
	 * code and including outdoor channels.  The 802.11 layer
	 * is resposible for filtering this list based on settings
	 * like the phy mode.
	 */
	if (countrycode != -1)
		ath_countrycode = countrycode;
	if (outdoor != -1)
		ath_outdoor = outdoor;
	if (xchanmode != -1)
		ath_xchanmode = xchanmode;
	error = _phy_getchannels(mdev, ath_countrycode, ath_outdoor, ath_xchanmode);
	if (error != 0)
		goto bad;

	/*
	 * Setup rate tables for all potential media types.
	 */
	_phy_rate_setup(mdev, IEEE80211_MODE_11A);
	_phy_rate_setup(mdev, IEEE80211_MODE_11B);
	_phy_rate_setup(mdev, IEEE80211_MODE_11G);
	_phy_rate_setup(mdev, IEEE80211_MODE_TURBO_A);
	_phy_rate_setup(mdev, IEEE80211_MODE_TURBO_G);
	/* NB: setup here so ath_rate_update is happy */
	ath_setcurmode(sc, IEEE80211_MODE_11A);

	/*
	 * Allocate tx+rx descriptors and populate the lists.
	 */
	error = _phy_desc_alloc(sc);
	if (error != 0)
	{
		mdev_printf(mdev, "failed to allocate descriptors: %d\n", error);
		goto bad;
	}

	/*
	 * Allocate hardware transmit queues: one queue for
	 * beacon frames and one data queue for each QoS
	 * priority.  Note that the hal handles reseting
	 * these queues at the needed time.
	 *
	 * XXX PS-Poll
	 */
	sc->sc_bhalq = _phy_beaconq_setup(ah);
	if (sc->sc_bhalq == (u_int) -1)
	{
		mdev_printf(mdev, "unable to setup a beacon xmit queue!\n");
		goto bad2;
	}
	sc->sc_cabq = ath_txq_setup(sc, HAL_TX_QUEUE_CAB, 0);
	if (sc->sc_cabq == NULL)
	{
		mdev_printf(mdev, "unable to setup CAB xmit queue!\n");
		error = EIO;
		goto bad2;
	}
	/* NB: insure BK queue is the lowest priority h/w queue */
	if (!ath_tx_setup(sc, WME_AC_BK, HAL_WME_AC_BK))
	{
		mdev_printf(mdev, "unable to setup xmit queue for %s traffic!\n",	ieee80211_wme_acnames[WME_AC_BK]);
		error = EIO;
		goto bad2;
	}
	
	if (!ath_tx_setup(sc, WME_AC_BE, HAL_WME_AC_BE) ||
	    !ath_tx_setup(sc, WME_AC_VI, HAL_WME_AC_VI) ||
	    !ath_tx_setup(sc, WME_AC_VO, HAL_WME_AC_VO))
	{
		/* 
		 * Not enough hardware tx queues to properly do WME;
		 * just punt and assign them all to the same h/w queue.
		 * We could do a better job of this if, for example,
		 * we allocate queues when we switch from station to AP mode.
		 */
		if (sc->sc_ac2q[WME_AC_VI] != NULL)
			ath_tx_cleanupq(sc, sc->sc_ac2q[WME_AC_VI]);
		if (sc->sc_ac2q[WME_AC_BE] != NULL)
			ath_tx_cleanupq(sc, sc->sc_ac2q[WME_AC_BE]);
		sc->sc_ac2q[WME_AC_BE] = sc->sc_ac2q[WME_AC_BK];
		sc->sc_ac2q[WME_AC_VI] = sc->sc_ac2q[WME_AC_BK];
		sc->sc_ac2q[WME_AC_VO] = sc->sc_ac2q[WME_AC_BK];
	}

	/* 
	 * Special case certain configurations.  Note the
	 * CAB queue is handled by these specially so don't
	 * include them when checking the txq setup mask.
	 */
	switch (sc->sc_txqsetup &~ (1<<sc->sc_cabq->axq_qnum))
	{
		case 0x01:
			ATH_INIT_TQUEUE(&sc->sc_txtq, _phy_tx_tasklet_q0, mdev);
			break;
		case 0x0f:
			ATH_INIT_TQUEUE(&sc->sc_txtq, _phy_tx_tasklet_q0123, mdev);
			break;
		default:
			ATH_INIT_TQUEUE(&sc->sc_txtq, _phy_tx_tasklet, mdev);
			break;
	}

	/*
	 * Setup rate control.  Some rate control modules
	 * call back to change the anntena state so expose
	 * the necessary entry points.
	 * XXX maybe belongs in struct ath_ratectrl?
	 */
	sc->sc_setdefantenna = ath_setdefantenna;
	sc->sc_rc = phy_rate_attach(sc);
	if (sc->sc_rc == NULL)
	{
		error = EIO;
		goto bad2;
	}

	init_timer(&sc->sc_scan_ch);
	sc->sc_scan_ch.function = _phy_next_scan;
	sc->sc_scan_ch.data = (unsigned long) mdev;

	init_timer(&sc->sc_cal_ch);
	sc->sc_cal_ch.function = _phy_calibrate;
	sc->sc_cal_ch.data = (unsigned long) mdev;

#if 0
	sc->sc_blinking = 0;
	sc->sc_ledstate = 1;
	sc->sc_ledon = 0;			/* low true */
	sc->sc_ledidle = (2700*HZ)/1000;	/* 2.7sec */

	init_timer(&sc->sc_ledtimer);
	sc->sc_ledtimer.function = ath_led_off;
	sc->sc_ledtimer.data = (unsigned long) sc;
	/*
	 * Auto-enable soft led processing for IBM cards and for
	 * 5211 minipci cards.  Users can also manually enable/disable
	 * support with a sysctl.
	 */
	sc->sc_softled = (devid == AR5212_DEVID_IBM || devid == AR5211_DEVID);
	if (sc->sc_softled)
	{
		ath_hal_gpioCfgOutput(ah, sc->sc_ledpin);
		ath_hal_gpioset(ah, sc->sc_ledpin, !sc->sc_ledon);
	}
#endif

	/* lizhijie*/
	_phy_init_mesh_dev(mdev);

	
	ic->ic_dev = mdev;
	ic->ic_devstats = &sc->sc_devstats;
	ic->ic_init = _phy_init;
	ic->ic_reset = _phy_reset;
	ic->ic_newassoc = _phy_newassoc;
	ic->ic_updateslot = _phy_updateslot;
	ic->ic_wme.wme_update = ath_wme_update;
	
	/* XXX not right but it's not used anywhere important */
	ic->ic_phytype = IEEE80211_T_OFDM;
#if 0
	ic->ic_opmode = IEEE80211_M_STA;
#else
	ic->ic_opmode = IEEE80211_M_HOSTAP;
#endif
	ic->ic_caps =
		  IEEE80211_C_IBSS		/* ibss, nee adhoc, mode */
		| IEEE80211_C_HOSTAP		/* hostap mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_SHSLOT		/* short slot time supported */
		| IEEE80211_C_WPA		/* capable of WPA1+WPA2 */
		;

	/* initialize management queue	 */
	skb_queue_head_init(&ic->ic_mgtq);

	/* Query the hal to figure out h/w crypto support. */
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_WEP))
		ic->ic_caps |= IEEE80211_C_WEP;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_AES_OCB))
		ic->ic_caps |= IEEE80211_C_AES;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_AES_CCM))
		ic->ic_caps |= IEEE80211_C_AES_CCM;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_CKIP))
		ic->ic_caps |= IEEE80211_C_CKIP;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_TKIP))
	{
		ic->ic_caps |= IEEE80211_C_TKIP;
		/*
		 * Check if h/w does the MIC and/or whether the
		 * separate key cache entries are required to
		 * handle both tx+rx MIC keys.
		 */
		if (ath_hal_ciphersupported(ah, HAL_CIPHER_MIC))
			ic->ic_caps |= IEEE80211_C_TKIPMIC;
		if (ath_hal_tkipsplit(ah))
			sc->sc_splitmic = 1;
	}
	/* Transmit Power Control
	 * TPC support can be done either with a global cap or
	 * per-packet support.  The latter is not available on
	 * all parts.  We're a bit pedantic here as all parts
	 * support a global cap.
	 */
	sc->sc_hastpc = ath_hal_hastpc(ah);
	if (sc->sc_hastpc || ath_hal_hastxpowlimit(ah))
		ic->ic_caps |= IEEE80211_C_TXPMGT;

	/*
	 * Mark WME capability only if we have sufficient
	 * hardware queues to do proper priority scheduling.
	 */
	if (sc->sc_ac2q[WME_AC_BE] != sc->sc_ac2q[WME_AC_BK])
		ic->ic_caps |= IEEE80211_C_WME;

	/** Check for frame bursting capability. */
	if (ath_hal_hasbursting(ah))
		ic->ic_caps |= IEEE80211_C_BURST;

	/*
	 * Indicate we need the 802.11 header padded to a
	 * 32-bit boundary for 4-address and QoS frames.
	 */
	ic->ic_flags |= IEEE80211_F_DATAPAD;

	/* Query the hal about antenna support. */
	if (ath_hal_hasdiversity(ah))
	{
		sc->sc_hasdiversity = 1;
		sc->sc_diversity = ath_hal_getdiversity(ah);
	}
	sc->sc_defant = ath_hal_getdefantenna(ah);

	/*
	 * Not all chips have the VEOL(Virtual EOL) support we want to
	 * use with IBSS beacons; check here for it.
	 */
	sc->sc_hasveol = ath_hal_hasveol(ah);

	sc->sc_rxfilter = 0;

#if 0	
	sc->sc_rawdev_enabled = 0;
	sc->sc_rawdev.type = ARPHRD_IEEE80211;
#endif

	/* get mac address from hardware */
	ath_hal_getmac(ah, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(mdev->dev_addr, ic->ic_myaddr);

	/* call MI attach routine. */
	ieee80211_ifattach(ic);
	/* override default methods */
	ic->ic_node_alloc = _phy_node_alloc;
	sc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = _phy_node_free;
	ic->ic_node_getrssi = _phy_node_getrssi;
	
	sc->sc_recv_mgmt = ic->ic_recv_mgmt;
	ic->ic_recv_mgmt = _phy_recv_mgmt;
	
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = _phy_newstate;
	
	ic->ic_crypto.cs_key_alloc = ath_key_alloc;
	ic->ic_crypto.cs_key_delete = ath_key_delete;
	ic->ic_crypto.cs_key_set = ath_key_set;
	ic->ic_crypto.cs_key_update_begin = ath_key_update_begin;
	ic->ic_crypto.cs_key_update_end = ath_key_update_end;

	radar_init(ic);

	/* complete initialization */
	ieee80211_media_init(ic, _phy_media_change, ieee80211_media_status);

	if (swjtu_mesh_register_port(mdev))
	{
		MESH_ERR_INFO( "%s: unable to register mesh device\n", mdev->name);
		goto bad3;
	}

	/*
	 * Attach dynamic MIB vars and announce support
	 * now that we have a device name with unit number.
	 */
#ifdef CONFIG_SYSCTL
	ath_dynamic_sysctl_register(sc);
	ath_rate_dynamic_sysctl_register(sc);
	ieee80211_sysctl_register(ic);
#endif

//	sc->sc_debug = PHY_DEBUG_OPTIONS;
	ic->ic_debug = IEEE80211_MSG_ANY;
	
	ieee80211_announce(ic);
	_phy_announce(sc);
	return 0;
	
bad3:
	ieee80211_ifdetach(ic);
	phy_rate_detach(sc->sc_rc);
bad2:
	if (sc->sc_txq[WME_AC_BK].axq_qnum != (u_int) -1)
	{
		ATH_TXQ_LOCK_DESTROY(&sc->sc_txq[WME_AC_BK]);
	}
	if (sc->sc_txq[WME_AC_BE].axq_qnum != (u_int) -1)
	{
		ATH_TXQ_LOCK_DESTROY(&sc->sc_txq[WME_AC_BE]);
	}
	if (sc->sc_txq[WME_AC_VI].axq_qnum != (u_int) -1)
	{
		ATH_TXQ_LOCK_DESTROY(&sc->sc_txq[WME_AC_VI]);
	}
	if (sc->sc_txq[WME_AC_VO].axq_qnum != (u_int) -1) {
		ATH_TXQ_LOCK_DESTROY(&sc->sc_txq[WME_AC_VO]);
	}
	ath_tx_cleanup(sc);
	_phy_desc_free(sc);
bad:
	if (ah)
	{
		hw_hal_detach(ah);
	}
	
	sc->sc_invalid = 1;
	return error;
}

