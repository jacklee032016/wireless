/*
 * $Id: ashfc_hw_init.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */
 
#include <linux/pci.h>
#include <linux/delay.h>

#include "dchannel.h"
#include "bchannel.h"
#include "layer1.h"
#include "helper.h"

#if 0
#define SPIN_DEBUG
#define LOCK_STATISTIC
#endif
#include "hw_lock.h"
#include "ashfc.h"

/* table entry in the PCI devices list */
typedef struct
{
	int vendor_id;
	int vendor_sub;
	int device_id;
	int device_sub;
	char *vendor_name;
	char *card_name;
	int type;
	int clock2;				/* enable Double Clock mode */
	int leds;
} PCI_ENTRY;

#define VENDOR_CCD "Cologne Chip AG"

static const PCI_ENTRY id_list[] =
{
	{0x1397, 0x1397, 0x08B4, 0xB520, VENDOR_CCD, "HFC-4S IOB4ST", 			4, 1, 2},
	{0x1393, 0x1393, 0x08B4, 0xB520, VENDOR_CCD, "Moxa HFC-4S IOB4ST", 			4, 1, 2},
	{0x1397, 0x1397, 0x08B4, 0x08B4, VENDOR_CCD, "HFC-4S Eval", 				4, 0, 0},
#if 0	
	{0x1397, 0x1397, 0x08B4, 0xB620, VENDOR_CCD, "HFC-4S", 					4, 1, 2},
	{0x1397, 0x1397, 0x08B4, 0xB560, VENDOR_CCD, "HFC-4S Beronet Card", 		4, 1, 2},
	{0x1397, 0x1397, 0x16B8, 0x16B8, VENDOR_CCD, "HFC-8S Eval", 				8, 0, 0},
#endif	
	{0, 0, 0, 0, NULL, NULL, 0, 0, 0},
};



/***************** free hardware resources used by driver ************/
void ashfc_release_io(ashfc_t *hc)
{
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s : entered release IO resources\n", hc->name);
#endif

	/* irq off */
	HFC_outb(hc, R_IRQ_CTRL, 0);

	/* soft reset */
	hc->hw.r_cirm |= V_SRES;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(10);
	hc->hw.r_cirm &= ~V_SRES;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(10); /* instead of 'wait' that may cause locking */

	/* disable memory mapped ports / io ports */
	pci_write_config_word(hc->pci_dev, PCI_COMMAND, 0);
#if CONFIG_HFCMULTI_PCIMEM
	if (hc->pci_membase)
		iounmap((void *)hc->pci_membase);
#else
	if (hc->pci_iobase)
		release_region(hc->pci_iobase, 8);
#endif

}

/***************** find pci device and set it up ********************/
int ashfc_setup_pci(ashfc_t *hc)
{
	int i;
	struct pci_dev *tmp_dev = NULL;
	ashfc_t *hc_tmp, *next;

	/* go into 0-state (we might already be due to zero-filled-object */
	i = 0;
	while(i < 32)
	{
		if (hc->chan[i].dch)
			hc->chan[i].dch->ph_state = 0;
		i++;
	}

	/* loop all card ids */
	i = 0;
	while (id_list[i].vendor_id)
	{
		/* only the given type is searched */
		if (id_list[i].type != hc->type)
		{
			i++;
			continue;
		}
#if 0		
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "ashfc_setup_pci(): investigating card entry %d (looking for type %d)\n", i, hc->type);
#endif

inuse:
		tmp_dev = pci_find_subsys(id_list[i].vendor_id, id_list[i].device_id, id_list[i].vendor_sub, id_list[i].device_sub, tmp_dev);
		if (tmp_dev)
		{
			/* skip if already in use */
			list_for_each_entry_safe(hc_tmp, next, &ASHFC_obj.ilist, list)
			{
				if (hc_tmp->pci_dev == tmp_dev)
				{
					if (ashfc_debug & ASHFC_DEBUG_INIT)
						printk(KERN_INFO "%s : found a pci card: '%s' card name: '%s', but already in use, so we skip.\n",hc->name, id_list[i].vendor_name, id_list[i].card_name);
					goto inuse;
				}
			}
			break;
		}
		i++;
	}
	
	if (!tmp_dev)
	{
		printk(KERN_WARNING "%s : No PCI card found\n", hc->name);
		return (-ENODEV);
	}

