/*
 * $Author: lizhijie $
 * $Log: assist_lib.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.3  2005/06/24 07:30:33  lijie
 * *** empty log message ***
 *
 * Revision 1.2  2005/04/26 06:06:10  lizhijie
 * *** empty log message ***
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.9  2005/01/06 05:36:33  fengshikui
 * add busy tone detect by fengshikui 2005.1.6
 *
 * Revision 1.8  2004/12/20 03:18:15  lizhijie
 * Add DSP library for FSK detect/generate, DTMF detect and other test code
 *
 * Revision 1.7  2004/12/11 06:12:19  lizhijie
 * merge into CVS
 *
 * Revision 1.6  2004/12/11 05:05:30  eagle
 * add test_gain.c
 *
 * Revision 1.5  2004/12/09 07:49:00  lizhijie
 * modify tone generator program into CVS
 *
 * Revision 1.4  2004/11/29 08:25:03  eagle
 * 2229 by chenchaoxin
 *
 * Revision 1.3  2004/11/25 07:59:51  lizhijie
 * recommit all
 *
 * Revision 1.2  2004/11/22 01:54:04  lizhijie
 * add some user module into CVS
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#ifndef __ASSIST_LIB_H__
#define __ASSIST_LIB_H__
#define trace		printf("%s[%d]\r\n", __FUNCTION__, __LINE__);

#define FALSE   0
#define TRUE    (!FALSE)

/* used in call process control */
#define AS_ERROR_TIMEOUT		-1
#define AS_ERROR_ONHOOK		-2

#ifndef AS_ELAST
#define AS_ELAST 500
#endif

#define AS_SPAN_DEV		"/dev/astel/span"

#include "as_dev_ctl.h"
#include "as_ring_param.h"


/*********************************************************************
 *  Following are library function used for the purpose of driver control l 
*********************************************************************/
/* from as_lib_ring_param.c */
int as_ring_param_reset( struct as_si_ring_para *param);


/* from as_lib_tonezone.c */
int as_lib_set_zone( char *country);
int as_lib_set_zone_japan();


/*********************************************************************
 *  Following are utils function used in driver 
*********************************************************************/
int as_error_msg(char *fmt, ...);


/*********************************************************************
 *  Following are library function used by user app such as call 
*********************************************************************/
/* from as_lib_gsm.c */
void as_gsm_file_play(char *gsmfile, int devfd);

/* from as_lib_ring.c */
int as_ring_with_dtmf_caller_id( int  fd  , unsigned char *call_id);
int as_ring_on_hook( int  fd );
/* set device to ONHOOK status, eg. reset the device */
int as_lib_onhook(int fd);
/* set the device to OFFHOOK status, eg. begin to communicate */
int as_lib_offhook(int fd);
/* get an event of the device */
int as_lib_event_get(int fd);
/* wait RINGOFF event, 
 * when it used on a FXO device, this is first RINGEROFF
 * when is used on a FXS device, this is OFFHOOK
*/
void as_lib_wait_offhook(int fd);
void as_lib_wait_onhook(int fd);



/* from as_lib_tones.c */
int as_tone_play_stop( int fd );
int as_tone_play_dial( int fd );
int as_tone_play_busy( int  fd ) ;
int as_tone_play_ring( int  fd ) ;
int as_tone_play_congestion( int  fd ) ;
int as_tone_play_callwait( int  fd ) ;
int as_tone_play_dialrecall( int  fd ) ;
int as_tone_play_record(  int  fd  ) ;
int as_tone_play_info(  int  fd  ) ;
int as_tone_play_custom_1(  int  fd ) ;
int as_tone_play_custom_2( int  fd ) ;
int as_tone_play_stutter(  int  fd ) ;


/* from and for as_lib_law.c */
unsigned char   as_lib_law_linear2alaw (short linear);
unsigned char  as_lib_law_linear2ulaw(short sample);
short as_lib_law_ulaw2linear(unsigned char sample);
short as_lib_law_alaw2linear(unsigned char sample);
short as_lib_law_full_ulaw2linear(unsigned char sample);
short as_lib_law_full_alaw2linear(unsigned char sample);


/*add by fengshikui 2005.1.6*/
#include"as_busy_detect.h"
/*busy detect function*/
int as_dsp_busydetect(as_tone_detector *dsp,unsigned char *buff,int length);
as_tone_detector *as_dsp_new(void);
void as_dsp_free(as_tone_detector *dsp);
int set_law(as_tone_detector * dsp,int law);


#define A_LAW_CODE		0
#define U_LAW_CODE		1  /* default code method of our driver */
#define	LINEAR2XLAW(s,law) ((law == U_LAW_CODE) ? as_lib_law_linear2ulaw(s) : as_lib_law_linear2alaw(s)) 
#define	XLAW(c,law) ((law ==  U_LAW_CODE) ? as_lib_law_ulaw2linear(c): as_lib_law_alaw2linear(c) )
#define	FULLXLAW(c,law) ((law ==  U_LAW_CODE) ? as_lib_law_full_ulaw2linear(c): as_lib_law_full_alaw2linear(c))

