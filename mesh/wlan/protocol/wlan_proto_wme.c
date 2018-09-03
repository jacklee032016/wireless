/*
* $Id: wlan_proto_wme.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_protocol.h"

/*
 * WME protocol support.  The following parameters come from the spec.
 */
typedef struct phyParamType
{
	u_int8_t 		aifsn; 
	u_int8_t 		logcwmin;
	u_int8_t 		logcwmax; 
	u_int16_t 	txopLimit;
	u_int8_t 		acm;
} paramType;

static const struct phyParamType phyParamForAC_BE[IEEE80211_MODE_MAX] = 
{
	{ 3, 4, 6 },		/* IEEE80211_MODE_AUTO */
	{ 3, 4, 6 },		/* IEEE80211_MODE_11A */ 
	{ 3, 5, 7 },		/* IEEE80211_MODE_11B */ 
	{ 3, 4, 6 },		/* IEEE80211_MODE_11G */ 
	{ 3, 5, 7 },		/* IEEE80211_MODE_FH */ 
	{ 2, 3, 5 },		/* IEEE80211_MODE_TURBO_A */ 
	{ 2, 3, 5 },		/* IEEE80211_MODE_TURBO_G */ 
};

static const struct phyParamType phyParamForAC_BK[IEEE80211_MODE_MAX] = 
{
	{ 7, 4, 10 },		/* IEEE80211_MODE_AUTO */
	{ 7, 4, 10 },		/* IEEE80211_MODE_11A */ 
	{ 7, 5, 10 },		/* IEEE80211_MODE_11B */ 
	{ 7, 4, 10 },		/* IEEE80211_MODE_11G */ 
	{ 7, 5, 10 },		/* IEEE80211_MODE_FH */ 
	{ 7, 3, 10 },		/* IEEE80211_MODE_TURBO_A */ 
	{ 7, 3, 10 },		/* IEEE80211_MODE_TURBO_G */ 
};

static const struct phyParamType phyParamForAC_VI[IEEE80211_MODE_MAX] = 
{
	{ 1, 3, 4,  94 },	/* IEEE80211_MODE_AUTO */
	{ 1, 3, 4,  94 },	/* IEEE80211_MODE_11A */ 
	{ 1, 4, 5, 188 },	/* IEEE80211_MODE_11B */ 
	{ 1, 3, 4,  94 },	/* IEEE80211_MODE_11G */ 
	{ 1, 4, 5, 188 },	/* IEEE80211_MODE_FH */ 
	{ 1, 2, 3,  94 },	/* IEEE80211_MODE_TURBO_A */ 
	{ 1, 2, 3,  94 },	/* IEEE80211_MODE_TURBO_G */ 
};

static const struct phyParamType phyParamForAC_VO[IEEE80211_MODE_MAX] = 
{
	{ 1, 2, 3,  47 },	/* IEEE80211_MODE_AUTO */
	{ 1, 2, 3,  47 },	/* IEEE80211_MODE_11A */ 
	{ 1, 3, 4, 102 },	/* IEEE80211_MODE_11B */ 
	{ 1, 2, 3,  47 },	/* IEEE80211_MODE_11G */ 
	{ 1, 3, 4, 102 },	/* IEEE80211_MODE_FH */ 
	{ 1, 2, 2,  47 },	/* IEEE80211_MODE_TURBO_A */ 
	{ 1, 2, 2,  47 },	/* IEEE80211_MODE_TURBO_G */ 
};

static const struct phyParamType bssPhyParamForAC_BE[IEEE80211_MODE_MAX] = 
{
	{ 3, 4, 10 },		/* IEEE80211_MODE_AUTO */
	{ 3, 4, 10 },		/* IEEE80211_MODE_11A */ 
	{ 3, 5, 10 },		/* IEEE80211_MODE_11B */ 
	{ 3, 4, 10 },		/* IEEE80211_MODE_11G */ 
	{ 3, 5, 10 },		/* IEEE80211_MODE_FH */ 
	{ 2, 3, 10 },		/* IEEE80211_MODE_TURBO_A */ 
	{ 2, 3, 10 },		/* IEEE80211_MODE_TURBO_G */ 
};

