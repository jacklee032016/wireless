/*
* $Id: phy_rate_sample_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "phy.h"
#include "phy_rate_sample.h"

MODULE_AUTHOR("John Bicket");
MODULE_DESCRIPTION("SampleRate bit-rate selection algorithm for Atheros devices");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif


static int __init phy_rate_sample_init(void)
{
	PHY_DEBUG_INFO("%s: %s\n", phyRateSampleDevInfo, phyRateSampleVersion);

#ifdef CONFIG_SYSCTL
	ath_sysctl_header = register_sysctl_table(ath_root_table, 1);
#endif
	return (0);
}
module_init( phy_rate_sample_init);

static void __exit phy_rate_sample_exit(void)
{
#ifdef CONFIG_SYSCTL
	if (ath_sysctl_header != NULL)
		unregister_sysctl_table(ath_sysctl_header);
#endif

	PHY_DEBUG_INFO("%s: unloaded\n", phyRateSampleDevInfo);
}
module_exit(phy_rate_sample_exit );

EXPORT_SYMBOL(phy_rate_node_init);
EXPORT_SYMBOL(phy_rate_node_cleanup);
EXPORT_SYMBOL(ath_rate_node_copy);
EXPORT_SYMBOL(phy_rate_findrate);
EXPORT_SYMBOL(phy_rate_setupxtxdesc);
EXPORT_SYMBOL(phy_rate_tx_complete);
EXPORT_SYMBOL(phy_rate_newassoc);
EXPORT_SYMBOL(phy_rate_newstate);
EXPORT_SYMBOL(phy_rate_attach);
EXPORT_SYMBOL(phy_rate_detach);



