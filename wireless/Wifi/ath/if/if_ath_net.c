/*
* $Id: if_ath_net.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>

#include <asm/uaccess.h>

#include "if_ethersubr.h"		/* for ETHER_IS_MULTICAST */
#include "if_media.h"
#include "if_llc.h"

#include <ieee80211_var.h>

#ifdef CONFIG_NET_WIRELESS
#include <net/iw_handler.h>
#endif

#include "radar.h"

#include "if_athvar.h"
#include "ah_desc.h"
#include "ah_devid.h"			/* XXX to identify IBM cards */
#include "ah.h"

#include "if_ath.h"

static int ath_stop_locked(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: invalid %u flags 0x%x\n",	__func__, sc->sc_invalid, dev->flags);

	ATH_LOCK_ASSERT(sc);
	if (dev->flags & IFF_RUNNING) {
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
		netif_stop_queue(dev);
		if (sc->sc_rawdev_enabled) 
			netif_stop_queue(&sc->sc_rawdev);

		dev->flags &= ~IFF_RUNNING;
		if (!sc->sc_invalid) {
			if (sc->sc_softled) {
				del_timer(&sc->sc_ledtimer);
				ath_hal_gpioset(ah, sc->sc_ledpin,
					!sc->sc_ledon);
				sc->sc_blinking = 0;
			}
			ath_hal_intrset(ah, 0);
		}
		ath_draintxq(sc);
		if (!sc->sc_invalid) {
			ath_stoprecv(sc);
			ath_hal_phydisable(ah);
		} else
			sc->sc_rxlink = NULL;
		ath_beacon_free(sc);
	}
	return 0;
}

int ath_init(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;
	int error = 0;

	ATH_LOCK(sc);

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: mode %d\n", __func__, ic->ic_opmode);

	/*
	 * Stop anything previously setup.  This is safe
	 * whether this is the first time through or not.
	 */
	ath_stop_locked(dev);

	/*
	 * Change our interface type if we are in monitor mode.
	 */
	dev->type = (ic->ic_opmode == IEEE80211_M_MONITOR) ?
		ARPHRD_IEEE80211_PRISM : ARPHRD_ETHER;

	/*
	 * The basic interface to setting the hardware in a good
	 * state is ``reset''.  On return the hardware is known to
	 * be powered up and with interrupts disabled.  This must
	 * be followed by initialization of the appropriate bits
	 * and then setup of the interrupt mask.
	 */
	sc->sc_curchan.channel = ic->ic_ibss_chan->ic_freq;
	sc->sc_curchan.channelFlags = ath_chan2flags(ic, ic->ic_ibss_chan);
#ifdef HAS_VMAC
	if (!ath_hal_reset(ah, IEEE80211_M_MONITOR, &sc->sc_curchan, AH_FALSE, &status))
#else	   
	if (!ath_hal_reset(ah, ic->ic_opmode, &sc->sc_curchan, AH_FALSE, &status))
#endif
	{
		if_printf(dev, "unable to reset hardware; hal status %u\n", status);
		error = -EIO;
		goto done;
	}

	/*
	 * This is needed only to setup initial state
	 * but it's best done after a reset.
	 */
	ath_update_txpow(sc);

	/*
	 * Setup the hardware after reset: the key cache
	 * is filled as needed and the receive engine is
	 * set going.  Frame transmit is handled entirely
	 * in the frame output path; there's nothing to do
	 * here except setup the interrupt mask.
	 */
	ath_initkeytable(sc);		/* XXX still needed? */
	if (ath_startrecv(sc) != 0)
	{
		if_printf(dev, "unable to start recv logic\n");
		error = -EIO;
		goto done;
	}

	/*
	 * Enable interrupts.
	 */
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

	dev->flags |= IFF_RUNNING;
	ic->ic_state = IEEE80211_S_INIT;

	/*
	 * The hardware should be ready to go now so it's safe
	 * to kick the 802.11 state machine as it's likely to
	 * immediately call back to us to send mgmt frames.
	 */
	ni = ic->ic_bss;
	ni->ni_chan = ic->ic_ibss_chan;
	ath_chan_change(sc, ni->ni_chan);

#if WITH_MESH
	mesh_route_dev_create(dev);
#endif

	if (ic->ic_opmode != IEEE80211_M_MONITOR) 
	{
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	}
	else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
done:
	ATH_UNLOCK(sc);
	return error;
}

/*
 * Stop the device, grabbing the top-level lock to protect
 * against concurrent entry through ath_init (which can happen
 * if another thread does a system call and the thread doing the
 * stop is preempted).
 */
