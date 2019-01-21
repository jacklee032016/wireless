/*
$Log: zl_version.h,v $
Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
AS600 Drivers

Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
rebuild

Revision 1.1  2005/05/26 05:10:04  lizhijie
add zarlink 5023x driver into CVS

$Id: zl_version.h,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
Version info for ZarLink Drivers, Li Zhijie, 2005.05.21 
*/
#ifndef __ZL_VERSION_H__
#define __ZL_VERSION_H__

#define AS_VERSION_MAJOR		"01"  /* major version number ,start from 1 */
#define AS_VERSION_MINOR		"0"	/* minor, from 0 to 9 */
#define AS_VERSION_RELEASE		"00"	/* from 01 to 99 */
#define AS_VERSION_MODIFY		"00"	/* from 00 to 99 */

#if  __KERNEL__
	#define AS_VERSION_INFO(moduleName)	"%s( version: %s.%s.%s-%s %s %s)\r\n", (moduleName), \
		AS_VERSION_MAJOR,AS_VERSION_MINOR, AS_VERSION_RELEASE, AS_VERSION_MODIFY, __DATE__, __TIME__
#else
	#define AS_VERSION_INFO(moduleName)  	"%s( version: %s.%s.%s-%s %s %s)\r\n", (moduleName), \
		AS_VERSION_MAJOR,AS_VERSION_MINOR, AS_VERSION_RELEASE, AS_VERSION_MODIFY, __DATE__, __TIME__
#endif

#endif
