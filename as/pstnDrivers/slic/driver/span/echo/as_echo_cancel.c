#include "asstel.h"

void as_ec_chunk(struct as_dev_chan *ss, unsigned char *rxchunk, const unsigned char *txchunk)
{
	short rxlin, txlin;
	int x;
	unsigned long flags;
	spin_lock_irqsave(&ss->lock, flags);

	/* Perform echo cancellation on a chunk if necessary */
	if (ss->ec) 
	{

		if (ss->echostate & __ECHO_STATE_MUTE) 
		{
			/* Special stuff for training the echo can */
			for (x=0;x<AS_CHUNKSIZE;x++) 
			{
				rxlin = AS_XLAW(rxchunk[x], ss);
				txlin = AS_XLAW(txchunk[x], ss);
				if (ss->echostate == ECHO_STATE_PRETRAINING) 
				{
					if (--ss->echotimer <= 0) 
					{
						ss->echotimer = 0;
						ss->echostate = ECHO_STATE_STARTTRAINING;
					}
				}
				if ((ss->echostate == ECHO_STATE_AWAITINGECHO) && (txlin > 8000)) 
				{
					ss->echolastupdate = 0;
					ss->echostate = ECHO_STATE_TRAINING;
				}
				if (ss->echostate == ECHO_STATE_TRAINING) 
				{
					if (echo_can_traintap(ss->ec, ss->echolastupdate++, rxlin)) 
					{
#if 0
						printk("Finished training (%d taps trained)!\n", ss->echolastupdate);
#endif						
						ss->echostate = ECHO_STATE_ACTIVE;
					}
				}
				rxlin = 0;
				rxchunk[x] = AS_LIN2X((int)rxlin, ss);
			}
		}

		else 
		{
			for (x=0;x<AS_CHUNKSIZE;x++) 
			{
				rxlin = AS_XLAW(rxchunk[x], ss);
				rxlin = echo_can_update(ss->ec, AS_XLAW(txchunk[x], ss), rxlin);
				rxchunk[x] = AS_LIN2X((int)rxlin, ss);
			}
		}

	}
	else
#if ECHO_DEBUG
		printk("No echo canceler");
#endif

	spin_unlock_irqrestore(&ss->lock, flags);
}

