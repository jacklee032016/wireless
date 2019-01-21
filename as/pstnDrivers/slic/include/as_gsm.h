/*
 * $Author: lizhijie $
 * $Log: as_gsm.h,v $
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
#ifndef __AS_GSM_H__
#define __AS_GSM_H__

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>
#ifdef __linux__
#include <endian.h>
#else
#include <machine/endian.h>
#endif

#include "gsm.h"

#define AS_GSM_FRAME_LENGTH		33
#define AS_GSM_DATA_LENGTH		160

struct AS_GSM_DATA_FRAME
{
	unsigned short  data[AS_GSM_DATA_LENGTH];
};

typedef struct AS_GSM_DATA_FRAME	as_gsm_data_frame_t;

struct AS_GSM_FRAME
{
	unsigned char data[AS_GSM_FRAME_LENGTH];
};

typedef struct AS_GSM_FRAME	as_gsm_frame_t;

struct as_gsm_engine
{
	gsm _gsm;
	int inited;
};

typedef struct as_gsm_engine as_gsm_engine_t ;


void as_gsm_file_play(char *gsmfile, int devfd);

#endif

