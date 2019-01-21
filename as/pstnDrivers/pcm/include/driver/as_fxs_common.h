/*
 * $Revision: 1.1.1.1 $
 * $Author: lizhijie $
 * $Id: as_fxs_common.h,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/
#ifndef  __AS_FXS_COMMON_H__
#define __AS_FXS_COMMON_H__
/*
 * Assist PCM-Analog Interface Driver
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

#define DEBUG

#include "asstel.h"

/* following are the internal registers of Tiger 320 */
#define WC_CNTL    				0x00
#define WC_OPER					0x01
#define WC_AUXC    				0x02		/* AUX Control register */
#define WC_AUXD    				0x03		/* AUX Data Register  */
#define WC_MASK0   				0x04
#define WC_MASK1   				0x05
#define WC_INTSTAT 				0x06
#define WC_AUXR					0x07

#define WC_DMAWS				0x08
#define WC_DMAWI				0x0c
#define WC_DMAWE				0x10
#define WC_DMARS				0x18
#define WC_DMARI				0x1c
#define WC_DMARE				0x20

#define WC_AUXFUNC				0x2b
#define WC_SERCTL				0x2d
#define WC_FSCDELAY			0x2f

#define NUM_CARDS 				4

#define MAX_ALARMS 				10

struct wcfxs 
{
/* for multi-cards support , lzj 2004.11.25 */
	int module; 			/* start from 1 for readable */		
	struct wcfxs *next;
	char name[32];

	struct pci_dev *dev;
	struct as_dev_span *span;
	
	int usecount;
	int intcount;
	int dead;
	int freeregion;
	
	int cards;				/* totoal number of card that check out */
	struct as_dev_chan *chans[NUM_CARDS];
	spinlock_t lock;

	unsigned long ioaddr;  /* base address for tiger address space, not the PCI config space  page 10*/
	dma_addr_t 	readdma;
	dma_addr_t	writedma;
	volatile int *writechunk;					/* Double-word aligned write memory */
	volatile int *readchunk;					/* Double-word aligned read memory */

#if AS_DEBUG_TIGER
/* added for debug , lzj 2005.03.07 */
	char*  (*read_info)(struct wcfxs *wc);
/* added for different interrupts of READ/WRITE in IXP422, lizhijie 2004.12.15 */
	int write_interrupts;
	int write_ends;
	int read_interrupts;
	int read_ends;
#endif

};

int __devinit as_fxs_init_one(struct pci_dev *pdev, const struct pci_device_id *ent);
void __devexit as_fxs_remove_one(struct pci_dev *pdev);
void as_fxs_release(struct wcfxs *wc);


void as_fxs_interrupt(int irq, void *dev_id, struct pt_regs *regs);

void as_fxs_stop_dma(struct wcfxs *wc);
void as_fxs_restart_dma(struct wcfxs *wc);


#if AS_DEBUG_TIGER
char *as_fxs_tiger_info(struct wcfxs *wc);
#endif

#endif

