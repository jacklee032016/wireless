/*
 * $Author: lizhijie $
 * $Log: as_fxs_common.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.5  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.4  2004/12/11 06:14:18  lizhijie
 * add some header files for Warnning
 *
 * Revision 1.3  2004/11/29 01:59:56  lizhijie
 * some bug about echo features
 *
 * Revision 1.2  2004/11/26 12:34:15  lizhijie
 * add multi-card support
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#ifndef  __AS_FXS_COMMON_H__
#define __AS_FXS_COMMON_H__

/*
 * Wilcard TDM400P TDM FXS/FXO Interface Driver for Zapata Telephony interface
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

#define DEBUG

#include "as_fxs.h"
#include "proslic.h"

struct fxo_mode 
{
	char *name;
	/* FXO */
	int ohs;
	int ohs2;
	int rz;
	int rt;
	int ilim;
	int dcv;
	int mini;
	int acim;
};


#include "asstel.h"

#define NUM_FXO_REGS 60

#define WC_MAX_IFACES 1

/* following are the internal registers of Tiger 320 */
#define WC_CNTL    	0x00
#define WC_OPER		0x01
#define WC_AUXC    	0x02		/* AUX Control register */
#define WC_AUXD    	0x03		/* AUX Data Register  */
#define WC_MASK0   	0x04
#define WC_MASK1   	0x05
#define WC_INTSTAT 	0x06
#define WC_AUXR		0x07

#define WC_DMAWS	0x08
#define WC_DMAWI	0x0c
#define WC_DMAWE	0x10
#define WC_DMARS	0x18
#define WC_DMARI	0x1c
#define WC_DMARE	0x20

#define WC_AUXFUNC	0x2b
#define WC_SERCTL	0x2d
#define WC_FSCDELAY	0x2f

#define WC_REGBASE	0xc0  /*tiger PIB address space base address */

/* following are registers of prepherial device connectted on the PIB bus of Tiger 320 */
#define WC_SYNC		0x0
#define WC_TEST		0x1
#define WC_CS		0x2
#define WC_VER		0x3

#define BIT_CS		(1 << 2)
#define BIT_SCLK		(1 << 3)
#define BIT_SDI		(1 << 4)
#define BIT_SDO		(1 << 5)

#define FLAG_EMPTY	0
#define FLAG_WRITE	1
#define FLAG_READ	2

#define RING_DEBOUNCE	64		/* Ringer Debounce (in ms) */
#define BATT_DEBOUNCE	64		/* Battery debounce (in ms) */
#define BATT_THRESH	3		/* Anything under this is "no battery" */

#define OHT_TIMER		6000	/* How long after RING to retain OHT */

#define FLAG_DOUBLE_CLOCK	(1 << 0)

#define NUM_CARDS 4

#define MAX_ALARMS 10

#define MOD_TYPE_FXS	0
#define MOD_TYPE_FXO	1

#define MINPEGTIME	10 * 8		/* 30 ms peak to peak gets us no more than 100 Hz */
#define PEGTIME		50 * 8		/* 50ms peak to peak gets us rings of 10 Hz or more */
#define PEGCOUNT	5		/* 5 cycles of pegging means RING */

#define NUM_CAL_REGS 12

struct calregs 
{
	unsigned char vals[NUM_CAL_REGS];
};

struct wcfxs 
{
	struct pci_dev *dev;
	struct as_dev_span *span;
	unsigned char ios;
	int usecount;
	int intcount;
	int dead;
	int flags;
	int freeregion;
	int alt;
	int curcard;
	int cards;				/* totoal number of card that check out */
	int cardslot[NUM_CARDS];	/* slot number of checked modules ,added by lizhijie 2004.11.13*/
	int cardflag;		/* Bit-map of present cards */
	spinlock_t lock;

