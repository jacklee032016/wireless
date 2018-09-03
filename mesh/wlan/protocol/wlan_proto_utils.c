/*
* $Id: wlan_proto_utils.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "_protocol.h"

/* XXX tunables */

const char *ieee80211_mgt_subtype_name[] = 
{
	"assoc_req",	"assoc_resp",	"reassoc_req",	"reassoc_resp",
	"probe_req",	"probe_resp",	"reserved#6",	"reserved#7",
	"beacon",	"atim",		"disassoc",	"auth",
#if WITH_MESH	
	"deauth",	"action#13",	"reserved#14",	"reserved#15"
#else
	"deauth",	"reserved#13",	"reserved#14",	"reserved#15"
#endif
};
EXPORT_SYMBOL(ieee80211_mgt_subtype_name);

const char *ieee80211_ctl_subtype_name[] = 
{
	"reserved#0",	"reserved#1",	"reserved#2",	"reserved#3",
	"reserved#3",	"reserved#5",	"reserved#6",	"reserved#7",
	"reserved#8",	"reserved#9",	"ps_poll",	"rts",
	"cts",		"ack",		"cf_end",	"cf_end_ack"
};
EXPORT_SYMBOL(ieee80211_ctl_subtype_name);

const char *ieee80211_state_name[IEEE80211_S_MAX] = 
{
	"INIT",		/* IEEE80211_S_INIT */
	"SCAN",		/* IEEE80211_S_SCAN */
	"AUTH",		/* IEEE80211_S_AUTH */
	"ASSOC",	/* IEEE80211_S_ASSOC */
	"RUN"		/* IEEE80211_S_RUN */
};
EXPORT_SYMBOL(ieee80211_state_name);

const char *ieee80211_wme_acnames[] = 
{
	"WME_AC_BE",
	"WME_AC_BK",
	"WME_AC_VI",
	"WME_AC_VO",
	"WME_UPSD",
};
EXPORT_SYMBOL(ieee80211_wme_acnames);

void ieee80211_print_essid(const u_int8_t *essid, int len)
{
	const u_int8_t *p; 
	int i;

	if (len > IEEE80211_NWID_LEN)
		len = IEEE80211_NWID_LEN;
	/* determine printable or not */
	for (i = 0, p = essid; i < len; i++, p++)
	{
		if (*p < ' ' || *p > 0x7e)
			break;
	}

	if (i == len)
	{
		printf("\"");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%c", *p);
		printf("\"");
	}
	else
	{
		printf("0x");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%02x", *p);
	}
}

#if 0
void ieee80211_dump_pkt(const u_int8_t *buf, int len, int rate, int rssi)
{
}
#else
void ieee80211_dump_pkt(const u_int8_t *buf, int len, int rate, int rssi)
{
	const struct ieee80211_frame *wh;
	int i;

	wh = (const struct ieee80211_frame *)buf;
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK)
	{
		case IEEE80211_FC1_DIR_NODS:
			printf("NODS %s", swjtu_mesh_mac_sprintf(wh->i_addr2));
			printf("->%s", swjtu_mesh_mac_sprintf(wh->i_addr1));
			printf("(%s)", swjtu_mesh_mac_sprintf(wh->i_addr3));
			break;
		case IEEE80211_FC1_DIR_TODS:
			printf("TODS %s", swjtu_mesh_mac_sprintf(wh->i_addr2));
			printf("->%s", swjtu_mesh_mac_sprintf(wh->i_addr3));
			printf("(%s)", swjtu_mesh_mac_sprintf(wh->i_addr1));
			break;
		case IEEE80211_FC1_DIR_FROMDS:
			printf("FRDS %s", swjtu_mesh_mac_sprintf(wh->i_addr3));
			printf("->%s", swjtu_mesh_mac_sprintf(wh->i_addr1));
			printf("(%s)", swjtu_mesh_mac_sprintf(wh->i_addr2));
			break;
		case IEEE80211_FC1_DIR_DSTODS:
			printf("DSDS %s", swjtu_mesh_mac_sprintf((const u_int8_t *)&wh[1]));
			printf("->%s", swjtu_mesh_mac_sprintf(wh->i_addr3));
			printf("(%s", swjtu_mesh_mac_sprintf(wh->i_addr2));
			printf("->%s)", swjtu_mesh_mac_sprintf(wh->i_addr1));
			break;
	}

	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK)
	{
		case IEEE80211_FC0_TYPE_DATA:
			printf(" data");
			break;
		case IEEE80211_FC0_TYPE_MGT:
			printf(" %s", ieee80211_mgt_subtype_name[
			    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK)
			    >> IEEE80211_FC0_SUBTYPE_SHIFT]);
			break;
		default:
			printf(" type#%d", wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);
			break;
	}

	if (wh->i_fc[1] & IEEE80211_FC1_WEP)
	{
		int i;
		printf(" WEP [IV");
		for (i = 0; i < IEEE80211_WEP_IVLEN; i++)
			printf(" %.02x", buf[sizeof(*wh)+i]);
		printf(" KID %u]", buf[sizeof(*wh)+i] >> 6);
	}

	if (rate >= 0)
		printf(" %dM", rate / 2);

	if (rssi >= 0)
		printf(" +%d", rssi);
	printf("\n");

	if (len > 0)
	{
		for (i = 0; i < len; i++) {
			if ((i & 1) == 0)
				printf(" ");
			printf("%02x", buf[i]);
		}
		printf("\n");
	}
}
#endif
EXPORT_SYMBOL(ieee80211_dump_pkt);

