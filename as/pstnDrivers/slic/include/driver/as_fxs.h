/*
 * $Author: lizhijie $
 * $Log: as_fxs.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
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
#ifndef  __AS_FXS_H__
#define __AS_FXS_H__
/*
 * Wilcard S100P FXS Interface Driver for Zapata Telephony interface
 */

#include <linux/ioctl.h>

/* added 2004.11.17 */
#define RING_WAVEFORM_SINE	1
#define RING_WAVEFORM_TRAP	2


#define AS_WITH_FXO_MODULE	1

#define NUM_REGS	  109
#define NUM_INDIRECT_REGS 105

struct wcfxs_stats 
{
	int tipvolt;	/* TIP voltage (mV) */
	int ringvolt;	/* RING voltage (mV) */
	int batvolt;	/* VBAT voltage (mV) */
};

struct wcfxs_regs 
{
	unsigned char direct[NUM_REGS];
	unsigned short indirect[NUM_INDIRECT_REGS];
};

struct wcfxs_regop 
{
	int indirect;
	unsigned char reg;
	unsigned short val;
};

#define WCFXS_GET_STATS	_IOR (AS_CTL_CODE, 60, struct wcfxs_stats)
#define WCFXS_GET_REGS		_IOR (AS_CTL_CODE, 61, struct wcfxs_regs)
#define WCFXS_SET_REG		_IOW (AS_CTL_CODE, 62, struct wcfxs_regop)
/* added by assist , 2004.11.17 */
#define WCFXS_SET_RING_PARA _IOW (AS_CTL_CODE, 63, struct as_si_reg_ring_para)

#endif