	/* FXO Stuff */
	union 
	{
		struct 
		{
			unsigned int pegtimer[NUM_CARDS];
			int pegcount[NUM_CARDS];
			int peg[NUM_CARDS];
			int ring[NUM_CARDS];
#if 1			
			int wasringing[NUM_CARDS];
#endif			
			int ringdebounce[NUM_CARDS];
			int offhook[NUM_CARDS];
			int battdebounce[NUM_CARDS];
			int nobatttimer[NUM_CARDS];
			int battery[NUM_CARDS];
		} fxo;/* FXO device : with FXS signal */
		struct 
		{
			int oldrxhook[NUM_CARDS];
			int debouncehook[NUM_CARDS];
			int lastrxhook[NUM_CARDS];
			int debounce[NUM_CARDS];
			int ohttimer[NUM_CARDS];
			int idletxhookstate[NUM_CARDS];		/* IDLE changing hook state */
			int lasttxhook[NUM_CARDS];
			int palarms[NUM_CARDS];
			struct calregs calregs[NUM_CARDS];
		} fxs;/* FXS device : with FXO signal */
	} mod;

	/* Receive hook state and debouncing */
	int modtype[NUM_CARDS];

	unsigned long ioaddr;  /* base address for tiger address space, not the PCI config space  page 10*/
	dma_addr_t 	readdma;
	dma_addr_t	writedma;
	volatile int *writechunk;					/* Double-word aligned write memory */
	volatile int *readchunk;					/* Double-word aligned read memory */

/* for multi-cards support , lzj 2004.11.25 */
	int module; 			/* start from 1 for readable */		
	struct wcfxs *next;
	char name[32];

#if AS_DEBUG_TIGER
/* added for debug , lzj 2004.12.08 */
	char*  (*read_info)(struct wcfxs *wc);
#endif
/* added for different interrupts of READ/WRITE in IXP422, lizhijie 2004.12.15 */
	int write_interrupts;
	int write_ends;
	int read_interrupts;
	int read_ends;

	struct as_dev_chan *chans[NUM_CARDS];
};


struct wcfxs_desc 
{
	char *name;
	int flags;
};


int __devinit as_fxs_init_one(struct pci_dev *pdev, const struct pci_device_id *ent);
void __devexit as_fxs_remove_one(struct pci_dev *pdev);
void as_fxs_release(struct wcfxs *wc);


int as_fxs_hooksig(struct as_dev_chan *chan, as_txsig_t txsig);
int as_fxs_close(struct as_dev_chan *chan);
int as_fxs_watchdog(struct as_dev_chan *span, int event);
int as_fxs_open(struct as_dev_chan *chan);
int as_fxs_ioctl(struct as_dev_chan *chan, unsigned int cmd, unsigned long data);

void as_fxs_interrupt(int irq, void *dev_id, struct pt_regs *regs);

void as_fxs_stop_dma(struct wcfxs *wc);
void as_fxs_reset_tdm(struct wcfxs *wc);
void as_fxs_restart_dma(struct wcfxs *wc);
void as_fxs_disable_interrupts(struct wcfxs *wc);	


unsigned char as_fxs_get_dtmf(struct wcfxs *wc, int card );
void as_fxs_enable_dtmf_detect(struct wcfxs *wc, int card);


void as_fxs_setcreg(struct wcfxs *wc, unsigned char reg, unsigned char val);
unsigned char as_fxs_getcreg(struct wcfxs *wc, unsigned char reg);

void as_fxs_setreg(struct wcfxs *wc, int card, unsigned char reg, unsigned char value);
unsigned char as_fxs_getreg(struct wcfxs *wc, int card, unsigned char reg);
void as_reset_spi(struct wcfxs *wc, int card);

void as_fxs_setreg_nolock(struct wcfxs *wc, int card, unsigned char reg, unsigned char value);
unsigned char as_fxs_getreg_nolock(struct wcfxs *wc, int card, unsigned char reg);
int as_fxs_init_proslic(struct wcfxs *wc, int card, int fast, int manual, int sane);
int __wcfxs_proslic_setreg_indirect(struct wcfxs *wc, int card, unsigned char address, unsigned short data);
int __wcfxs_proslic_getreg_indirect(struct wcfxs *wc, int card, unsigned char address);

int as_fxs_init_voicedaa(struct wcfxs *wc, int card, int fast, int manual, int sane);

/* from as_fxs_tone.c */
int as_fxs_play_tone( struct as_dev_chan *chan);

#if AS_DEBUG_TIGER
/* from as_fxs_tiger.c , added by lizhijie 2004.12.08 */
char *as_fxs_tiger_info(struct wcfxs *wc);
#endif

#endif
