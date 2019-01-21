/*
 * $Author: lizhijie $
 * $Log: as_mf_digits.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:45  fengshikui
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
#ifndef __AS_MF_DIGITS_H__
#define AS_MF_DIGITS_H__

#define DEFAULT_DTMF_LENGTH	100 * 8

/* Dial frequency tables */
typedef struct
{
	char    chr;    /* character representation */
	float   f1;     /* first freq */
	float   f2;     /* second freq */
} AS_DIAL;
 
AS_DIAL as_dtmf_dial[] = 
{
	{ '0',941.0,1336.0 },
	{ '1',697.0,1209.0 },
	{ '2',697.0,1336.0 },
	{ '3',697.0,1477.0 },
	{ '4',770.0,1209.0 },
	{ '5',770.0,1336.0 },
	{ '6',770.0,1477.0 },
	{ '7',852.0,1209.0 },
	{ '8',852.0,1336.0 },
	{ '9',852.0,1477.0 },
	{ '*',941.0,1209.0 },
	{ '#',941.0,1477.0 },
	{ 'A',697.0,1633.0 },
	{ 'B',770.0,1633.0 },
	{ 'C',852.0,1633.0 },
	{ 'D',941.0,1633.0 },
	{ 0,0,0 }
} ;
 
AS_DIAL as_mf_dial[] = 
{
	{ '0',1300.0,1500.0 },
	{ '1',700.0,900.0 },
	{ '2',700.0,1100.0 },
	{ '3',900.0,1100.0 },
	{ '4',700.0,1300.0 },
	{ '5',900.0,1300.0 },
	{ '6',1100.0,1300.0 },
	{ '7',700.0,1500.0 },
	{ '8',900.0,1500.0 },
	{ '9',1100.0,1500.0 },
	{ '*',1100.0,1700.0 }, /* KP */
	{ '#',1500.0,1700.0 }, /* ST */
	{ 'A',900.0,1700.0 }, /* ST' */
	{ 'B',1300.0,1700.0}, /* ST'' */
	{ 'C',700.0,1700.0}, /* ST''' */
	{ 0,0,0 }
} ;                                                                             

#endif

