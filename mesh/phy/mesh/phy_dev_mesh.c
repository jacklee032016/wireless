/*
* $Id: phy_dev_mesh.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#include "_dev_mesh.h"

static int __phy_stop_locked(MESH_DEVICE *mdev)
{
	struct ath_softc *sc = (struct ath_softc *)mdev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: invalid %u flags 0x%x\n",	__func__, sc->sc_invalid, mdev->flags);

	ATH_LOCK_ASSERT(sc);
	if ( mdev->flags & MESHF_RUNNING)
	{
		/*
		 * Shutdown the hardware and driver:
		 *    reset 802.11 state machine
		 *    stop output from above
		 *    disable interrupts
		 *    turn off timers
		 *    turn off the radio
		 *    clear transmit machinery
		 *    clear receive machinery
		 *    drain and release tx queues
		 *    reclaim beacon resources
		 *    power down hardware
		 *
		 * Note that some of this work is not possible if the
		 * hardware is gone (invalid).
		 */
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
		meshif_stop_queue(mdev);
#if 0		
		if (sc->sc_rawdev_enabled) 
			netif_stop_queue(&sc->sc_rawdev);
#endif

		mdev->flags &= ~MESHF_RUNNING;
		if (!sc->sc_invalid)
		{
#if 0		
			if (sc->sc_softled)
			{
				del_timer(&sc->sc_ledtimer);
				ath_hal_gpioset(ah, sc->sc_ledpin, !sc->sc_ledon);
				sc->sc_blinking = 0;
			}
#endif			
			ath_hal_intrset(ah, 0);
		}
		
		ath_draintxq(sc);
		if (!sc->sc_invalid)
		{
			_phy_stoprecv(sc);
			ath_hal_phydisable(ah);
		}
		else
			sc->sc_rxlink = NULL;

		ath_beacon_free(sc);
	}
	return 0;
}

