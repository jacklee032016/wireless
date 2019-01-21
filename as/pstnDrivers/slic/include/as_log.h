/*
 * $Author: lizhijie $
 * $Log: as_log.h,v $
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
#ifndef  __AS_LOG_H__
#define __AS_LOG_H__

  typedef enum _as_trace_level
  {
	AS_TRACE_LEVEL0 = 0,
#define AS_FATAL    0
	AS_TRACE_LEVEL1 = 1,
#define AS_BUG      1
	AS_TRACE_LEVEL2 = 2,
#define AS_ERROR    2
	AS_TRACE_LEVEL3 = 3,
#define AS_WARNING  3
	AS_TRACE_LEVEL4 = 4,
#define AS_INFO1    4
	AS_TRACE_LEVEL5 = 5,
#define AS_INFO2    5
	AS_TRACE_LEVEL6 = 6,
#define AS_INFO3    6
	AS_TRACE_LEVEL7 = 7,
#define AS_INFO4    7
	AS_END_TRACE_LEVEL = 8
  }as_trace_level_t;

int osip_trace (char *filename, int linenumber, as_trace_level_t level, FILE * f,  char *chfr, ...);

#ifdef AS_ENABLE_TRACE
	#define AS_TRACE(P) P
#else
	#define AS_TRACE(P) do {} while (0)
#endif


#endif

