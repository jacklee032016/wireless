/*
* $Id: phy_rate_sample_sysctl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "phy.h"
#include "phy_rate_sample.h"

#ifdef CONFIG_SYSCTL

int read_rate_stats(struct ieee80211_node *ni, int size, char *buf, int space)
{
	struct ath_node *an = ATH_NODE(ni);
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	int x = 0;
	char *p = buf;
	int size_bin = 0;
	size_bin = size_to_bin(size);
	p += sprintf(p, "%s\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr));
	p += sprintf(p, "rate tt/perfect failed/pkts avg_tries last_tx\n");
	
	for (x = 0; x < sn->num_rates; x++)
	{
		int a = 1;
		int t = 1;

		p += sprintf(p, "%s", (x == sn->current_rate[size_bin]) ? "*" : " ");

		p += sprintf(p, "%3d%s", sn->rates[x].rate/2, (sn->rates[x].rate & 0x1) != 0 ? ".5" : "  ");

		p += sprintf(p, " %4d/%4d %2d/%3d",
			     sn->stats[size_bin][x].average_tx_time,
			     sn->stats[size_bin][x].perfect_tx_time,
			     sn->stats[size_bin][x].successive_failures,
			     sn->stats[size_bin][x].total_packets);

		if (sn->stats[size_bin][x].total_packets)
		{
			a = sn->stats[size_bin][x].total_packets;
			t = sn->stats[size_bin][x].tries;
		}
		p += sprintf(p, " %d.%02d ", t/a, (t*100/a) % 100);
		if (sn->stats[size_bin][x].last_tx)
		{
			unsigned d = jiffies - sn->stats[size_bin][x].last_tx;
			p += sprintf(p, "%d.%02d", d / HZ, d % HZ);
		}
		else
		{
			p += sprintf(p, "-");
		}
		p += sprintf(p, "\n");
	}
	return (p - buf);
}

struct dd
{
	char *buf;
	int space;	
	int packet_size;
};

static void read_rate_stats_cb(void *v, struct ieee80211_node *ni)
{
	struct dd *thunk = v;
	int s = 0;
	/* assume each node needs 500 bytes */
	if (thunk->space < 500)
	{
		return;
	}
	s = read_rate_stats(ni, thunk->packet_size, thunk->buf, thunk->space);
	thunk->buf += s;
	thunk->space -= s;
	return;
}

