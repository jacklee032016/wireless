/*
 * $Author: lizhijie $
 * $Id: as_isdn_version.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
*/

#ifndef __AS_ISDN_VERSION_H__
#define __AS_ISDN_VERSION_H__

#define AS_VERSION_MAJOR		"01"  /* major version number ,start from 1 */
#define AS_VERSION_MINOR		"0"	/* minor, from 0 to 9 */
#define AS_VERSION_RELEASE		"00"	/* from 01 to 99 */
#define AS_VERSION_MODIFY		"00"	/* from 00 to 99 */

#if  __KERNEL__
	#define AS_VERSION_INFO(moduleName)	"Assist ISDN : %s( version: %s.%s.%s-%s %s %s)\n", (moduleName), \
		AS_VERSION_MAJOR,AS_VERSION_MINOR, AS_VERSION_RELEASE, AS_VERSION_MODIFY, __DATE__, __TIME__
#else
	#define AS_VERSION_INFO(moduleName) 	("Assist ISDN : %s( version: %s.%s.%s-%s %s %s)\n", (moduleName), \
		AS_VERSION_MAJOR,AS_VERSION_MINOR, AS_VERSION_RELEASE, AS_VERSION_MODIFY, __DATE__, __TIME__)
#endif

#endif