static const struct phyParamType bssPhyParamForAC_VI[IEEE80211_MODE_MAX] = 
{
	{ 2, 3, 4,  94 },	/* IEEE80211_MODE_AUTO */
	{ 2, 3, 4,  94 },	/* IEEE80211_MODE_11A */ 
	{ 2, 4, 5, 188 },	/* IEEE80211_MODE_11B */ 
	{ 2, 3, 4,  94 },	/* IEEE80211_MODE_11G */ 
	{ 2, 4, 5, 188 },	/* IEEE80211_MODE_FH */ 
	{ 2, 2, 3,  94 },	/* IEEE80211_MODE_TURBO_A */ 
	{ 2, 2, 3,  94 },	/* IEEE80211_MODE_TURBO_G */ 
};

static const struct phyParamType bssPhyParamForAC_VO[IEEE80211_MODE_MAX] = 
{
	{ 2, 2, 3,  47 },	/* IEEE80211_MODE_AUTO */
	{ 2, 2, 3,  47 },	/* IEEE80211_MODE_11A */ 
	{ 2, 3, 4, 102 },	/* IEEE80211_MODE_11B */ 
	{ 2, 2, 3,  47 },	/* IEEE80211_MODE_11G */ 
	{ 2, 3, 4, 102 },	/* IEEE80211_MODE_FH */ 
	{ 1, 2, 2,  47 },	/* IEEE80211_MODE_TURBO_A */ 
	{ 1, 2, 2,  47 },	/* IEEE80211_MODE_TURBO_G */ 
};

void ieee80211_wme_initparams(struct ieee80211com *ic)
{
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	const paramType *pPhyParam, *pBssPhyParam;
	struct wmeParams *wmep;
	int i;

	if ((ic->ic_caps & IEEE80211_C_WME) == 0)
		return;

	for (i = 0; i < WME_NUM_AC; i++)
	{
		switch (i)
		{
			case WME_AC_BK:
				pPhyParam = &phyParamForAC_BK[ic->ic_curmode];
				pBssPhyParam = &phyParamForAC_BK[ic->ic_curmode];
				break;
			case WME_AC_VI:
				pPhyParam = &phyParamForAC_VI[ic->ic_curmode];
				pBssPhyParam = &bssPhyParamForAC_VI[ic->ic_curmode];
				break;
			case WME_AC_VO:
				pPhyParam = &phyParamForAC_VO[ic->ic_curmode];
				pBssPhyParam = &bssPhyParamForAC_VO[ic->ic_curmode];
				break;
			case WME_AC_BE:
			default:
				pPhyParam = &phyParamForAC_BE[ic->ic_curmode];
				pBssPhyParam = &bssPhyParamForAC_BE[ic->ic_curmode];
				break;
		}

		wmep = &wme->wme_wmeChanParams.cap_wmeParams[i];
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		{
			wmep->wmep_acm = pPhyParam->acm;
			wmep->wmep_aifsn = pPhyParam->aifsn;	
			wmep->wmep_logcwmin = pPhyParam->logcwmin;	
			wmep->wmep_logcwmax = pPhyParam->logcwmax;		
			wmep->wmep_txopLimit = pPhyParam->txopLimit;
		}
		else
		{
			wmep->wmep_acm = pBssPhyParam->acm;
			wmep->wmep_aifsn = pBssPhyParam->aifsn;	
			wmep->wmep_logcwmin = pBssPhyParam->logcwmin;	
			wmep->wmep_logcwmax = pBssPhyParam->logcwmax;		
			wmep->wmep_txopLimit = pBssPhyParam->txopLimit;

		}
		
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
			"%s: %s chan [acm %u aifsn %u log2(cwmin) %u "
			"log2(cwmax) %u txpoLimit %u]\n", __func__
			, ieee80211_wme_acnames[i]
			, wmep->wmep_acm
			, wmep->wmep_aifsn
			, wmep->wmep_logcwmin
			, wmep->wmep_logcwmax
			, wmep->wmep_txopLimit
		);

		wmep = &wme->wme_wmeBssChanParams.cap_wmeParams[i];
		wmep->wmep_acm = pBssPhyParam->acm;
		wmep->wmep_aifsn = pBssPhyParam->aifsn;	
		wmep->wmep_logcwmin = pBssPhyParam->logcwmin;	
		wmep->wmep_logcwmax = pBssPhyParam->logcwmax;		
		wmep->wmep_txopLimit = pBssPhyParam->txopLimit;
		
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
			"%s: %s  bss [acm %u aifsn %u log2(cwmin) %u "
			"log2(cwmax) %u txpoLimit %u]\n", __func__
			, ieee80211_wme_acnames[i]
			, wmep->wmep_acm
			, wmep->wmep_aifsn
			, wmep->wmep_logcwmin
			, wmep->wmep_logcwmax
			, wmep->wmep_txopLimit
		);
	}
	
	/* NB: check ic_bss to avoid NULL deref on initial attach */
	if (ic->ic_bss != NULL)
	{
		/*
		 * Calculate agressive mode switching threshold based
		 * on beacon interval.  This doesn't need locking since
		 * we're only called before entering the RUN state at
		 * which point we start sending beacon frames.
		 */
		wme->wme_hipri_switch_thresh =
			(HIGH_PRI_SWITCH_THRESH * ic->ic_bss->ni_intval) / 100;
		ieee80211_wme_updateparams(ic);
	}
}

