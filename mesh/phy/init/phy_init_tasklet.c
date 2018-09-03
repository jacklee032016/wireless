/*
* $Id: phy_init_tasklet.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_phy_init.h"
#include "radar.h"
#include "phy_hw_desc.h"

/* tasklet used when initialized */


/* Process completed xmit descriptors from the specified queue. */
static void __ath_tx_processq(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_buf *bf;
	struct ath_desc *ds;
	struct ieee80211_node *ni;
	struct ath_node *an;
	int sr, lr, pri;
	HAL_STATUS status;

	DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: tx queue %u head %p link %p\n",
		__func__, txq->axq_qnum,	(caddr_t)(uintptr_t) ath_hal_gettxbuf(sc->sc_ah, txq->axq_qnum),	txq->axq_link);
	
	for (;;)
	{
		ATH_TXQ_LOCK(txq);
		txq->axq_intrcnt = 0;	/* reset periodic desc intr count */
		bf = STAILQ_FIRST(&txq->axq_q);
		if (bf == NULL)
		{
			txq->axq_link = NULL;
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		ds = bf->bf_desc;		/* NB: last decriptor */
		status = ath_hal_txprocdesc(ah, ds);
#ifdef AR_DEBUG
		if (sc->sc_debug & ATH_DEBUG_XMIT_DESC)
			ath_printtxbuf(bf, status == HAL_OK);
#endif
		if (status == HAL_EINPROGRESS) {
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		if (ds == txq->axq_lastdsWithCTS)
			txq->axq_lastdsWithCTS = NULL;
		if (ds == txq->axq_gatingds)
			txq->axq_gatingds = NULL;
		ATH_TXQ_REMOVE_HEAD(txq, bf_list);
		ATH_TXQ_UNLOCK(txq);

		ni = bf->bf_node;
		if (ni != NULL)
		{
			an = ATH_NODE(ni);
			if (ds->ds_txstat.ts_status == 0)
			{
				u_int8_t txant = ds->ds_txstat.ts_antenna;
				sc->sc_stats.ast_ant_tx[txant]++;
				sc->sc_ant_tx[txant]++;
				if (ds->ds_txstat.ts_rate & HAL_TXSTAT_ALTRATE)
					sc->sc_stats.ast_tx_altrate++;
				sc->sc_stats.ast_tx_rssi =
					ds->ds_txstat.ts_rssi;

				ATH_RSSI_LPF(an->an_halstats.ns_avgtxrssi,	ds->ds_txstat.ts_rssi);
				pri = M_WME_GETAC(bf->bf_skb);
				if (pri >= WME_AC_VO)
					ic->ic_wme.wme_hipri_traffic++;
				ni->ni_inact = ni->ni_inact_reload;
			}
			else
			{
				if (ds->ds_txstat.ts_status & HAL_TXERR_XRETRY)
					sc->sc_stats.ast_tx_xretries++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_FIFO)
					sc->sc_stats.ast_tx_fifoerr++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_FILT)
					sc->sc_stats.ast_tx_filtered++;
			}
			sr = ds->ds_txstat.ts_shortretry;
			lr = ds->ds_txstat.ts_longretry;
			sc->sc_stats.ast_tx_shortretry += sr;
			sc->sc_stats.ast_tx_longretry += lr;
			/*
			 * Hand the descriptor to the rate control algorithm.
			 */
			phy_rate_tx_complete(sc, an, ds);
			/*
			 * Reclaim reference to node.
			 *
			 * NB: the node may be reclaimed here if, for example
			 *     this is a DEAUTH message that was sent and the
			 *     node was timed out due to inactivity.
			 */
			ieee80211_free_node(ni);
		}
		
		bus_unmap_single(sc->sc_bdev, bf->bf_skbaddr, bf->bf_skb->len, BUS_DMA_TODEVICE);

#if 0
		if (sc->sc_rawdev_enabled)
		{
			ath_tx_capture(&sc->sc_rawdev, ds, bf->bf_skb);
		}
		else
#endif			
		{
			dev_kfree_skb(bf->bf_skb);
		}
		bf->bf_skb = NULL;
		bf->bf_node = NULL;

		ATH_TXBUF_LOCK(sc);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK(sc);
	}
}


/**********************************************************
* Tasklet of MGMT
***********************************************************/
void _phy_fatal_tasklet(TQUEUE_ARG data)//void *data)
{
	MESH_DEVICE *dev = (MESH_DEVICE *)data;

	mdev_printf(dev, "hardware error; resetting\n");
	_phy_reset(dev);
}

void _phy_radar_tasklet (TQUEUE_ARG data)//void *data)
{
	MESH_DEVICE *dev = (MESH_DEVICE *)data;
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c;

	c = radar_handle_interference (ic);

	if (c == NULL)
	{
 		_phy_stop (dev);
		printk ("%s: FATAL ERROR - All available channels are marked as being interfered by radar. Stopping radio.\n", dev->name);
		return;
	}

	ic->ic_des_chan = c;
	ic->ic_ibss_chan = c;
	ieee80211_new_state (ic, IEEE80211_S_INIT, -1);
	_phy_init (dev);
}

/* overflow */
void _phy_rxorn_tasklet(TQUEUE_ARG data)
{
	MESH_DEVICE *dev = (MESH_DEVICE *)data;

	mdev_printf(dev, "rx FIFO overrun; resetting\n");
	_phy_reset(dev);
}

