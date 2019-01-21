/*
 * $Log: as_misc_version.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
 * rebuild
 *
 * Revision 1.4  2005/11/30 07:23:32  wangwei
 * 增加烧录93LC66程序，可以通过条件WITH_93LC66=no选择编译
 *
 * Revision 1.3  2005/09/07 10:02:39  wangwei
 * no message
 *
 * Revision 1.2  2005/06/15 05:05:33  wangwei
 * 版本号修改为 01.2.02-00
 *
 * Revision 1.1  2005/06/07 09:16:21  lizhijie
 * add into CVS
 *
 * $Id: as_misc_version.h,v 1.1.1.1 2006/11/29 09:16:54 lizhijie Exp $
 * Version info for Misc Drivers, Li Zhijie, 2005.06.07 
*/
#ifndef __AS_MISC_VERSION_H__
#define __AS_MISC_VERSION_H__

#define AS_VERSION_MAJOR		"01"  /* major version number ,start from 1 */
#define AS_VERSION_MINOR		"3"	/* minor, from 0 to 9 */
#define AS_VERSION_RELEASE		"00"	/* from 01 to 99 */
#define AS_VERSION_MODIFY		"00"	/* from 00 to 99 */


#define   AS_VERSION_INFO(moduleName)	  moduleName " (version: " \
	AS_VERSION_MAJOR"." AS_VERSION_MINOR"."	AS_VERSION_RELEASE \
	"-" AS_VERSION_MODIFY" " __DATE__ " " __TIME__ ")\r\n"  
	
#endif
