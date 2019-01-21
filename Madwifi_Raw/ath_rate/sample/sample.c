/*-
 * Copyright (c) 2005 John Bicket
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/cache.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>

#include <asm/uaccess.h>

#include <net80211/if_media.h>
#include <net80211/ieee80211_var.h>

#include "if_athvar.h"
#include "ah_desc.h"

#include "sample.h"

#define	SAMPLE_DEBUG
#ifdef SAMPLE_DEBUG
enum {
	ATH_DEBUG_RATE		= 0x00000010,	/* rate control */
};
#define	DPRINTF(sc, _fmt, ...) do {				\
	if (sc->sc_debug & ATH_DEBUG_RATE)			\
		printf(_fmt, __VA_ARGS__);			\
} while (0)
#else
#define	DPRINTF(sc, _fmt, ...)
#endif

/*
 * This file is an implementation of the SampleRate algorithm
 * in "Bit-rate Selection in Wireless Networks"
 * (http://www.pdos.lcs.mit.edu/papers/jbicket-ms.ps)
 *
 * SampleRate chooses the bit-rate it predicts will provide the most
 * throughput based on estimates of the expected per-packet
 * transmission time for each bit-rate.  SampleRate periodically sends
 * packets at bit-rates other than the current one to estimate when
 * another bit-rate will provide better performance. SampleRate
 * switches to another bit-rate when its estimated per-packet
 * transmission time becomes smaller than the current bit-rate's.
 * SampleRate reduces the number of bit-rates it must sample by
 * eliminating those that could not perform better than the one
 * currently being used.  SampleRate also stops probing at a bit-rate
 * if it experiences several successive losses.
 *
 * The difference between the algorithm in the thesis and the one in this
 * file is that the one in this file uses a ewma instead of a window.
 *
 * Also, this implementation tracks the average transmission time for
 * a few different packet sizes independently for each link.
 *
 */

static	int ath_smoothing_rate = 95;	/* ewma percentage (out of 100) */
static	int ath_sample_rate = 10;	/* send a different bit-rate 1/X packets */

static void	ath_rate_ctl_start(struct ath_softc *, struct ieee80211_node *);
static void	ath_rate_ctl_reset(struct ath_softc *, struct ieee80211_node *);




static __inline int size_to_bin(int size) 
{
	int x = 0;
	for (x = 0; x < NUM_PACKET_SIZE_BINS; x++) {
		if (size < packet_size_bins[x]) {
			return x;
		}
	}
	return NUM_PACKET_SIZE_BINS-1;
}
static __inline int bin_to_size(int index) {
	return packet_size_bins[index];
}
/*
 * Setup rate codes for management/control frames.  We force
 * all such frames to the lowest rate.
 */
static void
ath_rate_setmgtrates(struct ath_softc *sc, struct ath_node *an)
{
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	/* setup rates for management frames */
	/* XXX management/control frames always go at lowest speed */
	an->an_tx_mgtrate = rt->info[0].rateCode;
	an->an_tx_mgtratesp = an->an_tx_mgtrate
			    | rt->info[0].shortPreamble;
}


void
ath_rate_node_init(struct ath_softc *sc, struct ath_node *an){
	DPRINTF(sc, "%s:\n", __func__);
	/* NB: assumed to be zero'd by caller */
	ath_rate_setmgtrates(sc, an);
}
EXPORT_SYMBOL(ath_rate_node_init);

void
ath_rate_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
	DPRINTF(sc, "%s:\n", __func__);
}
EXPORT_SYMBOL(ath_rate_node_cleanup);

void
ath_rate_node_copy(struct ath_softc *sc,
		   struct ath_node *dst, const struct ath_node *src)
{
	struct sample_node *odst = ATH_NODE_SAMPLE(dst);
	const struct sample_node *osrc = (const struct sample_node *)&src[1];

	DPRINTF(sc, "%s:\n", __func__);
	memcpy(odst, osrc, sizeof(struct sample_node));
}
EXPORT_SYMBOL(ath_rate_node_copy);


/*
 * returns the ndx with the lowest average_tx_time,
 * or -1 if all the average_tx_times are 0.
 */