#if WITH_ASHFC_DEBUG_INIT
	/* found a card */
	printk(KERN_INFO "%s : Chip: '%s'; Card : '%s'; Clock: %s\n", hc->name, id_list[i].vendor_name, id_list[i].card_name, (id_list[i].clock2)?"double":"normal");
#endif
	hc->pci_dev = tmp_dev;
	if (id_list[i].clock2)
		test_and_set_bit(HFC_CHIP_CLOCK2, &hc->chip);
	
	if (hc->pci_dev->irq <= 0)
	{
		printk(KERN_WARNING "%s : No IRQ for PCI card found.\n", hc->name);
		return (-EIO);
	}

	if (pci_enable_device(hc->pci_dev))
	{
		printk(KERN_WARNING "%s : Error enabling PCI card.\n", hc->name);
		return (-EIO);
	}
	hc->leds = id_list[i].leds;
#if CONFIG_HFCMULTI_PCIMEM
	hc->pci_membase = (char *) get_pcibase(hc->pci_dev, 1);
	if (!hc->pci_membase)
	{
		printk(KERN_WARNING "%s : No IO-Memory for PCI card found\n", hc->name );
		return (-EIO);
	}

	if (!(hc->pci_membase = ioremap((ulong) hc->pci_membase, 256)))
	{
		printk(KERN_WARNING "%s : failed to remap io address space. (internal error)\n", hc->name );
		hc->pci_membase = NULL;
		return (-EIO);
	}
	printk(KERN_INFO "%s : defined at MEMBASE %#x IRQ %d HZ %d leds-type %d\n", hc->name, (u_int) hc->pci_membase, hc->pci_dev->irq, HZ, hc->leds);
	pci_write_config_word(hc->pci_dev, PCI_COMMAND, PCI_ENA_MEMIO);
#else
	hc->pci_iobase = pci_resource_start(hc->pci_dev, 0);
//	hc->pci_iobase = (u_int) get_pcibase(hc->pci_dev, 0);
	if (!hc->pci_iobase)
	{
		printk(KERN_WARNING "%s : No IO for PCI card found\n", hc->name);
		return (-EIO);
	}
	if (!request_region(hc->pci_iobase, 8, "as_hfc4s"))
	{
		printk(KERN_WARNING "%s : failed to rquest address space at 0x%04x (internal error)\n", hc->name, hc->pci_iobase);
		hc->pci_iobase = 0;
		return (-EIO);
	}

#if WITH_ASHFC_DEBUG_INIT
	printk(KERN_INFO "%s : defined at IOBASE %#x IRQ %d HZ %d leds-type %d\n", hc->name, (u_int) hc->pci_iobase, hc->pci_dev->irq, HZ, hc->leds);
#endif

	pci_write_config_word(hc->pci_dev, PCI_COMMAND, PCI_ENA_REGIO);
#endif

	/* At this point the needed PCI config is done */
	/* fifos are still not enabled */
        lock_dev(hc, 0);

#ifdef SPIN_DEBUG
	printk(KERN_ERR "spin_lock_adr=%p now(%p)\n", &hc->lock.spin_adr, hc->lock.spin_adr);
	printk(KERN_ERR "busy_lock_adr=%p now(%p)\n", &hc->lock.busy_adr, hc->lock.busy_adr);
#endif
	unlock_dev(hc);
	return (0);
}

