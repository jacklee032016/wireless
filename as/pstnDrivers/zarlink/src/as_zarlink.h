/*
 * $Log: as_zarlink.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/05/26 05:10:04  lizhijie
 * add zarlink 5023x driver into CVS
 *
 * $Id: as_zarlink.h,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
 * as_zarlink.h  
 * defination for zarlink 50235 chip, lizhijie 2005.05.16
*/

#ifndef __AS_ZARLINK_H__
#define  __AS_ZARLINK_H__

//#include <linux/module.h>
//#include <linux/sched.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <asm/hardware.h>
#include <linux/slab.h>

#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/delay.h>

/* common MACRO */
#define PCM_FRAME_DURATION					125		/* 125 us */ 		

typedef  enum
{
	AS_ZARLINK_DISABLE = 	0,
	AS_ZARLINK_ENABLE =	1
}AS_ENABLE_STATUS;

/* MACRO about IXP 422 expand bus */
#define  ZARLINK_CS_REGISTER					IXP425_EXP_CS1

#define  ZARLINK_ENABLE							(1<<31)		/* enable CS signal */

#define ZARLINK_CYCLE_T1						(3<<28)
#define ZARLINK_CYCLE_T2						(3<<26)
#define ZARLINK_CYCLE_T3						(3<<22)
#define ZARLINK_CYCLE_T4						(1<<20)
#define ZARLINK_CYCLE_T5						(2<<16)

#define  ZARLINK_CYCLE_TYPE						(1<<14)		/* motorola cycle ;bit 15:14 */
#define ZARLINK_MEM_SIZE						(2<<10)		/* CNFG, bit 13:10 , eg. 2k, 2**11, a10~a0 */


#define ZARLINK_BYTE_ACCESS					(1<<6)
#define  ZARLINK_MULTI_MODE					(0<<4)		/* non-multiplexed */
#define  ZAARLINK_SPLIT_TRANSFER_DISABLE		(0<<3)
#define  ZAARLINK_WRITE_PROTEECT_DISABLE	 	(1<<1)
#define  ZARLINK_BUS_WIDTH_8					(1<<0)		/* data bus size is 8 bits */

/* value set in EXP_TIMING_CS_1 register */
#define ZARLINK_CONTROL_STATUS		ZARLINK_ENABLE | \
					ZARLINK_CYCLE_T1 | \
					ZARLINK_CYCLE_T2 | \
					ZARLINK_CYCLE_T3 | \
					ZARLINK_CYCLE_T4 | \
					ZARLINK_CYCLE_T5 | \
					ZARLINK_CYCLE_TYPE | \
					ZARLINK_MEM_SIZE | \
					ZARLINK_BYTE_ACCESS | \
					ZARLINK_MULTI_MODE | \
					ZAARLINK_SPLIT_TRANSFER_DISABLE | \
					ZAARLINK_WRITE_PROTEECT_DISABLE | \
					ZARLINK_BUS_WIDTH_8

#define ZARLINK_BASE_ADDRESS		IXP425_EXP_BUS_CS1_BASE_PHYS
#define ZARLINK_WINDOW_SIZE		0x00000800



/* ZARLINK involved */
/* register deefination */

/* Controler Register 1 and 2 of ECB must be write twice in the gap of (350ns ~ 20us) */
#define ZARLINK_CH_CR1								0x00
#define ZARLINK_CH_CR2								0x01
#define ZARLINK_CH_CR3								0x08
#define ZARLINK_CH_CR4								0x09

#define ZARLINK_CH_SR								0x02

#define ZARLINK_CH_FLATDELAY_R					0x04
#define ZARLINK_CH_DECAY_STEP_SIZE_R				0x06
#define ZARLINK_CH_DECAY_STEP_NUMBER_R			0x07

#define ZARLINK_CH_NOISE_SCALING_R				0x0a
#define ZARLINK_CH_NOISE_CONTROL_R				0x0b

/* following is 16 bit channel register */
#define ZARLINK_CH_RIN_PEAK_R						0x0c
#define ZARLINK_CH_SIN_PEAK_R						0x0E
#define ZARLINK_CH_ERROR_PEAK_R					0x10

