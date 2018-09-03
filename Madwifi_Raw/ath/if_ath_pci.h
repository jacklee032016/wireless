/*-
 * $Id: if_ath_pci.h,v 1.1.1.1 2006/11/30 16:59:50 lizhijie Exp $
 */

#ifndef _DEV_ATH_PCI_H_
#define _DEV_ATH_PCI_H_

#include <linux/pci.h>
#define bus_map_single		pci_map_single
#define bus_unmap_single	pci_unmap_single
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
#define bus_dma_sync_single	pci_dma_sync_single_for_cpu
#define	PCI_SAVE_STATE(a,b)	pci_save_state(a)
#define	PCI_RESTORE_STATE(a,b)	pci_restore_state(a)
#else
#define bus_dma_sync_single	pci_dma_sync_single
#define	PCI_SAVE_STATE(a,b)	pci_save_state(a,b)
#define	PCI_RESTORE_STATE(a,b)	pci_restore_state(a,b)
#endif
#define bus_alloc_consistent	pci_alloc_consistent
#define bus_free_consistent	pci_free_consistent
#define BUS_DMA_FROMDEVICE	PCI_DMA_FROMDEVICE
#define BUS_DMA_TODEVICE	PCI_DMA_TODEVICE

#endif   /* _DEV_ATH_PCI_H_ */
