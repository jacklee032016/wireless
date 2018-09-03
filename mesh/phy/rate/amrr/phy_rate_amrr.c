/* $Id: phy_rate_amrr.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $ */
 
/* AMRR rate control. See:
 * http://www-sop.inria.fr/rapports/sophia/RR-5208.html
 * "IEEE 802.11 Rate Adaptation: A Practical Approach" by
 *    Mathieu Lacage, Hossein Manshaei, Thierry Turletti 
 */

#include "phy.h"
#include "phy_hw_desc.h"

#include "phy_rate_amrr.h"

static	int ath_rateinterval 					= 1000;		/* rate ctl interval (ms)  */
static	int ath_rate_max_success_threshold	= 10;
static	int ath_rate_min_success_threshold	= 1;

static void	__phy_rate_amrr_update(struct ath_softc *, struct ieee80211_node *,int rate);
static void	__phy_rate_amrr_ctl_start(struct ath_softc *, struct ieee80211_node *);

static void __node_reset (struct amrr_node *amn)
{
	amn->amn_tx_try0_cnt = 0;
	amn->amn_tx_try1_cnt = 0;
	amn->amn_tx_try2_cnt = 0;
	amn->amn_tx_try3_cnt = 0;
	amn->amn_tx_failure_cnt = 0;
  	amn->amn_success = 0;
  	amn->amn_recovery = 0;
  	amn->amn_success_threshold = ath_rate_min_success_threshold;
}

/* The code below assumes that we are dealing with hardware multi rate retry
 * I have no idea what will happen if you try to use this module with another
 * type of hardware. Your machine might catch fire or it might work with
 * horrible performance... */
static void __phy_rate_amrr_update(struct ath_softc *sc, struct ieee80211_node *ni, int rate)
{
	struct ath_node *an = ATH_NODE(ni);
	struct amrr_node *amn = ATH_NODE_AMRR(an);
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	u_int8_t rix;

	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	DPRINTF(sc, ATH_DEBUG_LED, "%s: set xmit rate for %s to %dM\n", __func__, swjtu_mesh_mac_sprintf(ni->ni_macaddr),
	    ni->ni_rates.rs_nrates > 0 ? (ni->ni_rates.rs_rates[rate] & IEEE80211_RATE_VAL) / 2 : 0);

	ni->ni_txrate = rate;
	/* XXX management/control frames always go at the lowest speed */
	an->an_tx_mgtrate = rt->info[0].rateCode;
	an->an_tx_mgtratesp = an->an_tx_mgtrate | rt->info[0].shortPreamble;
	/*
	 * Before associating a node has no rate set setup
	 * so we can't calculate any transmit codes to use.
	 * This is ok since we should never be sending anything
	 * but management frames and those always go at the
	 * lowest hardware rate.
	 */
	if (ni->ni_rates.rs_nrates > 0)
	{
		amn->amn_tx_rix0 = sc->sc_rixmap[ni->ni_rates.rs_rates[rate] & IEEE80211_RATE_VAL];
		amn->amn_tx_rate0 = rt->info[amn->amn_tx_rix0].rateCode;
		amn->amn_tx_rate0sp = amn->amn_tx_rate0 |rt->info[amn->amn_tx_rix0].shortPreamble;
		
		if (sc->sc_mrretry)
		{
			amn->amn_tx_try0 = 1;
			amn->amn_tx_try1 = 1;
			amn->amn_tx_try2 = 1;
			amn->amn_tx_try3 = 1;
			if (--rate >= 0)
			{
				rix = sc->sc_rixmap[ni->ni_rates.rs_rates[rate]&IEEE80211_RATE_VAL];
				amn->amn_tx_rate1 = rt->info[rix].rateCode;
				amn->amn_tx_rate1sp = amn->amn_tx_rate1 |rt->info[rix].shortPreamble;
			}
			else
			{
				amn->amn_tx_rate1 = amn->amn_tx_rate1sp = 0;
			}

			if (--rate >= 0)
			{
				rix = sc->sc_rixmap[ni->ni_rates.rs_rates[rate]&IEEE80211_RATE_VAL];
				amn->amn_tx_rate2 = rt->info[rix].rateCode;
				amn->amn_tx_rate2sp = amn->amn_tx_rate2 |rt->info[rix].shortPreamble;
			}
			else
			{
				amn->amn_tx_rate2 = amn->amn_tx_rate2sp = 0;
			}

			if (rate > 0)
			{
				/* NB: only do this if we didn't already do it above */
				amn->amn_tx_rate3 = rt->info[0].rateCode;
				amn->amn_tx_rate3sp = an->an_tx_mgtrate | rt->info[0].shortPreamble;
			}
			else
			{
				amn->amn_tx_rate3 = amn->amn_tx_rate3sp = 0;
			}
		}
		else
		{
			amn->amn_tx_try0 = ATH_TXMAXTRY;
			/* theorically, these statements are useless because
			 *  the code which uses them tests for an_tx_try0 == ATH_TXMAXTRY
			 */
			amn->amn_tx_try1 = 0;
			amn->amn_tx_try2 = 0;
			amn->amn_tx_try3 = 0;
			amn->amn_tx_rate1 = amn->amn_tx_rate1sp = 0;
			amn->amn_tx_rate2 = amn->amn_tx_rate2sp = 0;
			amn->amn_tx_rate3 = amn->amn_tx_rate3sp = 0;
		}
	}
	__node_reset (amn);
}


