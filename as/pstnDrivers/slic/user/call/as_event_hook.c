#include <pthread.h>

#include "as_global.h"


int as_off_hook_event_handler(as_device_t *dev)
{
	int res;
	
	pthread_t threadid;
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

//	dev->state = AS_DEVICE_STATE_USED;
//	as_set_hook(dev, AS_DEVICE_STATE_USED );

	switch( dev->signal_type ) 
	{
		case AS_SIGNAL_TYPE_FXO :
//			as_enable_echo_cancel( dev );/* only needed in FXS interface */
			/* as caller , as callee should be implemented in future */
			res = as_tone_play_dial( dev );

//			device = as_new_device( dev, 0, 0, 0, 0, 0);
			
//			pthread_create(&threadid, &attr, as_dialing_thread, dev);
			
#if 0
			/* The dev is immediately up.  Start right away */
			res = tone_zone_play_tone(i->subs[SUB_REAL].zfd, ZT_TONE_RINGTONE);
			chan = zt_new(i, AST_STATE_RING, 1, SUB_REAL, 0, 0);
			if (!chan) {
					ast_log(LOG_WARNING, "Unable to start PBX on dev %d\n", i->dev);
					res = tone_zone_play_tone(i->subs[SUB_REAL].zfd, ZT_TONE_CONGESTION);
					if (res < 0)
						ast_log(LOG_WARNING, "Unable to play congestion tone on dev %d\n", i->dev);
				}
			} else {
				/* Check for callerid, digits, etc */
				chan = zt_new(i, AST_STATE_RESERVED, 0, SUB_REAL, 0, 0);
				if (chan) {
					if (has_voicemail(i))
						res = tone_zone_play_tone(i->subs[SUB_REAL].zfd, ZT_TONE_STUTTER);
					else
						res = tone_zone_play_tone(i->subs[SUB_REAL].zfd, ZT_TONE_DIALTONE);
					if (res < 0) 
						ast_log(LOG_WARNING, "Unable to play dialtone on dev %d\n", i->dev);
					if (pthread_create(&threadid, &attr, ss_thread, chan)) {
						ast_log(LOG_WARNING, "Unable to start simple switch thread on dev %d\n", i->dev);
						res = tone_zone_play_tone(i->subs[SUB_REAL].zfd, ZT_TONE_CONGESTION);
						if (res < 0)
							ast_log(LOG_WARNING, "Unable to play congestion tone on dev %d\n", i->dev);
						ast_hangup(chan);
					}
				} else
					ast_log(LOG_WARNING, "Unable to create dev\n");
			}
#endif			
			break;
		case AS_SIGNAL_TYPE_FXS:
		{/* FXO interface */
#if 0
			chan = zt_new(i, AST_STATE_RING, 0, SUB_REAL, 0, 0);
			if (chan && pthread_create(&threadid, &attr, ss_thread, chan)) 
			{
				ast_log(LOG_WARNING, "Unable to start simple switch thread on dev %d\n", i->dev);
				res = tone_zone_play_tone(i->subs[SUB_REAL].zfd, ZT_TONE_CONGESTION);
				if (res < 0)
					ast_log(LOG_WARNING, "Unable to play congestion tone on dev %d\n", i->dev);
				ast_hangup(chan);
			} 
			else if (!chan) 
			{
				ast_log(LOG_WARNING, "Cannot allocate new structure on dev %d\n", i->dev);
			}
#endif
			as_error_msg( "Has not been implemented for FXO interface on dev %s\n", dev->name );
			res = as_tone_play_congestion( dev );

			break;
		}

		default:
		{
			as_error_msg("Don't know how to handle ring/answer with signalling %d on dev %s\n", dev->signal_type, dev->name );
			res = as_tone_play_congestion(dev );
			if (res < 0)
				as_error_msg( "Unable to play congestion tone on dev %s\n", dev->name);
			return -1;
		}
	}

	return res;
}

int as_on_hook_event_handler(as_device_t *dev)
{
	int res;

	dev->state = AS_DEVICE_STATE_AVAILABLE;
	
	switch( dev->signal_type) 
	{
		case AS_SIGNAL_TYPE_FXO:
		case AS_SIGNAL_TYPE_FXS:
//			as_disable_echo_cancel( dev );
			as_set_hook(dev, AS_DEVICE_STATE_USED);
			usleep(1);
			res = as_tone_play_stop( dev );
			/* this can be called after other operation in OFF_HOOK mode of the dev has 
			complete such echo_cancel or stop */
			as_set_hook(dev, AS_DEVICE_STATE_AVAILABLE );
//			as_ring_on_hook( dev);
			break;
		default:
			as_error_msg( "Don't know how to handle on hook with signalling %d on dev %s\n", dev->signal_type, dev->name);
			res = as_tone_play_stop(dev);
			return -1;
	}
	return res;
}