static int __phy_ioctl(MESH_DEVICE *dev, unsigned long data, int cmd)
{
#define	IS_RUNNING(dev) \
	((dev->flags & (MESHF_RUNNING|MESHF_UP)) == (MESHF_RUNNING|MESHF_UP))
	
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	int error = 0;

	PHY_DEBUG_INFO("%s\n", __FUNCTION__);
	ATH_LOCK(sc);
	switch (cmd)
	{
		case SWJTU_PHY_STARTUP: //SIOCSIFFLAGS
			if (IS_RUNNING(dev))
			{
				/*
				 * To avoid rescanning another access point,
				 * do not call _phy_init() here.  Instead,
				 * only reflect promisc mode settings.
				 */
				_phy_mode_init(dev);
			}
			else //if (dev->flags & MESHF_UP)
			{
				/*
				 * Beware of being called during attach/detach
				 * to reset promiscuous mode.  In that case we
				 * will still be marked UP but not RUNNING.
				 * However trying to re-init the interface
				 * is the wrong thing to do as we've already
				 * torn down much of our state.  There's
				 * probably a better way to deal with this.
				 */
				if (!sc->sc_invalid && ic->ic_bss != NULL)
				{
					ktrace;
					_phy_init(dev);	/* XXX lose error */
					ktrace;
				}	
			}
			break;

		case SWJTU_PHY_STOP:
			__phy_stop_locked(dev);
			break;
			
		case SIOCADDMULTI:
		case SIOCDELMULTI:
			/*
			 * The upper layer has already installed/removed
			 * the multicast address(es), just recalculate the
			 * multicast filter for the card.
			 */
			if (dev->flags & MESHF_RUNNING)
				_phy_mode_init(dev);
			break;
		case SIOCGATHSTATS:
			/* NB: embed these numbers to get a consistent view */
//			struct ifreq *ifr = (struct ifreq *)data;
			sc->sc_stats.ast_tx_packets = ic->ic_devstats->tx_packets;
			sc->sc_stats.ast_rx_packets = ic->ic_devstats->rx_packets;
			sc->sc_stats.ast_rx_rssi = ieee80211_getrssi(ic);
			ATH_UNLOCK(sc);
#if 0			
			if (copy_to_user(ifr->ifr_data, &sc->sc_stats,   sizeof (sc->sc_stats)))
				error = -EFAULT;
			else
				error = 0;
#endif			
			break;
#if 1			
		case SIOCGATHDIAG:
			if (!capable(CAP_NET_ADMIN))
				error = -EPERM;
			else
				error = _phy_ioctl_diag(sc, (struct ath_diag *) data);
			break;
#endif

#if 0			
		case SIOCETHTOOL:
			if (copy_from_user(&cmd, ifr->ifr_data, sizeof(cmd)))
				error = -EFAULT;
			else
				error = ath_ioctl_ethtool(sc, cmd, ifr->ifr_data);
				break;
#endif				
		default:
			ktrace;
			if (cmd >= MESHW_IO_FIRST && cmd <= MESHW_IO_LAST)
			{
				struct meshw_req 	iwr;

				if (copy_from_user(&iwr, (void *)data, sizeof(struct meshw_req)))
					return -EFAULT;

				iwr.ifr_name[MESHNAMSIZ-1] = 0;


				/* If command is `set a parameter', or
				 * `get the encoding parameters', check if
				 * the user has the right to do it */
				if (MESHW_IS_WRITE(cmd) || (cmd == MESHW_IO_R_ENCODE))
				{
					if(!capable(CAP_NET_ADMIN))
					{
			ktrace;
						error = -EPERM;
						break;
					}	
				}
#if 0				
				dev_load(ifr.ifr_name);
				rtnl_lock();
#endif				
				/* Follow me in net/core/wireless.c */
				error = meshw_ioctl_process(dev, (unsigned long )&iwr, cmd);
#if 0
				rtnl_unlock();
#endif				
			ktrace;
				if (!error && MESHW_IS_READ(cmd) && copy_to_user((void *)data, &iwr, sizeof(struct meshw_req)))
					error = -EFAULT;
				break;
			}
			error = wlan_ioctl(ic, data, cmd);
			if (error == -ENETRESET)
			{
				if (IS_RUNNING(dev) && ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
					_phy_init(dev);	/* XXX lose error */
				error = 0;
			}
			
			if (error == -ERESTART)
				error = IS_RUNNING(dev) ? _phy_reset(dev) : 0;
			break;
	}

	PHY_DEBUG_INFO("%s", __FUNCTION__);
	
	ATH_UNLOCK(sc);
	return error;
#undef IS_RUNNING
}

/* startup hw */
int _phy_init(MESH_DEVICE *mdev)
{
	struct ath_softc *sc = (struct ath_softc *)mdev->priv;
	struct ieee80211com *ic;// = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ath_hal *ah;// = sc->sc_ah;
	HAL_STATUS status;
	int error = 0;

	KASSERT((sc!=NULL), ("SC is null"));
	ic = &sc->sc_ic;
	KASSERT((ic!=NULL), ("IC is null"));
	ah = sc->sc_ah;
	KASSERT((ah!=NULL), ("HAL is null"));
//	ATH_LOCK(sc);

	ktrace;
	DPRINTF(sc, ATH_DEBUG_RESET, "%s: mode %d\n", __func__, ic->ic_opmode);
	ktrace;

	/*
	 * Stop anything previously setup.  This is safe
	 * whether this is the first time through or not.
	 */
	__phy_stop_locked(mdev);
	ktrace;

	/*
	 * The basic interface to setting the hardware in a good
	 * state is ``reset''.  On return the hardware is known to
	 * be powered up and with interrupts disabled.  This must
	 * be followed by initialization of the appropriate bits
	 * and then setup of the interrupt mask.
	 */
	sc->sc_curchan.channel = ic->ic_ibss_chan->ic_freq;
	sc->sc_curchan.channelFlags = _phy_chan2flags(ic, ic->ic_ibss_chan);
#ifdef HAS_VMAC
	if (!ath_hal_reset(ah, IEEE80211_M_MONITOR, &sc->sc_curchan, AH_FALSE, &status))
#else	   
	if (!ath_hal_reset(ah, ic->ic_opmode, &sc->sc_curchan, AH_FALSE, &status))
#endif
	{
		mdev_printf( mdev, "unable to reset hardware; hal status %u\n", status);
		error = -EIO;
		goto done;
	}
	ktrace;

	/*
	 * This is needed only to setup initial state
	 * but it's best done after a reset.
	 */
	_phy_update_txpow(sc);
	ktrace;

	/*
	 * Setup the hardware after reset: the key cache
	 * is filled as needed and the receive engine is
	 * set going.  Frame transmit is handled entirely
	 * in the frame output path; there's nothing to do
	 * here except setup the interrupt mask.
	 */
	_phy_initkeytable(sc);		/* XXX still needed? */
	ktrace;
	if (_phy_startrecv(sc) != 0)
	{
		mdev_printf(mdev, "unable to start recv logic\n");
		error = -EIO;
		goto done;
	}
	ktrace;

	/* Enable interrupts. */
	sc->sc_imask = HAL_INT_RX | HAL_INT_TX
		  | HAL_INT_RXEOL | HAL_INT_RXORN
		  | HAL_INT_FATAL | HAL_INT_GLOBAL;	// TODO: compiler warning integer overflow in expression
	/*
	 * Enable MIB interrupts when there are hardware phy counters.
	 * Note we only do this (at the moment) for station mode.
	 */
	if (sc->sc_needmib && ic->ic_opmode == IEEE80211_M_STA)
		sc->sc_imask |= HAL_INT_MIB;
	ath_hal_intrset(ah, sc->sc_imask);
	ktrace;

	mdev->flags |= MESHF_RUNNING;

	mdev->flags |= MESHF_UP;

	ic->ic_state = IEEE80211_S_INIT;
	ktrace;

	/*
	 * The hardware should be ready to go now so it's safe
	 * to kick the 802.11 state machine as it's likely to
	 * immediately call back to us to send mgmt frames.
	 */
	ni = ic->ic_bss;
	ni->ni_chan = ic->ic_ibss_chan;
	_phy_chan_change(sc, ni->ni_chan);

#if 0//WITH_MESH
	mesh_route_dev_create(mdev);
#endif
	ktrace;

	if (ic->ic_opmode != IEEE80211_M_MONITOR) 
	{
	ktrace;
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	}
	else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	ktrace;
	
done:
	ktrace;
	
//	ATH_UNLOCK(sc);
	return error;
}

/*
 * Stop the device, grabbing the top-level lock to protect
 * against concurrent entry through _phy_init (which can happen
 * if another thread does a system call and the thread doing the
 * stop is preempted).
 */
int _phy_stop(MESH_DEVICE *mdev)
{
	struct ath_softc *sc = (struct ath_softc *)mdev->priv;
	int error;

	ATH_LOCK(sc);
	error = __phy_stop_locked(mdev);
	if (error == 0 && !sc->sc_invalid)
	{
		/*
		 * Set the chip in full sleep mode.  Note that we are
		 * careful to do this only when bringing the interface
		 * completely to a stop.  When the chip is in this state
		 * it must be carefully woken up or references to
		 * registers in the PCI clock domain may freeze the bus
		 * (and system).  This varies by chip and is mostly an
		 * issue with newer parts that go to sleep more quickly.
		 */
		ath_hal_setpower(sc->sc_ah, HAL_PM_FULL_SLEEP, 0);
	}

#if 0// WITH_MESH
	mesh_route_dev_destory(mdev);
#endif

	ATH_UNLOCK(sc);
	return error;
}

/* Reset the hardware w/o losing operational state.  This is basically a more efficient way of doing _phy_stop, _phy_init,
 * followed by state transitions to the current 802.11 operational state. 
 * Used to recover from various errors and to reset or reload hardware state.
 */
int _phy_reset(MESH_DEVICE *mdev)
{
	struct ath_softc *sc = (struct ath_softc *)mdev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_channel *c;
	HAL_STATUS status;

	mdev_printf(mdev, "resetting\n");
	
	/*
	 * Convert to a HAL channel description with the flags
	 * constrained to reflect the current operating mode.
	 */
	c = ic->ic_ibss_chan;
	sc->sc_curchan.channel = c->ic_freq;
	sc->sc_curchan.channelFlags = _phy_chan2flags(ic, c);

	ath_hal_intrset(ah, 0);		/* disable interrupts */
	ath_draintxq(sc);		/* stop xmit side */
	_phy_stoprecv(sc);		/* stop recv side */
	/* NB: indicate channel change so we do a full reset */
#ifdef HAS_VMAC
	if (!ath_hal_reset(ah, IEEE80211_M_MONITOR, &sc->sc_curchan, AH_TRUE, &status))
#else
	if (!ath_hal_reset(ah, ic->ic_opmode, &sc->sc_curchan, AH_TRUE, &status))
#endif
//		mdev_printf(dev, "%s: unable to reset hardware: '%s' (%u)\n", __func__, hal_status_desc[status], status);
		mdev_printf(mdev, "%s: unable to reset hardware:  (%u)\n", __func__,  status);

	_phy_update_txpow(sc);		/* update tx power state */
	if (_phy_startrecv(sc) != 0)	/* restart recv */
		mdev_printf(mdev, "%s: unable to start recv logic\n", __func__);
	/*
	 * We may be doing a reset in response to an ioctl
	 * that changes the channel so update any state that
	 * might change as a result.
	 */
	_phy_chan_change(sc, c);
	if (ic->ic_state == IEEE80211_S_RUN)
		ath_beacon_config(sc);	/* restart beacons */
	ath_hal_intrset(ah, sc->sc_imask);

	if (ic->ic_state == IEEE80211_S_RUN)
		meshif_wake_queue( mdev);	/* restart xmit */

#if 0
	if (sc->sc_rawdev_enabled)
		meshif_wake_queue(&sc->sc_rawdev);
#endif

	return 0;
}

/* TX packet */
int _phy_start(struct sk_buff *skb, MESH_DEVICE *mdev)
{
#define CLEANUP()	\
	do{ \
		ATH_TXBUF_LOCK_BH(sc); \
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list); \
		ATH_TXBUF_UNLOCK_BH(sc); \
		if (ni != NULL)	\
			ieee80211_free_node(ni);	\
	} while (0)

	struct ath_softc *sc = (struct ath_softc *)mdev->priv;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	struct ieee80211_cb *cb;
	struct sk_buff *skb0;
	struct ieee80211_frame *wh;
//	struct ether_header *eh;
	
	int ret = 0;
	int counter = 0;

	if (( mdev->flags & MESHF_RUNNING) == 0 || sc->sc_invalid)
	{/* net device is not running */
		DPRINTF(sc, ATH_DEBUG_XMIT,	"%s: discard, invalid %d flags %x\n",	__func__, sc->sc_invalid, mdev->flags);
		sc->sc_stats.ast_tx_invalid++;
		return -ENETDOWN;
	}
#if 0
#ifdef HAS_VMAC
	else if (sc->sc_vmac) {
	  // Keep packets out of the device when in softmac mode
	  dev_kfree_skb_any(skb);
	  return 0;
	}

#endif	
#endif

	for (;;)
	{/* Grab a TX buffer and associated resources. */
		ATH_TXBUF_LOCK_BH(sc);
		bf = STAILQ_FIRST(&sc->sc_txbuf);
		if (bf != NULL)
			STAILQ_REMOVE_HEAD(&sc->sc_txbuf, bf_list);
		
		/* XXX use a counter and leave at least one for mgmt frames */
		if (STAILQ_EMPTY(&sc->sc_txbuf))
		{
			DPRINTF(sc, ATH_DEBUG_XMIT, "%s: stop queue\n", __func__);
			sc->sc_stats.ast_tx_qstop++;
			meshif_stop_queue( mdev);
#if 0			
			if (sc->sc_rawdev_enabled)
				meshif_stop_queue(&sc->sc_rawdev);
#endif			
		}
		
		ATH_TXBUF_UNLOCK_BH(sc);

		if (bf == NULL)
		{/* NB: should not happen */
			DPRINTF(sc, ATH_DEBUG_ANY, "%s: out of xmit buffers\n", __func__);
			sc->sc_stats.ast_tx_nobuf++;
			break;
		}

		/*
		 * Poll the management queue for frames; they
		 * have priority over normal data frames.
		 */
		IF_DEQUEUE(&ic->ic_mgtq, skb0);
		if (skb0 == NULL)
		{/* no mgmt frame */
			if (counter++ > 200)
				DPRINTF(sc, ATH_DEBUG_FATAL, "%s (%s): endlessloop (data) (counter=%i)\n", __func__, mdev->name, counter);

			if (!skb)
			{/* NB: no data (called for mgmt) */
				ATH_TXBUF_LOCK_BH(sc);
				STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				ATH_TXBUF_UNLOCK_BH(sc);
				break;
			}
			DPRINTF(sc, ATH_DEBUG_ANY, "%s: data packet are sending\n", __func__);
			
			/*
			 * No data frames go out unless we're associated; this
			 * should not happen as the 802.11 layer does not enable
			 * the xmit queue until we enter the RUN state.
			 */
			if (ic->ic_state != IEEE80211_S_RUN)
			{
				DPRINTF(sc, ATH_DEBUG_ANY, "%s: ignore data packet, state %u\n", __func__, ic->ic_state);
				sc->sc_stats.ast_tx_discard++;
				ATH_TXBUF_LOCK_BH(sc);
				STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				ATH_TXBUF_UNLOCK_BH(sc);
				break;
			}
			/*now sk_buff is data tx by upper layer */
			/* 
			 * Find the node for the destination so we can do
			 * things like power save and fast frames aggregation.
			 */
			 
			 /* must be enhanced here, lizhijie */
#if 0			 
			if (skb->len < sizeof(struct ether_header))
			{
				ic->ic_stats.is_tx_nobuf++;   /* XXX */
				ni = NULL;

				ret = 0; /* error return */
				CLEANUP();
				break;
			}
#endif

#if 0//WITH_MESH
			if(mesh_packet_output(mdev, skb) )
			{
				ret = 0;
				CLEANUP();
				break;
			}
#endif

			/* must be enhanced here, lizhijie */
 			wh = (struct ieee80211_frame *)skb->data;
			ni = ieee80211_find_txnode(ic, wh->i_addr1);//r_dhost);
			if (ni == NULL)
			{/* NB: ieee80211_find_txnode does stat+msg */
				ret = 0; /* error return */
				CLEANUP();
				break;
			}
 
			cb = (struct ieee80211_cb *)skb->cb;
			if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) && (cb->flags & M_PWR_SAV) == 0)
			{
				/* Station in power save mode; pass the frame
				 * to the 802.11 layer and continue.  We'll get
				 * the frame back when the time is right. */
				ieee80211_pwrsave(ic, ni, skb);
				/* don't free this on function exit point */
				skb = NULL;
				CLEANUP();
				break;
			}
			
			/* calculate priority so we can find the tx queue */
			if (ieee80211_classify(ic, skb, ni))
			{
				DPRINTF(sc, ATH_DEBUG_XMIT,	"%s: discard, classification failure\n", __func__);
				ret = 0; /* error return */
				CLEANUP();
				break;
			}
			sc->sc_devstats.tx_packets++;
			sc->sc_devstats.tx_bytes += skb->len;

#if 0
			skb = ieee80211_encap(ic, skb, ni);
			if (skb == NULL)
			{
				DPRINTF(sc, ATH_DEBUG_ANY, "%s: encapsulation failure\n",	__func__);
				sc->sc_stats.ast_tx_encap++;

				ret = 0; /* error return */
				CLEANUP();
				break;
			}
#else
			{
				switch (ic->ic_opmode)
				{
#if 0				
					case IEEE80211_M_STA:
						wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
						IEEE80211_ADDR_COPY(wh->i_addr3, wh->i_addr1);
						break;
#endif						
					case IEEE80211_M_IBSS:
					case IEEE80211_M_AHDEMO:
						wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
						/** NB: always use the bssid from ic_bss as the
						 *     neighbor's may be stale after an ibss merge
						 */
						IEEE80211_ADDR_COPY(wh->i_addr3, ic->ic_bss->ni_bssid);
						break;
					case IEEE80211_M_HOSTAP:
						wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
						IEEE80211_ADDR_COPY(wh->i_addr3, wh->i_addr2); /* my MAC address */
						break;
					case IEEE80211_M_MONITOR:
					default:
						DPRINTF(sc, ATH_DEBUG_XMIT,	"%s: discard, IC is in not support mode\n", __func__);
						ret = 0; /* error return */
						CLEANUP();
						break;
				}


			}
#endif			
			/*
			 * using unified sk_buff for transmit data and management frames
			 */
			skb0 = skb;
		}
		else
		{/* there are mgmt frame. How about data skb txed???,lzj  */
			cb = (struct ieee80211_cb *)skb0->cb;
			ni = cb->ni;

			if (counter++ > 200)
			    DPRINTF(sc, ATH_DEBUG_FATAL, "%s (%s): endlessloop (mgnt) (counter=%i)\n", __func__, mdev->name, counter);

			wh = (struct ieee80211_frame *) skb0->data;
			if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			{
				/* fill time stamp */
				u_int64_t tsf;
				u_int32_t *tstamp;

				tsf = ath_hal_gettsf64(ah);
				/* XXX: adjust 100us delay to xmit */
				tsf += 100;
				tstamp = (u_int32_t *)&wh[1];
				tstamp[0] = htole32(tsf & 0xffffffff);
				tstamp[1] = htole32(tsf >> 32);
			}
			sc->sc_stats.ast_tx_mgmt++;
		}

		if (ath_tx_start(mdev, ni, bf, skb0))
		{
		        printk("%s error: ath_tx_start failed\n", __func__);
			ret = 0; 	/* TODO: error value */
			skb = NULL;	/* ath_tx_start() already freed this */
			CLEANUP();
			continue;
		}

		/** the data frame is last */
		if (skb0 == skb)
		{
			skb = NULL;	/* will be released by tx_processq */
			break; 
		}
		sc->sc_tx_timer = 5;
		mod_timer(&ic->ic_slowtimo, jiffies + HZ);
	}
	
	if (skb)
		dev_kfree_skb(skb);
	return ret;	/* NB: return !0 only in a ``hard error condition'' */