/* from and for as_lib_dtmf_detect.c */
#define DTMF_DETECT_STEP_SIZE		102
/* return :
	0 : no dtmd caller id detect, 
	>0, numbr of detected caller ID,
	<0, error
 Parameters :
 	length : should be the times of DTMF_DETECT_STEP_SIZE
 	callerID : keep the detected caller ID, must allocate in advanced
 	max : max length of caller ID you want get  */
int as_lib_dtmf_detect(short  samples[], int length, char *callerId, int max);


/* from and for as_lib_dtmf_generator.c */
#define	AS_MAXDTMF 64
#define	AS_DTMF_BLOCKSIZE 204	/* perfect for goertzel */
#define	AS_DEFAULT_DTMFLENGTH 	100 /* default tone length -- 100 ms */

/* dial a digit string 
 digitstring : digit string to dial in ascii 
 length : length of every tone(char) in ms,default is 100 ms
 */
int as_lib_dial(int fd , char *digitstring ,int length );


/* from as_lib_hard_dtmf.c 
 * receive the DTMF caller ID with hardware in Si3210, 
 * fd : must be a FXS device
 * phone : buffer return the caller ID, it must be allocate in advanced
 * max : max phone number, less than the buffer size of 'phone'
 * timeout : timeout between every char in ms
 * return : -1 : timeout, -2: ONHOOK, >0: total of phone number 
*/
int as_lib_hard_dmtf_caller_id( int fd, unsigned char *phone, int max , int timeout);


#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
int as_buf_get_info(  int  fd ,struct as_bufferinfo *info);
int as_buf_para_set(  int  fd ,struct as_bufferinfo *info);

int as_get_block_size(int fd,int *block_size);
int as_set_block_size(int fd,int block_size);

/* Get event on queue */
int as_get_event(int fd,int event);
int as_set_law(int fd,int law);
int as_set_linear(int fd,int linear);


int as_get_chan_para(int fd,struct as_params *para);
int as_set_chan_para(int fd,struct as_params *para);

int as_get_gains(int fd,struct as_gains *gain);
int as_set_gains(int fd,struct as_gains *gain);
/**************************************************
**********get the TIP RING VBAT voltage************
**************************************************/

int as_get_wcfxs_stats(int fd,struct wcfxs_stats *stats);
int as_get_wcfxs_regs(int fd,struct wcfxs_regs *regs);
int as_set_wcfxs_regs(int fd,struct wcfxs_regop *regop);


int as_set_candence(int fd,struct as_ring_cadence *candence);


int as_set_channel_para(int channelno,struct as_channel_para *channel_para);
int as_set_global_para(struct as_global_para *global_para);

int as_get_channel_state(struct as_channel_state **channel_states);

void debug_buf_info( struct as_bufferinfo *info);
char *as_dev_event_debug(int fd, int event);
int as_get_channel_num(int *chan_num);
void as_ring(int fd);

#include "as_lib_dsp.h"

/* from as_lib_tone_gen_play.c */
int as_lib_put_audio_sample(as_dsp_gen_engine *dsp, int fd,float y,int flag);
int as_lib_finish_audio(as_dsp_gen_engine *dsp, int fd,int flag);
/* send silence   len :  length in ms */
int as_lib_silence(as_dsp_gen_engine *engine, int fd ,int len );


/* from as_fsk_generator.c  */
/* ring the phone and send a Caller*ID string. Returns 0 if caller did not
answer during this function, 1 if caller answered after receiving CLID, and
2 if caller answered before receiving CLID */
/* pointer to buffer in which to return caller's number */
/* pointer to buffer in which to return caller's name */
int as_dsp_lib_ring_with_fsk_callerid(as_dsp_gen_engine *dsp, int fd , char *number , char *name );

/* from as_dtmf_generator.c */
/* dial a digit string 
 digitstring : digit string to dial in ascii 
 length : length of every tone(char) in ms,default is 100 ms
 */
int as_dsp_lib_dial_with_dtmf_caller_id(as_dsp_gen_engine *dsp,  int fd , char *digitstring ,int length );

/* from as_dsp_gain.c */
/* gain is float in the form of dB */
int as_dsp_set_gain(as_dsp_gen_engine *dsp, int fd, float rxgain, float txgain);

int as_dsp_play_tone_dial(as_dsp_gen_engine *dsp,int fd);
int as_dsp_play_tone_busy(as_dsp_gen_engine *dsp,int fd);
int as_dsp_play_tone_ringback(as_dsp_gen_engine *dsp,int fd);

#endif