static int _ashfc_init_pcm(ashfc_t *hc)
{
	int i;
	int cnt = 0;
	u_long val, val2 = 0;
	
	/* set pcm mode & reset */
	if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip))
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s: PCM setting into slave mode\n", hc->name);
#endif
	}
	else
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s: PCM setting into master mode\n", hc->name);
#endif
		hc->hw.r_pcm_md0 |= V_PCM_MD;
	}
	i = 0;
	
	HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0x90);	/* select R_PCM_MD1 */
	if (hc->slots == 32)
		HFC_outb(hc, R_PCM_MD1, 0x00);	/* 2 Mbit/s, C4IO is 4.094Mhz, 32 slots */
	if (hc->slots == 64)
		HFC_outb(hc, R_PCM_MD1, 0x10);	/* 4 Mbit/s, C4IO is 8.192Mhz, 64 slots */
	if (hc->slots == 128)
		HFC_outb(hc, R_PCM_MD1, 0x20);	/* 8 Mbit/s, C4IO is 16.384Mhz, 128 slots */

	HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0xa0);	/* select R_PCM_MD2 */
	/* PCM mode 2 : PCM PLL sync src : ST interface etc */
	HFC_outb(hc, R_PCM_MD2, 0x00);
	HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0x00);

	while (i < 256)
	{
		HFC_outb_(hc, R_SLOT, i);			/* select PCM slot */
		HFC_outb_(hc, A_SL_CFG, 0);		/* HFC channel for slot i */
		HFC_outb_(hc, A_CONF, 0);		/* Conference for slot i */
		hc->slot_owner[i] = -1;
		i++;
	}

	/* set clock speed */
	if (test_bit(HFC_CHIP_CLOCK2, &hc->chip))
	{/* when OSC_IN is 24.576Mhz */
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s: PCM Fpcm Setting double clock\n", hc->name );
#endif
		HFC_outb(hc, R_BRG_PCM_CFG, V_PCM_CLK);		/* Fpcm must be 49.152Mhz */
	}

	/* check if R_F0_CNT counts */
	val = HFC_inb(hc, R_F0_CNTL);
	val += HFC_inb(hc, R_F0_CNTH) << 8;
#if 0	
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "ASHFC F0_CNT %ld after status ok\n", val);
#endif	
	unlock_dev(hc);
	set_current_state(TASK_UNINTERRUPTIBLE);
	while (cnt < 50)
	{ /* max 50 ms */
		schedule_timeout((HZ*10)/1000); /* Timeout 10ms */
		cnt+=10;
		val2 = HFC_inb(hc, R_F0_CNTL);
		val2 += HFC_inb(hc, R_F0_CNTH) << 8;
		if (val2 >= val+4) /* wait 4 pulses */
			break;
	}
	lock_dev(hc, 0);

#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
	{
#if 1
		printk(KERN_ERR  "%s : PCM 125us FSYNC test -- %ld pulse in about after %dms\n", hc->name, val2-val, cnt);
#else
		printk(KERN_ERR  "ASHFC F0_CNT %ld after %dms\n", val2, cnt);
#endif
	}
#endif

	if (val2 < val+4)
	{
		printk(KERN_ERR "%s : PCM ERROR 125us pulse not counting.\n", hc->name);
		if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip))
		{
			printk(KERN_ERR "%s : PCM This happens in PCM slave mode without connected master.\n", hc->name);
		}
		if (!test_bit(HFC_CHIP_CLOCK_IGNORE, &hc->chip))
			return(-EIO);
	}

	return 0;
}