#undef CLEANUP
}

static void __phy_tx_timeout(MESH_DEVICE *dev)
{
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	sc->sc_stats.ast_watchdog++;
	_phy_init(dev);
}

static int __phy_change_mtu(MESH_DEVICE *dev, int mtu) 
{
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	int error;

	if (!(ATH_MIN_MTU < mtu && mtu <= ATH_MAX_MTU))
	{
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid %d, min %u, max %u\n", __func__, mtu, ATH_MIN_MTU, ATH_MAX_MTU);
		return -EINVAL;
	}
	DPRINTF(sc, ATH_DEBUG_ANY, "%s: %d\n", __func__, mtu);

	ATH_LOCK(sc);
	dev->mtu = mtu;
	/* NB: the rx buffers may need to be reallocated */
	tasklet_disable(&sc->sc_rxtq);
	error = -_phy_reset(dev);
	tasklet_enable(&sc->sc_rxtq);
	ATH_UNLOCK(sc);

	return error;
}

static int __phy_set_mac_address(MESH_DEVICE *dev, void *addr)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct sockaddr *mac = addr;
	int error;

	if (meshif_running(dev))
	{
		DPRINTF(sc, ATH_DEBUG_ANY,"%s: cannot set address; device running\n", __func__);
		return -EBUSY;
	}
	DPRINTF(sc, ATH_DEBUG_ANY, "%s: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", __func__,
		mac->sa_data[0], mac->sa_data[1], mac->sa_data[2],	mac->sa_data[3], mac->sa_data[4], mac->sa_data[5]);

	ATH_LOCK(sc);
	/* XXX not right for multiple vap's */
	IEEE80211_ADDR_COPY(ic->ic_myaddr, mac->sa_data);
	IEEE80211_ADDR_COPY(dev->dev_addr, mac->sa_data);
	ath_hal_setmac(ah, dev->dev_addr);
	error = -_phy_reset(dev);
	ATH_UNLOCK(sc);

	return error;
}

