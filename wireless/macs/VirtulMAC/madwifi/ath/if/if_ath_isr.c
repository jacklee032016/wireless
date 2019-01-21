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

#include <net80211/ieee80211_var.h>

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

/* Interrupt handler.  Most of the actual processing is deferred. */
irqreturn_t ath_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	struct ath_softc *sc = dev->priv;
	struct ath_hal *ah = sc->sc_ah;
	HAL_INT status;
	int needmark;

	if (sc->sc_invalid)
	{	/* The hardware is not ready/present, don't touch anything.
		 * Note this can happen early on if the IRQ is shared.	 */
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid; ignored\n", __func__);
		return IRQ_NONE;
	}
	if (!ath_hal_intrpend(ah))		/* shared irq, not for us */
		return IRQ_NONE;
	
	if ((dev->flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP))
	{
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags 0x%x\n", __func__, dev->flags);
		ath_hal_getisr(ah, &status);	/* clear ISR */
		ath_hal_intrset(ah, 0);		/* disable further intr's */
		return IRQ_HANDLED;
	}
	needmark = 0;
	/*
	 * Figure out the reason(s) for the interrupt.  Note
	 * that the hal returns a pseudo-ISR that may include
	 * bits we haven't explicitly enabled so we mask the
	 * value to insure we only process bits we requested.
	 */
	ath_hal_getisr(ah, &status);		/* NB: clears ISR too */
	DPRINTF(sc, ATH_DEBUG_INTR, "%s: status 0x%x\n", __func__, status);
	status &= sc->sc_imask;			/* discard unasked for bits */
	
	if (status & HAL_INT_FATAL)
	{	/*
		 * Fatal errors are unrecoverable.  Typically
		 * these are caused by DMA errors.  Unfortunately
		 * the exact reason is not (presently) returned
		 * by the hal.
		 */
		sc->sc_stats.ast_hardware++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		ATH_SCHEDULE_TQUEUE(&sc->sc_fataltq, &needmark);
	}
	else if (status & HAL_INT_RXORN)
	{
		sc->sc_stats.ast_rxorn++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		ATH_SCHEDULE_TQUEUE(&sc->sc_rxorntq, &needmark);
	}
	else
	{
		if (status & HAL_INT_SWBA)
		{
			/*
			 * Software beacon alert--time to send a beacon.
			 * Handle beacon transmission directly; deferring
			 * this is too slow to meet timing constraints
			 * under load.
			 */
			ath_beacon_tasklet(dev);
		}
		if (status & HAL_INT_RXEOL)
		{
			/*
			 * NB: the hardware should re-read the link when
			 *     RXE bit is written, but it doesn't work at
			 *     least on older hardware revs.
			 */
			sc->sc_stats.ast_rxeol++;
			sc->sc_rxlink = NULL;
		}
		if (status & HAL_INT_TXURN)
		{
			sc->sc_stats.ast_txurn++;
			/* bump tx trigger level */
			ath_hal_updatetxtriglevel(ah, AH_TRUE);
		}
		if (status & HAL_INT_RX)
#ifdef HAS_VMAC
		{
			if ((sc->sc_vmac) &&	/*(sc->sc_ic.ic_opmode == IEEE80211_M_MONITOR) &&*/
				!((sc->sc_vmac_options & CU_SOFTMAC_ATH_DEFER_ALL_RX)))
			{
				if (ath_cu_softmac_handle_rx(dev,1))
				{
					ATH_SCHEDULE_TQUEUE(&sc->sc_vmac_worktq,&needmark);
				}
			}
			else
			{
				ATH_SCHEDULE_TQUEUE(&sc->sc_rxtq, &needmark);
			}
		}
#else
			ATH_SCHEDULE_TQUEUE(&sc->sc_rxtq, &needmark);
#endif
		if (status & HAL_INT_TX)
#ifdef HAS_VMAC
		{
			if ((sc->sc_vmac) &&	/*(sc->sc_ic.ic_opmode == IEEE80211_M_MONITOR) &&*/
				!(sc->sc_vmac_options & CU_SOFTMAC_ATH_DEFER_ALL_TXDONE))
			{
				if (ath_cu_softmac_handle_txdone(dev,1))
				{
					ATH_SCHEDULE_TQUEUE(&sc->sc_vmac_worktq, &needmark);
				}
			}
			else
			{
				ATH_SCHEDULE_TQUEUE(&sc->sc_txtq, &needmark);
			}
		}
#else
			ATH_SCHEDULE_TQUEUE(&sc->sc_txtq, &needmark);
#endif
		if (status & HAL_INT_BMISS)
		{
			sc->sc_stats.ast_bmiss++;
			ATH_SCHEDULE_TQUEUE(&sc->sc_bmisstq, &needmark);
		}
		if (status & HAL_INT_MIB)
		{
			sc->sc_stats.ast_mib++;
			/*
			 * Disable interrupts until we service the MIB
			 * interrupt; otherwise it will continue to fire.
			 */
			ath_hal_intrset(ah, 0);
			/*
			 * Let the hal handle the event.  We assume it will
			 * clear whatever condition caused the interrupt.
			 */
			ath_hal_mibevent(ah, &ATH_NODE(sc->sc_ic.ic_bss)->an_halstats);
			ath_hal_intrset(ah, sc->sc_imask);
		}
	}
	
	if (needmark)
	{
		mark_bh(IMMEDIATE_BH);
	}
	return IRQ_HANDLED;
}

