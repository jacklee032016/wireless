/*
* $Id: if_ath_sysctrl.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
*/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>

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

#include "if_ath_pci.h"

#include "if_ath.h"


#ifdef CONFIG_SYSCTL
/* Dynamic (i.e. per-device) sysctls.  These are automatically mirrored in /proc/sys. */
enum
{
	ATH_SLOTTIME			= 1,
	ATH_ACKTIMEOUT		= 2,
	ATH_CTSTIMEOUT		= 3,
	ATH_SOFTLED			= 4,
	ATH_LEDPIN				= 5,
	ATH_COUNTRYCODE		= 6,
	ATH_REGDOMAIN			= 7,
	ATH_DEBUG				= 8,
	ATH_TXANTENNA			= 9,
	ATH_RXANTENNA			= 10,
	ATH_DIVERSITY			= 11,
	ATH_TXINTRPERIOD		= 12,
	ATH_TPSCALE     			= 13,
	ATH_TPC         			= 14,
	ATH_TXPOWLIMIT  		= 15,	
	ATH_VEOL        			= 16,
	ATH_BINTVAL			= 17,
	ATH_RAWDEV      		= 18,
	ATH_RAWDEV_TYPE 		= 19,
	ATH_RXFILTER   			= 20,
	ATH_RADARSIM   		= 21,
};

static	int mindwelltime = 100;			/* 100ms */
static	int mincalibrate = 1;			/* once a second */
static	int maxint = 0x7fffffff;		/* 32-bit big */

#define	CTL_AUTO	-2	/* cannot be CTL_ANY or CTL_NONE */

#define NUM_RADIOTAP_ELEMENTS 18

static int radiotap_elem_to_bytes[NUM_RADIOTAP_ELEMENTS] = 
{
	8, /* IEEE80211_RADIOTAP_TSFT */
	 1, /* IEEE80211_RADIOTAP_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RATE */
	 4, /* IEEE80211_RADIOTAP_CHANNEL */
	 2, /* IEEE80211_RADIOTAP_FHSS */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_LOCK_QUALITY */
	 2, /* IEEE80211_RADIOTAP_TX_ATTENUATION */
	 2, /* IEEE80211_RADIOTAP_DB_TX_ATTENUATION */
	 1, /* IEEE80211_RADIOTAP_DBM_TX_POWER */
	 1, /* IEEE80211_RADIOTAP_ANTENNA */
	 1, /* IEEE80211_RADIOTAP_DB_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DB_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_RX_FLAGS */
	 2, /* IEEE80211_RADIOTAP_TX_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RTS_RETRIES */
	 1, /* IEEE80211_RADIOTAP_DATA_RETRIES */
	};



/*
 * the following rt_* functions deal with verifying that a valid
 * radiotap header is on a packet as well as functions to extracting
 * what information is included.
 * XXX maybe these should go in ieee_radiotap.c
 */
static int rt_el_present(struct ieee80211_radiotap_header *th, u_int32_t element)
{
	if (element > NUM_RADIOTAP_ELEMENTS)
		return 0;
	return th->it_present & (1 << element);
}

static int rt_check_header(struct ieee80211_radiotap_header *th, int len) 
{
	int bytes = 0;
	int x = 0;
	if (th->it_version != 0) 
		return 0;

	if (th->it_len < sizeof(struct ieee80211_radiotap_header))
		return 0;
	
	for (x = 0; x < NUM_RADIOTAP_ELEMENTS; x++) {
		if (rt_el_present(th, x))
		    bytes += radiotap_elem_to_bytes[x];
	}

	if (th->it_len < sizeof(struct ieee80211_radiotap_header) + bytes) 
		return 0;
	
	if (th->it_len > len)
		return 0;

	return 1;
}

static u_int8_t *rt_el_offset(struct ieee80211_radiotap_header *th, u_int32_t element)
{
	unsigned int x = 0;
	u_int8_t *offset = ((u_int8_t *) th) + sizeof(struct ieee80211_radiotap_header);
	for (x = 0; x < NUM_RADIOTAP_ELEMENTS && x < element; x++) {
		if (rt_el_present(th, x))
			offset += radiotap_elem_to_bytes[x];
	}

	return offset;
}

