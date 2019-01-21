/*
 * $Author: lizhijie $
 * $Log: as_dsp_private.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1  2004/12/20 03:27:44  lizhijie
 * add DSP engine and gain code into CVS
 *
 * $Revision: 1.1.1.1 $
*/
#ifndef  __AS_DSP_PRIVATE_H__
#define __AS_DSP_PRIVATE_H__


float as_dsp_get_localtone(as_dsp_gen_engine *dsp, float ddr,float ddi);
float as_dsp_get_localtone_df(as_dsp_gen_engine *dsp, float ddr,float ddi,float ddr1, float ddi1);
int as_dsp_put_audio_sample(as_dsp_gen_engine *dsp, int fd,float y,int flag);
void as_dsp_coeff_reset(as_dsp_gen_engine *dsp);
int as_dsp_arbtones(as_dsp_gen_engine *dsp, int fd , float f0 , float f1 , int len ,int flag );
int as_dsp_finish_audio(as_dsp_gen_engine *dsp, int fd,int flag);
int as_dsp_silence(as_dsp_gen_engine *engine, int fd ,int len );

#endif

