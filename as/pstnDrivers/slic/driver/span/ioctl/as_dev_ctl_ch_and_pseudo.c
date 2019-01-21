/*
 * $Author: lizhijie $
 * $Log: as_dev_ctl_ch_and_pseudo.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.5  2004/12/11 06:27:29  lizhijie
 * some comile warning
 *
 * Revision 1.4  2004/12/11 05:05:30  eagle
 * add test_gain.c
 *
 * Revision 1.3  2004/11/29 08:25:03  eagle
 * 2229 by chenchaoxin
 *
 * Revision 1.2  2004/11/29 01:58:14  lizhijie
 * some little bug about channel gain
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"

static void  __as_channel_ctl_flush(struct as_dev_chan *chan,  int mode)
{
	int j;
	unsigned long flags;
	
	spin_lock_irqsave(&chan->lock, flags);
	if (mode & AS_FLUSH_READ)  /* if for read (input) */
	{
	  /* initialize read buffers and pointers */
		chan->inreadbuf = 0;
		chan->outreadbuf = -1;
		for (j=0;j<chan->numbufs;j++) 
		{
				/* Do we need this? */
			chan->readn[j] = 0;
			chan->readidx[j] = 0;
		}
		wake_up_interruptible(&chan->readbufq);  /* wake_up_interruptible waiting on read */
		wake_up_interruptible(&chan->sel); /* wake_up_interruptible waiting on select */
   	}
			
	if (mode & AS_FLUSH_WRITE) /* if for write (output) */
	{
		chan->outwritebuf = -1;
		chan->inwritebuf = 0;
		for (j=0;j<chan->numbufs;j++) 
		{
				/* Do we need this? */
			chan->writen[j] = 0;
			chan->writeidx[j] = 0;
		}
		wake_up_interruptible(&chan->writebufq); /* wake_up_interruptible waiting on write */
		wake_up_interruptible(&chan->sel);  /* wake_up_interruptible waiting on select */
			   /* if IO MUX wait on write empty, well, this
				certainly *did* empty the write */
//		if (chan->iomask & ZT_IOMUX_WRITEEMPTY)
//			wake_up_interruptible(&chan->eventbufq); /* wake_up_interruptible waiting on IOMUX */
	}
	
	if (mode & AS_FLUSH_EVENT) /* if for events */
	{
		chan->eventinidx = chan->eventoutidx = 0;
	}
	spin_unlock_irqrestore(&chan->lock, flags);

}


/*ZT_DIAL ioctl, used to control DTMF digital into kernel 
* lzj 
*/
static int __as_channel_ctl_set_dialing_string(struct as_dev_chan *chan ,unsigned long data)
{
	struct as_dialoperation tdo;
	unsigned long flags;
	int rv;
	
	if (copy_from_user(&tdo, (struct as_dialoperation *)data, sizeof(tdo)))
			return -EIO;
		
	rv = 0;
	/* Force proper NULL termination */
	tdo.dialstr[AS_MAX_DTMF_BUF - 1] = '\0';
	spin_lock_irqsave(&chan->lock, flags);

	switch(tdo.op) 
	{
/*case 1 */	
		case AS_DIAL_OP_CANCEL:
			chan->curtone = NULL;
			chan->dialing = 0;
			chan->txdialbuf[0] = '\0';
			chan->tonep = 0;
			break;
/*case 2 */	
		case AS_DIAL_OP_REPLACE:
			strcpy(chan->txdialbuf, tdo.dialstr);
			chan->dialing = 1;
			printk("DTMF str is '%s'\r\n", chan->txdialbuf);
			if(! chan->dsp )
			{
				rv = -ENODATA;
				break;
			}
/*			
			rv = (chan->dsp->play_dtmf)(chan->dsp, chan);
*/			
			rv = (chan->dsp->play_caller_id)(chan->dsp, chan);
			break;
/*case 3 */	
		case AS_DIAL_OP_APPEND:
			if (strlen(tdo.dialstr) + strlen(chan->txdialbuf) >= AS_MAX_DTMF_BUF)
			{
				rv = -EBUSY;
				break;
			}
			strncpy(chan->txdialbuf + strlen(chan->txdialbuf), tdo.dialstr, AS_MAX_DTMF_BUF - strlen(chan->txdialbuf));
			if (!chan->dialing)
			{
				chan->dialing = 1;
				if(! chan->dsp )
				{
					rv = -ENODATA;
					break;
				}	
#if 0				
				rv = (chan->dsp->play_dtmf)(chan->dsp, chan);
#endif
				rv = (chan->dsp->play_caller_id)(chan->dsp, chan);

			}
			break;
		default:
			rv = -EINVAL;
	}
	spin_unlock_irqrestore(&chan->lock, flags);
	return rv;
}

