/*
 * Revision 1.2.1.0  2005/05/27 17:31  wangwei
 * rework Linefeed Control setting[register 64]
 *
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_fxs_handler.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.3  2005/05/27 09:39:14  wangwei
 * add Linefeed Control setting[register 64]
 *
 * Revision 1.2  2005/04/20 03:15:41  lizhijie
 * no message
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.5  2004/12/31 09:38:11  fengshikui
 * open #if   then cancel one fucntion
 *
 * Revision 1.4  2004/12/17 07:36:51  lizhijie
 * add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
 *
 * Revision 1.3  2004/12/11 05:35:51  lizhijie
 * add Tiger320 debug info
 *
 * Revision 1.2  2004/11/25 07:59:24  lizhijie
 * recommit all
 *
*/
#include "as_fxs_common.h"

#include "as_dev_ctl.h"

#include "as_ring_param.h"

extern int debug ;

#if 0
static void __as_fxs_set_ring_parameters(struct wcfxs *wc, struct as_dev_chan *chan, 	struct as_si_reg_ring_para *param)
{
	char flags;
	
	flags=as_fxs_getreg(wc, chan->chanpos - 1, 34);

	if( param->dc_offset==0)
	{
		flags&=0xfd;
	}
	else 
	{
		flags|=0x02;
	}

	if( param->waveform==RING_WAVEFORM_SINE )
	{
				as_fxs_setreg(wc, chan->chanpos - 1,34,flags&0xfe);
	}
	else
	{
		as_fxs_setreg(wc, chan->chanpos - 1,34,flags);
	}

	__wcfxs_proslic_setreg_indirect(wc, chan->chanpos-1, 19,  param->dc_offset);
	__wcfxs_proslic_setreg_indirect(wc, chan->chanpos-1, 20,  param->ring_freq);
	__wcfxs_proslic_setreg_indirect(wc, chan->chanpos-1, 21,  param->ring_amp);
	__wcfxs_proslic_setreg_indirect(wc, chan->chanpos-1, 22,  param->ring_init_phase);
	__wcfxs_proslic_setreg_indirect(wc, chan->chanpos-1, 40,  param->bias_adjust);
}
#endif

