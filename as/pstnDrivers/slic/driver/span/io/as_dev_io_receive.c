/*
 * $Author: lizhijie $
 * $Log: as_dev_io_receive.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.4  2004/11/26 12:34:01  lizhijie
 * add multi-card support
 *
 * Revision 1.3  2004/11/25 07:49:24  lizhijie
 * remove some comments
 *
 * Revision 1.2  2004/11/22 01:54:04  lizhijie
 * add some user module into CVS
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"
#include "as_fxs_common.h"

static inline void __as_io_channel_in_rbs_itimer_expire(struct as_dev_chan *chan)
{
	/* the only way this could have gotten here, is if a channel
	    went off hook longer then the wink or flash detect timeout */
	/* Called with chan->lock held */
	switch(chan->sig)
	{
		case AS_SIG_FXOLS:  /* if FXO, its definitely on hook */
		case AS_SIG_FXOGS:
		case AS_SIG_FXOKS:
			as_channel_qevent_nolock(chan, AS_EVENT_ONHOOK);
			chan->gotgs = 0; 
			break;
		default:  /* otherwise, its definitely off hook */
			as_channel_qevent_nolock(chan, AS_EVENT_RINGOFFHOOK); 
			break;
	}
}

static void __as_io_channel_in_hooksig_pvt(struct as_dev_chan *chan, as_rxsig_t rxsig)
{
	/* State machines for receive hookstate transitions 
		called with chan->lock held */
	if ((chan->rxhooksig) == rxsig) 
		return;
	
	chan->rxhooksig = rxsig;
	
	switch(chan->sig) 
	{
/****************************************************************
 * below , process for FXO devices                                                              *
 ****************************************************************/	
/* Case 2 : When FXS signal is used in this channel: this is a FXO device  */
		case AS_SIG_FXSKS:  /* FXS Kewlstart */
		  /* ignore a bit poopy if loop not closed and stable */
			if (chan->txstate != AS_TXSTATE_OFFHOOK) 
				break;
		/* fall through intentionally */
/* Case 2 : When FXS signal is used in this channel: this is a FXO device  */
		case AS_SIG_FXSGS:  /* FXS Groundstart */
			if (rxsig == AS_RXSIG_ONHOOK) 
			{
				chan->ringdebtimer = AS_RING_DEBOUNCE_TIME;
				chan->ringtrailer = 0;
				if (chan->txstate != AS_TXSTATE_DEBOUNCE) 
				{
					chan->gotgs = 0;
					as_channel_qevent_nolock(chan, AS_EVENT_ONHOOK);
				}
			}
			break;
			
/************************************************************* 
 * Below are process for FXS devices 								  *
 *************************************************************/
/* Case 3 : When FXO signal is used in this channel: this is a FXS device  */
	   	case AS_SIG_FXOGS: /* FXO Groundstart */
			if (rxsig == AS_RXSIG_START) 
			{
			  /* if havent got gs, report it */
				if (!chan->gotgs) 
				{
					as_channel_qevent_nolock(chan, AS_EVENT_RINGOFFHOOK);
					chan->gotgs = 1;
				}
			}
		/* fall through intentionally */

/* Case 3 : When FXO signal is used in this channel: this is a FXS device  */
		case AS_SIG_FXOLS: /* FXO Loopstart */
		case AS_SIG_FXOKS: /* FXO Kewlstart */
			switch(rxsig) 
			{
				case AS_RXSIG_OFFHOOK: /* went off hook */
			  /* if asserting ring, stop it */
					if (chan->txstate == AS_TXSTATE_START) 
					{
						as_channel_rbs_sethook(chan,AS_TXSIG_OFFHOOK, AS_TXSTATE_AFTERSTART, AS_AFTERSTART_TIME);
					}
					chan->kewlonhook = 0;
#if AS_DEBUG
					printk("Off hook on channel %d, itimer = %d, gotgs = %d\n", chan->channo, chan->itimer, chan->gotgs);
#endif
					if (chan->itimer) /* if timer still running */
					{
#if 0
			    			int plen = chan->itimerset - chan->itimer;
						if (plen <= ZT_MAXPULSETIME)
			    			{
								if (plen >= ZT_MINPULSETIME)
								{
									chan->pulsecount++;
									chan->pulsetimer = ZT_PULSETIMEOUT;
									chan->itimer = chan->itimerset;
									if (chan->pulsecount == 1)
										__qevent(chan,ZT_EVENT_PULSE_START); 
								} 
			    			} 
						else 
#endif							
							as_channel_qevent_nolock(chan,AS_EVENT_WINKFLASH); 
					} 
					else 
					{
				  /* if havent got GS detect */
						if (!chan->gotgs) 
						{
							as_channel_qevent_nolock(chan,AS_EVENT_RINGOFFHOOK); 
							chan->gotgs = 1;
							chan->itimerset = chan->itimer = 0;
						}
					}
					chan->itimerset = chan->itimer = 0;
					break;
		    		case AS_RXSIG_ONHOOK: /* went on hook */
					chan->gotgs = 0;
//					as_channel_hangup( chan );
					chan->txstate = AS_TXSTATE_ONHOOK;
					as_channel_qevent_nolock(chan,AS_EVENT_ONHOOK); 
					
			  /* if not during offhook debounce time */
					if ((chan->txstate != AS_TXSTATE_DEBOUNCE) &&
			    			(chan->txstate != AS_TXSTATE_KEWL) && 
			    			(chan->txstate != AS_TXSTATE_AFTERKEWL)) 
			    		{
						chan->itimerset = chan->itimer = chan->rxflashtime * 8;
					}
					if (chan->txstate == AS_TXSTATE_KEWL)
						chan->kewlonhook = 1;
					
					break;
		    		default:
					break;
			}
			
		default:
			break;
	}
}