/* Set the starting transmit rate for a node. */
static void __phy_rate_amrr_ctl_start(struct ath_softc *sc, struct ieee80211_node *ni)
{
#define	RATE(_ix)	(ni->ni_rates.rs_rates[(_ix)] & IEEE80211_RATE_VAL)
	struct ieee80211com *ic = &sc->sc_ic;
	int srate;

	KASSERT(ni->ni_rates.rs_nrates > 0, ("no rates"));
	if (ic->ic_fixed_rate == -1)
	{
		/*
		 * No fixed rate is requested. For 11b start with
		 * the highest negotiated rate; otherwise, for 11g
		 * and 11a, we start "in the middle" at 24Mb or 36Mb.
		 */
		srate = ni->ni_rates.rs_nrates - 1;

		if (sc->sc_curmode != IEEE80211_MODE_11B)
		{
			/*
			 * Scan the negotiated rate set to find the
			 * closest rate.
			 */
			/* NB: the rate set is assumed sorted */
			for (; srate >= 0 && RATE(srate) > 72; srate--)
				;
			KASSERT(srate >= 0, ("bogus rate set"));
		}
	}
	else
	{
		/*
		 * A fixed rate is to be used; ic_fixed_rate is an
		 * index into the supported rate set.  Convert this
		 * to the index into the negotiated rate set for
		 * the node.  We know the rate is there because the
		 * rate set is checked when the station associates.
		 */
		const struct ieee80211_rateset *rs =	&ic->ic_sup_rates[ic->ic_curmode];
		int r = rs->rs_rates[ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
		/* NB: the rate set is assumed sorted */
		srate = ni->ni_rates.rs_nrates - 1;
		for (; srate >= 0 && RATE(srate) != r; srate--)
			;
		
		KASSERT(srate >= 0,("fixed rate %d not in rate set", ic->ic_fixed_rate));
	}
	
	__phy_rate_amrr_update(sc, ni, srate);
#undef RATE
}

static void __phy_rate_amrr_cb(void *arg, struct ieee80211_node *ni)
{
	struct ath_softc *sc = arg;
	__phy_rate_amrr_update(sc, ni, 0);
}

/* Reset the rate control state for each 802.11 state transition. */
void phy_rate_newstate(struct ath_softc *sc, enum ieee80211_state state)
{
	struct amrr_softc *asc = (struct amrr_softc *) sc->sc_rc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;

	if (state == IEEE80211_S_INIT)
	{
		del_timer(&asc->timer);
		return;
	}

	if (ic->ic_opmode == IEEE80211_M_STA)
	{
		/** Reset local xmit state; this is really only
		 * meaningful when operating in station mode. */
		ni = ic->ic_bss;
		if (state == IEEE80211_S_RUN)
		{
			__phy_rate_amrr_ctl_start(sc, ni);
		}
		else
		{
			__phy_rate_amrr_update(sc, ni, 0);
		}
	}
	else
	{
		/* When operating as a station the node table holds
		 * the AP's that were discovered during scanning.
		 * For any other operating mode we want to reset the
		 * tx rate state of each node.	 */
		ieee80211_iterate_nodes(&ic->ic_sta, __phy_rate_amrr_cb, sc);
		__phy_rate_amrr_update(sc, ic->ic_bss, 0);
	}

	if (ic->ic_fixed_rate == -1 && state == IEEE80211_S_RUN)
	{
		int interval;
		
		/* Start the background rate control thread if we
		 * are not configured to use a fixed xmit rate. */
		interval = ath_rateinterval;
		if (ic->ic_opmode == IEEE80211_M_STA)
			interval /= 2;
		mod_timer(&asc->timer, jiffies + ((HZ * interval) / 1000));
	}

}

/* Examine and potentially adjust the transmit rate. */
static void __phy_rate_ctl(void *arg, struct ieee80211_node *ni)
{
	struct ath_softc *sc = arg;
	struct amrr_node *amn = ATH_NODE_AMRR(ATH_NODE (ni));
	int old_rate;

#define is_success(amn) \
(amn->amn_tx_try1_cnt  < (amn->amn_tx_try0_cnt/10))
#define is_enough(amn) \
(amn->amn_tx_try0_cnt > 10)
#define is_failure(amn) \
(amn->amn_tx_try1_cnt > (amn->amn_tx_try0_cnt/3))
#define is_max_rate(ni) \
((ni->ni_txrate + 1) >= ni->ni_rates.rs_nrates)
#define is_min_rate(ni) \
(ni->ni_txrate == 0)

	old_rate = ni->ni_txrate;
  
  	DPRINTF (sc,ATH_DEBUG_LED,  "cnt0: %d cnt1: %d cnt2: %d cnt3: %d -- threshold: %d\n",
		 amn->amn_tx_try0_cnt, amn->amn_tx_try1_cnt, amn->amn_tx_try2_cnt, amn->amn_tx_try3_cnt, amn->amn_success_threshold);

  	if (is_success (amn) && is_enough (amn))
	{
		amn->amn_success++;
		if (amn->amn_success == amn->amn_success_threshold && !is_max_rate (ni))
		{
  			amn->amn_recovery = 1;
  			amn->amn_success = 0;
  			ni->ni_txrate++;
			DPRINTF (sc, ATH_DEBUG_LED, "increase rate to %d\n", ni->ni_txrate);
  		}
		else
		{
			amn->amn_recovery = 0;
		}
  	}
	else if (is_failure (amn))
	{
  		amn->amn_success = 0;
  		if (!is_min_rate (ni))
		{
  			if (amn->amn_recovery)
			{
  				/* recovery failure. */
  				amn->amn_success_threshold *= 2;
  				amn->amn_success_threshold = min (amn->amn_success_threshold,  (u_int)ath_rate_max_success_threshold);
 				DPRINTF (sc, ATH_DEBUG_LED, "decrease rate recovery thr: %d\n", amn->amn_success_threshold);
  			}
			else
			{
  				/* simple failure. */
 				amn->amn_success_threshold = ath_rate_min_success_threshold;
 				DPRINTF (sc,ATH_DEBUG_LED, "decrease rate normal thr: %d\n", amn->amn_success_threshold);
  			}
			amn->amn_recovery = 0;
  			ni->ni_txrate--;
   		} else {
			amn->amn_recovery = 0;
		}

   	}
	
	if (is_enough (amn) || old_rate != ni->ni_txrate)
	{/* reset counters. */
		amn->amn_tx_try0_cnt = 0;
		amn->amn_tx_try1_cnt = 0;
		amn->amn_tx_try2_cnt = 0;
		amn->amn_tx_try3_cnt = 0;
		amn->amn_tx_failure_cnt = 0;
	}

	if (old_rate != ni->ni_txrate)
	{
		__phy_rate_amrr_update(sc, ni, ni->ni_txrate);
	}
}

static void __phy_rate_amrr_ctl(unsigned long data)
{
	MESH_DEVICE *dev = (MESH_DEVICE *)data;
	struct ath_softc *sc = dev->priv;
	struct amrr_softc *asc = (struct amrr_softc *) sc->sc_rc;
	struct ieee80211com *ic = &sc->sc_ic;
	int interval;

	if (dev->flags & MESHF_RUNNING)
	{
		sc->sc_stats.ast_rate_calls++;

		if (ic->ic_opmode == IEEE80211_M_STA)
			__phy_rate_ctl(sc, ic->ic_bss);	/* NB: no reference */
		else
			ieee80211_iterate_nodes(&ic->ic_sta, __phy_rate_ctl, sc);
	}

	interval = ath_rateinterval;
	if (ic->ic_opmode == IEEE80211_M_STA)
		interval /= 2;
	asc->timer.expires = jiffies + ((HZ * interval) / 1000);
	add_timer(&asc->timer);
}

/* is not referenced by other modules */
void phy_rate_findrate(struct ath_softc *sc, struct ath_node *an,
	int shortPreamble, size_t frameLen, u_int8_t *rix, int *try0, u_int8_t *txrate)
{
	struct amrr_node *amn = ATH_NODE_AMRR(an);

	*rix = amn->amn_tx_rix0;
	*try0 = amn->amn_tx_try0;

	if (shortPreamble)
		*txrate = amn->amn_tx_rate0sp;
	else
		*txrate = amn->amn_tx_rate0;
}

void phy_rate_setupxtxdesc(struct ath_softc *sc, struct ath_node *an,
	struct ath_desc *ds, int shortPreamble, u_int8_t rix)
{
	struct amrr_node *amn = ATH_NODE_AMRR(an);

	ath_hal_setupxtxdesc(sc->sc_ah, ds
		, amn->amn_tx_rate1sp, amn->amn_tx_try1	/* series 1 */
		, amn->amn_tx_rate2sp, amn->amn_tx_try2	/* series 2 */
		, amn->amn_tx_rate3sp, amn->amn_tx_try3	/* series 3 */
	);
}

void phy_rate_tx_complete(struct ath_softc *sc, struct ath_node *an, const struct ath_desc *ds)
{
	struct amrr_node *amn = ATH_NODE_AMRR(an);
	int sr = ds->ds_txstat.ts_shortretry;
	int lr = ds->ds_txstat.ts_longretry;
	int retry_count = sr + lr;

	amn->amn_tx_try0_cnt++;
	if (retry_count == 1)
	{
		amn->amn_tx_try1_cnt++;
	}
	else if (retry_count == 2)
	{
		amn->amn_tx_try1_cnt++;
		amn->amn_tx_try2_cnt++;
	}
	else if (retry_count == 3)
	{
		amn->amn_tx_try1_cnt++;
		amn->amn_tx_try2_cnt++;
		amn->amn_tx_try3_cnt++;
	}
	else if (retry_count > 3)
	{
		amn->amn_tx_try1_cnt++;
		amn->amn_tx_try2_cnt++;
		amn->amn_tx_try3_cnt++;
		amn->amn_tx_failure_cnt++;
	}
}

void phy_rate_newassoc(struct ath_softc *sc, struct ath_node *an, int isnew)
{
	if (isnew)
		__phy_rate_amrr_ctl_start(sc, &an->an_node);
}


void phy_rate_node_init(struct ath_softc *sc, struct ath_node *an)
{/* NB: assumed to be zero'd by caller */
	__phy_rate_amrr_update(sc, &an->an_node, 0);
}

void phy_rate_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
}


struct ath_ratectrl *phy_rate_attach(struct ath_softc *sc)
{
	struct amrr_softc *asc;

	asc = kmalloc(sizeof(struct amrr_softc), GFP_ATOMIC);
	if (asc == NULL)
		return NULL;
	asc->arc.arc_space = sizeof(struct amrr_node);
	init_timer(&asc->timer);
	asc->timer.data = (unsigned long) &sc->sc_dev;
	asc->timer.function = __phy_rate_amrr_ctl;

	return &asc->arc;
}

void phy_rate_detach(struct ath_ratectrl *arc)
{
	struct amrr_softc *asc = (struct amrr_softc *) arc;

	del_timer(&asc->timer);
	kfree(asc);
}