int as_fxs_ioctl(struct as_dev_chan *chan, unsigned int cmd, unsigned long data)
{
	struct wcfxs_stats stats;
	struct wcfxs_regs regs;
	struct wcfxs_regop regop;
	struct wcfxs *wc = chan->private;
	struct as_si_reg_ring_para ringpara; /*added by assist */
	
	int x;
	unsigned char flags;
	
	switch (cmd) 
	{
#if 0	
	case ZT_ONHOOKTRANSFER:
		if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_FXS)
			return -EINVAL;
		if (get_user(x, (int *)data))
			return -EFAULT;
		wc->mod.fxs.ohttimer[chan->chanpos - 1] = x << 3;
		wc->mod.fxs.idletxhookstate[chan->chanpos - 1] = 0x2;	/* OHT mode when idle */
		if (wc->mod.fxs.lasttxhook[chan->chanpos - 1] == 0x1) 
		{
				/* Apply the change if appropriate */
				wc->mod.fxs.lasttxhook[chan->chanpos - 1] = 0x2;
				wcfxs_setreg(wc, chan->chanpos - 1, 64, wc->mod.fxs.lasttxhook[chan->chanpos - 1]);
		}
		break;
#endif		
	case WCFXS_GET_STATS:
		if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXS) 
		{
			stats.tipvolt = as_fxs_getreg(wc, chan->chanpos - 1, 80) * -376;
			stats.ringvolt = as_fxs_getreg(wc, chan->chanpos - 1, 81) * -376;
			stats.batvolt = as_fxs_getreg(wc, chan->chanpos - 1, 82) * -376;
		} 
		else if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXO) 
		{
			stats.tipvolt = (signed char)as_fxs_getreg(wc, chan->chanpos - 1, 29) * 1000;
			stats.ringvolt = (signed char)as_fxs_getreg(wc, chan->chanpos - 1, 29) * 1000;
			stats.batvolt = (signed char)as_fxs_getreg(wc, chan->chanpos - 1, 29) * 1000;
		} 
		else 
			return -EINVAL;

		if (copy_to_user((struct wcfxs_stats *)data, &stats, sizeof(stats)))
			return -EFAULT;
		break;
		
	case WCFXS_GET_REGS:
		if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXS) 
		{
			for (x=0;x<NUM_INDIRECT_REGS;x++)
				regs.indirect[x] = __wcfxs_proslic_getreg_indirect(wc, chan->chanpos -1, x);
			for (x=0;x<NUM_REGS;x++)
				regs.direct[x] = as_fxs_getreg(wc, chan->chanpos - 1, x);
		} 
		else 
		{
			memset(&regs, 0, sizeof(regs));
			for (x=0;x<NUM_FXO_REGS;x++)
				regs.direct[x] = as_fxs_getreg(wc, chan->chanpos - 1, x);
		}
		if (copy_to_user((struct wcfxs_regs *)data, &regs, sizeof(regs)))
			return -EFAULT;
		break;
		
	case WCFXS_SET_REG:
		if (copy_from_user(&regop, (struct wcfxs_regop *)data, sizeof(regop)))
			return -EFAULT;
		if (regop.indirect) 
		{
			if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_FXS)
				return -EINVAL;
			printk("Setting indirect %d to 0x%04x on %d\n", regop.reg, regop.val, chan->chanpos);
			__wcfxs_proslic_setreg_indirect(wc, chan->chanpos - 1, regop.reg, regop.val);
		} 
		else 
		{
			regop.val &= 0xff;
			printk("Setting direct %d to %04x on %d\n", regop.reg, regop.val, chan->chanpos);
			as_fxs_setreg(wc, chan->chanpos - 1, regop.reg, regop.val);
		}
		break;

	case  WCFXS_SET_RING_PARA:
		if (copy_from_user(&ringpara, (struct as_si_reg_ring_para *)data, sizeof(struct as_si_reg_ring_para)))
			return -EFAULT;
		
		if (wc->modtype[chan->chanpos - 1] != MOD_TYPE_FXS)
		{
			return -EINVAL;			
		}
		else 		
		{	
			flags=as_fxs_getreg(wc, chan->chanpos - 1,34);
			if(ringpara.dc_offset==0)
			{
				flags&=0xfd;
			}
			else 
			{
				flags|=0x02;
			}
			if(ringpara.waveform==1)//RING_WAVEFORM_SINE
			{
				as_fxs_setreg(wc, chan->chanpos - 1,34,flags&0xfe);
			}
			else
			{
				as_fxs_setreg(wc, chan->chanpos - 1,34,flags);
			}
			__wcfxs_proslic_setreg_indirect(wc, chan->chanpos - 1,19,ringpara.dc_offset);
			__wcfxs_proslic_setreg_indirect(wc, chan->chanpos - 1,20,ringpara.ring_freq);
			__wcfxs_proslic_setreg_indirect(wc, chan->chanpos - 1,21,ringpara.ring_amp);
			__wcfxs_proslic_setreg_indirect(wc, chan->chanpos - 1,22,ringpara.ring_init_phase);
			__wcfxs_proslic_setreg_indirect(wc, chan->chanpos - 1,40,ringpara.bias_adjust);
		}
		break;
		
		
	default:
		return -ENOTTY;
	}
	return 0;

}

int as_fxs_open(struct as_dev_chan *chan)
{
	struct wcfxs *wc = chan->private;
	if (!(wc->cardflag & (1 << (chan->chanpos - 1))))
		return -ENODEV;
	if (wc->dead)
		return -ENODEV;
	wc->usecount++;

	MOD_INC_USE_COUNT;

	return 0;
}