static int __as_channel_ctl_get_gain(struct as_dev_chan *chan, unsigned long data)
{
	int  j;
	struct as_gains gain;
	
	if ( !chan ) 
			return(-EINVAL);
	
	if (copy_from_user(&gain,(struct as_gains *) data,sizeof(gain)))
		return -EIO;
		
	if (!(chan->flags & AS_CHAN_FLAG_AUDIO)) 
		return (-EINVAL);
		
	gain.chan = chan->channo ; /* put the span # in here */
	for (j=0;j<256;j++)  
	{
		gain.txgain[j] = chan->txgain[j];
		gain.rxgain[j] = chan->rxgain[j];
	}
		
	if (copy_to_user((struct as_gains *) data,&gain,sizeof(gain)))
		return -EIO;
	return 0;
}

static int __as_channel_ctl_set_gain(struct as_dev_chan *chan, unsigned long data)
{
	int  j;
	struct as_gains gain;

	
	if ( !chan ) 
		return(-EINVAL);
	
	if (copy_from_user(&gain,(struct as_gains *) data,sizeof(gain)))
		return -EIO;
		
	if (!(chan->flags & AS_CHAN_FLAG_AUDIO)) 
		return (-EINVAL);
		
	if (!chan->gainalloc) 
	{
		chan->rxgain = kmalloc(512, GFP_KERNEL);
			
		if (!chan->rxgain) 
		{
//			chan->rxgain = defgain;
			as_channel_set_default_gain( chan);
			return -ENOMEM;
		} 
		else 
		{
			chan->gainalloc = 1;
			chan->txgain = chan->rxgain + 256;
		}
	}
		
	gain.chan = chan->channo ; /* put the span # in here */
	for (j=0;j<256;j++) 
	{
		chan->rxgain[j] = gain.rxgain[j];
		chan->txgain[j] = gain.txgain[j];
	}

	as_channel_compare_gain( chan);
	if (copy_to_user((struct as_gains *) data,&gain,sizeof(gain)))
		return -EIO;
	return 0;
}

static void __as_channel_ctl_get_param(struct as_dev_chan *chan, unsigned long data)
{
	struct as_params param;
	
	if(!chan )
		return ;
	
	copy_from_user(&param,(struct as_params *)data,sizeof(param));
	
	if(param.channo != chan->channo );

	param.sigtype = chan->sig;  /* get signalling type */
		/* return non-zero if rx not in idle state */
#if 0		
	if (chan->span) 
	{
			j = zt_q_sig(chan); 
			if (j >= 0) 
			{ /* if returned with success */
				param.rxisoffhook = ((chan->rxsig & (j >> 8)) != (j & 0xff));
			}
			else 
			{
				param.rxisoffhook = ((chan->rxhooksig != ZT_RXSIG_ONHOOK) &&
					(chan->rxhooksig != ZT_RXSIG_INITIAL));
			}
	}
	else 
		param.rxisoffhook = 0;
#endif

	if (chan->span && chan->span->rbsbits ) 
	{
		param.rxbits = chan->rxsig;
		param.txbits = chan->txsig;
//		param.idlebits = chan->idlebits;
	} 
	else 
	{
		param.rxbits = -1;
		param.txbits = -1;
//		param.idlebits = 0;
	}
		
	if (chan->span && (chan->span->rbsbits || chan->span->hooksig) ) 
	{
		param.rxhooksig = chan->rxhooksig;
		param.txhooksig = chan->txhooksig;
	} 
	else 
	{
		param.rxhooksig = -1;
		param.txhooksig = -1;
	}
		
	param.prewinktime = chan->prewinktime; 
	param.preflashtime = chan->preflashtime;		
	param.winktime = chan->winktime;
	param.flashtime = chan->flashtime;
	param.starttime = chan->starttime;
	param.rxwinktime = chan->rxwinktime;
	param.rxflashtime = chan->rxflashtime;
	param.debouncetime = chan->debouncetime;
	param.channo = chan->channo;
	
	if (chan->span) 
		param.spanno = chan->span->spanno;
	else 
		param.spanno = 0;
	
	strncpy(param.name, chan->name, sizeof(param.name) - 1);
	param.chanpos = chan->chanpos;
	
	param.curlaw =  as_channel_check_law(chan);
	
	copy_to_user((struct as_params *)data,&param,sizeof(param));
}

