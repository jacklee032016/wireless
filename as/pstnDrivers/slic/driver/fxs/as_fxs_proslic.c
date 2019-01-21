/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_fxs_proslic.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.7  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.6  2004/12/11 05:35:51  lizhijie
 * add Tiger320 debug info
 *
 * Revision 1.5  2004/11/29 01:53:38  lizhijie
 * update some redudent code and register setting
 *
 * Revision 1.4  2004/11/26 12:33:31  lizhijie
 * add multi-card support
 *
 * Revision 1.3  2004/11/25 07:59:24  lizhijie
 * recommit all
 *
*/
#include "as_fxs_common.h"

extern int debug;
extern int fxshonormode ;
extern int _opermode ;
extern int lowpower ;
extern int boostringer ;

extern struct fxo_mode  fxo_modes[];
/*
 *  Define for audio vs. register based ring detection
 *  
 */
/* #define AUDIO_RINGCHECK  */
static alpha  indirect_regs[] =
{
{0,"DTMF_ROW_0_PEAK",0x55C2},
{1,"DTMF_ROW_1_PEAK",0x51E6},
{2,"DTMF_ROW2_PEAK",0x4B85},
{3,"DTMF_ROW3_PEAK",0x4937},
{4,"DTMF_COL1_PEAK",0x3333},
{5,"DTMF_FWD_TWIST",0x0202},
{6,"DTMF_RVS_TWIST",0x0202},
{7,"DTMF_ROW_RATIO_TRES",0x0198},
{8,"DTMF_COL_RATIO_TRES",0x0198},
{9,"DTMF_ROW_2ND_ARM",0x0611},
{10,"DTMF_COL_2ND_ARM",0x0202},
{11,"DTMF_PWR_MIN_TRES",0x00E5},
{12,"DTMF_OT_LIM_TRES",0x0A1C},
{13,"OSC1_COEF",0x7B30},
{14,"OSC1X",0x0063},
{15,"OSC1Y",0x0000},
{16,"OSC2_COEF",0x7870},
{17,"OSC2X",0x007D},
{18,"OSC2Y",0x0000},
{19,"RING_V_OFF",0x0000},
{20,"RING_OSC",0x7EF0},
{21,"RING_X",0x0160},
{22,"RING_Y",0x0000},
{23,"PULSE_ENVEL",0x2000},
{24,"PULSE_X",0x2000},
{25,"PULSE_Y",0x0000},
#if __ARM__
	{26,"RECV_DIGITAL_GAIN",0x1000},	// playback volume set lower
	{27,"XMIT_DIGITAL_GAIN",0x1000},
#else
//{26,"RECV_DIGITAL_GAIN",0x4000},	// playback volume set lower
{26,"RECV_DIGITAL_GAIN",0x2000},	// playback volume set lower
{27,"XMIT_DIGITAL_GAIN",0x4000},
//{27,"XMIT_DIGITAL_GAIN",0x2000},
#endif
{28,"LOOP_CLOSE_TRES",0x1000},
{29,"RING_TRIP_TRES",0x3600},
{30,"COMMON_MIN_TRES",0x1000},
{31,"COMMON_MAX_TRES",0x0200},
{32,"PWR_ALARM_Q1Q2",0x07C0},
{33,"PWR_ALARM_Q3Q4",0x2600},
{34,"PWR_ALARM_Q5Q6",0x1B80},
{35,"LOOP_CLOSURE_FILTER",0x8000},
{36,"RING_TRIP_FILTER",0x0320},
{37,"TERM_LP_POLE_Q1Q2",0x008C},
{38,"TERM_LP_POLE_Q3Q4",0x0100},
{39,"TERM_LP_POLE_Q5Q6",0x0010},
{40,"CM_BIAS_RINGING",0x0C00},
{41,"DCDC_MIN_V",0x0C00},
{42,"DCDC_XTRA",0x1000},
{43,"LOOP_CLOSE_TRES_LOW",0x1000},
};

static int acim2tiss[16] = { 0x0, 0x1, 0x4, 0x5, 0x7, 0x0, 0x0, 0x6, 0x0, 0x0, 0x0, 0x2, 0x0, 0x3 };