void _phy_bmiss_tasklet(TQUEUE_ARG data)
{
	MESH_DEVICE *dev = (MESH_DEVICE *)data;
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s\n", __func__);
	KASSERT(ic->ic_opmode == IEEE80211_M_STA, ("unexpect operating mode %u", ic->ic_opmode));
	if (ic->ic_state == IEEE80211_S_RUN)
	{
		/*
		 * Rather than go directly to scan state, try to
		 * reassociate first.  If that fails then the state
		 * machine will drop us into scanning after timing
		 * out waiting for a probe response.
		 */
		ieee80211_new_state(ic, IEEE80211_S_ASSOC, -1);
	}
}

/* Reset the hardware after detecting beacons have stopped. */
void _phy_bstuck_tasklet(TQUEUE_ARG data)
{
	MESH_DEVICE *mdev = (MESH_DEVICE *)data;
	struct ath_softc *sc = (struct ath_softc *)mdev->priv;

	mdev_printf(mdev, "stuck beacon; resetting (bmiss count %u)\n", sc->sc_bmisscount);

	_phy_reset(mdev);
}


/**********************************************************
* Tasklet of TX
***********************************************************/
/*
 * Deferred processing of transmit interrupt; special-cased
 * for a single hardware transmit queue (e.g. 5210 and 5211).
 */
/* process CAB queue and txq0 */
void _phy_tx_tasklet_q0(TQUEUE_ARG data)
{
	MESH_DEVICE *mdev = (MESH_DEVICE *)data;
	struct ath_softc *sc = (struct ath_softc *)mdev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

#ifdef HAS_VMAC
	if ((sc->sc_vmac) /*&&
	    (sc->sc_ic.ic_opmode == IEEE80211_M_MONITOR*/)
	{
		ath_cu_softmac_txdone_tasklet(data);
		return;
	}
#endif

	__ath_tx_processq(sc, &sc->sc_txq[0]);
	__ath_tx_processq(sc, sc->sc_cabq);

	sc->sc_tx_timer = 0;
#if 0
	if (sc->sc_softled)
		ath_led_event(sc, ATH_LED_TX);
#endif

        // TODO: okay???
	/*
	 * Don't wakeup unless we're associated; this insures we don't
	 * signal the upper layer it's ok to start sending data frames.
	 */
	/* XXX use a low watermark to reduce wakeups */
	if (ic->ic_state == IEEE80211_S_RUN)
		meshif_wake_queue(mdev);

#if 0
	if (sc->sc_rawdev_enabled)
		meshif_wake_queue(&sc->sc_rawdev);
#endif	
}

/*
 * Deferred processing of transmit interrupt; special-cased
 * for four hardware queues, 0-3 (e.g. 5212 w/ WME support).
 */
/* process CAB queue and txq0-3, total 5 queue */ 
void _phy_tx_tasklet_q0123(TQUEUE_ARG data)
{
	MESH_DEVICE 	*mdev = (MESH_DEVICE *)data;
	struct ath_softc 	*sc = (struct ath_softc *)mdev->priv;
	struct ieee80211com *ic = &sc->sc_ic;

#ifdef HAS_VMAC
	if ((sc->sc_vmac) /*&&
	    (sc->sc_ic.ic_opmode == IEEE80211_M_MONITOR)*/)
	{
		ath_cu_softmac_txdone_tasklet(data);
		return;
	}
#endif

	/*
	 * Process each active queue.
	 */
	__ath_tx_processq(sc, &sc->sc_txq[0]);
	__ath_tx_processq(sc, &sc->sc_txq[1]);
	__ath_tx_processq(sc, &sc->sc_txq[2]);
	__ath_tx_processq(sc, &sc->sc_txq[3]);
	__ath_tx_processq(sc, sc->sc_cabq);

	sc->sc_tx_timer = 0;

#if 0
	if (sc->sc_softled)
		ath_led_event(sc, ATH_LED_TX);
#endif

        // TODO: okay???
	/*
	 * Don't wakeup unless we're associated; this insures we don't
	 * signal the upper layer it's ok to start sending data frames.
	 */
	/* XXX use a low watermark to reduce wakeups */
	if (ic->ic_state == IEEE80211_S_RUN)
		meshif_wake_queue(mdev);
#if 0
	if (sc->sc_rawdev_enabled)
		meshif_wake_queue(&sc->sc_rawdev);
#endif	
}

/* Deferred processing of transmit interrupt. */
/* generic tx queue process, no more than 10 queue */
void _phy_tx_tasklet(TQUEUE_ARG data)
{
	MESH_DEVICE 	*mdev = (MESH_DEVICE *)data;
	struct ath_softc 	*sc = (struct ath_softc *)mdev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	int i;

#ifdef HAS_VMAC
	if ((sc->sc_vmac) /*&&
	    (sc->sc_ic.ic_opmode == IEEE80211_M_MONITOR)*/)
	{
		ath_cu_softmac_txdone_tasklet(data);
		return;
	}
#endif

	/*
	 * Process each active queue.
	 */
	/* XXX faster to read ISR_S0_S and ISR_S1_S to determine q's? */
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
	{
		if (ATH_TXQ_SETUP(sc, i))
			__ath_tx_processq(sc, &sc->sc_txq[i]);
	}

	sc->sc_tx_timer = 0;

#if 0
	if (sc->sc_softled)
		ath_led_event(sc, ATH_LED_TX);
#endif

        // TODO: okay???
	/*
	 * Don't wakeup unless we're associated; this insures we don't
	 * signal the upper layer it's ok to start sending data frames.
	 */
	/* XXX use a low watermark to reduce wakeups */
	if (ic->ic_state == IEEE80211_S_RUN)
		meshif_wake_queue(mdev);
#if 0
	if (sc->sc_rawdev_enabled)
		netif_wake_queue(&sc->sc_rawdev);
#endif

}