static void __as_channel_ctl_set_param(struct as_dev_chan *chan, unsigned long data)
{
	struct as_params param;

	if(!chan)
		return;
	
	copy_from_user(&param,(struct as_params *)data,sizeof(param));
		
	if(param.channo != chan->channo )
		return;
		/* NOTE: sigtype is *not* included in this */
		  /* get timing paramters */
	chan->prewinktime = param.prewinktime;
	chan->preflashtime = param.preflashtime;
	chan->winktime = param.winktime;
	chan->flashtime = param.flashtime;
	chan->starttime = param.starttime;
		/* Update ringtime if not using a tone zone */
//if (!chan->curzone)
//		chan->ringcadence[0] = chan->starttime;
	chan->rxwinktime = param.rxwinktime;
	chan->rxflashtime = param.rxflashtime;
	chan->debouncetime = param.debouncetime;
}

static int __as_channel_ctl_common(struct inode *node, struct file *file, unsigned int cmd, unsigned long data, int unit)
{
	struct as_dev_chan *chan = as_span_get_channel(unit);
	int j;
	unsigned long flags;

	switch(cmd) 
	{
		case AS_CTL_GET_PARAMS: /* get channel timing parameters */
			__as_channel_ctl_get_param( chan,  data);
			break;
	/* case 2: SET PARAM */	
		case AS_CTL_SET_PARAMS: /* set channel timing paramters */
			__as_channel_ctl_set_param( chan,  data);
			break;
/* case 3 : */		
		case AS_CTL_GETGAINS:  /* get gain stuff */
			return __as_channel_ctl_get_gain(chan,  data);
			break;
		
		case AS_CTL_SETGAINS:  /* set gain stuff */
			return __as_channel_ctl_set_gain( chan, data);
			break;
		
		case AS_CTL_CHAN_DIAG:/* channel diagnostic info on console  */
		{
			struct as_dev_chan mychan;
			char *msg;
			
			get_user(j, (int *)data); /* get channel number from user */

			chan = as_span_get_channel(j);
			if (!chan) 
				return -EINVAL;

			spin_lock_irqsave( &chan->lock, flags);
			memcpy(&mychan, chan ,sizeof(struct as_dev_chan));
			spin_unlock_irqrestore(&chan->lock, flags);

			msg = as_channel_debug_info(&mychan);
			printk("%s", msg);
			kfree(msg);
			break;
		}
		default:
			return (chan->ioctl)(chan, cmd, data );
//			return -ENOTTY;
	}

	return 0;
}