/****************************************************************************/
/* function called to reset the HFC chip. A complete software reset of chip */
/* and fifos is done. All configuration of the chip is done.                */
/****************************************************************************/
static int _ashfc_init_chip(ashfc_t *hc)
{
	u_long rev;
	u_char r_conf_en;
	int err;
	u_long val;

	/* reset all registers */
	memset(&hc->hw, 0, sizeof(ashfc_hw_t));

#if WITH_ASHFC_DEBUG_INIT
	/* revision check */
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: entered\n", __FUNCTION__);
#endif	
	val = HFC_inb(hc, R_CHIP_ID);
	rev = HFC_inb(hc, R_CHIP_RV);
	
#if WITH_ASHFC_DEBUG_INIT
	printk(KERN_INFO "Chip : %s", ((val>>4)==0x0C)? "HFC-4S": "Unknown");
	printk(KERN_INFO "ASHFC: resetting HFC with chip ID=0x%lx revision=%ld%s\n", val>>4, rev, (rev==0)?" (old FIFO handling)":"");
#endif
	if (rev == 0)
	{
		test_and_set_bit(HFC_CHIP_REVISION0, &hc->chip);
		printk(KERN_WARNING "ASHFC: NOTE: Your chip is revision 0, ask Cologne Chip for update. Newer chips have a better FIFO handling. Old chips still work but may have slightly lower HDLC transmit performance.\n");
	}
	if (rev > 1)
	{
		printk(KERN_WARNING "ASHFC: WARNING: This driver doesn't consider chip revision = %ld. The chip / bridge may not work.\n", rev);
	}

	/* set s-ram size */
	hc->Flen = 0x10;
	hc->Zmin = 0x80;
	hc->Zlen = 384;
	hc->DTMFbase = 0x1000;
	if (test_bit(HFC_CHIP_EXRAM_128, &hc->chip))
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s: RAM changing to 128K extenal RAM\n", hc->name);
#endif		
		hc->hw.r_ctrl |= V_EXT_RAM;
		hc->hw.r_ram_sz = ASHFC_RAM_128K;
		hc->Flen = 0x20;
		hc->Zmin = 0xc0;
		hc->Zlen = 1856;
		hc->DTMFbase = 0x2000;
	}

	if (test_bit(HFC_CHIP_EXRAM_512, &hc->chip))
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s: RAM changing to 512K extenal RAM\n", hc->name );
#endif		
		hc->hw.r_ctrl |= V_EXT_RAM;
		hc->hw.r_ram_sz = ASHFC_RAM_512K;
		hc->Flen = 0x20;
		hc->Zmin = 0xc0;
		hc->Zlen = 8000;
		hc->DTMFbase = 0x2000;
	}

	/* we only want the real Z2 read-pointer for revision > 0 */
	if (!test_bit(HFC_CHIP_REVISION0, &hc->chip))
		hc->hw.r_ram_sz |= V_FZ_MD;		/*p.104 */

	/* soft reset */
	HFC_outb(hc, R_CTRL, hc->hw.r_ctrl);
	HFC_outb(hc, R_RAM_SZ, hc->hw.r_ram_sz);
	HFC_outb(hc, R_FIFO_MD, 0);				/* FIFO is in Simple Mode */
	hc->hw.r_cirm = V_SRES | V_HFCRES | V_PCMRES | V_STRES | V_RLD_EPR;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(10);			/* at least 10 ns */
	hc->hw.r_cirm = 0;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(10);
	HFC_outb(hc, R_RAM_SZ, hc->hw.r_ram_sz);

	err = _ashfc_init_pcm(hc);
	if(err)
		return err;
	
	/* set up timer */
	HFC_outb(hc, R_TI_WD, ashfc_poll_timer);
	hc->hw.r_irqmsk_misc |= V_TI_IRQMSK;

	/* set DTMF detection */
	if (test_bit(HFC_CHIP_DTMF, &hc->chip))
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s : DTMF enabling detection for all B-channel\n", hc->name);
#endif
		hc->hw.r_dtmf = V_DTMF_EN | V_DTMF_STOP;
		if (test_bit(HFC_CHIP_ULAW, &hc->chip))
			hc->hw.r_dtmf |= V_ULAW_SEL;
		HFC_outb(hc, R_DTMF_N, 102-1);			/* number of algorithm step : duration is 12.75 ms */
		hc->hw.r_irqmsk_misc |= V_DTMF_IRQMSK;
	}

	/* conference engine */
	if (test_bit(HFC_CHIP_ULAW, &hc->chip))
		r_conf_en = V_CONF_EN | V_ULAW;
	else
		r_conf_en = V_CONF_EN;
	HFC_outb(hc, R_CONF_EN, r_conf_en);

	/* setting leds */
	if(hc->leds)
	{/* HFC-4S OEM */
		HFC_outb(hc, R_GPIO_SEL, 0xf0);		/* enable GPIO 8--15 */
		HFC_outb(hc, R_GPIO_EN1, 0xff);		/* GPIO 8--15 as output */
		HFC_outb(hc, R_GPIO_OUT1, 0x00);		/* GPIO 8--16 output data */
	}

	/* set master clock of S/T interface */
//	if (hc->masterclk >= 0)
	{
		hc->masterclk =0;
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s : ST setting master clock to port %d (0..%d)\n", hc->name, hc->masterclk, hc->type-1);
#endif
		hc->hw.r_st_sync = hc->masterclk | V_AUTO_SYNC;	/* enable manual sync */
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync);
	}

	/* setting misc irq */
	HFC_outb(hc, R_IRQMSK_MISC, hc->hw.r_irqmsk_misc);

#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: done\n", __FUNCTION__);
#endif
	return(0);
}