static int ATH_SYSCTL_DECL(ath_sysctl_rate_stats, ctl, write, filp, buffer, lenp, ppos)
{
	struct ath_softc *sc = ctl->extra1;
	int packet_size = (int) ctl->extra2;
	struct ieee80211com *ic = &sc->sc_ic;
	char tmp_buf[PAGE_SIZE]; 
	int tmp_buf_size = MIN(PAGE_SIZE, *lenp);

	struct dd thunk;

	thunk.buf = tmp_buf;
	thunk.space = tmp_buf_size;
	thunk.packet_size = packet_size;

#if	LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)
	if (tmp_buf_size && filp->f_pos == 0)
#else
	if (tmp_buf_size && ppos != NULL && *ppos == 0)
#endif
	{
		if (ic->ic_opmode == IEEE80211_M_STA)
		{
			read_rate_stats_cb(&thunk, ic->ic_bss);
		}
		else
		{
			ieee80211_iterate_nodes(&ic->ic_sta, read_rate_stats_cb, (void *) &thunk);
		}
		
		if (!copy_to_user(buffer, tmp_buf, tmp_buf_size - thunk.space))
		{
			*lenp = tmp_buf_size - thunk.space;
		}
		else 
		{
			*lenp = 0;
		}
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

#define	CTL_AUTO	-2	/* cannot be CTL_ANY or CTL_NONE */

/*
 * Dynamic (i.e per-device) sysctls.
 */
static const ctl_table sample_dynamic_sysctl_template[] =
{
	{
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "stats_250",
		.mode		= 0444,
		.proc_handler	= ath_sysctl_rate_stats,
		.extra2       = (void *) 250,
	},
	{
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "stats_1600",
		.mode		= 0444,
		.proc_handler	= ath_sysctl_rate_stats,
		.extra2       = (void *) 1600,
	},
	{
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "stats_3000",
		.mode		= 0444,
		.proc_handler	= ath_sysctl_rate_stats,
		.extra2       = (void *) 3000,
	},
	{ 0 }
};

void ath_rate_dynamic_sysctl_register(struct ath_softc *sc)
{
	struct sample_softc *ssc = ATH_SOFTC_SAMPLE(sc);
	int i, space;

	space = 7*sizeof(struct ctl_table) + sizeof(sample_dynamic_sysctl_template);
	ssc->sysctls = kmalloc(space, GFP_KERNEL);

	if (ssc->sysctls == NULL)
	{
		printk("%s: no memory for sysctl table for %s!\n", phyRateSampleDevInfo, sc->sc_dev.name);
		return;
	}

	/* setup the table */
	memset(ssc->sysctls, 0, space);
	ssc->sysctls[0].ctl_name = CTL_DEV;
	ssc->sysctls[0].procname = "dev";
	ssc->sysctls[0].mode = 0555;
	ssc->sysctls[0].child = &ssc->sysctls[2];
	/* [1] is NULL terminator */
	ssc->sysctls[2].ctl_name = SWJTU_MESH_SYSCTRL_DEV_AUTO;
	ssc->sysctls[2].procname = sc->sc_dev.name;
	ssc->sysctls[2].mode = 0555;
	ssc->sysctls[2].child = &ssc->sysctls[4];
	/* [3] is NULL terminator */
	ssc->sysctls[4].ctl_name = SWJTU_MESH_SYSCTRL_DEV_AUTO;
	ssc->sysctls[4].procname = "rate";
	ssc->sysctls[4].mode = 0555;
	ssc->sysctls[4].child = &ssc->sysctls[6];
	/* [5] is NULL terminator */
	/* copy in pre-defined data */
	memcpy(&ssc->sysctls[6], sample_dynamic_sysctl_template,
		sizeof(sample_dynamic_sysctl_template));

	/* add in dynamic data references */
	for (i = 6; ssc->sysctls[i].ctl_name; i++)
		if (ssc->sysctls[i].extra1 == NULL)
			ssc->sysctls[i].extra1 = sc;

	/* and register everything */
	ssc->sysctl_header = register_sysctl_table(ssc->sysctls, 1);
	if (!ssc->sysctl_header)
	{
		printk("%s: failed to register sysctls for %s!\n", phyRateSampleDevInfo, sc->sc_dev.name);
		kfree(ssc->sysctls);
		ssc->sysctls = NULL;
	}

}
EXPORT_SYMBOL(ath_rate_dynamic_sysctl_register);

/*
 * Static (i.e. global) sysctls.
 */
enum {
	DEV_ATH		= 9			/* XXX known by many */
};

static int min_smoothing_rate = 0;		
static int max_smoothing_rate = 99;		

static int min_sample_rate = 2;
static int max_sample_rate = 100;


static ctl_table ath_rate_static_sysctls[] =
{
	{
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "smoothing_rate",
		.mode		= 0644,
		.data		= &ath_smoothing_rate,
		.maxlen		= sizeof(ath_smoothing_rate),
		.extra1		= &min_smoothing_rate,
		.extra2		= &max_smoothing_rate,
		.proc_handler	= proc_dointvec_minmax
	},
	{
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "sample_rate",
		.mode		= 0644,
		.data		= &ath_sample_rate,
		.maxlen		= sizeof(ath_sample_rate),
		.extra1		= &min_sample_rate,
		.extra2		= &max_sample_rate,
		.proc_handler	= proc_dointvec_minmax
	},
	{ 0 }
};

static ctl_table ath_rate_table[] =
{
	{
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "rate",
		.mode		= 0555,
		.child		= ath_rate_static_sysctls
	},
	{ 0 }
};

static ctl_table ath_ath_table[] =
{
	{
		.ctl_name	= DEV_ATH,
		.procname	= "ath",
		.mode		= 0555,
		.child		= ath_rate_table
	},
	{ 0 }
};

static ctl_table ath_root_table[] =
{
	{
		.ctl_name	= CTL_DEV,
		.procname	= "dev",
		.mode		= 0555,
		.child		= ath_ath_table
	}, 
	{ 0 }
};

static struct ctl_table_header *ath_sysctl_header;

#endif /* CONFIG_SYSCTL */

