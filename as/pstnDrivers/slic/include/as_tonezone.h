/*
 * $Author: lizhijie $
 * $Log: as_tonezone.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/04/26 06:06:10  lizhijie
 * *** empty log message ***
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:46  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:04  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:51  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:09  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#ifndef __AS_TONEZONE_H__
#define __AS_TONEZONE_H__

#include "as_dsp.h"

#define LEVEL -10

struct tone_zone_sound 
{
	int toneid;
	char data[256];				/* Actual zone description */
	/* Description is a series of tones of the format:
	   [!]freq1[+freq2][/time] separated by commas.  There
	   are no spaces.  The sequence is repeated back to the 
	   first tone description not preceeded by !.  time is
	   specified in milliseconds */
};

struct tone_zone 
{
	int zone_id;					/* Zone number */
	char country[10];			/* Country code */
	char description[40];		/* Description */
	int ringcadence[AS_MAX_CADENCE];	/* Ring cadence */
	struct tone_zone_sound tones[AS_TONE_MAX];
};

extern struct tone_zone builtin_zones[];


#define USED_ZONE_INDEX	0

#endif