/* this is a export symbol which is called by low layer driver to enqueue a received 
 * signal from low layer device , for example wcfxs call it in ISR to notify a ZT_RXSIG_RING  */
void as_io_channel_hooksig(struct as_dev_chan *chan, as_rxsig_t rxsig)
{
	  /* skip if no change */
	unsigned long flags;
	spin_lock_irqsave(&chan->lock, flags);
	__as_io_channel_in_hooksig_pvt(chan,rxsig);
	spin_unlock_irqrestore(&chan->lock, flags);
}


static inline void __as_io_channel_in_putaudio_chunk(struct as_dev_chan *ms, unsigned char *rxb)
{
	/* We transmit data from our master channel */
	/* Called with ss->lock held */
	/* Linear version of received data */
	short putlin[AS_CHUNKSIZE];
	int x;
/*
silence the data after dialing immediately  
*/
	if (ms->dialing) 
		ms->afterdialingtimer = 50;
	else if (ms->afterdialingtimer) 
		ms->afterdialingtimer--;
	if (ms->afterdialingtimer && (!(ms->flags & AS_CHAN_FLAG_PSEUDO))) 
	{
		/* Be careful since memset is likely a macro */
		rxb[0] = AS_LIN2X(0, ms);
		memset(&rxb[1], rxb[0], AS_CHUNKSIZE - 1);  /* receive as silence if dialing */
	} 

	for (x=0;x<AS_CHUNKSIZE;x++) 
	{
		rxb[x] = ms->rxgain[rxb[x]];
		putlin[x] = AS_XLAW(rxb[x], ms);
	}

	if (ms->ec) 
	{
		for (x=0;x<AS_CHUNKSIZE;x++) 
		{/* detect the tone which stop echo canceler */
			if (echo_can_disable_detector_update(&ms->rxecdis, putlin[x])) 
			{
				printk("Disabled echo canceller because of tone (rx) on channel %d\n", ms->channo);
				ms->echocancel = 0;
				ms->echostate = ECHO_STATE_IDLE;
				ms->echolastupdate = 0;
				ms->echotimer = 0;
				kfree(ms->ec);
				ms->ec = NULL;
				break;
			}
		}
	}

	if (!(ms->flags &  AS_CHAN_FLAG_PSEUDO)) 
	{
		memcpy(ms->putlin, putlin, AS_CHUNKSIZE * sizeof(short));
		memcpy(ms->putraw, rxb, AS_CHUNKSIZE);
	}
	
}


static inline void __as_io_channel_in_putbuf_chunk(struct as_dev_chan *ms, unsigned char *rxb)
{
	/* We transmit data from our master channel */
	/* Called with ss->lock held */
	/* Our receive buffer */
	unsigned char *buf;

	int oldbuf;
	int eof=0;
	int abort=0;
	int left;

	int bytes = AS_CHUNKSIZE;

	while(bytes) 
	{
		abort = 0;
		eof = 0;
		/* Next, figure out if we've got a buffer to receive into */
		if (ms->inreadbuf > -1) 
		{
			/* Read into the current buffer */
			buf = ms->readbuf[ms->inreadbuf];
			left = ms->blocksize - ms->readidx[ms->inreadbuf];
			if (left > bytes)
				left = bytes;
			memcpy(buf + ms->readidx[ms->inreadbuf], rxb, left);
			rxb += left;
			ms->readidx[ms->inreadbuf] += left;
			bytes -= left;
				/* End of frame is decided by block size of 'N' */
			eof = (ms->readidx[ms->inreadbuf] >= ms->blocksize);

			if (eof)  
			{/* Finished with this buffer, try another. */
				oldbuf = ms->inreadbuf;
				ms->readn[ms->inreadbuf] = ms->readidx[ms->inreadbuf];
#if AS_DEBUG
				printk("EOF, len is %d in channel %d\n", ms->readn[ms->inreadbuf], ms->channo );
#endif
				{
					ms->inreadbuf = (ms->inreadbuf + 1) % ms->numbufs;
					if (ms->inreadbuf == ms->outreadbuf) 
					{
						/* Whoops, we're full, and have no where else
						to store into at the moment.  We'll drop it
						until there's a buffer available */
#if AS_DEBUG
						printk("Out of storage space\n");
#endif
						ms->inreadbuf = -1;
						/* Enable the receiver in case they've got POLICY_WHEN_FULL */
						ms->rxdisable = 0;
					}
					if (ms->outreadbuf < 0) 
					{ /* start out buffer if not already */
						ms->outreadbuf = oldbuf;
					}
/* In the very orignal driver, it was quite well known to me (Jim) that there
was a possibility that a channel sleeping on a receive block needed to
be potentially woken up EVERY time a buffer was filled, not just on the first
one, because if only done on the first one there is a slight timing potential
of missing the wakeup (between where it senses the (lack of) active condition
(with interrupts disabled) and where it does the sleep (interrupts enabled)
in the read or iomux call, etc). That is why the read and iomux calls start
with an infinite loop that gets broken out of upon an active condition,
otherwise keeps sleeping and looking. The part in this code got "optimized"
out in the later versions, and is put back now. */
					if (!ms->rxdisable) 
					{ /* if receiver enabled */
						/* Notify a blocked reader that there is data available
						to be read, unless we're waiting for it to be full */
#if AS_DEBUG
						printk("Notifying reader data in block %d\n", oldbuf);
#endif
						wake_up_interruptible(&ms->readbufq);
						wake_up_interruptible(&ms->sel);
//						if (ms->iomask & ZT_IOMUX_READ)
//							wake_up_interruptible(&ms->eventbufq);
					}
				}
			}

			if (abort) 
			{
				/* Start over reading frame */
				ms->readidx[ms->inreadbuf] = 0;

//				if ((ms->flags & AS_CHAN_FLAG_OPEN) && !ss->span->alarms) 
						/* Notify the receiver... */
//					__qevent(ss->master, abort);
			}
		} 
		else /* No place to receive -- drop on the floor */
			break;
	}
}

