/*
* $Id: if_ath_rx.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
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


/* Add additional headers to a received frame and netif_rx it on a monitor or raw device */
static void ath_rx_capture(struct net_device *dev, struct ath_desc *ds, struct sk_buff *skb)
{
#define	IS_QOS_DATA(wh) \
	((wh->i_fc[0] & (IEEE80211_FC0_TYPE_MASK|IEEE80211_FC0_SUBTYPE_MASK))==\
		(IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS))
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	int len = ds->ds_rxstat.rs_datalen;
	struct ieee80211_frame *wh;
	u_int32_t tsf;

	KASSERT(ic->ic_flags & IEEE80211_F_DATAPAD, ("data padding not enabled?"));
	/* Remove pad bytes */
	wh = (struct ieee80211_frame *) skb->data;
	if (IS_QOS_DATA(wh))
	{
		int headersize = ieee80211_hdrsize(wh);
		int padbytes = roundup(headersize,4) - headersize;

		/* Copy up 802.11 header and strip h/w padding. */
		if (padbytes > 0)
		{
			memmove(skb->data + padbytes, skb->data, headersize);
			skb_pull(skb, padbytes);
			len -= padbytes;
		}
	}

	switch (dev->type)
	{
		case ARPHRD_IEEE80211:
			break;
		case ARPHRD_IEEE80211_PRISM:
		{
			wlan_ng_prism2_header *ph;
			if (skb_headroom(skb) < sizeof(wlan_ng_prism2_header))
			{
				DPRINTF(sc, ATH_DEBUG_RECV, "%s: prism not enough headroom %d/%d\n", __func__, skb_headroom(skb), sizeof(wlan_ng_prism2_header));
				goto bad;
			}
			ph = (wlan_ng_prism2_header *) skb_push(skb, sizeof(wlan_ng_prism2_header));
			memset(ph, 0, sizeof(wlan_ng_prism2_header));
			
			ph->msgcode = DIDmsg_lnxind_wlansniffrm;
			ph->msglen = sizeof(wlan_ng_prism2_header);
			strcpy(ph->devname, sc->sc_dev.name);
			
			ph->hosttime.did = DIDmsg_lnxind_wlansniffrm_hosttime;
			ph->hosttime.status = 0;
			ph->hosttime.len = 4;
			ph->hosttime.data = jiffies;
			
			/* Pass up tsf clock in mactime */
			ph->mactime.did = DIDmsg_lnxind_wlansniffrm_mactime;
			ph->mactime.status = 0;
			ph->mactime.len = 4;
			/*
			 * Rx descriptor has the low 15 bits of the tsf at
			 * the time the frame was received.  Use the current
			 * tsf to extend this to 32 bits.
			 */
			tsf = ath_hal_gettsf32(sc->sc_ah);
			if ((tsf & 0x7fff) < ds->ds_rxstat.rs_tstamp)
				tsf -= 0x8000;
			ph->mactime.data = ds->ds_rxstat.rs_tstamp | (tsf &~ 0x7fff);
			
			ph->istx.did = DIDmsg_lnxind_wlansniffrm_istx;
			ph->istx.status = 0;
			ph->istx.len = 4;
			ph->istx.data = P80211ENUM_truth_false;
			
			ph->frmlen.did = DIDmsg_lnxind_wlansniffrm_frmlen;
			ph->frmlen.status = 0;
			ph->frmlen.len = 4;
			ph->frmlen.data = len;
			
			ph->channel.did = DIDmsg_lnxind_wlansniffrm_channel;
			ph->channel.status = 0;
			ph->channel.len = 4;
			ph->channel.data = ieee80211_mhz2ieee(ic->ic_ibss_chan->ic_freq,0);
			
			ph->rssi.did = DIDmsg_lnxind_wlansniffrm_rssi;
			ph->rssi.status = 0;
			ph->rssi.len = 4;
			ph->rssi.data = ds->ds_rxstat.rs_rssi;
			
			ph->noise.did = DIDmsg_lnxind_wlansniffrm_noise;
			ph->noise.status = 0;
			ph->noise.len = 4;
			ph->noise.data = -95;
			
			ph->signal.did = DIDmsg_lnxind_wlansniffrm_signal;
			ph->signal.status = 0;
			ph->signal.len = 4;
			ph->signal.data = -95 + ds->ds_rxstat.rs_rssi;
			
			ph->rate.did = DIDmsg_lnxind_wlansniffrm_rate;
			ph->rate.status = 0;
			ph->rate.len = 4;
			ph->rate.data = sc->sc_hwmap[ds->ds_rxstat.rs_rate].ieeerate;
			break;
		}

		case ARPHRD_IEEE80211_RADIOTAP:
		{
			struct ath_rx_radiotap_header *th;
			if (skb_headroom(skb) < sizeof(struct ath_rx_radiotap_header))
			{
				DPRINTF(sc, ATH_DEBUG_RECV, "%s: radiotap not enough headroom %d/%d\n", __func__, 
					skb_headroom(skb), sizeof(struct ath_rx_radiotap_header));
				goto bad;
			}
			th = (struct ath_rx_radiotap_header  *) skb_push(skb, sizeof(struct ath_rx_radiotap_header));
			memset(th, 0, sizeof(struct ath_rx_radiotap_header));

			th->wr_ihdr.it_version = 0;
			th->wr_ihdr.it_len = sizeof(struct ath_rx_radiotap_header);
			th->wr_ihdr.it_present = ATH_RX_RADIOTAP_PRESENT;
			th->wr_flags = IEEE80211_RADIOTAP_F_FCS;
			th->wr_rate = sc->sc_hwmap[ds->ds_rxstat.rs_rate].ieeerate;
			th->wr_chan_freq = ic->ic_ibss_chan->ic_freq;
			th->wr_chan_flags = ic->ic_ibss_chan->ic_flags;
			th->wr_antenna = ds->ds_rxstat.rs_antenna;
			th->wr_antsignal = ds->ds_rxstat.rs_rssi;
			th->wr_rx_flags = 0;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_CRC)
				th->wr_rx_flags |= IEEE80211_RADIOTAP_F_RX_BADFCS;

			break;
		}
		default:
			break;
	}
	
	skb->dev = dev;
	skb->mac.raw = skb->data;
	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = __constant_htons(0x0019);  /* ETH_P_80211_RAW */

	netif_rx(skb);
	return;

 bad:
	dev_kfree_skb(skb);
	return;