int ieee80211_fix_rate(struct ieee80211com *ic, struct ieee80211_node *ni, int flags)
{
#define	RV(v)	((v) & IEEE80211_RATE_VAL)
	int i, j, ignore, error;
	int okrate, badrate, fixedrate;
	struct ieee80211_rateset *srs, *nrs;
	u_int8_t r;

	/*
	 * If the fixed rate check was requested but no
	 * fixed has been defined then just remove it.
	 */
	if ((flags & IEEE80211_F_DOFRATE) && ic->ic_fixed_rate < 0)
		flags &= ~IEEE80211_F_DOFRATE;
	error = 0;
	okrate = badrate = fixedrate = 0;
	srs = &ic->ic_sup_rates[ieee80211_chan2mode(ic, ni->ni_chan)];
	nrs = &ni->ni_rates;
	for (i = 0; i < nrs->rs_nrates; ) {
		ignore = 0;
		if (flags & IEEE80211_F_DOSORT) {
			/*
			 * Sort rates.
			 */
			for (j = i + 1; j < nrs->rs_nrates; j++) {
				if (RV(nrs->rs_rates[i]) > RV(nrs->rs_rates[j])) {
					r = nrs->rs_rates[i];
					nrs->rs_rates[i] = nrs->rs_rates[j];
					nrs->rs_rates[j] = r;
				}
			}
		}
		r = nrs->rs_rates[i] & IEEE80211_RATE_VAL;
		badrate = r;
		if (flags & IEEE80211_F_DOFRATE) {
			/*
			 * Check any fixed rate is included. 
			 */
			if (r == RV(srs->rs_rates[ic->ic_fixed_rate]))
				fixedrate = r;
		}
		if (flags & IEEE80211_F_DONEGO) {
			/*
			 * Check against supported rates.
			 */
			for (j = 0; j < srs->rs_nrates; j++) {
				if (r == RV(srs->rs_rates[j])) {
					/*
					 * Overwrite with the supported rate
					 * value so any basic rate bit is set.
					 * This insures that response we send
					 * to stations have the necessary basic
					 * rate bit set.
					 */
					nrs->rs_rates[i] = srs->rs_rates[j];
					break;
				}
			}
			if (j == srs->rs_nrates) {
				/*
				 * A rate in the node's rate set is not
				 * supported.  If this is a basic rate and we
				 * are operating as an AP then this is an error.
				 * Otherwise we just discard/ignore the rate.
				 * Note that this is important for 11b stations
				 * when they want to associate with an 11g AP.
				 */
				if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
				    (nrs->rs_rates[i] & IEEE80211_RATE_BASIC))
					error++;
				ignore++;
			}
		}
		if (flags & IEEE80211_F_DODEL) {
			/*
			 * Delete unacceptable rates.
			 */
			if (ignore) {
				nrs->rs_nrates--;
				for (j = i; j < nrs->rs_nrates; j++)
					nrs->rs_rates[j] = nrs->rs_rates[j + 1];
				nrs->rs_rates[j] = 0;
				continue;
			}
		}
		if (!ignore)
			okrate = nrs->rs_rates[i];
		i++;
	}
	if (okrate == 0 || error != 0 ||
	    ((flags & IEEE80211_F_DOFRATE) && fixedrate == 0))
		return badrate | IEEE80211_RATE_BASIC;
	else
		return RV(okrate);