int ath_stop(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	int error;

	ATH_LOCK(sc);
	error = ath_stop_locked(dev);
	if (error == 0 && !sc->sc_invalid) {
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

#if WITH_MESH
	mesh_route_dev_destory(dev);

#endif

	ATH_UNLOCK(sc);
	return error;
}

/* Reset the hardware w/o losing operational state.  This is basically a more efficient way of doing ath_stop, ath_init,
 * followed by state transitions to the current 802.11 operational state. 
 * Used to recover from various errors and to reset or reload hardware state.
 */
int ath_reset(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_channel *c;
	HAL_STATUS status;

	if_printf(dev, "resetting\n");
	
	/*
	 * Convert to a HAL channel description with the flags
	 * constrained to reflect the current operating mode.
	 */
	c = ic->ic_ibss_chan;
	sc->sc_curchan.channel = c->ic_freq;
	sc->sc_curchan.channelFlags = ath_chan2flags(ic, c);

	ath_hal_intrset(ah, 0);		/* disable interrupts */
	ath_draintxq(sc);		/* stop xmit side */
	ath_stoprecv(sc);		/* stop recv side */
	/* NB: indicate channel change so we do a full reset */
#ifdef HAS_VMAC
	if (!ath_hal_reset(ah, IEEE80211_M_MONITOR, &sc->sc_curchan, AH_TRUE, &status))
#else
	if (!ath_hal_reset(ah, ic->ic_opmode, &sc->sc_curchan, AH_TRUE, &status))
#endif
//		if_printf(dev, "%s: unable to reset hardware: '%s' (%u)\n", __func__, hal_status_desc[status], status);
		if_printf(dev, "%s: unable to reset hardware:  (%u)\n", __func__,  status);

	ath_update_txpow(sc);		/* update tx power state */
	if (ath_startrecv(sc) != 0)	/* restart recv */
		if_printf(dev, "%s: unable to start recv logic\n", __func__);
	/*
	 * We may be doing a reset in response to an ioctl
	 * that changes the channel so update any state that
	 * might change as a result.
	 */
	ath_chan_change(sc, c);
	if (ic->ic_state == IEEE80211_S_RUN)
		ath_beacon_config(sc);	/* restart beacons */
	ath_hal_intrset(ah, sc->sc_imask);

	if (ic->ic_state == IEEE80211_S_RUN)
		netif_wake_queue(dev);	/* restart xmit */

	if (sc->sc_rawdev_enabled)
		netif_wake_queue(&sc->sc_rawdev);
	return 0;
}

/* TX packet used both by net_device and VMAC */
int ath_start(struct sk_buff *skb, struct net_device *dev)
{
#define CLEANUP()	\
	do{ \
		ATH_TXBUF_LOCK_BH(sc); \
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list); \
		ATH_TXBUF_UNLOCK_BH(sc); \
		if (ni != NULL)	\
			ieee80211_free_node(ni);	\
	} while (0)

	struct ath_softc *sc = dev->priv;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	struct ieee80211_cb *cb;
	struct sk_buff *skb0;
	struct ieee80211_frame *wh;
	struct ether_header *eh;
	
	int ret = 0;
	int counter = 0;

	if ((dev->flags & IFF_RUNNING) == 0 || sc->sc_invalid)
	{/* net device is not running */
		DPRINTF(sc, ATH_DEBUG_XMIT,	"%s: discard, invalid %d flags %x\n",	__func__, sc->sc_invalid, dev->flags);
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
			netif_stop_queue(dev);
			if (sc->sc_rawdev_enabled)
				netif_stop_queue(&sc->sc_rawdev);
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
			    DPRINTF(sc, ATH_DEBUG_FATAL, "%s (%s): endlessloop (data) (counter=%i)\n", __func__, dev->name, counter);

			if (!skb)
			{/* NB: no data (called for mgmt) */
				ATH_TXBUF_LOCK_BH(sc);
				STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				ATH_TXBUF_UNLOCK_BH(sc);
				break;
			}
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
			if (skb->len < sizeof(struct ether_header))
			{
				ic->ic_stats.is_tx_nobuf++;   /* XXX */
				ni = NULL;

				ret = 0; /* error return */
				CLEANUP();
				break;
			}
			
#if WITH_MESH
			if(mesh_packet_output(dev, skb) )
			{
				ret = 0;
				CLEANUP();
				break;
			}
#endif
			
			eh = (struct ether_header *)skb->data;
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
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

			/*
			 * Encapsulate the packet for transmission.
			 */
			skb = ieee80211_encap(ic, skb, ni);
			if (skb == NULL)
			{
				DPRINTF(sc, ATH_DEBUG_ANY, "%s: encapsulation failure\n",	__func__);
				sc->sc_stats.ast_tx_encap++;

				ret = 0; /* error return */
				CLEANUP();
				break;
			}
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
			    DPRINTF(sc, ATH_DEBUG_FATAL, "%s (%s): endlessloop (mgnt) (counter=%i)\n", __func__, dev->name, counter);

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

		if (ath_tx_start(dev, ni, bf, skb0))
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

static void ath_tx_timeout(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	sc->sc_stats.ast_watchdog++;
	ath_init(dev);
}


static int ath_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
#define	IS_RUNNING(dev) \
	((dev->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	int error = 0;

	ATH_LOCK(sc);
	switch (cmd)
	{
		case SIOCSIFFLAGS:
			if (IS_RUNNING(dev))
			{
				/*
				 * To avoid rescanning another access point,
				 * do not call ath_init() here.  Instead,
				 * only reflect promisc mode settings.
				 */
				ath_mode_init(dev);
			}
			else if (dev->flags & IFF_UP)
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
					ath_init(dev);	/* XXX lose error */
			}
			else
				ath_stop_locked(dev);
			break;
		case SIOCADDMULTI:
		case SIOCDELMULTI:
			/*
			 * The upper layer has already installed/removed
			 * the multicast address(es), just recalculate the
			 * multicast filter for the card.
			 */
			if (dev->flags & IFF_RUNNING)
				ath_mode_init(dev);
			break;
		case SIOCGATHSTATS:
			/* NB: embed these numbers to get a consistent view */
			sc->sc_stats.ast_tx_packets = ic->ic_devstats->tx_packets;
			sc->sc_stats.ast_rx_packets = ic->ic_devstats->rx_packets;
			sc->sc_stats.ast_rx_rssi = ieee80211_getrssi(ic);
			ATH_UNLOCK(sc);
			if (copy_to_user(ifr->ifr_data, &sc->sc_stats,
			    sizeof (sc->sc_stats)))
				error = -EFAULT;
			else
				error = 0;
			break;
		case SIOCGATHDIAG:
			if (!capable(CAP_NET_ADMIN))
				error = -EPERM;
			else
				error = ath_ioctl_diag(sc, (struct ath_diag *) ifr);
			break;
		case SIOCETHTOOL:
			if (copy_from_user(&cmd, ifr->ifr_data, sizeof(cmd)))
				error = -EFAULT;
			else
				error = ath_ioctl_ethtool(sc, cmd, ifr->ifr_data);
				break;
		default:
			error = ieee80211_ioctl(ic, ifr, cmd);
			if (error == -ENETRESET)
			{
				if (IS_RUNNING(dev) && 
				    ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
					ath_init(dev);	/* XXX lose error */
				error = 0;
			}
			if (error == -ERESTART)
				error = IS_RUNNING(dev) ? ath_reset(dev) : 0;
			break;
	}
	ATH_UNLOCK(sc);
	return error;
#undef IS_RUNNING
}

static int ath_change_mtu(struct net_device *dev, int mtu) 
{
	struct ath_softc *sc = dev->priv;
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
	error = -ath_reset(dev);
	tasklet_enable(&sc->sc_rxtq);
	ATH_UNLOCK(sc);

	return error;
}

static int ath_set_mac_address(struct net_device *dev, void *addr)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct sockaddr *mac = addr;
	int error;

	if (netif_running(dev))
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
	error = -ath_reset(dev);
	ATH_UNLOCK(sc);

	return error;
}

/* Return netdevice statistics. */
struct net_device_stats *ath_getstats(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct net_device_stats *stats = &sc->sc_devstats;

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

#ifdef CONFIG_NET_WIRELESS
/* Return wireless extensions statistics. */
static struct iw_statistics *ath_iw_getstats(struct net_device *dev)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iw_statistics *is = &sc->sc_iwstats;

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
#endif

#if IEEE80211_VLAN_TAG_USED
static void ath_vlan_register(struct net_device *dev, struct vlan_group *grp)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_vlan_register(ic, grp);
}

static void ath_vlan_kill_vid(struct net_device *dev, unsigned short vid)
{
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_vlan_kill_vid(ic, vid);
}
#endif /* IEEE80211_VLAN_TAG_USED */

void ath_net_init(struct net_device *dev)
{
	ether_setup(dev);
	dev->open = ath_init;
	dev->stop = ath_stop;
	dev->hard_start_xmit = ath_start;
	dev->tx_timeout = ath_tx_timeout;
	dev->watchdog_timeo = 5 * HZ;			/* XXX */
	dev->set_multicast_list = ath_mode_init;
	dev->do_ioctl = ath_ioctl;
	dev->get_stats = ath_getstats;
	dev->set_mac_address = ath_set_mac_address;
 	dev->change_mtu = &ath_change_mtu;
	dev->tx_queue_len = ATH_TXBUF;			/* TODO? 1 for mgmt frame */
#ifdef CONFIG_NET_WIRELESS
	dev->get_wireless_stats = ath_iw_getstats;
	ieee80211_ioctl_iwsetup(&ath_iw_handler_def);
	dev->wireless_handlers = &ath_iw_handler_def;
#endif /* CONFIG_NET_WIRELESS */

#if IEEE80211_VLAN_TAG_USED
	dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
	dev->vlan_rx_register = ath_vlan_register;
	dev->vlan_rx_kill_vid = ath_vlan_kill_vid;
#endif /* IEEE80211_VLAN_TAG_USED */

}

