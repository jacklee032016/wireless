/* 
 * $Log: watchdog_test.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
 * rebuild
 *
 * Revision 1.2  2005/06/07 09:27:25  lizhijie
 * build enable/disable/update 3 programs
 *
 * Revision 1.1  2005/06/07 09:16:21  lizhijie
 * add into CVS
 *
 * $Id: watchdog_test.c,v 1.1.1.1 2006/11/29 09:16:54 lizhijie Exp $
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

AS_WATCHDOG_COMMAND cmd_enable =
{
	type			:	AS_WATCHDOG_ENABLE,
	value		:	AS_IXP_WATCHDOG_DEFAULT_COUNT	
};

AS_WATCHDOG_COMMAND cmd_update =
{
	type			:	AS_WATCHDOG_UPDATE,
	value		:	AS_IXP_WATCHDOG_DEFAULT_COUNT	
};

AS_WATCHDOG_COMMAND cmd_disable =
{
	type			:	AS_WATCHDOG_DISABLE,
	value		:	AS_IXP_WATCHDOG_DEFAULT_COUNT	
};



int main(int argc, char *argv[])
{
	int fd;
	char str[128];
	int res = 0;

	sprintf(str, "/proc/%s/%s",  AS_MISC_PROC_DIR_NAME , AS_MISC_PROC_WATCHDOG);
	fd = open(str, O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", str, strerror(errno));
		exit(1);
	}

#if TEST_ENABLE
	res = write(fd, &cmd_enable, sizeof(AS_WATCHDOG_COMMAND) );
#endif

#if TEST_DISBALE
	res = write(fd, &cmd_disable, sizeof(AS_WATCHDOG_COMMAND) );
#endif

#if TEST_UPDATE
	res = write(fd, &cmd_update, sizeof(AS_WATCHDOG_COMMAND) );
#endif
	if(res < 0)
	{
		fprintf(stderr, "Write %s failed : %s\n", str, strerror(errno));
	}
	
	close(fd);
	return 0;
}


