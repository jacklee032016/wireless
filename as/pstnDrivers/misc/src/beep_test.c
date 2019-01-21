/* 
 * $Log: beep_test.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
 * rebuild
 *
 * Revision 1.1  2005/06/15 05:49:44  wangwei
 * 增加蜂鸣器用户程序接口测试
 *
 * Revision 1.2  2005/06/07 09:27:25  lizhijie
 * build enable/disable/update 3 programs
 *
 * Revision 1.1  2005/06/07 09:16:21  lizhijie
 * add into CVS
 *
 * $Id: beep_test.c,v 1.1.1.1 2006/11/29 09:16:54 lizhijie Exp $
 * Test program driver for IXP4xx watchDog Chip 
 * watchdog_test.c : interface for dynamic operation for WatchDog chip in IXP422 
 * Li Zhijie, 2005.06.07
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_misc_dev.h"

AS_BEEP_COMMAND beep_enable =
{
	type			:	AS_BEEP_ENABLE,
	value		:	0	
};

AS_BEEP_COMMAND beep_disable =
{
	type			:	AS_BEEP_DISABLE,
	value		:	0	
};

int main(int argc, char *argv[])
{
	int fd;
	char str[128];
	int res = 0;

	sprintf(str, "/proc/%s/%s",  AS_MISC_PROC_DIR_NAME , AS_MISC_PROC_BEEP);
	fd = open(str, O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", str, strerror(errno));
		exit(1);
	}

#if TEST_ENABLE
	res = write(fd, &beep_enable, sizeof(AS_BEEP_COMMAND) );
#endif

#if TEST_DISBALE
	res = write(fd, &beep_disable, sizeof(AS_BEEP_COMMAND) );
#endif

	if(res < 0)
	{
		fprintf(stderr, "Write %s failed : %s\n", str, strerror(errno));
	}
	close(fd);
	return 0;
}