/*********************** called for card mode init message **************************/
/* init work mode of HFC card */
void _ashfc_initmode(ashfc_t *hc)
{
#define CLKDEL_TE	0x0E	/* CLKDEL in TE mode , refer to P.213, it should be 0x0E */

#define CLKDEL_NT	0x0C	/* CLKDEL in NT mode (0x60 MUST not be included!) */

	int nt_mode;
	unsigned char r_sci_msk, a_st_wr_state;
	int i, port;
	dchannel_t *dch;

	lock_dev(hc, 0);

	/* 4/8 S */
	i = 0;
	while (i < hc->type)
	{/* i is cardNumber, from 0--3 when 4S */	
		/* D channel setup */
		nt_mode = test_bit(HFC_CFG_NTMODE, &hc->chan[(i<<2)+2].cfg);
		hc->chan[(i<<2)+2].slot_tx = -1;
		hc->chan[(i<<2)+2].slot_rx = -1;
		hc->chan[(i<<2)+2].conf = -1;
		ashfc_chan_mode(hc, (i<<2)+2, nt_mode?ISDN_PID_L1_NT_S0:ISDN_PID_L1_TE_S0, -1, 0, -1, 0);

		hc->chan[(i<<2)+2].dch->hw_bh = ashfc_dch_bh;
		hc->chan[(i<<2)+2].dch->dbusytimer.function = (void *) ashfc_dbusy_timer;
		hc->chan[(i<<2)+2].dch->dbusytimer.data = (long) &hc->chan[(i<<2)+2].dch;
		init_timer(&hc->chan[(i<<2)+2].dch->dbusytimer);
	
		/* B1 channel setup */
		hc->chan[i<<2].bch->hw_bh = ashfc_bch_bh;
		hc->chan[i<<2].slot_tx = -1;
		hc->chan[i<<2].slot_rx = -1;
		hc->chan[i<<2].conf = -1;
		ashfc_chan_mode(hc, i<<2, ISDN_PID_NONE, -1, 0, -1, 0);
		hc->chan[i<<2].bch->protocol = ISDN_PID_NONE;
		
		/* B2 channel setup */
		hc->chan[(i<<2)+1].bch->hw_bh = ashfc_bch_bh;
		hc->chan[(i<<2)+1].slot_tx = -1;
		hc->chan[(i<<2)+1].slot_rx = -1;
		hc->chan[(i<<2)+1].conf = -1;
		ashfc_chan_mode(hc, (i<<2)+1, ISDN_PID_NONE, -1, 0, -1, 0);
		hc->chan[(i<<2)+1].bch->protocol = ISDN_PID_NONE;
		i++;
	}


	/* set up ST interface */
	r_sci_msk = 0;
	i = 0;
	while(i < 32)
	{
		if (!(dch = hc->chan[i].dch))
		{/* find index of D channel */
			i++;
			continue;
		}

		port = hc->chan[i].port;
		HFC_outb(hc, R_ST_SEL, port);/* select interface */

		if (test_bit(HFC_CFG_NTMODE, &hc->chan[i].cfg))
		{
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_INIT)
				printk(KERN_ERR  "%s: ST port %d is NT-mode\n", hc->name, port);
#endif			
			/* clock delay */
			HFC_outb(hc, A_ST_CLK_DLY, CLKDEL_NT | 0x60);		/* refer p.213 */
			a_st_wr_state = NT_STATE_G1_DEACTIVATED; 			/* G1 */
			hc->hw.a_st_ctrl0[port] = V_ST_MD;					/* flag for NT mode */
		}
		else
		{
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_INIT)
				printk(KERN_ERR  "%s: ST port %d is TE-mode\n", hc->name, port);
#endif			
			/* clock delay */
			HFC_outb(hc, A_ST_CLK_DLY, CLKDEL_TE);
			a_st_wr_state = TE_STATE_F2_SENSING; /* F2 */
			hc->hw.a_st_ctrl0[port] = 0;
		}

		if (!test_bit(HFC_CFG_NONCAP_TX, &hc->chan[i].cfg))
		{
			hc->hw.a_st_ctrl0[port] |= V_TX_LI;
		}
		/* line setup */
		HFC_outb(hc, A_ST_CTRL0,  hc->hw.a_st_ctrl0[port]);

		/* disable E-channel when NT mode or configuration */
		if (test_bit(HFC_CFG_NTMODE, &hc->chan[i].cfg) || test_bit(HFC_CFG_DIS_ECHANNEL, &hc->chan[i].cfg))
		{/* ignore E channel dara: D channel always sends data regardless of the received E-channel bit*/
			HFC_outb(hc, A_ST_CTRL1, V_E_IGNO);	
		}

		/* enable B-channel receive */
		HFC_outb(hc, A_ST_CTRL2,  V_B1_RX_EN | V_B2_RX_EN);

		/* state machine setup */
		HFC_outb(hc, A_ST_WR_STATE, a_st_wr_state | V_ST_LD_STA);	/* load stateMachine*/
		udelay(6); /* wait at least 5,21us */
		HFC_outb(hc, A_ST_WR_STATE, a_st_wr_state);	/* clear load */
		r_sci_msk |= 1 << port;
		i++;
	}

	/* state machine interrupts */
	HFC_outb(hc, R_SCI_MSK, r_sci_msk);

	/* set interrupts & global interrupt */
	hc->hw.r_irq_ctrl = V_FIFO_IRQ | V_GLOB_IRQ_EN;

	unlock_dev(hc);

}