static int __wait_access(struct wcfxs *wc, int card)
{
	unsigned char data;
	long origjiffies;
	int count = 0;

	#define MAX 6000 /* attempts */


	origjiffies = jiffies;
    /* Wait for indirect access */
	while (count++ < MAX)
	{
		data = as_fxs_getreg_nolock(wc, card, I_STATUS);

		if (!data)
			return 0;
	 }

	if(count > (MAX-1)) 
		printk(" ##### Loop error (%02x) #####\n", data);

	return 0;
}

int __wcfxs_proslic_setreg_indirect(struct wcfxs *wc, int card, unsigned char address, unsigned short data)
{
	unsigned long flags;
	int res = -1;
	spin_lock_irqsave(&wc->lock, flags);
	if(!__wait_access(wc, card)) 
	{
		as_fxs_setreg_nolock(wc, card, IDA_LO, (unsigned char)(data & 0xFF));
		as_fxs_setreg_nolock(wc, card, IDA_HI, (unsigned char)((data & 0xFF00)>>8));

		as_fxs_setreg_nolock(wc, card, IAA, address);
		res = 0;
	};
	spin_unlock_irqrestore(&wc->lock, flags);
	return res;
}

int __wcfxs_proslic_getreg_indirect(struct wcfxs *wc, int card, unsigned char address)
{ 
	unsigned long flags;
	int res = -1;
	char *p=NULL;
	spin_lock_irqsave(&wc->lock, flags);

	if (!__wait_access(wc, card)) 
	{
		as_fxs_setreg_nolock(wc, card, IAA, address);
		if (!__wait_access(wc, card)) 
		{
			unsigned char data1, data2;

			data1 = as_fxs_getreg_nolock(wc, card, IDA_LO);
			data2 = as_fxs_getreg_nolock(wc, card, IDA_HI);

			res = data1 | (data2 << 8);
		} 
		else
			p = "Failed to wait inside\n";
	} else
		p = "failed to wait\n";
	spin_unlock_irqrestore(&wc->lock, flags);
	if (p)
		printk(p);
	return res;
}

static int __wcfxs_proslic_init_indirect_regs(struct wcfxs *wc, int card)
{
	unsigned char i;

	for (i=0; i<sizeof(indirect_regs) / sizeof(indirect_regs[0]); i++)
	{
		if(__wcfxs_proslic_setreg_indirect(wc, card, indirect_regs[i].address,indirect_regs[i].initial))
			return -1;
	}

	return 0;
}

static int __wcfxs_proslic_verify_indirect_regs(struct wcfxs *wc, int card)
{ 
	int passed = 1;
	unsigned short i, initial;
	int j;

	for (i=0; i<sizeof(indirect_regs) / sizeof(indirect_regs[0]); i++) 
	{
		if((j = __wcfxs_proslic_getreg_indirect(wc, card, (unsigned char) indirect_regs[i].address)) < 0) {
			printk("Failed to read indirect register %d\n", i);
			return -1;
		}
		initial= indirect_regs[i].initial;

		if ( j != initial )
		{
			 printk("!!!!!!! %s  iREG %X = %X  should be %X\n",
				indirect_regs[i].name,indirect_regs[i].address,j,initial );
			 passed = 0;
		}	
	}

	if (passed) 
	{
		if (debug)
			printk("Init Indirect Registers completed successfully.\n");
	} 
	else 
	{
		printk(" !!!!! Init Indirect Registers UNSUCCESSFULLY.\n");
		return -1;
	}
	return 0;
}