#undef RV
}

/*
 * Reset 11g-related state.
 */
void ieee80211_reset_erp(struct ieee80211com *ic)
{
	ic->ic_flags &= ~IEEE80211_F_USEPROT;
	ic->ic_nonerpsta = 0;
	ic->ic_longslotsta = 0;
	/*
	 * Short slot time is enabled only when operating in 11g
	 * and not in an IBSS.  We must also honor whether or not
	 * the driver is capable of doing it.
	 */
	ieee80211_set_shortslottime(ic,
		(ic->ic_curmode == IEEE80211_MODE_11A ||
		ic->ic_curmode == IEEE80211_MODE_TURBO_A) ||
		((ic->ic_curmode == IEEE80211_MODE_11G ||
		ic->ic_curmode == IEEE80211_MODE_TURBO_G) &&
		ic->ic_opmode == IEEE80211_M_HOSTAP &&
		(ic->ic_caps & IEEE80211_C_SHSLOT)));
	/*
	 * Set short preamble and ERP barker-preamble flags.
	 */
	if ((ic->ic_curmode == IEEE80211_MODE_11A ||
	    ic->ic_curmode == IEEE80211_MODE_TURBO_A) ||
	    (ic->ic_caps & IEEE80211_C_SHPREAMBLE)) {
		ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
		ic->ic_flags &= ~IEEE80211_F_USEBARKER;
	} else {
		ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
		ic->ic_flags |= IEEE80211_F_USEBARKER;
	}
}

/* Set the short slot time state and notify the driver */
void ieee80211_set_shortslottime(struct ieee80211com *ic, int onoff)
{
	if (onoff)
		ic->ic_flags |= IEEE80211_F_SHSLOT;
	else
		ic->ic_flags &= ~IEEE80211_F_SHSLOT;
	/* notify driver */
	if (ic->ic_updateslot != NULL)
		ic->ic_updateslot(ic->ic_dev);
}

/* Check if the specified rate set supports ERP.
 * NB: the rate set is assumed to be sorted.
 */
int ieee80211_iserp_rateset(struct ieee80211com *ic, struct ieee80211_rateset *rs)
{
#define N(a)	(sizeof(a) / sizeof(a[0]))
	static const int rates[] = { 2, 4, 11, 22, 12, 24, 48 };
	int i, j;

	if (rs->rs_nrates < N(rates))
		return 0;
	for (i = 0; i < N(rates); i++) {
		for (j = 0; j < rs->rs_nrates; j++) {
			int r = rs->rs_rates[j] & IEEE80211_RATE_VAL;
			if (rates[i] == r)
				goto next;
			if (r > rates[i])
				return 0;
		}
		return 0;
	next:
		;
	}
	return 1;
#undef N
}

/*
 * Mark the basic rates for the 11g rate table based on the
 * operating mode.  For real 11g we mark all the 11b rates
 * and 6, 12, and 24 OFDM.  For 11b compatibility we mark only
 * 11b rates.  There's also a pseudo 11a-mode used to mark only
 * the basic OFDM rates.
 */
void ieee80211_set11gbasicrates(struct ieee80211_rateset *rs, enum ieee80211_phymode mode)
{
	static const struct ieee80211_rateset basic[] = 
	{
		{ 0 },			/* IEEE80211_MODE_AUTO */
		{ 3, { 12, 24, 48 } },	/* IEEE80211_MODE_11A */
		{ 2, { 2, 4 } },		/* IEEE80211_MODE_11B */
		{ 4, { 2, 4, 11, 22 } },	/* IEEE80211_MODE_11G (mixed b/g) */
		{ 0 },			/* IEEE80211_MODE_FH */
						/* IEEE80211_MODE_PUREG (not yet) */
		{ 7, { 2, 4, 11, 22, 12, 24, 48 } },
	};
	int i, j;

	for (i = 0; i < rs->rs_nrates; i++)
	{
		rs->rs_rates[i] &= IEEE80211_RATE_VAL;
		for (j = 0; j < basic[mode].rs_nrates; j++)
			if (basic[mode].rs_rates[j] == rs->rs_rates[i])
			{
				rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
				break;
			}
	}
}