/* Return statistics. */
MESH_DEVICE_STATS *_phy_getstats(MESH_DEVICE *dev)
{
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	MESH_DEVICE_STATS *stats = &sc->sc_devstats;

	/* update according to private statistics */
	stats->tx_errors = sc->sc_stats.ast_tx_encap
			 + sc->sc_stats.ast_tx_nonode
			 + sc->sc_stats.ast_tx_xretries
			 + sc->sc_stats.ast_tx_fifoerr
			 + sc->sc_stats.ast_tx_filtered
			 ;
	stats->tx_dropped = sc->sc_stats.ast_tx_nobuf
			+ sc->sc_stats.ast_tx_nobufmgt;
	stats->rx_errors = sc->sc_stats.ast_rx_tooshort
			+ sc->sc_stats.ast_rx_crcerr
			+ sc->sc_stats.ast_rx_fifoerr
			+ sc->sc_stats.ast_rx_badcrypt
			;
	stats->rx_crc_errors = sc->sc_stats.ast_rx_crcerr;

	return stats;
}

/* Return wireless extensions statistics. */
static struct meshw_statistics *__phy_iw_getstats(MESH_DEVICE *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct meshw_statistics *is = &sc->sc_iwstats;

	ieee80211_iw_getstats(ic, is);
	/* add in statistics maintained by the driver */
	is->discard.code += sc->sc_stats.ast_rx_badcrypt;
	is->discard.retries += sc->sc_stats.ast_tx_xretries;
	is->discard.misc += sc->sc_stats.ast_tx_encap
			 + sc->sc_stats.ast_tx_nonode
			 + sc->sc_stats.ast_tx_xretries
			 + sc->sc_stats.ast_tx_fifoerr
			 + sc->sc_stats.ast_tx_filtered
			 + sc->sc_stats.ast_tx_nobuf
			 + sc->sc_stats.ast_tx_nobufmgt;
			 ;
	is->miss.beacon = sc->sc_stats.ast_bmiss;

	return &sc->sc_iwstats;
}

