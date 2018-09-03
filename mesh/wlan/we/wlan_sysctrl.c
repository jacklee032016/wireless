/*
* $Id: wlan_sysctrl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "wlan.h"

#ifdef CONFIG_SYSCTL
#include <linux/ctype.h>

static int proc_read_node(char *page, int space, struct ieee80211com *ic, void *arg)
{
	char buf[1024];
	char *p = buf;
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_rateset *rs;
	int i;
	u_int16_t temp;

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
	{
		/* Assume each node needs 300 bytes */ 
		if (p - buf > space - 300)
			break;
		
		p += sprintf(p, "\nmacaddr: <%s>\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr));
		if (ni == ic->ic_bss)
			p += sprintf(p, "BSS\n");
		p += sprintf(p, "  rssi: %d dBm ;", ic->ic_node_getrssi(ni));
		p += sprintf(p, "refcnt: %d\n", ieee80211_node_refcnt(ni));

		p += sprintf(p, "  capinfo:");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_ESS)
			p += sprintf(p, " ess");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS)
			p += sprintf(p, " ibss");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_CF_POLLABLE)
			p += sprintf(p, " cfpollable");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_CF_POLLREQ)
			p += sprintf(p, " cfpollreq");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			p += sprintf(p, " privacy");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_CHNL_AGILITY)
			p += sprintf(p, " chanagility");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PBCC)
			p += sprintf(p, " pbcc");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)
			p += sprintf(p, " shortpreamble");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME)
			p += sprintf(p, " shortslot");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_RSN)
			p += sprintf(p, " rsn");
		if (ni->ni_capinfo & IEEE80211_CAPINFO_DSSSOFDM)
			p += sprintf(p, " dsssofdm");
		p += sprintf(p, "\n");

		p += sprintf(p, "  freq: %d MHz (channel %d)\n", ni->ni_chan->ic_freq,	ieee80211_mhz2ieee(ni->ni_chan->ic_freq, 0));

		p += sprintf(p, "  opmode:");
		if (IEEE80211_IS_CHAN_A(ni->ni_chan))
			p += sprintf(p, " a");
		if (IEEE80211_IS_CHAN_B(ni->ni_chan))
			p += sprintf(p, " b");
		if (IEEE80211_IS_CHAN_PUREG(ni->ni_chan))
			p += sprintf(p, " pureg");
		if (IEEE80211_IS_CHAN_G(ni->ni_chan))
			p += sprintf(p, " g");
		p += sprintf(p, "\n");

		rs = &ni->ni_rates;
		if (ni->ni_txrate >= 0 && ni->ni_txrate < rs->rs_nrates)
		{
			p += sprintf(p, "  txrate: ");
			for (i = 0; i < rs->rs_nrates; i++)
			{
				p += sprintf(p, "%s%d%sMbps",
					(i != 0 ? " " : ""),
					(rs->rs_rates[i] & IEEE80211_RATE_VAL) / 2,
					((rs->rs_rates[i] & 0x1) != 0 ? ".5" : ""));
				if (i == ni->ni_txrate)
					p += sprintf(p, "*"); /* current rate */
			}
			p += sprintf(p, "\n");
		}
		else
			p += sprintf(p, "  txrate: %d ? (rs_nrates: %d)\n", ni->ni_txrate, ni->ni_rates.rs_nrates);

		p += sprintf(p, "  txpower %d vlan %d\n", ni->ni_txpower, ni->ni_vlan);

		p += sprintf(p, "  txseq: %d  rxseq: %d fragno %d rxfragstamp %d\n", ni->ni_txseqs[0],
				ni->ni_rxseqs[0] >> IEEE80211_SEQ_SEQ_SHIFT,
				ni->ni_rxseqs[0] & IEEE80211_SEQ_FRAG_MASK,
				ni->ni_rxfragstamp);

		if (ic->ic_opmode == IEEE80211_M_IBSS || ni->ni_associd != 0)
			temp = ic->ic_inact_run;
		else if (ieee80211_node_is_authorized(ni))
			temp = ic->ic_inact_auth;
		else
			temp = ic->ic_inact_init;
		temp = (temp - ni->ni_inact) * IEEE80211_INACT_WAIT;
		p += sprintf(p, "  fails: %d  inact: %d\n", ni->ni_fails, temp);
	}
	
	IEEE80211_NODE_UNLOCK(nt);
	return copy_to_user(page, buf, p - buf) ? 0 : (p - buf);
}