/************************ initialize the card ************************/
/* start timer irq, wait some time and check if we have interrupts.
 * if not, reset chip and try again. */
static int _ashfc_hardware_init(ashfc_t *hc)
{
	int try_cnt = 1; /* as long as there is no trouble */
	int err = -EIO;


	lock_dev(hc, 0);
	if (request_irq(hc->pci_dev->irq, ashfc_interrupt, SA_SHIRQ, "ASHFC", hc))
	{
		printk(KERN_WARNING "%s : Could not get interrupt %d.\n", hc->name, hc->pci_dev->irq);
		unlock_dev(hc);
		return(-EIO);
	}
	hc->irq = hc->pci_dev->irq;

	while (try_cnt)
	{
		if ((err = _ashfc_init_chip(hc)))
			goto error;
		
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s : IRQ(%d) Test -- Count = %d before enabled\n", hc->name, hc->irq, hc->irqcnt);
#endif

		/* 1 : Finally enable IRQ output, this is only allowed, if an IRQ routine is allready
		 * established for this HFC, so don't do that earlier */
		unlock_dev(hc);
		HFC_outb(hc, R_IRQ_CTRL, V_GLOB_IRQ_EN);
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout((100*HZ)/1000); /* Timeout 100ms :10 ms?? */

		/* 2 : turn IRQ off until chip is completely initialized */
		HFC_outb(hc, R_IRQ_CTRL, 0);

		/* 3 : check IRQ count */
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s : IRQ(%d) Test -- count = %d after test\r\n", hc->name, hc->irq, hc->irqcnt);
#endif
		if (hc->irqcnt)
		{
#if WITH_ASHFC_DEBUG_INIT
			if (ashfc_debug & ASHFC_DEBUG_INIT)
				printk(KERN_ERR  "%s: done\n", __FUNCTION__);
#endif			
			return(0);
		}
		
		lock_dev(hc, 0);
		printk(KERN_WARNING "%s : IRQ(%d) getting no interrupts during init (try %d)\n", hc->name, hc->irq, try_cnt);
		try_cnt--;
		err = -EIO;
	}

error:
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_WARNING "%s: free irq %d\n", __FUNCTION__, hc->irq);
#endif

	free_irq(hc->irq, hc);
	hc->irq = 0;
	unlock_dev(hc);

#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: done (err=%d)\n", __FUNCTION__, err);
#endif
	return(err);
}


int ashfc_hw_init( ashfc_t *hc)
{
	int err;
	
	/* run card setup */
#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
		printk(KERN_ERR  "%s: Setting up Card(%d)\n", hc->name, hc->id);
#endif
#if 0
	if ((err = _ashfc_setup_pci(hc)))
	{
		return err;
//		goto free_channels;
	}
#endif

	if ((err = _ashfc_hardware_init(hc)))
	{
#if WITH_ASHFC_DEBUG_INIT
		if (ashfc_debug & ASHFC_DEBUG_INIT)
		{
			printk(KERN_ERR  "%s: do release_io_as_hfc4s\n", __FUNCTION__);
		}
#endif
		ashfc_release_io(hc);
		return err;
//		goto free_channels;
	}

#if WITH_ASHFC_DEBUG_INIT
	if (ashfc_debug & ASHFC_DEBUG_INIT)
			printk(KERN_ERR  "%s : Init work mode for Card(%d)\n", hc->name, hc->id);
#endif
	_ashfc_initmode(hc);

	/* now turning on irq for this card */
	HFC_outb(hc, R_IRQ_CTRL, hc->hw.r_irq_ctrl);

	return 0;
}

