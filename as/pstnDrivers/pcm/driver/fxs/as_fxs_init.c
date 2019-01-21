/*
 * $Revision: 1.1.1.1 $
 * $Author: lizhijie $
 * $Log: as_fxs_init.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/06/27 06:00:44  lizhijie
 * no message
 *
 * Driver for PCM-Analog Card
*/
#include "as_fxs_common.h"

static struct pci_device_id as_fxs_pci_tbl[] = 
{
	{ 0xe159, 0x0001, PCI_ANY_ID, PCI_ANY_ID, 0, 0,  /*(unsigned long) &as_fxse*/ },
#if 0		
	{ 0xe159, 0x0001, PCI_ANY_ID, PCI_ANY_ID, 0, 0, /* (unsigned long) &as_fxse*/ },	
	{ 0xe159, 0x0001, PCI_ANY_ID, PCI_ANY_ID, 0, 0, /* (unsigned long) &as_fxse*/ },	
	{ 0xe159, 0x0001, PCI_ANY_ID, PCI_ANY_ID, 0, 0, /* (unsigned long) &as_fxse*/ },	
#endif	
	{ 0, }
};

MODULE_DEVICE_TABLE(pci,   as_fxs_pci_tbl);

static struct pci_driver as_fxs_driver = 
{
	name: 	"astdm_analog",
	probe: 	as_fxs_init_one,
	remove:	as_fxs_remove_one,
	suspend: NULL,
	resume:	NULL,
	id_table: as_fxs_pci_tbl,
};

static int __init as_fxs_init(void)
{
	int res;
	res = pci_module_init(&as_fxs_driver);
	trace
	if (res)
	{
		return -ENODEV;
	}
	
	printk(AS_VERSION_INFO("pcm") );
	
	return 0;
}

static void __exit as_fxs_cleanup(void)
{
	pci_unregister_driver(&as_fxs_driver);
}



MODULE_DESCRIPTION("Asttel low layer driver for PCM-Analog Card");
MODULE_AUTHOR("Chengdu R&D <supports@assistcn.com>");
MODULE_LICENSE("GPL");

module_init(as_fxs_init);
module_exit(as_fxs_cleanup);