int as_fxs_watchdog(struct as_dev_chan *chan, int event)
{
	printk("TDM: Restarting DMA\n");
	as_fxs_restart_dma(chan->private);
	return 0;
}

int as_fxs_close(struct as_dev_chan *chan)
{
	struct wcfxs *wc = chan->private;
	int x;
	wc->usecount--;

	MOD_DEC_USE_COUNT;

	for (x=0;x<wc->cards;x++)
		wc->mod.fxs.idletxhookstate[x] = 1;
	/* If we're dead, release us now */
	if (!wc->usecount && wc->dead) 
		as_fxs_release(wc);
	return 0;
}

/* this is tele signal andler registered in upper driver 
then the handler driver provided by zaptel is not used 
*/
int as_fxs_hooksig(struct as_dev_chan *chan, as_txsig_t txsig)
{
	struct wcfxs *wc = chan->private;
	int reg=0;
	if (wc->modtype[chan->chanpos - 1] == MOD_TYPE_FXO) 
	{
		/* XXX Enable hooksig for FXO XXX */
		switch(txsig) 
		{
			case AS_TXSIG_START:
			case AS_TXSIG_OFFHOOK:
				wc->mod.fxo.offhook[chan->chanpos - 1] = 1;
	/* Si3050 page 63 : DAA control 1 register : 
	* make the device monitor the hook signal  
	lzj */		
				as_fxs_setreg_nolock(wc, chan->chanpos - 1, 5, 0x9);
				break;
			case AS_TXSIG_ONHOOK:
				wc->mod.fxo.offhook[chan->chanpos - 1] = 0;
				as_fxs_setreg_nolock(wc, chan->chanpos - 1, 5, 0x8);
				break;
			default:
				printk("wcfxo: Can't set tx state to %d\n", txsig);
		}
	} 
	else 
	{
		switch(txsig) 
		{
			case AS_TXSIG_ONHOOK:
				switch(chan->sig) 
				{
#if 1  /* add below codes by fengshikui,previous version is "#if 0"*/
		/*canceled by wangwei in 2005/05/27  
		solve buf of ring*/
					case AS_SIG_FXOKS:
					case AS_SIG_FXOLS:
						wc->mod.fxs.lasttxhook[chan->chanpos-1] = wc->mod.fxs.idletxhookstate[chan->chanpos-1];
						break;
					case AS_SIG_FXOGS:
						wc->mod.fxs.lasttxhook[chan->chanpos-1] = AS_LINEFEED_FORWARD_ONHOOK;
						break;
#endif						
					default:
					/*canceled by fengshikui*/
					/* uncomment this line for ring stop ,lizhijie, 2005.02.26 */
						wc->mod.fxs.lasttxhook[chan->chanpos-1] = AS_LINEFEED_TIP_OPEN;
						break;
				}
				break;
			case AS_TXSIG_OFFHOOK:
				switch(chan->sig) 
				{
	//		case AS_SIG_EM:
	//			wc->mod.fxs.lasttxhook[chan->chanpos-1] = 5;
	//			break;
					default:
						wc->mod.fxs.lasttxhook[chan->chanpos-1] = wc->mod.fxs.idletxhookstate[chan->chanpos-1];
						break;
				}
				break;
			case AS_TXSIG_START:
				wc->mod.fxs.lasttxhook[chan->chanpos-1] = AS_LINEFEED_RINGING;
				break;
			case AS_TXSIG_KEWL:
				wc->mod.fxs.lasttxhook[chan->chanpos-1] = AS_LINEFEED_OPEN;
				break;
			default:
				printk("wcfxs: Can't set tx state to %d\n", txsig);
		}
		if (debug)
			printk("Setting FXS hook state to %d (%02x)\n", txsig, reg);

/* Si 3021 page 87: linefeed contorl register */
		as_fxs_setreg_nolock(wc, chan->chanpos - 1, 64, wc->mod.fxs.lasttxhook[chan->chanpos-1]);
	}
	return 0;
}