/** ath_start for raw 802.11 packets. */
static int ath_start_raw(struct sk_buff *skb, struct net_device *dev)
{
#define	CTS_DURATION \
	ath_hal_computetxtime(ah, rt, IEEE80211_ACK_LEN, cix, AH_TRUE)
#define	updateCTSForBursting(_ah, _ds, _txq) \
	ath_hal_updateCTSForBursting(_ah, _ds, \
	    _txq->axq_linkbuf != NULL ? _txq->axq_linkbuf->bf_desc : NULL, \
	    _txq->axq_lastdsWithCTS, _txq->axq_gatingds, \
	    txopLimit, CTS_DURATION)
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	const struct chanAccParams *cap = &ic->ic_wme.wme_chanParams;
	struct ath_txq *txq;
	struct ath_buf *bf;
	HAL_PKT_TYPE atype;
	int pktlen, hdrlen, try0, pri, dot11Rate, txpower; 
	u_int8_t ctsrate, ctsduration, txrate;
	u_int8_t cix = 0xff;         /* NB: silence compiler */
	u_int flags = 0;
	struct ieee80211_frame *wh; 
	struct ath_desc *ds;
	const HAL_RATE_TABLE *rt;

#ifdef HAS_VMAC
	// Keep packets out of the device if it isn't up OR it's
	// been appropriated by the VMAC
	if ((sc->sc_dev.flags & IFF_RUNNING) == 0 || sc->sc_invalid ||
	    sc->sc_vmac) {
#else
	if ((sc->sc_dev.flags & IFF_RUNNING) == 0 || sc->sc_invalid) {
#endif
		/* device is not up... silently discard packet */
		dev_kfree_skb(skb);
		return 0;
	}

	/*
	 * Grab a TX buffer and associated resources.
	 */
	ATH_TXBUF_LOCK_BH(sc);
	bf = STAILQ_FIRST(&sc->sc_txbuf);
	if (bf != NULL)
		STAILQ_REMOVE_HEAD(&sc->sc_txbuf, bf_list);
	/* XXX use a counter and leave at least one for mgmt frames */
	if (STAILQ_EMPTY(&sc->sc_txbuf)) {
		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: stop queue\n", __func__);
		sc->sc_stats.ast_tx_qstop++;
		netif_stop_queue(&sc->sc_dev);
		netif_stop_queue(&sc->sc_rawdev);
	}
	ATH_TXBUF_UNLOCK_BH(sc);
	
	if (bf == NULL) {		/* NB: should not happen */
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: out of xmit buffers\n",
			__func__);
		sc->sc_stats.ast_tx_nobuf++;
		dev_kfree_skb(skb);
		return 0;
	}

	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
	flags = HAL_TXDESC_INTREQ | HAL_TXDESC_CLRDMASK;
	try0 = ATH_TXMAXTRY;
	dot11Rate = 0;
	ctsrate = 0;
	ctsduration = 0;
	pri = 0;
	txpower = 60;
	txq = sc->sc_ac2q[pri];
	txrate = rt->info[0].rateCode;
	atype =  HAL_PKT_TYPE_NORMAL;

	/*
	 * strip any physical layer header off the skb, if it is
	 * present, and read out any settings we can, like what txrate
	 * to send this packet at.
	 */
	switch(dev->type) {
	case ARPHRD_IEEE80211:
		break;
	case ARPHRD_IEEE80211_PRISM: {
		wlan_ng_prism2_header *ph = NULL;
		ph = (wlan_ng_prism2_header *) skb->data;
		/* does it look like there is a prism header here? */
		if (skb->len > sizeof (wlan_ng_prism2_header) &&
		    ph->msgcode == DIDmsg_lnxind_wlansniffrm &&
		    ph->rate.did == DIDmsg_lnxind_wlansniffrm_rate) {
			dot11Rate = ph->rate.data;
			skb_pull(skb, sizeof(wlan_ng_prism2_header));
		} 
		break;
	}
	case ARPHRD_IEEE80211_RADIOTAP: {
		struct ieee80211_radiotap_header *th = (struct ieee80211_radiotap_header *) skb->data;
		if (rt_check_header(th, skb->len)) {
			if (rt_el_present(th, IEEE80211_RADIOTAP_RATE)) {
				dot11Rate = *((u_int8_t *) rt_el_offset(th, 
					      IEEE80211_RADIOTAP_RATE));
			}
			if (rt_el_present(th, IEEE80211_RADIOTAP_DATA_RETRIES)) {
				try0 = 1 + *((u_int8_t *) rt_el_offset(th, 
					      IEEE80211_RADIOTAP_DATA_RETRIES));
			}
			if (rt_el_present(th, IEEE80211_RADIOTAP_DBM_TX_POWER)) {
				txpower = *((u_int8_t *) rt_el_offset(th, 
					      IEEE80211_RADIOTAP_DBM_TX_POWER));
				if (txpower > 60) 
					txpower = 60;
				
			}

			skb_pull(skb, th->it_len);
		}
		break;
	}
	default:
		/* nothing */
		break;
	}
	
	if (dot11Rate != 0) {
		int index = sc->sc_rixmap[dot11Rate & IEEE80211_RATE_VAL];
		if (index >= 0 && index < rt->rateCount) {
			txrate = rt->info[index].rateCode;
		}
	}

	wh = (struct ieee80211_frame *) skb->data;
	pktlen = skb->len + IEEE80211_CRC_LEN;
	hdrlen = sizeof(struct ieee80211_frame);

	if (hdrlen < pktlen) 
		hdrlen = pktlen;

	if (IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
		sc->sc_stats.ast_tx_noack++;
	}
	
	if (IFF_DUMPPKTS(sc, ATH_DEBUG_XMIT))
		ieee80211_dump_pkt(skb->data, skb->len,
				   sc->sc_hwmap[txrate].ieeerate, -1);

	/*
	 * Load the DMA map so any coalescing is done.  This
	 * also calculates the number of descriptors we need.
	 */
	bf->bf_skbaddr = bus_map_single(sc->sc_bdev,
		skb->data, pktlen, BUS_DMA_TODEVICE);
	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: skb %p [data %p len %u] skbaddr %lx\n",
		__func__, skb, skb->data, skb->len, (long unsigned int) bf->bf_skbaddr);
	if (BUS_DMA_MAP_ERROR(bf->bf_skbaddr)) {
		if_printf(dev, "%s: DMA mapping failed\n", __func__);
		dev_kfree_skb(skb);
		bf->bf_skb = NULL;
		sc->sc_stats.ast_tx_busdma++;

		ATH_TXBUF_LOCK_BH(sc);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK_BH(sc);
		return -EIO;
	}
	bf->bf_skb = skb;
	bf->bf_node = NULL;

	/* setup descriptors */
	ds = bf->bf_desc;
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	/*
	 * Formulate first tx descriptor with tx controls.
	 */
	/* XXX check return value? */
	ath_hal_setuptxdesc(ah, ds
		, pktlen		/* packet length */
		, hdrlen		/* header length */
		, atype			/* Atheros packet type */
		, txpower     	        /* txpower */
		, txrate, try0		/* series 0 rate/tries */
		, HAL_TXKEYIX_INVALID	/* key cache index */
		, sc->sc_txantenna	/* antenna mode */
		, flags			/* flags */
		, ctsrate		/* rts/cts rate */
		, ctsduration		/* rts/cts duration */
	);

	/*
	 * Fillin the remainder of the descriptor info.
	 */
	ds->ds_link = 0;
	ds->ds_data = bf->bf_skbaddr;
	ath_hal_filltxdesc(ah, ds
		, skb->len		/* segment length */
		, AH_TRUE		/* first segment */
		, AH_TRUE		/* last segment */
		, ds			/* first descriptor */
	);

	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: Q%d: %08x %08x %08x %08x %08x %08x\n",
	    __func__, txq->axq_qnum, ds->ds_link, ds->ds_data,
	    ds->ds_ctl0, ds->ds_ctl1, ds->ds_hw[0], ds->ds_hw[1]);
	/*
	 * Insert the frame on the outbound list and
	 * pass it on to the hardware.
	 */
	ATH_TXQ_LOCK_BH(txq);
	if (flags & (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA)) {
		u_int32_t txopLimit = IEEE80211_TXOP_TO_US(
			cap->cap_wmeParams[pri].wmep_txopLimit);
		/*
		 * When bursting, potentially extend the CTS duration
		 * of a previously queued frame to cover this frame
		 * and not exceed the txopLimit.  If that can be done
		 * then disable RTS/CTS on this frame since it's now
		 * covered (burst extension).  Otherwise we must terminate
		 * the burst before this frame goes out so as not to
		 * violate the WME parameters.  All this is complicated
		 * as we need to update the state of packets on the
		 * (live) hardware queue.  The logic is buried in the hal
		 * because it's highly chip-specific.
		 */
		if (txopLimit != 0) {
			sc->sc_stats.ast_tx_ctsburst++;
			if (updateCTSForBursting(ah, ds, txq) == 0) {
				/*
				 * This frame was not covered by RTS/CTS from
				 * the previous frame in the burst; update the
				 * descriptor pointers so this frame is now
				 * treated as the last frame for extending a
				 * burst.
				 */
				txq->axq_lastdsWithCTS = ds;
				/* set gating Desc to final desc */
				txq->axq_gatingds =
					(struct ath_desc *)txq->axq_link;
			} else
				sc->sc_stats.ast_tx_ctsext++;
		}
	}
	ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);
	DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: txq depth = %d\n",
		__func__, txq->axq_depth);
	if (txq->axq_link == NULL) {
		ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
		DPRINTF(sc, ATH_DEBUG_XMIT,
			"%s: TXDP[%u] = %p (%p) depth %d\n", __func__,
			txq->axq_qnum, (caddr_t)bf->bf_daddr, bf->bf_desc,
			txq->axq_depth);
	} else {
		*txq->axq_link = bf->bf_daddr;
		DPRINTF(sc, ATH_DEBUG_XMIT,
			"%s: link[%u](%p)=%p (%p) depth %d\n", __func__,
			txq->axq_qnum, txq->axq_link,
			(caddr_t)bf->bf_daddr, bf->bf_desc, txq->axq_depth);
	}
	txq->axq_link = &bf->bf_desc->ds_link;
	/*
	 * The CAB queue is started from the SWBA handler since
	 * frames only go out on DTIM and to avoid possible races.
	 */
	if (txq != sc->sc_cabq)
		ath_hal_txstart(ah, txq->axq_qnum);
	ATH_TXQ_UNLOCK_BH(txq);

	sc->sc_devstats.tx_packets++;
	sc->sc_devstats.tx_bytes += skb->len;
	sc->sc_dev.trans_start = jiffies;
	sc->sc_rawdev.trans_start = jiffies;

	return 0;