/* Update WME parameters for ourself and the BSS. */
void ieee80211_wme_updateparams_locked(struct ieee80211com *ic)
{
	static const paramType phyParam[IEEE80211_MODE_MAX] = 
		{
		{ 2, 4, 10, 64 },	/* IEEE80211_MODE_AUTO */ 
		{ 2, 4, 10, 64 },	/* IEEE80211_MODE_11A */ 
		{ 2, 5, 10, 64 },	/* IEEE80211_MODE_11B */ 
		{ 2, 4, 10, 64 },	/* IEEE80211_MODE_11G */ 
		{ 2, 5, 10, 64 },	/* IEEE80211_MODE_FH */ 
		{ 1, 3, 10, 64 },	/* IEEE80211_MODE_TURBO_A */ 
		{ 1, 3, 10, 64 },	/* IEEE80211_MODE_TURBO_G */ 
	};
	
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	const struct wmeParams *wmep;
	struct wmeParams *chanp, *bssp;
	int i;

       	/* set up the channel access parameters for the physical device */
	for (i = 0; i < WME_NUM_AC; i++)
	{
		chanp = &wme->wme_chanParams.cap_wmeParams[i];
		wmep = &wme->wme_wmeChanParams.cap_wmeParams[i];
		chanp->wmep_aifsn = wmep->wmep_aifsn;
		chanp->wmep_logcwmin = wmep->wmep_logcwmin;
		chanp->wmep_logcwmax = wmep->wmep_logcwmax;
		chanp->wmep_txopLimit = wmep->wmep_txopLimit;

		chanp = &wme->wme_bssChanParams.cap_wmeParams[i];
		wmep = &wme->wme_wmeBssChanParams.cap_wmeParams[i];
		chanp->wmep_aifsn = wmep->wmep_aifsn;
		chanp->wmep_logcwmin = wmep->wmep_logcwmin;
		chanp->wmep_logcwmax = wmep->wmep_logcwmax;
		chanp->wmep_txopLimit = wmep->wmep_txopLimit;
	}

	/*
	 * This implements agressive mode as found in certain
	 * vendors' AP's.  When there is significant high
	 * priority (VI/VO) traffic in the BSS throttle back BE
	 * traffic by using conservative parameters.  Otherwise
	 * BE uses agressive params to optimize performance of
	 * legacy/non-QoS traffic.
	 */
        if ((ic->ic_opmode == IEEE80211_M_HOSTAP &&
	     (wme->wme_flags & WME_F_AGGRMODE) == 0) ||
	    (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	     (ic->ic_bss->ni_flags & IEEE80211_NODE_QOS) == 0) ||
	    (ic->ic_flags & IEEE80211_F_WME) == 0)
	{
		chanp = &wme->wme_chanParams.cap_wmeParams[WME_AC_BE];
		bssp = &wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE];

		chanp->wmep_aifsn = bssp->wmep_aifsn =
			phyParam[ic->ic_curmode].aifsn;
		chanp->wmep_logcwmin = bssp->wmep_logcwmin =
			phyParam[ic->ic_curmode].logcwmin;
		chanp->wmep_logcwmax = bssp->wmep_logcwmax =
			phyParam[ic->ic_curmode].logcwmax;
		chanp->wmep_txopLimit = bssp->wmep_txopLimit =
			(ic->ic_caps & IEEE80211_C_BURST) ?phyParam[ic->ic_curmode].txopLimit : 0;		
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
			"%s: %s [acm %u aifsn %u log2(cwmin) %u "
			"log2(cwmax) %u txpoLimit %u]\n", __func__
			, ieee80211_wme_acnames[WME_AC_BE]
			, chanp->wmep_acm
			, chanp->wmep_aifsn
			, chanp->wmep_logcwmin
			, chanp->wmep_logcwmax
			, chanp->wmep_txopLimit
		);
	}
	
	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
	    ic->ic_sta_assoc < 2 && (wme->wme_flags & WME_F_AGGRMODE) == 0) {
        	static const u_int8_t logCwMin[IEEE80211_MODE_MAX] = {
              		3,	/* IEEE80211_MODE_AUTO */
              		3,	/* IEEE80211_MODE_11A */
              		4,	/* IEEE80211_MODE_11B */
              		3,	/* IEEE80211_MODE_11G */
              		4,	/* IEEE80211_MODE_FH */
              		3,	/* IEEE80211_MODE_TURBO_A */
              		3,	/* IEEE80211_MODE_TURBO_G */
		};
		chanp = &wme->wme_chanParams.cap_wmeParams[WME_AC_BE];
		bssp = &wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE];

		chanp->wmep_logcwmin = bssp->wmep_logcwmin = 
			logCwMin[ic->ic_curmode];
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
			"%s: %s log2(cwmin) %u\n", __func__
			, ieee80211_wme_acnames[WME_AC_BE]
			, chanp->wmep_logcwmin
		);
    	}	
	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {	/* XXX ibss? */
		/*
		 * Arrange for a beacon update and bump the parameter
		 * set number so associated stations load the new values.
		 */
		wme->wme_bssChanParams.cap_info =
			(wme->wme_bssChanParams.cap_info+1) & WME_QOSINFO_COUNT;
		ic->ic_flags |= IEEE80211_F_WMEUPDATE;
	}

	wme->wme_update(ic);

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
		"%s: WME params updated, cap_info 0x%x\n", __func__,
		ic->ic_opmode == IEEE80211_M_STA ?
			wme->wme_wmeChanParams.cap_info :
			wme->wme_bssChanParams.cap_info);
}

void ieee80211_wme_updateparams(struct ieee80211com *ic)
{

	if (ic->ic_caps & IEEE80211_C_WME)
	{
		IEEE80211_BEACON_LOCK(ic);
		ieee80211_wme_updateparams_locked(ic);
		IEEE80211_BEACON_UNLOCK(ic);
	}
}