#undef IS_QOS_DATA
}

void ath_rx_tasklet(TQUEUE_ARG data)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((caddr_t)(_sc)->sc_desc + \
		((_pa) - (_sc)->sc_desc_daddr)))
		
	struct net_device *dev = (struct net_device *)data;
	struct ath_buf *bf;
	struct ath_softc *sc = dev->priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds;
	struct sk_buff *skb;
	struct ieee80211_node *ni;
	struct ath_node *an;
	int len, type;
	u_int phyerr;
	HAL_STATUS status;

	DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s\n", __func__);
	
#ifdef HAS_VMAC
	if ((sc->sc_vmac) /*&&(sc->sc_ic.ic_opmode == IEEE80211_M_MONITOR)*/)
	{
		ath_cu_softmac_rx_tasklet(data);
		return;
	}
#endif

	do
	{
		bf = STAILQ_FIRST(&sc->sc_rxbuf);
		if (bf == NULL)
		{		/* XXX ??? can this happen */
			if_printf(dev, "%s: no buffer!\n", __func__);
			break;
		}
		ds = bf->bf_desc;
		if (ds->ds_link == bf->bf_daddr)
		{
			/* NB: never process the self-linked entry at the end */
			break;
		}
		skb = bf->bf_skb;
		if (skb == NULL)
		{/* XXX ??? can this happen */
			if_printf(dev, "%s: no skbuff!\n", __func__);
			continue;
		}
		/* XXX sync descriptor memory */
		/*
		 * Must provide the virtual address of the current
		 * descriptor, the physical address, and the virtual
		 * address of the next descriptor in the h/w chain.
		 * This allows the HAL to look ahead to see if the
		 * hardware is done with a descriptor by checking the
		 * done bit in the following descriptor and the address
		 * of the current descriptor the DMA engine is working
		 * on.  All this is necessary because of our use of
		 * a self-linked list to avoid rx overruns.
		 */
		status = ath_hal_rxprocdesc(ah, ds, bf->bf_daddr, PA2DESC(sc, ds->ds_link));
#ifdef AR_DEBUG
		if (sc->sc_debug & ATH_DEBUG_RECV_DESC)
			ath_printrxbuf(bf, status == HAL_OK); 
#endif
		if (status == HAL_EINPROGRESS)
			break;
		STAILQ_REMOVE_HEAD(&sc->sc_rxbuf, bf_list);
		if (ds->ds_rxstat.rs_more) {
			/*
			 * Frame spans multiple descriptors; this
			 * cannot happen yet as we don't support
			 * jumbograms.  If not in monitor mode,
			 * discard the frame.
			 */
#ifndef ERROR_FRAMES
			/*
			 * Enable this if you want to see
			 * error frames in Monitor mode.
			 */
			if (ic->ic_opmode != IEEE80211_M_MONITOR)
			{
				sc->sc_stats.ast_rx_toobig++;
				goto rx_next;
			}
#endif
			/* fall thru for monitor mode handling... */
		}
		else if (ds->ds_rxstat.rs_status != 0)
		{
			if (ds->ds_rxstat.rs_status & HAL_RXERR_CRC)
				sc->sc_stats.ast_rx_crcerr++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_FIFO)
				sc->sc_stats.ast_rx_fifoerr++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_PHY) {
				sc->sc_stats.ast_rx_phyerr++;
				phyerr = ds->ds_rxstat.rs_phyerr & 0x1f;
				sc->sc_stats.ast_rx_phy[phyerr]++;

				if (phyerr == HAL_PHYERR_RADAR && ic->ic_opmode == IEEE80211_M_HOSTAP)
				{
					ATH_SCHEDULE_TQUEUE (&sc->sc_radartq, &needmark);
				}

				goto rx_next;
			}
			if (ds->ds_rxstat.rs_status & HAL_RXERR_DECRYPT) {
				/*
				 * Decrypt error.  If the error occurred
				 * because there was no hardware key, then
				 * let the frame through so the upper layers
				 * can process it.  This is necessary for 5210
				 * parts which have no way to setup a ``clear''
				 * key cache entry.
				 *
				 * XXX do key cache faulting
				 */
				if (ds->ds_rxstat.rs_keyix == HAL_RXKEYIX_INVALID)
					goto rx_accept;
				sc->sc_stats.ast_rx_badcrypt++;
			}
			if (ds->ds_rxstat.rs_status & HAL_RXERR_MIC) {
				sc->sc_stats.ast_rx_badmic++;
				/*
				 * Do minimal work required to hand off
				 * the 802.11 header for notifcation.
				 */
				/* XXX frag's and qos frames */
				len = ds->ds_rxstat.rs_datalen;
				if (len >= sizeof (struct ieee80211_frame)) {
					bus_dma_sync_single(sc->sc_bdev,
					    bf->bf_skbaddr, len,
					    BUS_DMA_FROMDEVICE);
					ieee80211_notify_michael_failure(ic,
						(struct ieee80211_frame *) skb->data,
					    sc->sc_splitmic ?
					        ds->ds_rxstat.rs_keyix-32 :
					        ds->ds_rxstat.rs_keyix
					);
				}
			}

			// TODO: correct?
			ic->ic_devstats->rx_errors++;

			/*
			 * accept error frames on the raw device
			 * or in monitor mode if we ask for them.
			 * we'll explicity drop them after capture.
			 */
			if (sc->sc_rxfilter & HAL_RX_FILTER_PHYERR)
				goto rx_accept;

			/*
			 * Reject error frames, we normally don't want
			 * to see them in monitor mode (in monitor mode
			 * allow through packets that have crypto problems).
			 */
			if ((ds->ds_rxstat.rs_status &~
			     (HAL_RXERR_DECRYPT|HAL_RXERR_MIC)) ||
			    sc->sc_ic.ic_opmode != IEEE80211_M_MONITOR)
				goto rx_next;
		}
