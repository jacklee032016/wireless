/*
* $Id: phy_pci_pm.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
* Power Management of PCI Device
*/

#include "phy.h"

#ifdef CONFIG_PM

void ath_suspend(MESH_DEVICE *dev)
{
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: flags %x\n", __func__, dev->flags);
#if 0	
	if (sc->sc_softled) 
	    	ath_hal_gpioset(ah, sc->sc_ledpin, 1);
#endif
	_phy_stop(dev);
}


void ath_resume(MESH_DEVICE *dev)
{
	struct ath_softc *sc = (struct ath_softc *)dev->priv;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: flags %x\n", __func__, dev->flags);
	_phy_init(dev);
#if 0	
	if (sc->sc_softled)
	{
	    	ath_hal_gpioCfgOutput(ah, sc->sc_ledpin);
	    	ath_hal_gpioset(ah, sc->sc_ledpin, 0);
	}
#endif	
}

static int ath_pci_suspend(struct pci_dev *pdev, u32 state)
{
	MESH_DEVICE *dev = pci_get_drvdata(pdev);

	ath_suspend(dev);
	PCI_SAVE_STATE(pdev, ((struct ath_pci_softc *)dev->priv)->aps_pmstate);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, 3);

	return (0);
}

static int ath_pci_resume(struct pci_dev *pdev)
{
	MESH_DEVICE *dev = pci_get_drvdata(pdev);
	u32 val;

	pci_enable_device(pdev);
	PCI_RESTORE_STATE(pdev, ((struct ath_pci_softc *)dev->priv)->aps_pmstate);
	/*
	 * Suspend/Resume resets the PCI configuration space, so we have to
	 * re-disable the RETRY_TIMEOUT register (0x41) to keep
	 * PCI Tx retries from interfering with C3 CPU state
	 *
	 * Code taken from ipw2100 driver - jg
	 */
	pci_read_config_dword(pdev, 0x40, &val);
	if ((val & 0x0000ff00) != 0)
		pci_write_config_dword(pdev, 0x40, val & 0xffff00ff);
	ath_resume(dev);

	return (0);
}
#endif /* CONFIG_PM */