/* check some register in every ProSLIC card to determine 
* whether this card is sane 
*/
static int __wcfxs_proslic_insane(struct wcfxs *wc, int card)
{
	int blah,insane_report;
	insane_report=1;

/* content of 0 register : refer SI3210 page 58 
 Product Part Number ID and Revision Number ID
*/		
	blah = as_fxs_getreg(wc, card, 0);
	if (debug) 
		printk("ProSLIC on module %d, product %d, version %d\n", card, (blah & 0x30) >> 4, (blah & 0xf));

#if	0
	if ((blah & 0x30) >> 4) 
	{
		printk("ProSLIC on module %d is not a 3210.\n", card);
		return -1;
	}
#endif
	if (((blah & 0xf) == 0) || ((blah & 0xf) == 0xf)) 
	{
		/* SLIC not loaded */
		printk("No version and product type info read for module '%d'\r\n", card);
		return -1;
	}
	if ((blah & 0xf) < 3) 
	{
		printk("ProSLIC 3210 version %d is too old\n", blah & 0xf);
		return -1;
	}
	
/* default : digital loopback is enabled in register 8, eg. content is 0x02  */
	blah = as_fxs_getreg(wc, card, 8);
	if (blah != 0x2) 
	{
		printk("ProSLIC on module %d insane (1) %x2 should be 2\n", card, blah);
		return -1;
	} 
	else if ( insane_report)
		printk("ProSLIC on module %d Reg 8 Reads %x2 Expected is 0x2\n",card,blah);

/* register 64 : linefeed control , default is 0x00 : LFS and LF is 'OPEN' */
	blah = as_fxs_getreg(wc, card, 64);
	if (blah != 0x0) 
	{
		printk("ProSLIC on module %d insane (2)\n", card);
		return -1;
	} 
	else if ( insane_report)
		printk("ProSLIC on module %d Reg 64 Reads %x0 Expected is 0x0\n",card,blah);

/* 11 : hybird control register : default is 0x33 , both is 0 db for pulse meter and audio */
	blah = as_fxs_getreg(wc, card, 11);
	if (blah != 0x33) 
	{
		printk("ProSLIC on module %d insane (3)\n", card);
		return -1;
	} 
	else if ( insane_report)
		printk("ProSLIC on module %d Reg 11 Reads %x Expected is 0x33\n",card,blah);

/* Just be sure it's setup right.  Indirect Address register */
	as_fxs_setreg(wc, card, 30, 0);

	if (debug) 
		printk("ProSLIC on module %d of card %s seems sane.\n", card, wc->name );
	return 0;
}

static int __wcfxs_proslic_powerleak_test(struct wcfxs *wc, int card)
{
	unsigned long origjiffies;
	unsigned char vbat;

	/* Turn off linefeed */
	as_fxs_setreg(wc, card, 64, 0);

	/* Power down */
	as_fxs_setreg(wc, card, 14, 0x10);

	/* Wait for one second */
	origjiffies = jiffies;

	while((vbat = as_fxs_getreg(wc, card, 82)) > 0x6) 
	{
		if ((jiffies - origjiffies) >= (HZ/2))
			break;;
	}

	if (vbat < 0x06) 
	{
		printk("Excessive leakage detected on module %d: %d volts (%02x) after %d ms\n", card,
		       376 * vbat / 1000, vbat, (int)((jiffies - origjiffies) * 1000 / HZ));
		return -1;
	} 
	else if (debug) 
	{
		printk("Post-leakage voltage: %d volts\n", 376 * vbat / 1000);
	}
	return 0;
}

static int __wcfxs_powerup_proslic(struct wcfxs *wc, int card, int fast)
{
	unsigned char vbat;
	unsigned long origjiffies;

/* Set period of DC-DC converter to 1/64 khz , this is the reset setting */
	as_fxs_setreg(wc, card, 92, 0xff /* was 0xff */);

	/* Wait for VBat to powerup */
	origjiffies = jiffies;

/* Disable powerdown , eg. automatic power control */
	as_fxs_setreg(wc, card, 14, 0);

	/* If fast, don't bother checking anymore */
	if (fast)
		return 0;

/* more than -79.172 v , this read-only register */
	while((vbat = as_fxs_getreg(wc, card, 82)) < 0xc0) 
	{
		/* Wait no more than 500ms */
		if ((jiffies - origjiffies) > HZ/2) 
		{
			break;
		}
	}

	if (vbat < 0xc0) 
	{
		printk("ProSLIC on module %d failed to powerup within %d ms (%d mV only)\n\n -- DID YOU REMEMBER TO PLUG IN THE HD POWER CABLE TO THE TDM400P??\n",
		       card, (int)(((jiffies - origjiffies) * 1000 / HZ)),
			vbat * 375);
		return -1;
	} 
	else if (debug) 
	{
		printk("ProSLIC on module %d powered up to -%d volts (%02x) in %d ms\n",
		       card, vbat * 376 / 1000, vbat, (int)(((jiffies - origjiffies) * 1000 / HZ)));
	}

	/* Engage DC-DC converter */
	as_fxs_setreg(wc, card, 93, 0x19 /* was 0x19 */);
#if 1
	origjiffies = jiffies;
	while(0x80 & as_fxs_getreg(wc, card, 93)) 
	{
		if ((jiffies - origjiffies) > 2 * HZ) 
		{
			printk("Timeout waiting for DC-DC calibration on module %d\n", card);
			return -1;
		}
	}

#if 1
	/* Wait a full two seconds */
	while((jiffies - origjiffies) < 2 * HZ);

	/* Just check to be sure */
	vbat = as_fxs_getreg(wc, card, 82);
	printk("ProSLIC on module %d powered up to -%d volts (%02x) in %d ms\n",
		       card, vbat * 376 / 1000, vbat, (int)(((jiffies - origjiffies) * 1000 / HZ)));
#endif
#endif
	return 0;

}

