/*
* $Id: phy_rate_onoe_sysctl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "phy.h"
#include "phy_rate_onoe.h"

static	int minrateinterval = 500;		/* 500ms */
static	int maxpercent = 100;			/* 100% */
static	int minpercent = 0;			/* 0% */
static	int maxint = 0x7fffffff;		/* 32-bit big */

int ath_rateinterval = 1000;		/* rate ctl interval (ms)  */
int ath_rate_raise = 10;		/* add credit threshold */
int ath_rate_raise_threshold = 10;	/* rate ctl raise threshold */

#ifdef CONFIG_SYSCTL
void ath_rate_dynamic_sysctl_register(struct ath_softc *sc)
{
}
EXPORT_SYMBOL(ath_rate_dynamic_sysctl_register);

/*
 * Static (i.e. global) sysctls.
 */
enum {
	DEV_ATH		= 9,			/* XXX known by many */
};

static ctl_table ath_rate_static_sysctls[] = 
{
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "interval",
		.mode		= 0644,
		.data		= &ath_rateinterval,
		.maxlen		= sizeof(ath_rateinterval),
		.extra1		= &minrateinterval,
		.extra2		= &maxint,
		.proc_handler	= proc_dointvec_minmax
	},
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "raise",
		.mode		= 0644,
		.data		= &ath_rate_raise,
		.maxlen		= sizeof(ath_rate_raise),
		.extra1		= &minpercent,
		.extra2		= &maxpercent,
		.proc_handler	= proc_dointvec_minmax
	},
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "raise_threshold",
		.mode		= 0644,
		.data		= &ath_rate_raise_threshold,
		.maxlen		= sizeof(ath_rate_raise_threshold),
		.proc_handler	= proc_dointvec
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
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
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