#undef updateCTSForBursting
#undef CTS_DURATION
}

static int ath_rawdev_attach(struct ath_softc *sc) 
{
	struct net_device *rawdev;
	unsigned t;
	rawdev = &sc->sc_rawdev;
	strcpy(rawdev->name, sc->sc_dev.name);
	strcat(rawdev->name, "raw");
	rawdev->priv = sc;

	/* ether_setup clobbers type, so save it */
	t = rawdev->type;
	ether_setup(rawdev);
	rawdev->type = t;

	rawdev->stop = NULL;
	rawdev->hard_start_xmit = ath_start_raw;
	rawdev->set_multicast_list = NULL;
	rawdev->get_stats = ath_getstats;
	rawdev->tx_queue_len = ATH_TXBUF;
	rawdev->flags |= IFF_NOARP;
	rawdev->flags &= ~IFF_MULTICAST;
	rawdev->mtu = IEEE80211_MAX_LEN;

	if (register_netdev(rawdev)) {
		goto bad;
	}

	if ((sc->sc_dev.flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
		netif_start_queue(&sc->sc_rawdev);

	sc->sc_rawdev_enabled = 1;

	return 0;
 bad:
	return -1;
}

void ath_rawdev_detach(struct ath_softc *sc) 
{
	if (sc->sc_rawdev_enabled)
	{
		sc->sc_rawdev_enabled = 0;
		netif_stop_queue(&sc->sc_rawdev);
		unregister_netdev(&sc->sc_rawdev);
	}
}

static int ATH_SYSCTL_DECL(ath_sysctl_halparam, ctl, write, filp, buffer, lenp, ppos)
{
	struct ath_softc *sc = ctl->extra1;
	struct ath_hal *ah = sc->sc_ah;
	u_int val;
	int ret;

	ctl->data = &val;
	ctl->maxlen = sizeof(val);
	if (write)
	{/* write */
		ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos);
		if (ret == 0)
		{
			switch (ctl->ctl_name)
			{
				case ATH_SLOTTIME:
					if (!ath_hal_setslottime(ah, val))
						ret = -EINVAL;
					break;
				case ATH_ACKTIMEOUT:
					if (!ath_hal_setacktimeout(ah, val))
						ret = -EINVAL;
					break;
				case ATH_CTSTIMEOUT:
					if (!ath_hal_setctstimeout(ah, val))
						ret = -EINVAL;
					break;
				case ATH_SOFTLED:
					if (val != sc->sc_softled) {
						if (val)
							ath_hal_gpioCfgOutput(ah,
								sc->sc_ledpin);
						ath_hal_gpioset(ah, sc->sc_ledpin,!val);
						sc->sc_softled = val;
					}
					break;
				case ATH_LEDPIN:
					sc->sc_ledpin = val;
					break;
				case ATH_DEBUG:
					sc->sc_debug = val;
					break;
				case ATH_TXANTENNA:
					/* XXX validate? */
					sc->sc_txantenna = val;
					break;
				case ATH_RXANTENNA:
					/* XXX validate? */
					ath_setdefantenna(sc, val);
					break;
				case ATH_DIVERSITY:
					/* XXX validate? */
					if (!sc->sc_hasdiversity)
						return -EINVAL;
					sc->sc_diversity = val;
					ath_hal_setdiversity(ah, val);
					break;
				case ATH_TXINTRPERIOD:
					sc->sc_txintrperiod = val;
					break;
	                        case ATH_TPSCALE:
	                                /* XXX validate? */
					// TODO: error handling
	                                ath_hal_settpscale(ah, val);
	                                break;
	                        case ATH_TPC:
	                                /* XXX validate? */
	                                if (!sc->sc_hastpc)
	                                        return -EINVAL;
	                                ath_hal_settpc(ah, val);
	                                break;
	                        case ATH_TXPOWLIMIT:
	                                /* XXX validate? */
	                                ath_hal_settxpowlimit(ah, val);
	                                break;
	                        case ATH_BINTVAL:
					if ((sc->sc_ic).ic_opmode != IEEE80211_M_HOSTAP &&
	    					(sc->sc_ic).ic_opmode != IEEE80211_M_IBSS)
	                    		    return -EINVAL;
	            			if (IEEE80211_BINTVAL_MIN <= val &&
	                			val <= IEEE80211_BINTVAL_MAX) {
	                    		    (sc->sc_ic).ic_lintval = val;
	                    		ret = -ENETRESET;              /* requires restart */
	            			} else
	                    		    ret = -EINVAL;
	                                break;
				case ATH_RAWDEV:
					if (val && !sc->sc_rawdev_enabled) {
						ath_rawdev_attach(sc);
					} else if (!val && sc->sc_rawdev_enabled) {
						ath_rawdev_detach(sc);
					}
					ath_reset(&sc->sc_dev);
					break;
				case ATH_RAWDEV_TYPE:
					switch (val)
					{
						case 0: 
							sc->sc_rawdev.type = ARPHRD_IEEE80211;
							break;
						case 1: 
							sc->sc_rawdev.type = ARPHRD_IEEE80211_PRISM;
							break;
						case 2: 
							sc->sc_rawdev.type = ARPHRD_IEEE80211_RADIOTAP;
							break;
						default:
							return -EINVAL;
					}
					ath_reset(&sc->sc_dev);
					break;
				case ATH_RXFILTER:
					sc->sc_rxfilter = val;
#ifdef HAS_VMAC
					ath_reset(&sc->sc_dev);
#else
#define 	HAL_RX_FILTER_ALLOWALL  0x3BF		/* should check the newly code from CVS, lzj, 2006.04.01 */

					ath_hal_setrxfilter(ah,HAL_RX_FILTER_ALLOWALL);
#endif
					break;
				case ATH_RADARSIM:
					ATH_SCHEDULE_TQUEUE (&sc->sc_radartq, &needmark);
					break;
				default:
					return -EINVAL;
			}
		}
	}
	else
	{/* read */
		switch (ctl->ctl_name)
		{
			case ATH_SLOTTIME:
				val = ath_hal_getslottime(ah);
				break;
			case ATH_ACKTIMEOUT:
				val = ath_hal_getacktimeout(ah);
				break;
			case ATH_CTSTIMEOUT:
				val = ath_hal_getctstimeout(ah);
				break;
			case ATH_SOFTLED:
				val = sc->sc_softled;
				break;
			case ATH_LEDPIN:
				val = sc->sc_ledpin;
				break;
			case ATH_COUNTRYCODE:
				ath_hal_getcountrycode(ah, &val);
				break;
			case ATH_REGDOMAIN:
				ath_hal_getregdomain(ah, &val);
				break;
			case ATH_DEBUG:
				val = sc->sc_debug;
				break;
			case ATH_TXANTENNA:
				val = sc->sc_txantenna;
				break;
			case ATH_RXANTENNA:
				val = ath_hal_getdefantenna(ah);
				break;
			case ATH_DIVERSITY:
				val = sc->sc_diversity;
				break;
			case ATH_TXINTRPERIOD:
				val = sc->sc_txintrperiod;
				break;
			case ATH_TPSCALE:
				ath_hal_gettpscale(ah, &val);
				break;
			case ATH_TPC:
				val = ath_hal_gettpc(ah);
				break;
			case ATH_TXPOWLIMIT:
				ath_hal_gettxpowlimit(ah, &val);
				break;
			case ATH_VEOL:
				val = sc->sc_hasveol;
	 			break;
			case ATH_BINTVAL:
				val = (sc->sc_ic).ic_lintval;
	 			break;
			case ATH_RAWDEV:
				val = sc->sc_rawdev_enabled;
	 			break;
			case ATH_RAWDEV_TYPE:
				switch (sc->sc_rawdev.type)
				{
					case ARPHRD_IEEE80211:
						val = 0;
						break;
					case ARPHRD_IEEE80211_PRISM:
						val = 1;
						break;
					case ARPHRD_IEEE80211_RADIOTAP:
						val = 2;
						break;
					default: 
						val = 0;
				}
	 			break;
			case ATH_RXFILTER:
#ifdef HAS_VMAC
			  val = ath_hal_getrxfilter(ah);
#else
				val = sc->sc_rxfilter;
#endif
				break;
			case ATH_RADARSIM:
				val = 0;
				break;
			default:
				return -EINVAL;
		}
		ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos);
	}/* end of read */
	return ret;
}