static int IEEE80211_SYSCTL_DECL(ieee80211_sysctl_stations, ctl, write, filp, buffer,lenp, ppos)
{
	struct ieee80211com *ic = ctl->extra1;
	int len = *lenp;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return -EINVAL;
#if	LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)
	if (len && filp->f_pos == 0)
	{
#else
	if (len && ppos != NULL && *ppos == 0)
	{
#endif
		*lenp = proc_read_node(buffer, len, ic, &ic->ic_sta);
#if	LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)
		filp->f_pos += *lenp;
#else
		*ppos += *lenp;
#endif
	}
	else
	{
		*lenp = 0;
	}
	return 0;
}


static int IEEE80211_SYSCTL_DECL(ieee80211_sysctl_debug, ctl, write, filp, buffer, lenp, ppos)
{
	struct ieee80211com *ic = ctl->extra1;
	u_int val;
	int ret;

	ctl->data = &val;
	ctl->maxlen = sizeof(val);
	if (write)
	{
		ret = IEEE80211_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,	lenp, ppos);
		if (ret == 0)
			ic->ic_debug = val;
	}
	else
	{
		val = ic->ic_debug;
		ret = IEEE80211_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,	lenp, ppos);
	}
	return ret;
}

#define	CTL_AUTO	-2	/* cannot be CTL_ANY or CTL_NONE */

static const ctl_table ieee80211_sysctl_template[] = 
{
	{ 
		.ctl_name	= CTL_AUTO,
		.procname	= "debug",
		.mode		= 0644,
		.proc_handler	= ieee80211_sysctl_debug
	},
	{ 
		.ctl_name	= CTL_AUTO,
		.procname	= "associated_sta",
		.mode		= 0444,
		.proc_handler	= ieee80211_sysctl_stations
	},
	{ 0 }
};

void ieee80211_sysctl_register(struct ieee80211com *ic)
{
	const char *cp;
	int i, space;

	space = 9*sizeof(struct ctl_table) + sizeof(ieee80211_sysctl_template);
	ic->ic_sysctls = kmalloc(space, GFP_KERNEL);
	if (ic->ic_sysctls == NULL)
	{
		printk("%s: no memory for sysctl table!\n", __func__);
		return;
	}

	/* setup the table */
	memset(ic->ic_sysctls, 0, space);
#if 0	
	ic->ic_sysctls[0].ctl_name = CTL_NET;
	ic->ic_sysctls[0].procname = "net";
#else
	ic->ic_sysctls[0].ctl_name = SWJTU_MESH_SYSCTRL_ROOT_DEV;
	ic->ic_sysctls[0].procname = SWJTU_MESH_DEV_NAME;
#endif
	ic->ic_sysctls[0].mode = 0555;
	ic->ic_sysctls[0].child = &ic->ic_sysctls[2];
	/* [1] is NULL terminator */

	ic->ic_sysctls[2].ctl_name = SWJTU_MESH_SYSCTRL_DEVICE;
	ic->ic_sysctls[2].procname = SWJTU_MESH_SYSCTRL_DEVICE_NAME;
	ic->ic_sysctls[2].mode = 0555;
	ic->ic_sysctls[2].child = &ic->ic_sysctls[4];

	ic->ic_sysctls[4].ctl_name = SWJTU_MESH_SYSCTRL_DEV_WIFI;
	ic->ic_sysctls[4].procname = SWJTU_MESH_SYSCTRL_WIFI_NAME;
	ic->ic_sysctls[4].mode = 0555;
	ic->ic_sysctls[4].child = &ic->ic_sysctls[6];

	ic->ic_sysctls[6].ctl_name = SWJTU_MESH_SYSCTRL_DEV_WIFI;
	for (cp = ic->ic_dev->name; *cp && !isdigit(*cp); cp++)
		;
	
	snprintf(ic->ic_procname, sizeof(ic->ic_procname), "mac%s", cp);
	ic->ic_sysctls[6].procname = ic->ic_procname;
	ic->ic_sysctls[6].mode = 0555;
	ic->ic_sysctls[6].child = &ic->ic_sysctls[8];
	/* [3] is NULL terminator */

	/* copy in pre-defined data */
	memcpy(&ic->ic_sysctls[8], ieee80211_sysctl_template,	sizeof(ieee80211_sysctl_template));

	/* add in dynamic data references */
	for (i = 8; ic->ic_sysctls[i].ctl_name; i++)
		if (ic->ic_sysctls[i].extra1 == NULL)
			ic->ic_sysctls[i].extra1 = ic;

	/* and register everything */
	ic->ic_sysctl_header = register_sysctl_table(ic->ic_sysctls, 1);
	if (!ic->ic_sysctl_header)
	{
		printk("%s: failed to register sysctls!\n", ic->ic_procname);
		kfree(ic->ic_sysctls);
		ic->ic_sysctls = NULL;
	}
}
EXPORT_SYMBOL(ieee80211_sysctl_register);

void ieee80211_sysctl_unregister(struct ieee80211com *ic)
{
	if (ic->ic_sysctl_header)
	{
		unregister_sysctl_table(ic->ic_sysctl_header);
		ic->ic_sysctl_header = NULL;
	}
	if (ic->ic_sysctls)
	{
		kfree(ic->ic_sysctls);
		ic->ic_sysctls = NULL;
	}
}
EXPORT_SYMBOL(ieee80211_sysctl_unregister);
#endif /* CONFIG_SYSCTL */