static int __wcfxs_proslic_manual_calibrate(struct wcfxs *wc, int card)
{
	unsigned long origjiffies;
	unsigned char i;

	as_fxs_setreg(wc, card, 21, 0);//(0)  Disable all interupts in DR21
	as_fxs_setreg(wc, card, 22, 0);//(0)Disable all interupts in DR21
	as_fxs_setreg(wc, card, 23, 0);//(0)Disable all interupts in DR21
	as_fxs_setreg(wc, card, 64, 0);//(0)

	as_fxs_setreg(wc, card, 97, 0x18); //(0x18)Calibrations without the ADC and DAC offset and without common mode calibration.
	as_fxs_setreg(wc, card, 96, 0x47); //(0x47)	Calibrate common mode and differential DAC mode DAC + ILIM

	origjiffies=jiffies;
	while( as_fxs_getreg(wc,card,96)!=0 )
	{
		if((jiffies-origjiffies)>80)
			return -1;
	}
//Initialized DR 98 and 99 to get consistant results.
// 98 and 99 are the results registers and the search should have same intial conditions.

/*******************************The following is the manual gain mismatch calibration****************************/
/*******************************This is also available as a function *******************************************/
	// Delay 10ms
	origjiffies=jiffies; 
	while((jiffies-origjiffies)<1);
	__wcfxs_proslic_setreg_indirect(wc, card, 88,0);
	__wcfxs_proslic_setreg_indirect(wc,card,89,0);
	__wcfxs_proslic_setreg_indirect(wc,card,90,0);
	__wcfxs_proslic_setreg_indirect(wc,card,91,0);
	__wcfxs_proslic_setreg_indirect(wc,card,92,0);
	__wcfxs_proslic_setreg_indirect(wc,card,93,0);

	as_fxs_setreg(wc, card, 98,0x10); // This is necessary if the calibration occurs other than at reset time
	as_fxs_setreg(wc, card, 99,0x10);

	for ( i=0x1f; i>0; i--)
	{
		as_fxs_setreg(wc, card, 98,i);
		origjiffies=jiffies; 
		while((jiffies-origjiffies)<4);
		if((as_fxs_getreg(wc,card,88)) == 0)
			break;
	} // for

	for ( i=0x1f; i>0; i--)
	{
		as_fxs_setreg(wc, card, 99,i);
		origjiffies=jiffies; 
		while((jiffies-origjiffies)<4);
		if((as_fxs_getreg(wc,card,89)) == 0)
			break;
	}//for

/*******************************The preceding is the manual gain mismatch calibration****************************/
/**********************************The following is the longitudinal Balance Cal***********************************/
	as_fxs_setreg(wc,card,64,1);
	while((jiffies-origjiffies)<10); // Sleep 100?

	as_fxs_setreg(wc, card, 64, 0);
	as_fxs_setreg(wc, card, 23, 0x4);  // enable interrupt for the balance Cal
	as_fxs_setreg(wc, card, 97, 0x1); // this is a singular calibration bit for longitudinal calibration
	as_fxs_setreg(wc, card, 96,0x40);

	as_fxs_getreg(wc,card,96); /* Read Reg 96 just cause */

	as_fxs_setreg(wc, card, 21, 0xFF);
	as_fxs_setreg(wc, card, 22, 0xFF);
	as_fxs_setreg(wc, card, 23, 0xFF);

	/**The preceding is the longitudinal Balance Cal***/
	return(0);

}

