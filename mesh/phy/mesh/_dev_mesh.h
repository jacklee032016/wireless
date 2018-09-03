/*
* $Id: _dev_mesh.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#ifndef	___DEV_MESH_H__
#define	___DEV_MESH_H__

#include "phy.h"

u_int _phy_chan2flags(struct ieee80211com *ic, struct ieee80211_channel *chan);
void	_phy_mode_init(MESH_DEVICE *);
void	_phy_chan_change(struct ath_softc *, struct ieee80211_channel *);

int	_phy_startrecv(struct ath_softc *);
void	_phy_stoprecv(struct ath_softc *);


#endif