static __inline int best_rate_ndx(struct sample_node *sn, int size_bin)
{
	int x = 0;
        int best_rate_ndx = 0;
        int best_rate_tt = 0;
        for (x = 0; x < sn->num_rates; x++) {
		int tt = sn->stats[size_bin][x].average_tx_time;
		if (tt > 0) {
			if (!best_rate_tt || best_rate_tt > tt) {
				best_rate_tt = tt;
				best_rate_ndx = x;
			}
		}
        }
        return (best_rate_tt) ? best_rate_ndx : -1;
}

/*
 * pick a ndx s.t. the perfect_tx_time
 * is less than the best bit-rate's average_tx_time
 * and the ndx has not had four successive failures.
 */
static __inline int pick_sample_ndx(struct sample_node *sn, int size_bin) 
{
	int x = 0;
	int best_ndx = best_rate_ndx(sn, size_bin);
	int best_tt = 0;
	int num_eligible = 0;
	
	if (best_ndx < 0) {
		/* no successes yet, send at the lowest bit-rate */
		return 0;
	}
	
	best_tt = sn->stats[size_bin][best_ndx].average_tx_time;
	sn->sample_num[size_bin]++;
	
	/*
	 * first, find the number of bit-rates we could potentially
	 * sample. we assume this list doesn't change a lot, so
	 * we will just cycle through them.
	 */
	for (x = 0; x < sn->num_rates; x++) {
		if (x != best_ndx && 
		    sn->stats[size_bin][x].perfect_tx_time < best_tt &&
		    sn->stats[size_bin][x].successive_failures < 4) {
			num_eligible++;
		}
	}
	
	if (num_eligible > 0) {
		int pick = sn->sample_num[size_bin] % num_eligible;
		for (x = 0; x < sn->num_rates; x++) {
			if (x != best_ndx && 
			    sn->stats[size_bin][x].perfect_tx_time < best_tt &&
			    sn->stats[size_bin][x].successive_failures < 4) {
				if (pick == 0) {
					return x;
				}
				pick--;
			}
		}
	}
	return best_ndx;
}


void
ath_rate_findrate(struct ath_softc *sc, struct ath_node *an,
		  HAL_BOOL shortPreamble, size_t frameLen,
		  u_int8_t *rix, int *try0, u_int8_t *txrate)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	int x;
	int ndx = 0;
	int size_bin = size_to_bin(frameLen);
	int best_ndx = best_rate_ndx(sn, size_bin);
	if (sn->static_rate_ndx != -1) {
		*try0 = 4;
		*rix = sn->rates[sn->static_rate_ndx].rix;
		*txrate = sn->rates[sn->static_rate_ndx].rateCode;
		return;
	}
	
	*try0 = 2;
	
	best_ndx = best_rate_ndx(sn, size_bin);
	if (!sn->packets_sent[size_bin] || 
	    sn->packets_sent[size_bin] % ath_sample_rate > 0) {
		/*
		 * for most packets, send the packet at the bit-rate with 
		 * the lowest estimated transmission time.
		 */
		if (best_ndx != -1) {
			ndx = best_ndx;
		} else {
			/* 
			 * no packet has succeeded, try the highest bitrate
			 * that hasn't failed.
			 */  
			for (ndx = sn->num_rates-1; ndx >= 0; ndx--) {
				if (sn->stats[size_bin][ndx].successive_failures == 0) {
					break;
				}
			}
		}
		if (size_bin == 0) {
			/* update the visible txrate for this node */
			an->an_node.ni_txrate = ndx;
		}
	} else {
		/* 
		 * before we pick a bit-rate to "sample", clear any
		 * stale stuff out.
		 */
		for (x = 0; x < sn->num_rates; x++) {
			if (jiffies - sn->stats[size_bin][x].last_tx > ((HZ * 10000)/1000)) {
				sn->stats[size_bin][x].average_tx_time = sn->stats[size_bin][x].perfect_tx_time;
				sn->stats[size_bin][x].successive_failures = 0;
				sn->stats[size_bin][x].tries = 0;
				sn->stats[size_bin][x].total_packets = 0;
				sn->stats[size_bin][x].packets_acked = 0;
			}
		}

		/* send the packet at a different bit-rate */
		ndx = pick_sample_ndx(sn, size_bin);
	}
	
	
	*rix = sn->rates[ndx].rix;
	if (shortPreamble) {
		*txrate = sn->rates[ndx].shortPreambleRateCode;
	} else {
		
		*txrate = sn->rates[ndx].rateCode;
	}
	
	sn->packets_sent[size_bin]++;
}
EXPORT_SYMBOL(ath_rate_findrate);