static void __as_io_channel_in_receive_chunk(struct as_dev_chan *chan, unsigned char *buf)
{
	/* Receive chunk of audio -- called with chan->lock held */
	char waste[AS_CHUNKSIZE];

	if (!buf) 
	{
		memset(waste, AS_LIN2X(0, chan), sizeof(waste));
		buf = waste;
	}
	
	if ((chan->flags & AS_CHAN_FLAG_AUDIO)  ) 
	{/* gain and echo train process */
		__as_io_channel_in_putaudio_chunk(chan, buf);
	}
	/* put it into the upper layer buffer pool */
	__as_io_channel_in_putbuf_chunk(chan, buf);
}

#if 0
int as_io_span_in_receive(struct as_dev_span *span)
#endif
int as_io_span_in_receive(void *fxs)
{
	unsigned long flags;
	struct as_dev_chan  *chan;
	int i;
	int x;
	/*added for multi-cards support , lizhijie 2004.11.25 */
	struct wcfxs *wc = (struct wcfxs *)fxs;
//	struct as_dev_span *span = wc->span;

#if 0
	chan = span->chans;
	while ( chan ) 
#endif		
	for (i=0;i<wc->cards;i++)
	{
		x = wc->cardslot[i];
		chan = wc->chans[x];

		spin_lock_irqsave(&chan->lock, flags);
			
		/* Process a normal channel */
		__as_io_channel_in_receive_chunk(chan, chan->readchunk);
			
		if (chan->itimer) 
		{
			chan->itimer -= AS_CHUNKSIZE;
			if (chan->itimer <= 0) 
			{
				__as_io_channel_in_rbs_itimer_expire(chan );
			}
		}

			
		if ( chan->ringdebtimer)
			chan->ringdebtimer--;
			
		if (chan->sig & __AS_SIG_FXS) 
		{
			if (chan->rxhooksig == AS_RXSIG_RING)
				chan->ringtrailer = AS_RINGTRAILER;
			else if (chan->ringtrailer) 
			{
				chan->ringtrailer-= AS_CHUNKSIZE;
					/* See if RING trailer is expired */
				if (!chan->ringtrailer && !chan->ringdebtimer) 
					as_channel_qevent_nolock( chan, AS_EVENT_RINGOFFHOOK);
			}
		}
			
		if ( chan->pulsetimer)
		{
			chan->pulsetimer--;
			if (chan->pulsetimer <= 0)
			{
				if (chan->pulsecount)
				{
					if (chan->pulsecount > 12) 
					{
						printk("Got pulse digit %d on %s???\n", chan->pulsecount, chan->name);
					} 
					else if (chan->pulsecount > 11) 
					{
						as_channel_qevent_nolock(chan, AS_EVENT_PULSEDIGIT | '#');
					} 
					else if (chan->pulsecount > 10) 
					{
						as_channel_qevent_nolock(chan, AS_EVENT_PULSEDIGIT | '*');
					} 
					else if (chan->pulsecount > 9) 
					{
						as_channel_qevent_nolock(chan, AS_EVENT_PULSEDIGIT | '0');
					} 
					else 
					{
						as_channel_qevent_nolock( chan, AS_EVENT_PULSEDIGIT | ('0' + chan->pulsecount));
					}
					chan->pulsecount = 0;
				}
			}
		}

		spin_unlock_irqrestore(&chan->lock, flags);
#if 0
		chan = chan->next;
#endif
	}

	return 0;
}

