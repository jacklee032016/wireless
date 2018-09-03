/*
* $Id: phy_sysctl_static.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "phy.h"

#ifdef CONFIG_SYSCTL
/*
 * Static (i.e. global) sysctls.  Note that the hal sysctls
 * are located under ours by sharing the setting for DEV_ATH.
 */
enum
{
	DEV_ATH		= 9,			/* XXX known by hal */
};

static	int mindwelltime = 100;			/* 100ms */
static	int mincalibrate = 1;			/* once a second */
static	int maxint = 0x7fffffff;		/* 32-bit big */

static ctl_table ath_static_sysctls[] =
{
#ifdef AR_DEBUG
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "debug",
		.mode		= 0644,
		.data		= &ath_debug,
		.maxlen		= sizeof(ath_debug),
		.proc_handler	= proc_dointvec
	},
#endif
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "countrycode",
		.mode		= 0444,
		.data		= &ath_countrycode,
		.maxlen		= sizeof(ath_countrycode),
		.proc_handler	= proc_dointvec
	},
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "regdomain",
		.mode		= 0444,
		.data		= &ath_regdomain,
		.maxlen		= sizeof(ath_regdomain),
		.proc_handler	= proc_dointvec
	},
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "outdoor",
		.mode		= 0444,
		.data		= &ath_outdoor,
		.maxlen		= sizeof(ath_outdoor),
		.proc_handler	= proc_dointvec
	},
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "xchanmode",
		.mode		= 0444,
		.data		= &ath_xchanmode,
		.maxlen		= sizeof(ath_xchanmode),
		.proc_handler	= proc_dointvec
	},
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "dwelltime",
		.mode		= 0644,
		.data		= &ath_dwelltime,
		.maxlen		= sizeof(ath_dwelltime),
		.extra1		= &mindwelltime,
		.extra2		= &maxint,
		.proc_handler	= proc_dointvec_minmax
	},
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "calibrate",
		.mode		= 0644,
		.data		= &ath_calinterval,
		.maxlen		= sizeof(ath_calinterval),
		.extra1		= &mincalibrate,
		.extra2		= &maxint,
		.proc_handler	= proc_dointvec_minmax
	},
	{ 0 }
};

static ctl_table ath_ath_table[] = 
{
	{ 
		.ctl_name	= DEV_ATH,
		.procname	= "ath",
		.mode		= 0555,
		.child		= ath_static_sysctls
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

void ath_sysctl_register(void)
{
	static int initialized = 0;

	if (!initialized)
	{
		ath_sysctl_header = register_sysctl_table(ath_root_table, 1);
		initialized = 1;
	}
}

void ath_sysctl_unregister(void)
{
	if (ath_sysctl_header)
		unregister_sysctl_table(ath_sysctl_header);
}

#endif

