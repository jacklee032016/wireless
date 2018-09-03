/*
* $Id: phy_rate_amrr_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "phy_rate_amrr.h"

EXPORT_SYMBOL(phy_rate_tx_complete);
EXPORT_SYMBOL(phy_rate_newassoc);
EXPORT_SYMBOL(phy_rate_newstate);
EXPORT_SYMBOL(phy_rate_attach);
EXPORT_SYMBOL(phy_rate_detach);
EXPORT_SYMBOL(phy_rate_node_init);
EXPORT_SYMBOL(phy_rate_node_cleanup);
EXPORT_SYMBOL(phy_rate_findrate);
EXPORT_SYMBOL(phy_rate_setupxtxdesc);

static char *phyRateAmrrVersion = "0.1";
static char *phyDevRateArmmInfo = "PHY RATE AMRR";

static int __init phy_rate_amrr_init(void)
{
	PHY_DEBUG_INFO("%s: %s\n", phyDevRateArmmInfo, phyRateAmrrVersion);

#ifdef CONFIG_SYSCTL
	ath_sysctl_header = register_sysctl_table(ath_root_table, 1);
#endif
	return (0);
}
module_init(phy_rate_amrr_init);

static void __exit exit_ath_rate_amrr(void)
{
#ifdef CONFIG_SYSCTL
	if (ath_sysctl_header != NULL)
		unregister_sysctl_table(ath_sysctl_header);
#endif

	PHY_DEBUG_INFO("%s: unloaded\n", phyDevRateArmmInfo);
}
module_exit( rate_amrr_exit);