/* minor number :255 , /dev/zap/pseudo  */
int as_chanandpseudo_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data, int unit)
{
	struct as_dev_chan *chan =as_span_get_channel(unit);
	unsigned long flags ;
	int i, j,  rv;

	if (!chan)
		return -EINVAL;

	switch(cmd) 
	{
/*case 1 : get channel status: 1, dialing now, eg. send out called ID, 0 no dialing  */	
		case AS_CTL_DIALING:
		{
			spin_lock_irqsave(&chan->lock, flags);
			j = chan->dialing;
			spin_unlock_irqrestore(&chan->lock, flags);
			if (copy_to_user((int *)data,&j,sizeof(int)))
				return -EIO;
			return 0;
		}
		
/*case 2 : manipulate a dialing string */	
		case AS_CTL_SET_DTMF_STR:
			return __as_channel_ctl_set_dialing_string(chan,  data);
		
/*case 3 : manupulate the info of buffer usage */	
		case AS_CTL_GET_BUFINFO:
		{
			struct as_bufferinfo bi;
			
			bi.rxbufpolicy = chan->rxbufpolicy;
			bi.txbufpolicy = chan->txbufpolicy;
			bi.numbufs = chan->numbufs;
			bi.bufsize = chan->blocksize;
		/* XXX FIXME! XXX */
			bi.readbufs = -1;
			bi.writebufs = -1;
			if (copy_to_user((struct as_bufferinfo *)data, &bi, sizeof(bi)))
				return -EIO;
			break;
		}
		
/*case 4 */	
		case AS_CTL_SET_BUFINFO:
		{
			struct as_bufferinfo bi;
			
			if (copy_from_user(&bi, (struct as_bufferinfo *)data, sizeof(bi)))
				return -EIO;
			if (bi.bufsize > AS_MAX_BLOCKSIZE)
				return -EINVAL;
			if (bi.bufsize < 16)
				return -EINVAL;
			if (bi.bufsize * bi.numbufs > AS_MAX_BUF_SPACE)
				return -EINVAL;
			chan->rxbufpolicy = bi.rxbufpolicy & 0x1;
			chan->txbufpolicy = bi.txbufpolicy & 0x1;
			if ((rv = as_channel_reallocbufs(chan,  bi.bufsize, bi.numbufs)))
				return (rv);
			break;
		}
		
/*case 5 */	
		case AS_CTL_GET_BLOCKSIZE:  /* get blocksize */
			put_user(chan->blocksize,(int *)data); /* return block size */
			break;
		
/*case 6 */	
		case AS_CTL_SET_BLOCKSIZE:  /* set blocksize */
			get_user(j,(int *)data);
		  /* cannot be larger than max amount */
			if (j > AS_MAX_BLOCKSIZE) 
				return(-EINVAL);
		  /* cannot be less then 16 */
			if (j < 16) 
				return(-EINVAL);
			if ((rv = as_channel_reallocbufs(chan, j, chan->numbufs)))
				return (rv);
			break;
		
/*case 7 */	
		case AS_CTL_FLUSH:  /* flush input buffer, output buffer, and/or event queue */
		{
			get_user(i,(int *)data);  /* get param */
			__as_channel_ctl_flush( chan, i);
			break;

		}
		
/*case 8 */	
		case AS_CTL_SYNC:  /* wait for no tx */
		{
			for(;;)  /* loop forever */
		   	{
				spin_lock_irqsave(&chan->lock, flags);
			  /* Know if there is a write pending */
				i = (chan->outwritebuf > -1);
				spin_unlock_irqrestore(&chan->lock, flags);
				if (!i) 
					break; /* skip if none */
				
				rv = as_schedule_delay(&chan->writebufq);
				if (rv) 
					return(rv);
			}
			break;
		}
#if 0		
/*case 9 */	
	case ZT_IOMUX: /* wait for something to happen */
		get_user(chan->iomask,(int*)data);  /* save mask */
		if (!chan->iomask) 
			return(-EINVAL);  /* cant wait for nothing */
		
		for(;;)  /* loop forever */
		   {
			  /* has to have SOME mask */
			ret = 0;  /* start with empty return value */
			spin_lock_irqsave(&chan->lock, flags);
			  /* if looking for read */
			if (chan->iomask & ZT_IOMUX_READ)
			   {
				/* if read available */
				if ((chan->outreadbuf > -1)  && !chan->rxdisable)
					ret |= ZT_IOMUX_READ;
			   }
			  /* if looking for write avail */
			if (chan->iomask & ZT_IOMUX_WRITE)
			   {
				if (chan->inwritebuf > -1)
					ret |= ZT_IOMUX_WRITE;
			   }
			  /* if looking for write empty */
			if (chan->iomask & ZT_IOMUX_WRITEEMPTY)
			   {
				  /* if everything empty -- be sure the transmitter is enabled */
				chan->txdisable = 0;
				if (chan->outwritebuf < 0)
					ret |= ZT_IOMUX_WRITEEMPTY;
			   }
			  /* if looking for signalling event */
			if (chan->iomask & ZT_IOMUX_SIGEVENT)
			   {
				  /* if event */
				if (chan->eventinidx != chan->eventoutidx)
					ret |= ZT_IOMUX_SIGEVENT;
			   }
			spin_unlock_irqrestore(&chan->lock, flags);
			  /* if something to return, or not to wait */
			if (ret || (chan->iomask & ZT_IOMUX_NOWAIT))
			   {
				  /* set return value */
				put_user(ret,(int *)data);
				break; /* get out of loop */
			   }
			rv = schluffen(&chan->eventbufq);
			if (rv) return(rv);
		   }
		  /* clear IO MUX mask */
		chan->iomask = 0;
		break;
#endif

/*case 10 */	
		case AS_CTL_GETEVENT:  /* Get event on queue */
		  	j = as_channel_deqevent_lock(chan);
			put_user(j,(int *)data);
			break;
		
/*case 15 */	
		case AS_CTL_CHANNO:  /* get channel number of stream */
			put_user(unit,(int *)data); /* return unit/channel number */
			break;

/*case 15 */	
		case AS_CTL_SETLAW:
			get_user(j, (int *)data);
			if ((j < 0) || (j > AS_LAW_ALAW))
				return -EINVAL;
			as_channel_set_law(chan, j);
			break;
			
/*case 15 */	
		case AS_CTL_SETLINEAR:
			get_user(j, (int *)data);
		/* Makes no sense on non-audio channels */
			if (!(chan->flags & AS_CHAN_FLAG_AUDIO))
				return -EINVAL;

			if (j)
				chan->flags |= AS_CHAN_FLAG_LINEAR;
			else
				chan->flags &= ~AS_CHAN_FLAG_LINEAR;
			break;

#if 0
/*************************************************
	Tone Zone operations
*************************************************/
/*case 13 */	
		case ZT_SETTONEZONE:
			get_user(j,(int *)data);
			spin_lock_irqsave(&chan->lock, flags);
			rv =  set_tone_zone(chan, j);
			spin_unlock_irqrestore(&chan->lock, flags);
			return rv;
		
/*case 14 */	
		case ZT_GETTONEZONE:
			spin_lock_irqsave(&chan->lock, flags);
			if (chan->curzone)
				rv = chan->tonezone;
			else
				rv = default_zone;
			spin_unlock_irqrestore(&chan->lock, flags);
			put_user(rv,(int *)data); /* return value */
			break;
#endif

/*case 15 */	
		case AS_CTL_SENDTONE:
			get_user(j,(int *)data);
			spin_lock_irqsave(&chan->lock, flags);
			if(!chan->dsp)
			{
				spin_unlock_irqrestore(&chan->lock, flags);
				return -ENODATA;
			}
			rv = (chan->dsp->play_tone)(chan->dsp, chan, j);	
			printk("dsp play tone\n");
			spin_unlock_irqrestore(&chan->lock, flags);
			return rv;


/*case 15 */	
		case AS_CTL_SETCADENCE:
		{
				struct as_ring_cadence cad;
				if (data) 
				{
			/* Use specific ring cadence */
					if (copy_from_user(&cad, (struct as_ring_cadence *)data, sizeof(cad)))
						return -EIO;
					spin_lock_irqsave(&chan->lock, flags);
					memcpy(chan->ringcadence, &cad, sizeof(chan->ringcadence));
					spin_unlock_irqrestore(&chan->lock, flags);
				} 
				else 
				{
			/* Reset to default */
				#if 0
					if (chan->curzone) 
					{
						memcpy(chan->ringcadence, chan->curzone->ringcadence, sizeof(chan->ringcadence));
					} 
					else 
					{
					#endif
					spin_lock_irqsave(&chan->lock, flags);
						memset(chan->ringcadence, 0, sizeof(chan->ringcadence));
						chan->ringcadence[0] = chan->starttime;
						chan->ringcadence[1] = AS_RINGOFFTIME;
					//}
					spin_unlock_irqrestore(&chan->lock, flags);
				}	
			break;
		}


		default:
		{
		/* Check for common ioctl's and private ones */
			rv = __as_channel_ctl_common(inode, file, cmd, data, unit);
		/* if no span, just return with value */
			if (!chan->span) 
				return rv;
			
			if ((rv == -ENOTTY) && chan->span->ioctl) 
				rv = chan->span->ioctl(chan, cmd, data);
		
			return rv;
		}	
		
	}

	return 0;
}