void
ath_rate_setupxtxdesc(struct ath_softc *sc, struct ath_node *an,
		      struct ath_desc *ds, HAL_BOOL shortPreamble, u_int8_t rix)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	int rateCode = -1;
	int frame_size = ds->ds_ctl0 & 0x0fff; /* low-order 12 bits of ds_ctl0 */
	int size_bin = size_to_bin(frame_size);
	int best_ndx = best_rate_ndx(sn, size_bin);

	if (best_ndx == -1 || !sn->stats[size_bin][best_ndx].packets_acked) {
		/* 
		 * no packet has succeeded, so also try twice at the lowest bitate.
		 */
		if (shortPreamble) {
			rateCode = sn->rates[0].shortPreambleRateCode;
		} else {
			rateCode = sn->rates[0].rateCode;
		}
	} else if (sn->rates[best_ndx].rix != rix) {
		/*
		 * we're trying a different bit-rate, and it could be lossy, 
		 * so if it fails try at the best bit-rate.
		 */
		if (shortPreamble) {
			rateCode = sn->rates[MAX(0,best_ndx-1)].shortPreambleRateCode;
		} else {
			rateCode = sn->rates[MAX(0,best_ndx-1)].rateCode;
		}
	}
	if (rateCode != -1) {
		ath_hal_setupxtxdesc(sc->sc_ah, ds
				     , rateCode, 1	/* series 1 */
				     , rateCode, 1	        /* series 2 */
				     , rateCode, 1	        /* series 3 */
				     );
	}
	
}
EXPORT_SYMBOL(ath_rate_setupxtxdesc);

void
ath_rate_tx_complete(struct ath_softc *sc,
		     struct ath_node *an, const struct ath_desc *ds)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	int rate = sc->sc_hwmap[ds->ds_txstat.ts_rate &~ HAL_TXSTAT_ALTRATE];
	int retries = ds->ds_txstat.ts_longretry;
	int initial_rate_failed = ((ds->ds_txstat.ts_rate & HAL_TXSTAT_ALTRATE)
				   || ds->ds_txstat.ts_status != 0 ||
				   retries > 3);
	int tt = 0;
	int rix = -1;
	int x = 0;
	int frame_size = ds->ds_ctl0 & 0x0fff; /* low-order 12 bits of ds_ctl0 */
	int size_bin = size_to_bin(frame_size);
	int size = bin_to_size(size_bin);
	if (!sn->num_rates) {
		DPRINTF(sc, "%s: no rates yet\n", __func__);
		return;
	}
	for (x = 0; x < sn->num_rates; x++) {
		if (sn->rates[x].rate == rate) {
			rix = x;
			break;
		}      
	}
	
	if (rix < 0 || rix > sn->num_rates) {
		/* maybe a management packet */
		return;
	}

	tt = calc_usecs_unicast_packet(sc, size, sn->rates[rix].rix, 
				       retries);

	DPRINTF(sc, "%s: rate %d rix %d frame_size %d (%d) retries %d status %d tt %d avg_tt %d perfect_tt %d ts-rate %d\n", 
		__func__, rate, rix, frame_size, size, retries, initial_rate_failed, tt, 
		sn->stats[size_bin][rix].average_tx_time,
		sn->stats[size_bin][rix].perfect_tx_time,
		ds->ds_txstat.ts_rate);
	
	if (sn->stats[size_bin][rix].total_packets < 7) {
		/* just average the first few packets */
		int avg_tx = sn->stats[size_bin][rix].average_tx_time;
		int packets = sn->stats[size_bin][rix].total_packets;
		sn->stats[size_bin][rix].average_tx_time = (tt+(avg_tx*packets))/(packets+1);
	} else {
		/* use a ewma */
		sn->stats[size_bin][rix].average_tx_time = 
			((sn->stats[size_bin][rix].average_tx_time * ath_smoothing_rate) + 
			 (tt * (100 - ath_smoothing_rate))) / 100;
	}
	
	if (initial_rate_failed) {
		/* 
		 * this packet failed - count this as a failure
		 * for larger packets also, since we assume
		 * if a small packet fails at a lower bit-rate 
		 * then a larger one will also.
		 */
		int y;
		for (y = size_bin; y < NUM_PACKET_SIZE_BINS; y++) {
			sn->stats[y][rix].successive_failures++;
			sn->stats[y][rix].last_tx = jiffies;
		}
	} else {
		sn->stats[size_bin][rix].packets_acked++;
		sn->stats[size_bin][rix].successive_failures = 0;
	}
	sn->stats[size_bin][rix].tries += (1+retries);
	sn->stats[size_bin][rix].last_tx = jiffies;
	sn->stats[size_bin][rix].total_packets++;
}
EXPORT_SYMBOL(ath_rate_tx_complete);