static int __wcfxs_proslic_calibrate(struct wcfxs *wc, int card)
{
	unsigned long origjiffies;
//	int x;
	/* Perform all calibrations */
	as_fxs_setreg(wc, card, 97, 0x1f);
	
	/* Begin, no speedup */
	as_fxs_setreg(wc, card, 96, 0x5f);

	/* Wait for it to finish */
	origjiffies = jiffies;
	while(as_fxs_getreg(wc, card, 96)) 
	{
		if ((jiffies - origjiffies) > 2 * HZ) 
		{
			printk("Timeout waiting for calibration of module %d\n", card);
			return -1;
		}
	}
	
#if 0	
	if (debug) 
	{
		/* Print calibration parameters */
		printk("After calibrated, Calibration Vector Regs 98 - 107: \n");
		for (x=98;x<108;x++) 
		{
			printk("%d: %02x\n", x, as_fxs_getreg(wc, card, x));
		}
	}
#endif	
	return 0;
}

/* return : 0, sucess; <0, failed */
int as_fxs_init_proslic(struct wcfxs *wc, int card, int fast, int manual, int sane)
{

	unsigned short tmp[5];
	unsigned char r19;
	int x;
	int fxsmode=0;

	/* By default, don't send on hook */
	wc->mod.fxs.idletxhookstate [card] = 1;

/*added by lzj 2004.09.08*/
	as_fxs_enable_dtmf_detect(wc,  card);
/* end of added */

	/* Sanity check the ProSLIC */
/* if it is not sanity, we think that it may be a FXO card */	
	trace
	if (!sane && __wcfxs_proslic_insane(wc, card))
		return -2;
	trace	
	if (sane) 
	{
		/* Make sure we turn off the DC->DC converter to prevent anything from blowing up */
		as_fxs_setreg(wc, card, 14, 0x10);
	}

	trace
	if (__wcfxs_proslic_init_indirect_regs(wc, card)) 
	{
		printk(KERN_INFO "Indirect Registers failed to initialize on module %d.\n", card);
		return -1;
	}

	/* Clear scratch pad area */
	__wcfxs_proslic_setreg_indirect(wc, card, 97,0);

	/* Clear digital loopback */
	as_fxs_setreg(wc, card, 8, 0);

	/* Revision C optimization */
	as_fxs_setreg(wc, card, 108, 0xeb);

	/* Disable automatic VBat switching for safety to prevent
	   Q7 from accidently turning on and burning out. */
	as_fxs_setreg(wc, card, 67, 0x17);

	/* Turn off Q7 */
	as_fxs_setreg(wc, card, 66, 1);

	/* Flush ProSLIC digital filters by setting to clear, while
	   saving old values */

	for (x=0;x<5;x++) 
	{
		tmp[x] = __wcfxs_proslic_getreg_indirect(wc, card, x + 35);
		__wcfxs_proslic_setreg_indirect(wc, card, x + 35, 0x8000);
	}

	/* Power up the DC-DC converter */
	if (__wcfxs_powerup_proslic(wc, card, fast)) 
	{
		printk("Unable to do INITIAL ProSLIC powerup on module %d\n", card);
		return -1;
	}

	trace

	if (!fast) 
	{
/* Check for power leaks */

		if (__wcfxs_proslic_powerleak_test(wc, card)) 
		{
			printk("ProSLIC module %d failed leakage test.  Check for short circuit\n", card);
		}
		/* Power up again */

		if (__wcfxs_powerup_proslic(wc, card, fast)) 
		{
			printk("Unable to do FINAL ProSLIC powerup on module %d\n", card);
			return -1;
		}
#if 1
		/* Perform calibration */
		if(manual) 
		{
			if (__wcfxs_proslic_manual_calibrate(wc, card)) 
			{
				//printk("Proslic failed on Manual Calibration\n");
				if (__wcfxs_proslic_manual_calibrate(wc, card)) 
				{
					printk("Proslic Failed on Second Attempt to Calibrate Manually. (Try -DNO_CALIBRATION in Makefile)\n");
					return -1;
				}
				printk("Proslic Passed Manual Calibration on Second Attempt\n");
			}
		}
		else 
		{
#endif			

	trace
			if(__wcfxs_proslic_calibrate(wc, card))  
			{
				//printk("ProSlic died on Auto Calibration.\n");
				if (__wcfxs_proslic_calibrate(wc, card)) 
				{
					printk("Proslic Failed on Second Attempt to Auto Calibrate\n");
					return -1;
				}
				printk("Proslic Passed Auto Calibration on Second Attempt\n");
			}
#if 1
		}
#endif
		/* Perform DC-DC calibration */
		as_fxs_setreg(wc, card, 93, 0x99);
		r19 = as_fxs_getreg(wc, card, 107);

		if ((r19 < 0x2) || (r19 > 0xd)) 
		{
			printk("DC-DC cal has a surprising direct 107 of 0x%02x!\n", r19);
			as_fxs_setreg(wc, card, 107, 0x8);
		}


		/* Save calibration vectors */
		for (x=0;x<NUM_CAL_REGS;x++)
			wc->mod.fxs.calregs[card].vals[x] = as_fxs_getreg(wc, card, 96 + x);

	} 
	else 
	{
		/* Restore calibration registers */
		for (x=0;x<NUM_CAL_REGS;x++)
			as_fxs_setreg(wc, card, 96 + x, wc->mod.fxs.calregs[card].vals[x]);
	}
	/* Calibration complete, restore original values */
	for (x=0;x<5;x++) 
	{
		__wcfxs_proslic_setreg_indirect(wc, card, x + 35, tmp[x]);
	}

	if (__wcfxs_proslic_verify_indirect_regs(wc, card)) 
	{
		printk(KERN_INFO "Indirect Registers failed verification.\n");
		return -1;
	}


#if 0
    /* Disable Auto Power Alarm Detect and other "features" */
    as_fxs_setreg(wc, card, 67, 0x0e);
    blah = as_fxs_getreg(wc, card, 67);
#endif

#if 0
    if (wcfxs_proslic_setreg_indirect(wc, card, 97, 0x0)) { // Stanley: for the bad recording fix
		 printk(KERN_INFO "ProSlic IndirectReg Died.\n");
		 return -1;
	}
#endif

 	// U-Law 8-bit interface
    as_fxs_setreg(wc, card, 1, 0x28);
/* modified to support A-Law SLIC ,lizhijie 2005.03.02 */
//	as_fxs_setreg(wc, card, 1, 0x20);

	 // Tx Start count low byte  0 :TX PCM timeslot : channel no
    as_fxs_setreg(wc, card, 2, (3-card) * 8);   
    as_fxs_setreg(wc, card, 3, 0);    // Tx Start count high byte 0 
	 // Rx Start count low byte  0 : RX PCM timeslot
    as_fxs_setreg(wc, card, 4, (3-card) * 8);    
    as_fxs_setreg(wc, card, 5, 0);    // Rx Start count high byte 0
    as_fxs_setreg(wc, card, 18, 0xff);     // clear all interrupt
    as_fxs_setreg(wc, card, 19, 0xff);
    as_fxs_setreg(wc, card, 20, 0xff);
    as_fxs_setreg(wc, card, 73, 0x04);

//#if AS_WITH_FXO_MODULE
	if (fxshonormode) 
	{
		fxsmode = acim2tiss[fxo_modes[_opermode].acim];
		as_fxs_setreg(wc, card, 10, 0x08 |fxsmode);
	}
//#endif

    if (lowpower)
    	as_fxs_setreg(wc, card, 72, 0x10);

#if 0
    as_fxs_setreg(wc, card, 21, 0x00); 	// enable interrupt
    as_fxs_setreg(wc, card, 22, 0x02); 	// Loop detection interrupt
    as_fxs_setreg(wc, card, 23, 0x01); 	// DTMF detection interrupt
#endif

#if 0
    /* Enable loopback */
    as_fxs_setreg(wc, card, 8, 0x2);
    as_fxs_setreg(wc, card, 14, 0x0);
    as_fxs_setreg(wc, card, 64, 0x0);
    as_fxs_setreg(wc, card, 1, 0x08);
#endif

	/* Beef up Ringing voltage to 89V */
	if (boostringer) 
	{

		if (__wcfxs_proslic_setreg_indirect(wc, card, 21, 0x1d1)) 
			return -1;
		printk("Boosting ringinger on slot %d (89V peak)\n", card + 1);
	} 
	else if (lowpower) 
	{

		if (__wcfxs_proslic_setreg_indirect(wc, card, 21, 0x108)) 
			return -1;
		printk("Reducing ring power on slot %d (50V peak)\n", card + 1);
	}

/* Audio Hybrid Adjust use the default set, then echo is minimized */
//	as_fxs_setreg(wc, card, 11, 0x0);

	return 0;
}