static const ctl_table ath_sysctl_template[] =
{
	{ .ctl_name	= ATH_SLOTTIME,
	  .procname	= "slottime",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_ACKTIMEOUT,
	  .procname	= "acktimeout",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_CTSTIMEOUT,
	  .procname	= "ctstimeout",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_SOFTLED,
	  .procname	= "softled",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_LEDPIN,
	  .procname	= "ledpin",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_COUNTRYCODE,
	  .procname	= "countrycode",
	  .mode		= 0444,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_REGDOMAIN,
	  .procname	= "regdomain",
	  .mode		= 0444,
	  .proc_handler	= ath_sysctl_halparam
	},
#ifdef AR_DEBUG
	{ .ctl_name	= ATH_DEBUG,
	  .procname	= "debug",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
#endif
	{ .ctl_name	= ATH_TXANTENNA,
	  .procname	= "txantenna",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_RXANTENNA,
	  .procname	= "rxantenna",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_DIVERSITY,
	  .procname	= "diversity",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_TXINTRPERIOD,
	  .procname	= "txintrperiod",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},/* 12 */
	{ .ctl_name	= ATH_TPSCALE,
	  .procname	= "tpscale",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_TPC,
	  .procname	= "tpc",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_TXPOWLIMIT,
	  .procname	= "txpowlimit",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_VEOL,
	  .procname	= "veol",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_BINTVAL,
	  .procname	= "bintval",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_RAWDEV,
	  .procname	= "rawdev",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_RAWDEV_TYPE,
	  .procname	= "rawdev_type",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_RXFILTER,
	  .procname	= "rxfilter",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ .ctl_name	= ATH_RADARSIM,
	  .procname	= "radarsim",
	  .mode		= 0644,
	  .proc_handler	= ath_sysctl_halparam
	},
	{ 0 }
};

void ath_dynamic_sysctl_register(struct ath_softc *sc)
{
	int i, space;

	space = 5*sizeof(struct ctl_table) + sizeof(ath_sysctl_template);
	sc->sc_sysctls = kmalloc(space, GFP_KERNEL);
	if (sc->sc_sysctls == NULL)
	{
		printk("%s: no memory for sysctl table!\n", __func__);
		return;
	}

	/* setup the table */
	memset(sc->sc_sysctls, 0, space);
	sc->sc_sysctls[0].ctl_name = CTL_DEV;
	sc->sc_sysctls[0].procname = "dev";
	sc->sc_sysctls[0].mode = 0555;
	sc->sc_sysctls[0].child = &sc->sc_sysctls[2];
	/* [1] is NULL terminator */
	sc->sc_sysctls[2].ctl_name = CTL_AUTO;
	sc->sc_sysctls[2].procname = sc->sc_dev.name;
	sc->sc_sysctls[2].mode = 0555;
	sc->sc_sysctls[2].child = &sc->sc_sysctls[4];
	/* [3] is NULL terminator */
	/* copy in pre-defined data */
	memcpy(&sc->sc_sysctls[4], ath_sysctl_template, sizeof(ath_sysctl_template));

	/* add in dynamic data references */
	for (i = 4; sc->sc_sysctls[i].ctl_name; i++)
		if (sc->sc_sysctls[i].extra1 == NULL)
			sc->sc_sysctls[i].extra1 = sc;

	/* and register everything */
	sc->sc_sysctl_header = register_sysctl_table(sc->sc_sysctls, 1);
	if (!sc->sc_sysctl_header)
	{
		printk("%s: failed to register sysctls!\n", sc->sc_dev.name);
		kfree(sc->sc_sysctls);
		sc->sc_sysctls = NULL;
	}

	/* initialize values */
#ifdef AR_DEBUG
	sc->sc_debug = ath_debug;
#endif
	sc->sc_txantenna = 0;		/* default to auto-selection */
	sc->sc_txintrperiod = ATH_TXINTR_PERIOD;
}

void ath_dynamic_sysctl_unregister(struct ath_softc *sc)
{
	if (sc->sc_sysctl_header)
	{
		unregister_sysctl_table(sc->sc_sysctl_header);
		sc->sc_sysctl_header = NULL;
	}
	if (sc->sc_sysctls)
	{
		kfree(sc->sc_sysctls);
		sc->sc_sysctls = NULL;
	}
}

/*
 * Static (i.e. global) sysctls.  Note that the hal sysctls
 * are located under ours by sharing the setting for DEV_ATH.
 */
enum
{
	DEV_ATH		= 9,			/* XXX known by hal */
};

static ctl_table ath_static_sysctls[] =
{
#ifdef AR_DEBUG
	{ .ctl_name	= CTL_AUTO,
	   .procname	= "debug",
	  .mode		= 0644,
	  .data		= &ath_debug,
	  .maxlen	= sizeof(ath_debug),
	  .proc_handler	= proc_dointvec
	},
#endif
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "countrycode",
	  .mode		= 0444,
	  .data		= &ath_countrycode,
	  .maxlen	= sizeof(ath_countrycode),
	  .proc_handler	= proc_dointvec
	},
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "regdomain",
	  .mode		= 0444,
	  .data		= &ath_regdomain,
	  .maxlen	= sizeof(ath_regdomain),
	  .proc_handler	= proc_dointvec
	},
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "outdoor",
	  .mode		= 0444,
	  .data		= &ath_outdoor,
	  .maxlen	= sizeof(ath_outdoor),
	  .proc_handler	= proc_dointvec
	},
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "xchanmode",
	  .mode		= 0444,
	  .data		= &ath_xchanmode,
	  .maxlen	= sizeof(ath_xchanmode),
	  .proc_handler	= proc_dointvec
	},
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "dwelltime",
	  .mode		= 0644,
	  .data		= &ath_dwelltime,
	  .maxlen	= sizeof(ath_dwelltime),
	  .extra1	= &mindwelltime,
	  .extra2	= &maxint,
	  .proc_handler	= proc_dointvec_minmax
	},
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "calibrate",
	  .mode		= 0644,
	  .data		= &ath_calinterval,
	  .maxlen	= sizeof(ath_calinterval),
	  .extra1	= &mincalibrate,
	  .extra2	= &maxint,
	  .proc_handler	= proc_dointvec_minmax
	},
	{ 0 }
};

static ctl_table ath_ath_table[] = 
{
	{ 
		.ctl_name	= DEV_ATH,
		.procname	= "ath",
		.mode		= 0555,
		.child	= ath_static_sysctls
	}, 
	{ 0 }
};

static ctl_table ath_root_table[] =
{
	{
		.ctl_name	= CTL_DEV,
		.procname	= "dev",
		.mode		= 0555,
		.child	= ath_ath_table
	}, 
	{ 0 }
};

static struct ctl_table_header *ath_sysctl_header;

void ath_sysctl_register(void)
{
	static int initialized = 0;

	if (!initialized)
	{
		ath_sysctl_header = register_sysctl_table(ath_root_table, 1);
		initialized = 1;
	}
}

void ath_sysctl_unregister(void)
{
	if (ath_sysctl_header)
		unregister_sysctl_table(ath_sysctl_header);
}

#endif

