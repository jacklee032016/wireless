/*
* $Id: _phy_init.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef	___PHY_INIT_H__
#define	___PHY_INIT_H__

#include "phy.h"

/* from phy_utils.c */
int _phy_rate_setup(MESH_DEVICE *mdev, u_int mode);
int _phy_desc_alloc(struct ath_softc *sc);
void _phy_desc_free(struct ath_softc *sc);
int _phy_beaconq_setup(struct ath_hal *ah);
struct ieee80211_node *_phy_node_alloc(struct ieee80211_node_table *nt);
void _phy_node_free(struct ieee80211_node *ni);
u_int8_t _phy_node_getrssi(const struct ieee80211_node *ni);
void _phy_recv_mgmt(struct ieee80211com *ic, struct sk_buff *skb,struct ieee80211_node *ni,	int subtype, int rssi, u_int32_t rstamp);
void _phy_announce(struct ath_softc *sc);

void _phy_calibrate(unsigned long arg);
void _phy_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew);
void	_phy_updateslot(MESH_DEVICE *);
int	_phy_newstate(struct ieee80211com *, enum ieee80211_state, int);

void	_phy_next_scan(unsigned long);
int	_phy_getchannels(MESH_DEVICE *, u_int cc,	HAL_BOOL outdoor, HAL_BOOL xchanmode);

/* from init_tasklet.c */
void _phy_fatal_tasklet(TQUEUE_ARG data);//void *data);
void _phy_radar_tasklet (TQUEUE_ARG data);//void *data);
void _phy_rxorn_tasklet(TQUEUE_ARG data);
void _phy_bmiss_tasklet(TQUEUE_ARG data);
void _phy_bstuck_tasklet(TQUEUE_ARG data);
void _phy_tx_tasklet_q0(TQUEUE_ARG data);
void _phy_tx_tasklet_q0123(TQUEUE_ARG data);
void _phy_tx_tasklet(TQUEUE_ARG data);

#endif