#if IEEE80211_VLAN_TAG_USED
static void __phy_vlan_register(MESH_DEVICE *dev, struct vlan_group *grp)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_vlan_register(ic, grp);
}

static void __phy_vlan_kill_vid(MESH_DEVICE *dev, unsigned short vid)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_vlan_kill_vid(ic, vid);
}
#endif /* IEEE80211_VLAN_TAG_USED */

int phy_read_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	MESH_DEVICE	*port = (MESH_DEVICE *)data;
	struct ath_softc 	*sc = (struct ath_softc *)port->priv;
	struct ieee80211com *ic = &(sc->sc_ic);
	int 				length = 0;
//	unsigned long  	remainder = 0, numerator = 0;
	
	if(!port )
	{
		MESH_WARN_INFO("Read Procedure of PHY is not allocated\n");
		return 0;
	}
	
	length += sprintf(buffer+length, "\n%s Initialized\n\n", port->name);
	length += sprintf(buffer+length, "\nMAC Status : %d\n\n", ic->ic_state);
	length += sprintf(buffer+length, "\n\n");

	*buffer_location = buffer + offset;
	length -= offset;
	if (length > buffer_length)
		length = buffer_length;
	else if (length < 0)
		length = 0;

	return length;

}

void _phy_init_mesh_dev(MESH_DEVICE *dev)
{
#if 0
	ether_setup(dev);
#endif
	dev->open = _phy_init;
	dev->stop = _phy_stop;
	dev->hard_start_xmit = _phy_start;
	dev->tx_timeout = __phy_tx_timeout;
	dev->watchdog_timeo = 5 * HZ;			/* XXX */
	dev->set_multicast_list = _phy_mode_init;
	dev->do_ioctl = __phy_ioctl;
	dev->get_stats = _phy_getstats;
	dev->set_mac_address = __phy_set_mac_address;
 	dev->change_mtu = &__phy_change_mtu;
	
	dev->tx_queue_len = ATH_TXBUF;			/* TODO? 1 for mgmt frame */
#if WITH_QOS
#else
	skb_queue_head_init(&dev->txPktQueue );
#endif

	dev->get_wireless_stats = __phy_iw_getstats;
	ieee80211_ioctl_iwsetup(&phy_meshw_handler_def);
	dev->wireless_handlers = &phy_meshw_handler_def;

#if IEEE80211_VLAN_TAG_USED
	dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
	dev->vlan_rx_register = __phy_vlan_register;
	dev->vlan_rx_kill_vid = __phy_vlan_kill_vid;
#endif /* IEEE80211_VLAN_TAG_USED */

	dev->procEntry.read = phy_read_proc;
	sprintf(dev->procEntry.name, "%s", dev->name ); 

	dev->flags |= MESHF_BROADCAST|MESHF_MULTICAST;

	dev->mtu = 1500;

	memset(dev->broadcast, 0xff, ETH_ALEN);
	
	/* Change our interface type if we are in monitor mode. */
#if 0	 
	/* const variable is defined in  linux/if_arp.h */
	dev->type = (ic->ic_opmode == IEEE80211_M_MONITOR) ? ARPHRD_IEEE80211_PRISM : ARPHRD_ETHER;
#else
	dev->type = MESH_TYPE_TRANSFER;
#endif
}

