/*
 * $Id: phy_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
#include "opt_ah.h"

#include "phy.h"

/* support for module parameters */
static char *ifname = MESH_WLAN_PHY_INFO;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,52))
MODULE_PARM(ifname, "s");
#else
module_param(ifname, charp, 0);
#endif

MODULE_PARM_DESC(ifname, "Interface name prefix (default: ath)");


struct ath_pci_softc
{
	struct ath_softc	aps_sc;
#ifdef CONFIG_PM
	u32				aps_pmstate[16];
#endif
};

/*
 * User a static table of PCI id's for now.  While this is the
 * "new way" to do things, we may want to switch back to having
 * the HAL check them by defining a probe method.
 */
static struct pci_device_id ath_pci_id_table[] __devinitdata =
{
	{ 0x168c, 0x0007, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x0012, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x0013, PCI_ANY_ID, PCI_ANY_ID },
	{ 0xa727, 0x0013, PCI_ANY_ID, PCI_ANY_ID },	/* 3com */
	{ 0x10b7, 0x0013, PCI_ANY_ID, PCI_ANY_ID },	/* 3com 3CRDAG675 */
	{ 0x168c, 0x1014, PCI_ANY_ID, PCI_ANY_ID },	/* IBM minipci 5212 */
	{ 0x168c, 0x0015, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x0016, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x0017, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x0018, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x0019, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x001a, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};

/* register IRQ, then ath attach, last hal probe ??? */
static int ath_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	unsigned long 			phymem;
	unsigned long 			mem;
	struct ath_pci_softc 	*sc;
	const char 			*athname;
	u_int8_t 				csz;
	u32					 val;
	MESH_DEVICE 		*dev;

	if (pci_enable_device(pdev))
		return (-EIO);

	/* XXX 32-bit addressing only */
	if (pci_set_dma_mask(pdev, 0xffffffff))
	{
		MESH_ERR_INFO( "ath_pci: 32-bit DMA not available\n");
		goto bad;
	}

	/*
	 * Cache line size is used to size and align various
	 * structures used to communicate with the hardware.
	 */
	pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &csz);
	if (csz == 0)
	{
		/*
		 * Linux 2.4.18 (at least) writes the cache line size
		 * register as a 16-bit wide register which is wrong.
		 * We must have this setup properly for rx buffer
		 * DMA to work so force a reasonable value here if it
		 * comes up zero.
		 */
		csz = L1_CACHE_BYTES / sizeof(u_int32_t);
		pci_write_config_byte(pdev, PCI_CACHE_LINE_SIZE, csz);
	}
	/*
	 * The default setting of latency timer yields poor results,
	 * set it to the value used by other systems.  It may be worth
	 * tweaking this setting more.
	 */
	pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 0xa8);

	pci_set_master(pdev);

	/*
	 * Disable the RETRY_TIMEOUT register (0x41) to keep
	 * PCI Tx retries from interfering with C3 CPU state.
	 *
	 * Code taken from ipw2100 driver - jg
	 */
	pci_read_config_dword(pdev, 0x40, &val);
	if ((val & 0x0000ff00) != 0)
		pci_write_config_dword(pdev, 0x40, val & 0xffff00ff);

	phymem = pci_resource_start(pdev, 0);
	if (!request_mem_region(phymem, pci_resource_len(pdev, 0), MESH_WLAN_PHY_INFO) )
	{
		MESH_ERR_INFO( "ath_pci: cannot reserve PCI memory region\n");
		goto bad;
	}

	mem = (unsigned long) ioremap(phymem, pci_resource_len(pdev, 0));
	if (!mem)
	{
		MESH_ERR_INFO("ath_pci: cannot remap PCI memory region\n") ;
		goto bad1;
	}

	sc = kmalloc(sizeof(struct ath_pci_softc), GFP_KERNEL);
	if (sc == NULL)
	{
		MESH_ERR_INFO( "ath_pci: no memory for device state\n");
		goto bad2;
	}
	memset(sc, 0, sizeof(struct ath_pci_softc));

	/*
	 * Mark the device as detached to avoid processing
	 * interrupts until setup is complete.
	 */
	sc->aps_sc.sc_invalid = 1;

	dev = &sc->aps_sc.sc_dev;	/* XXX blech, violate layering */

	/* use variable interface name prefix */
	strncpy(dev->name, ifname, MESHNAMSIZ - sizeof("%d") - 1);
//	strncat(dev->name, "%d", sizeof("%d"));

	dev->irq = pdev->irq;
	dev->mem_start = mem;
	dev->mem_end = mem + pci_resource_len(pdev, 0);
	dev->priv = sc;

	SET_MODULE_OWNER(dev);

	sc->aps_sc.sc_bdev = (void *) pdev;

	pci_set_drvdata(pdev, dev);

	if (request_irq(dev->irq, ath_intr, SA_SHIRQ, dev->name, dev))
	{
		MESH_WARN_INFO( "%s: request_irq failed\n", dev->name);
		goto bad3;
	}

	if (_phy_attach(id->device, dev) != 0)
		goto bad4;

	athname = ath_hal_probe(id->vendor, id->device);
	PHY_DEBUG_INFO( "%s: %s: mem=0x%lx, irq=%d\n", dev->name, athname ? athname : "Atheros ???", phymem, dev->irq);

	/* ready to process interrupts */
	sc->aps_sc.sc_invalid = 0;

	return 0;
bad4:
	free_irq(dev->irq, dev);
bad3:
	kfree(sc);
bad2:
	iounmap((void *) mem);
bad1:
	release_mem_region(phymem, pci_resource_len(pdev, 0));
bad:
	pci_disable_device(pdev);
	return (-ENODEV);
}

static void ath_pci_remove(struct pci_dev *pdev)
{
	MESH_DEVICE *dev = pci_get_drvdata(pdev);

	_phy_detach(dev);
	if (dev->irq)
		free_irq(dev->irq, dev);
	iounmap((void *) dev->mem_start);
	release_mem_region(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	pci_disable_device(pdev);
	
	kfree(dev);	//free_netdev(dev);
}

MODULE_DEVICE_TABLE(pci, ath_pci_id_table);

static struct pci_driver ath_pci_drv_id =
{
	.name		= "phy_pci",
	.id_table		= ath_pci_id_table,
	.probe		= ath_pci_probe,
	.remove		= ath_pci_remove,
#if 0 //#ifdef CONFIG_PM
	.suspend		= ath_pci_suspend,
	.resume		= ath_pci_resume,
#endif
	/* Linux 2.4.6 has save_state and enable_wake that are not used here */
};

#define	ATH_PCI_VERSION	"0.9.5.0-BSD"
static char *phyVersion = 	ATH_PCI_VERSION " (EXPERIMENTAL)";

static int __init phy_init(void)
{
	PHY_DEBUG_INFO(" %s\n", phyVersion);

	if (pci_register_driver(&ath_pci_drv_id) < 0)
	{
		MESH_ERR_INFO(": No PCI devices found, driver not installed.\n");
		pci_unregister_driver(&ath_pci_drv_id);
		return (-ENODEV);
	}
	
#ifdef CONFIG_SYSCTL
	ath_sysctl_register();
#endif
	return (0);
}

static void __exit phy_exit(void)
{
#ifdef CONFIG_SYSCTL
	ath_sysctl_unregister();
#endif
	pci_unregister_driver(&ath_pci_drv_id);

	PHY_DEBUG_INFO(": driver unloaded\n" );
}

module_init( phy_init);
module_exit( phy_exit);


