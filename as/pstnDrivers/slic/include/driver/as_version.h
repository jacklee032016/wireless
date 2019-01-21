/*
$Log: as_version.h,v $
Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
AS600 Drivers

Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
rebuild

Revision 1.3  2005/07/07 02:49:21  wangwei
no message

Revision 1.2  2005/04/26 06:02:06  lizhijie
modify version info format

Revision 1.1  2005/04/20 02:34:12  lizhijie
no message

$Id: as_version.h,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
*/
#ifndef __AS_VERSION_H__
#define __AS_VERSION_H__

#define AS_VERSION_MAJOR		"01"  /* major version number ,start from 1 */
#define AS_VERSION_MINOR		"0"	/* minor, from 0 to 9 */
#define AS_VERSION_RELEASE		"02"	/* from 01 to 99 */
#define AS_VERSION_MODIFY		"00"	/* from 00 to 99 */

#if  __KERNEL__
	#define AS_VERSION_INFO(moduleName)	"%s( version: %s.%s.%s-%s %s %s)\r\n", (moduleName), \
		AS_VERSION_MAJOR,AS_VERSION_MINOR, AS_VERSION_RELEASE, AS_VERSION_MODIFY, __DATE__, __TIME__
#else
	#define AS_VERSION_INFO(moduleName)  	"%s( version: %s.%s.%s-%s %s %s)\r\n", (moduleName), \
		AS_VERSION_MAJOR,AS_VERSION_MINOR, AS_VERSION_RELEASE, AS_VERSION_MODIFY, __DATE__, __TIME__
#endif

#endif
