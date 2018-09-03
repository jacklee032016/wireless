/*
* $Id: hw_sysctrl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "mesh.h"

#include <linux/sysctl.h>
#include <linux/proc_fs.h>

#ifdef CONFIG_SYSCTL
enum
{
	DEV_ATH		= 9			/* XXX must match driver */
};

#define	CTL_AUTO	-2	/* cannot be CTL_ANY or CTL_NONE */

/* following is used by HAL */
int	ath_hal_dma_beacon_response_time 	= 2;		/* in TU's */
int	ath_hal_sw_beacon_response_time 	= 10;	/* in TU's */
int	ath_hal_additional_swba_backoff 	= 0;		/* in TU's */

static ctl_table hw_hal_sysctls[] =
{
#ifdef AH_DEBUG
	{
		.ctl_name	= CTL_AUTO,
		.procname	= "debug",
		.mode		= 0644,
		.data		= &hw_hal_debug,
		.maxlen		= sizeof(hw_hal_debug),
		.proc_handler	= proc_dointvec
	},
#endif
	{
		.ctl_name	= CTL_AUTO,
		.procname	= "dma_beacon_response_time",
		.data		= &ath_hal_dma_beacon_response_time,
		.maxlen		= sizeof(ath_hal_dma_beacon_response_time),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.ctl_name	= CTL_AUTO,	
		.procname	= "sw_beacon_response_time",
		.mode		= 0644,
		.data		= &ath_hal_sw_beacon_response_time,
		.maxlen		= sizeof(ath_hal_sw_beacon_response_time),
		.proc_handler	= proc_dointvec
	},
	{ 
		.ctl_name	= CTL_AUTO,
		.procname	= "swba_backoff",
		.mode		= 0644,
		.data		= &ath_hal_additional_swba_backoff,
		.maxlen		= sizeof(ath_hal_additional_swba_backoff),
		.proc_handler	= proc_dointvec
	},
	{ 0 }
};

static ctl_table hw_hw_table[] =
{
	{
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_WIFI,
		.procname	= "hw",
		.mode		= 0555,
		.child		= hw_hal_sysctls
	},
	{ 0 }
};

static ctl_table hw_wifi_table[] =
{
	{
#if 0		
		.ctl_name	= DEV_ATH,
		.procname	= "hw",
#else		
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_WIFI,
		.procname	= SWJTU_MESH_SYSCTRL_WIFI_NAME,
#endif		
		.mode		= 0555,
		.child		= hw_hw_table
	},
	{ 0 }
};

static ctl_table hw_device_table[] =
{
	{
#if 0		
		.ctl_name	= DEV_ATH,
		.procname	= "hw",
#else		
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEVICE,
		.procname	= SWJTU_MESH_SYSCTRL_DEVICE_NAME,
#endif		
		.mode		= 0555,
		.child		= hw_wifi_table
	},
	{ 0 }
};

static ctl_table hw_mesh_root_table[] =
{
	{
#if 0		
		.ctl_name	= CTL_DEV,
		.procname	= "dev",
#else
		.ctl_name	= SWJTU_MESH_SYSCTRL_ROOT_DEV,
		.procname	= SWJTU_MESH_DEV_NAME,
#endif
		.mode		= 0555,
		.child		= hw_device_table
	},
	{ 0 }
};
static struct ctl_table_header *hw_sysctl_header;

void _hw_sysctl_register(void)
{
	static int initialized = 0;

	if (!initialized)
	{
		hw_sysctl_header = register_sysctl_table(hw_mesh_root_table, 1);
		initialized = 1;
	}
}

void _hw_sysctl_unregister(void)
{
	if (hw_sysctl_header)
		unregister_sysctl_table(hw_sysctl_header);
}
#endif /* CONFIG_SYSCTL */