void
ath_rate_newassoc(struct ath_softc *sc, struct ath_node *an, int isnew)
{
	DPRINTF(sc, "%s:\n", __func__);
	if (isnew)
		ath_rate_ctl_start(sc, &an->an_node);
}
EXPORT_SYMBOL(ath_rate_newassoc);



static void
ath_rate_ctl_reset(struct ath_softc *sc, struct ieee80211_node *ni)
{
	struct ath_node *an = ATH_NODE(ni);
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	int x = 0;
	int y = 0;

	for (y = 0; y < NUM_PACKET_SIZE_BINS; y++) {
		int size = bin_to_size(y);
		sn->packets_sent[y] = 0;
		sn->sample_num[y] = 0;

		for (x = 0; x < ni->ni_rates.rs_nrates; x++) {
			sn->stats[y][x].successive_failures = 0;
			sn->stats[y][x].tries = 0;
			sn->stats[y][x].total_packets = 0;
			sn->stats[y][x].packets_acked = 0;
			sn->stats[y][x].last_tx = 0;
			sn->stats[y][x].perfect_tx_time = calc_usecs_unicast_packet(sc, size, 
									      sn->rates[x].rix,
									      0);
			sn->stats[y][x].average_tx_time = sn->stats[y][x].perfect_tx_time;

			
			if (1) {
				DPRINTF(sc, "%s: %d rate %d rix %d rateCode %d perfect_tx_time %d \n", __func__, 
					x, sn->rates[x].rate, 
					sn->rates[x].rix, sn->rates[x].rateCode,
					sn->stats[0][x].perfect_tx_time);
			}
		}

	}
	
	/* set the visible bit-rate to the lowest one available */
	ni->ni_txrate = 0;

}

/*
 * Initialize the tables for a node.
 */
static void
ath_rate_ctl_start(struct ath_softc *sc, struct ieee80211_node *ni)
{
#define	RATE(_ix)	(ni->ni_rates.rs_rates[(_ix)] & IEEE80211_RATE_VAL)
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_node *an = ATH_NODE(ni);
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	const HAL_RATE_TABLE *rt = sc->sc_currates;

	int x;
	int srate;

        DPRINTF(sc, "%s:\n", __func__);
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
	KASSERT(ni->ni_rates.rs_nrates > 0, ("no rates"));
        sn->static_rate_ndx = -1;
	if (ic->ic_fixed_rate != -1) {
		/*
		 * A fixed rate is to be used; ic_fixed_rate is an
		 * index into the supported rate set.  Convert this
		 * to the index into the negotiated rate set for
		 * the node.  We know the rate is there because the
		 * rate set is checked when the station associates.
		 */
		const struct ieee80211_rateset *rs =
			&ic->ic_sup_rates[ic->ic_curmode];
		int r = rs->rs_rates[ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
		/* NB: the rate set is assumed sorted */
		srate = ni->ni_rates.rs_nrates - 1;
		for (; srate >= 0 && RATE(srate) != r; srate--)
			;
		KASSERT(srate >= 0,
			("fixed rate %d not in rate set", ic->ic_fixed_rate));
                sn->static_rate_ndx = srate;
                        
	}
	sn->num_rates = ni->ni_rates.rs_nrates;
        for (x = 0; x < ni->ni_rates.rs_nrates; x++) {
          sn->rates[x].rate = ni->ni_rates.rs_rates[x] & IEEE80211_RATE_VAL;
          sn->rates[x].rix = sc->sc_rixmap[sn->rates[x].rate];
          sn->rates[x].rateCode = rt->info[sn->rates[x].rix].rateCode;
          sn->rates[x].shortPreambleRateCode = 
		  rt->info[sn->rates[x].rix].rateCode | 
		  rt->info[sn->rates[x].rix].shortPreamble;
	}
	ath_rate_ctl_reset(sc, ni);

#undef RATE
}
/*
 * Reset the rate control state for each 802.11 state transition.
 */
void
ath_rate_newstate(struct ath_softc *sc, enum ieee80211_state state)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	if (ic->ic_opmode == IEEE80211_M_STA) {
		/*
		 * Reset local xmit state; this is really only
		 * meaningful when operating in station mode.
		 */
		ni = ic->ic_bss;
		ath_rate_ctl_start(sc, ni);
        } else {
		/*
		 * When operating as a station the node table holds
		 * the AP's that were discovered during scanning.
		 * For any other operating mode we want to reset the
		 * tx rate state of each node.
		 */
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list)
			ath_rate_ctl_start(sc, ni);
	}
}
EXPORT_SYMBOL(ath_rate_newstate);