#define ZARLINK_CH_DTDT_R							0x14	/* Double-Talk Detection Threshold */
#define ZARLINK_CH_NLPTH_R							0x18	/* NLP Threshold */
#define ZARLINK_CH_MU_STEPSIZE_R					0x1A
#define ZARLINK_CH_GAINS_R							0x1C 

#define ZARLINK_MAIN_CR_BASE						0x400

#define ZARLINK_FIFO_REG							0x410


/* value of register defination */
/* main control register */
#define ZL_MCR_WR_ALL								(0<<7)
#define ZL_MCR_ODE									(1<<6)
#define ZL_MCR_MIRQ									(1<<5)		/* MASK Interrupt : Tone Detect */
#define ZL_MCR_MTDBI								(1<<4) 		/* MASK Tone Detect B Interrupt */
#define ZL_MCR_MTDAI								(1<<3)
#define ZL_MCR_FORMAT								(1<<2)		/* 1 : ITU-T G.711 PCM code, otherwise, sign Magnitude PCM*/
#define ZL_MCR_ALAW								(0<<1)		/* 0 : u-Law, 1: A-law */

#define ZL_MCR_N_VALUE			ZL_MCR_MTDBI | \
							ZL_MCR_MTDAI | \
							ZL_MCR_FORMAT | \
							ZL_MCR_ALAW 

#define ZL_MCR_0_VALUE			ZL_MCR_WR_ALL | \
							ZL_MCR_ODE	| \
							ZL_MCR_MIRQ | \
							ZL_MCR_N_VALUE
							
/* value of channel register */
#define ZL_EXT_ENABLE_BIT						0
#define ZL_CR1_ADA_DISABLE_BIT					2
#define ZL_CR1_BYPASS_BIT						3
#define ZL_CR1_PAD_BIT							4	/* 12 dB of attenuation */
#define ZL_CR1_BBM_BIT							5	/* Back to Back */
#define ZL_CR1_INJ_DIS_BIT						6	/* noise inject disable */
#define ZL_CR1_RESET_BIT						7

#define ZL_CR2_MUTE_R_BIT						0
#define ZL_CR2_MUTE_S_BIT						1
#define ZL_CR2_HPF_DIS_BIT						2	/* offset nulling high pass filter */
#define ZL_CR2_NB_DIS_BIT						3	/* Narrow-band detector  */
#define ZL_CR2_AUTO_TD_BIT						4
#define ZL_CR2_NLP_DIS_BIT						5	/* non-linear processor */
#define ZL_CR2_PH_DIS_BIT						6
#define ZL_CR2_TD_DIS_BIT						7	/* Tone Detector */


#define ZL_CR1_INIT_VALUE						(0 )//|(1<<ZL_CR1_BYPASS_BIT)  )
#define ZL_CR2_INIT_VALUE						0

#define ZL_DEBUG(x)		printk(KERN_INFO "ZARLINK DRIVER : %s\r\n", x )

typedef struct 
{
	unsigned char name[64];
	
	int group_num;		/* group size which need to initialized */
	int chan_num;		/* normally this is 2*group_num */

	struct resource *resource;
	unsigned long map_private;
	
}AS_ZARLINK;


inline unsigned char as_zarlink_read(AS_ZARLINK *zr, unsigned long offset );
inline void as_zarlink_write(AS_ZARLINK *zr, unsigned long offset, unsigned char value);
char as_zarlink_ch_read(AS_ZARLINK *zr, unsigned int channel_no, int reg);
void as_zarlink_ch_write(AS_ZARLINK *zr, unsigned int channel_no, int reg, unsigned char value);
unsigned short as_zarlink_ch_read_16(AS_ZARLINK *zr, unsigned int chan_no, int reg);
void as_zarlink_ch_write_16(AS_ZARLINK *zr, unsigned int chan_no, int reg, unsigned short value);

int __init as_zarlink_hardware_check_config(AS_ZARLINK *zr);


#ifdef __KERNEL__
	#define trace		printk("%s[%d]\r\n", __FUNCTION__, __LINE__)
#else
	#define trace		printf("%s[%d]\r\n", __FUNCTION__, __LINE__)
#endif

#endif

