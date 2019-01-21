/*
 * $Author: lizhijie $
 * $Log: as_lib_ring_param.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:47  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:05  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:52  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:11  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>


#include "assist_lib.h"

#if 0
int as_gen_ring_para(int *ringcadence,struct as_si_regring_para *ringpara)
{

	int i;
	FILE *f=NULL;
	if ((f = fopen("ring_para_config.h", "w") )!=NULL ) 
	{	

		fprintf(f,"struct as_si_regring_para defringpara[]=\n\t{");

		for(i=0;i<2;i++)
		{
			fprintf(f,"{%d,%d,%d,%d,%d,%d},\n\t",ringpara[i].waveform,
				ringpara[i].bias_adjust,ringpara[i].ring_freq,ringpara[i].ring_amp,
				ringpara[i].ring_init_phase,ringpara[i].dc_offset);
		}
		fprintf(f,"{%d,%d,%d,%d,%d,%d}};",ringpara[i].waveform,
				ringpara[i].bias_adjust,ringpara[i].ring_freq,ringpara[i].ring_amp,
				ringpara[i].ring_init_phase,ringpara[i].dc_offset);
	}
	else 
	{
		fprintf(stderr, "Unable to open as_zone_tone_digits.h for writing\n");
		return 1;
	}

	return 0;
}
#endif

static struct as_si_reg_ring_para *__as_ring_param_translate(struct as_si_ring_para *para)
{
	struct as_si_reg_ring_para *reg_param;
	float coeff;
	float trise;

	reg_param = (struct as_si_reg_ring_para *)malloc(sizeof(struct as_si_reg_ring_para ));
	if(!reg_param)
	{
		fprintf(stderr, "No memory for ring parameter\r\n");
		return NULL;
	}
	
	reg_param->dc_offset=para->dc_offset*32768.0/96;
	reg_param->bias_adjust=0x0C00;
	
	if(para->waveform == RING_WAVEFORM_SINE)
	{
		reg_param->waveform=1;
		coeff=cos(2.0 * M_PI * (para->ring_freq / 1000.0));
		reg_param->ring_freq=coeff*32768.0;
		reg_param->ring_amp=0.25*sqrt((1-coeff)/(1+coeff))*para->ring_vpk*32768.0/96;
		reg_param->ring_init_phase=0;
	}
	else if(para->waveform ==RING_WAVEFORM_TRAP)
	{
		trise=.75*(1-1/((para->crest_factor)*(para->crest_factor)))/para->ring_freq;
		reg_param->ring_init_phase=0.5*8000.0/para->ring_freq;
		reg_param->ring_amp=para->ring_vpk*32768.0/96;
		reg_param->ring_freq=2.0*reg_param->ring_amp/(8000.0*trise);
		reg_param->waveform=2;
	}
	else
		fprintf(stderr, "Not support waveform with ID '%d'\r\n" , para->waveform);

	return reg_param;
}


/* this function should be extend to lookup channel info used in system currently */
char *as_chan_name_get_by_id(int chan_no)
{
	char name[40];

	sprintf(name, "/dev/asstel/%d",  chan_no);
	
	return strdup(name);
}

/* */
int as_ring_param_reset( struct as_si_ring_para *param)
{
	char *filename;
	int fd,res;
	struct as_si_reg_ring_para *reg;

	if(!param)
	{
		fprintf(stderr, "No ring parameters are defined\r\n");
		return -1;
	}

	reg = __as_ring_param_translate( param);
	if(!reg)
		return -1;

	filename = as_chan_name_get_by_id(param->channel_no);
	fd = open(filename, O_RDWR);
	if (fd < 0) 	
	{
		fprintf(stderr, "Unable to open %s: %s\n", filename, strerror(errno));
		return -1;
	}

	res = ioctl(fd, WCFXS_SET_RING_PARA, reg );
	if(res)
	{
		fprintf(stderr, "set ring para error: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