struct ath_ratectrl *
ath_rate_attach(struct ath_softc *sc)
{
	struct sample_softc *osc;
	DPRINTF(sc, "%s:\n", __func__);
	
	osc = kmalloc(sizeof(struct sample_softc), GFP_ATOMIC);
	if (osc == NULL)
		return NULL;
	osc->arc.arc_space = sizeof(struct sample_node);
	return &osc->arc;
}
EXPORT_SYMBOL(ath_rate_attach);

void
ath_rate_detach(struct ath_ratectrl *arc)
{
	struct sample_softc *osc = (struct sample_softc *) arc;
	
	kfree(osc);
}
EXPORT_SYMBOL(ath_rate_detach);

static	int min_smoothing_rate = 0;		
static	int max_smoothing_rate = 100;		

static int min_sample_rate = 2;
static int max_sample_rate = 100;

#define	CTL_AUTO	-2	/* cannot be CTL_ANY or CTL_NONE */

/*
 * Static (i.e. global) sysctls.
 */
enum {
	DEV_ATH		= 9,			/* XXX known by many */
};


static ctl_table ath_rate_static_sysctls[] = {
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "smoothing_rate",
	  .mode		= 0644,
	  .data		= &ath_smoothing_rate,
	  .maxlen	= sizeof(ath_smoothing_rate),
	  .extra1	= &min_smoothing_rate,
	  .extra2	= &max_smoothing_rate,
	  .proc_handler	= proc_dointvec_minmax
	},
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "sample_rate",
	  .mode		= 0644,
	  .data		= &ath_sample_rate,
	  .maxlen	= sizeof(ath_sample_rate),
	  .extra1	= &min_sample_rate,
	  .extra2	= &max_sample_rate,
	  .proc_handler	= proc_dointvec_minmax
	},
	{ 0 }
};
static ctl_table ath_rate_table[] = {
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "rate",
	  .mode		= 0555,
	  .child	= ath_rate_static_sysctls
	}, { 0 }
};
static ctl_table ath_ath_table[] = {
	{ .ctl_name	= DEV_ATH,
	  .procname	= "ath",
	  .mode		= 0555,
	  .child	= ath_rate_table
	}, { 0 }
};
static ctl_table ath_root_table[] = {
	{ .ctl_name	= CTL_DEV,
	  .procname	= "dev",
	  .mode		= 0555,
	  .child	= ath_ath_table
	}, { 0 }
};
static struct ctl_table_header *ath_sysctl_header;

MODULE_AUTHOR("John Bicket");
MODULE_DESCRIPTION("SampleRate bit-rate selection algorithm for Atheros devices");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

static char *version = ".9";
static char *dev_info = "ath_rate_sample";

static int __init
init_ath_rate_sample(void)
{
	printk(KERN_INFO "%s: %s\n", dev_info, version);

#ifdef CONFIG_SYSCTL
	ath_sysctl_header = register_sysctl_table(ath_root_table, 1);
#endif
	return (0);
}
module_init(init_ath_rate_sample);

static void __exit
exit_ath_rate_sample(void)
{
#ifdef CONFIG_SYSCTL
	if (ath_sysctl_header != NULL)
		unregister_sysctl_table(ath_sysctl_header);
#endif

	printk(KERN_INFO "%s: unloaded\n", dev_info);
}
module_exit(exit_ath_rate_sample);