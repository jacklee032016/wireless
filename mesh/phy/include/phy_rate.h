/*
* $Id: phy_rate.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#ifndef __PHY_RATE_H__
#define __PHY_RATE_H__

/*
 * Interface definitions for transmit rate control modules for the Atheros driver.
 *
 * A rate control module is responsible for choosing the transmit rate
 * for each data frame.  Management+control frames are always sent at
 * a fixed rate.
 *
 * Only one module may be present at a time; the driver references
 * rate control interfaces by symbol name.  If multiple modules are
 * to be supported we'll need to switch to a registration-based scheme
 * as is currently done, for example, for authentication modules.
 *
 * An instance of the rate control module is attached to each device
 * at attach time and detached when the device is destroyed.  The module
 * may associate data with each device and each node (station).  Both
 * sets of storage are opaque except for the size of the per-node storage
 * which must be provided when the module is attached.
 *
 * The rate control module is notified for each state transition and
 * station association/reassociation.  Otherwise it is queried for a
 * rate for each outgoing frame and provided status from each transmitted
 * frame.  Any ancillary processing is the responsibility of the module
 * (e.g. if periodic processing is required then the module should setup
 * it's own timer).
 *
 * In addition to the transmit rate for each frame the module must also
 * indicate the number of attempts to make at the specified rate.  If this
 * number is != ATH_TXMAXTRY then an additional callback is made to setup
 * additional transmit state.  The rate control code is assumed to write
 * this additional data directly to the transmit descriptor.
 */
struct ath_softc;
struct ath_node;
struct ath_desc;

struct ath_ratectrl
{
	size_t	arc_space;	/* space required for per-node state */
};
/*
 * Attach/detach a rate control module.
 */
struct ath_ratectrl *phy_rate_attach(struct ath_softc *);
void	phy_rate_detach(struct ath_ratectrl *);

#ifdef CONFIG_SYSCTL
/*
 * Allow rate control module to register dynamic sysctls, after
 * dev->name is filled in; Modules are expected to unregister dynamic
 * sysctls in phy_rate_detach().
 */
void    ath_rate_dynamic_sysctl_register(struct ath_softc *);
#endif /* CONFIG_SYSCTL */

/*
 * State storage handling.
 */
/*
 * Initialize per-node state already allocated for the specified
 * node; this space can be assumed initialized to zero.
 */
void	phy_rate_node_init(struct ath_softc *, struct ath_node *);
/*
 * Cleanup any per-node state prior to the node being reclaimed.
 */
void	phy_rate_node_cleanup(struct ath_softc *, struct ath_node *);
/*
 * Update rate control state on station associate/reassociate 
 * (when operating as an ap or for nodes discovered when operating
 * in ibss mode).
 */
void	phy_rate_newassoc(struct ath_softc *, struct ath_node *, int isNewAssociation);
/*
 * Update/reset rate control state for 802.11 state transitions.
 * Important mostly as the analog to phy_rate_newassoc when operating
 * in station mode.
 */
void	phy_rate_newstate(struct ath_softc *, enum ieee80211_state);

/*
 * Transmit handling.
 */
/*
 * Return the transmit info for a data packet.  If multi-rate state
 * is to be setup then try0 should contain a value other than ATH_TXMATRY
 * and phy_rate_setupxtxdesc will be called after deciding if the frame
 * can be transmitted with multi-rate retry.
 */
void	phy_rate_findrate(struct ath_softc *, struct ath_node *, int shortPreamble, size_t frameLen, u_int8_t *rix, int *try0, u_int8_t *txrate);
/*
 * Setup any extended (multi-rate) descriptor state for a data packet.
 * The rate index returned by phy_rate_findrate is passed back in.
 */
void	phy_rate_setupxtxdesc(struct ath_softc *, struct ath_node *, struct ath_desc *, int shortPreamble, u_int8_t rix);
/*
 * Update rate control state for a packet associated with the
 * supplied transmit descriptor.  The routine is invoked both
 * for packets that were successfully sent and for those that
 * failed (consult the descriptor for details).
 */
void	phy_rate_tx_complete(struct ath_softc *, struct ath_node *, const struct ath_desc *);
#endif