rx_accept:
		/*
		 * Sync and unmap the frame.  At this point we're
		 * committed to passing the sk_buff somewhere so
		 * clear buf_skb; this means a new sk_buff must be
		 * allocated when the rx descriptor is setup again
		 * to receive another frame.
		 */
		len = ds->ds_rxstat.rs_datalen;
		bus_dma_sync_single(sc->sc_bdev,
			bf->bf_skbaddr, len, BUS_DMA_FROMDEVICE);
		bus_unmap_single(sc->sc_bdev, bf->bf_skbaddr,
			sc->sc_rxbufsize, BUS_DMA_FROMDEVICE);
		bf->bf_skb = NULL;


		sc->sc_stats.ast_ant_rx[ds->ds_rxstat.rs_antenna]++;
		
		if (len < IEEE80211_ACK_LEN) {
			DPRINTF(sc, ATH_DEBUG_RECV,
				"%s: runt packet %d\n", __func__, len);
			sc->sc_stats.ast_rx_tooshort++;
			dev_kfree_skb(skb);
			goto rx_next;
		}

		KASSERT(len <= skb_tailroom(skb), 
			("not enough tailroom (%d vs %d)",
			 len, skb_tailroom(skb)));

		skb_put(skb, len);
		skb->protocol = ETH_P_CONTROL;		/* XXX */

		if (sc->sc_rawdev_enabled && (sc->sc_rawdev.flags & IFF_UP))
		{
			struct sk_buff *skb2;
			skb2 = skb_copy(skb, GFP_ATOMIC);
			if (skb2)
			{
				ath_rx_capture(&sc->sc_rawdev, ds, skb2);
			}
		}
		
		if (ic->ic_opmode == IEEE80211_M_MONITOR)
		{
			/*
			 * Monitor mode: discard anything shorter than
			 * an ack or cts, clean the skbuff, fabricate
			 * the Prism header existing tools expect,
			 * and dispatch.
			 */
			/* XXX TSF */

			ath_rx_capture(dev, ds, skb);
			goto rx_next;
		}


		/*
		 * At this point we have no need for error frames
		 * that aren't crypto problems since we're done
		 * with capture.
		 */
		if (ds->ds_rxstat.rs_status &~ (HAL_RXERR_DECRYPT|HAL_RXERR_MIC))
		{
			dev_kfree_skb(skb);
			goto rx_next;
		}
		/*
		 * From this point on we assume the frame is at least
		 * as large as ieee80211_frame_min; verify that.
		 */
		if (len < IEEE80211_MIN_LEN)
		{
			DPRINTF(sc, ATH_DEBUG_RECV, "%s: short packet %d\n", __func__, len);
			sc->sc_stats.ast_rx_tooshort++;
			dev_kfree_skb(skb);
			goto rx_next;
		}

		/*
		 * Normal receive.
		 */
		if (IFF_DUMPPKTS(sc, ATH_DEBUG_RECV))
		{
			ieee80211_dump_pkt(skb->data, len, sc->sc_hwmap[ds->ds_rxstat.rs_rate].ieeerate, ds->ds_rxstat.rs_rssi);
		}

		skb_trim(skb, skb->len - IEEE80211_CRC_LEN);

		/*
		 * Locate the node for sender, track state, and then
		 * pass the (referenced) node up to the 802.11 layer
		 * for its use.
		 */
		ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)skb->data);

		/*
		 * Track rx rssi and do any rx antenna management.
		 */
		an = ATH_NODE(ni);
		ATH_RSSI_LPF(an->an_avgrssi, ds->ds_rxstat.rs_rssi);
		if (sc->sc_diversity) {
			/*
			 * When using fast diversity, change the default rx
			 * antenna if diversity chooses the other antenna 3
			 * times in a row.
			 */
			if (sc->sc_defant != ds->ds_rxstat.rs_antenna) {
				if (++sc->sc_rxotherant >= 3)
					ath_setdefantenna(sc,
						ds->ds_rxstat.rs_antenna);
			} else
				sc->sc_rxotherant = 0;
		}

		/*
		 * Send frame up for processing.
		 */
		type = ieee80211_input(ic, skb, ni, ds->ds_rxstat.rs_rssi, ds->ds_rxstat.rs_tstamp);

		if (sc->sc_softled)
		{
			/*
			 * Blink for any data frame.  Otherwise do a
			 * heartbeat-style blink when idle.  The latter
			 * is mainly for station mode where we depend on
			 * periodic beacon frames to trigger the poll event.
			 */
			if (type == IEEE80211_FC0_TYPE_DATA)
			{
				sc->sc_rxrate = ds->ds_rxstat.rs_rate;
				ath_led_event(sc, ATH_LED_RX);
			}
			else if (jiffies - sc->sc_ledevent >= sc->sc_ledidle)
				ath_led_event(sc, ATH_LED_POLL);
		}

		/*
		 * Reclaim node reference.
		 */
		ieee80211_free_node(ni);
rx_next:
		STAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	} while (ath_rxbuf_init(sc, bf) == 0);

	/* rx signal state monitoring */
	ath_hal_rxmonitor(ah, &ATH_NODE(ic->ic_bss)->an_halstats);

#undef PA2DESC
}

