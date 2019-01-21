#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_misc_dev.h"

AS_93LC66_COMMAND m93lc66_write =
{
	type			:	AS_93LC66_WRITE,
	value		:	0	
};

AS_93LC66_COMMAND m93lc66_read =
{
	type			:	AS_93LC66_READ,
	value		:	0	
};

int main(int argc, char *argv[])
{
	int fd;
	char str[128];
	int res = 0;

	sprintf(str, "/proc/%s/%s",  AS_MISC_PROC_DIR_NAME , AS_MISC_PROC_93LC66);
	fd = open(str, O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", str, strerror(errno));
		exit(1);
	}

//#if TEST_ENABLE
	res = write(fd, &m93lc66_write, sizeof(AS_93LC66_COMMAND) );
//#endif

//#if TEST_DISBALE
	res = write(fd, &m93lc66_read, sizeof(AS_93LC66_COMMAND) );
//#endif

	if(res < 0)
	{
		fprintf(stderr, "Write %s failed : %s\n", str, strerror(errno));
	}
	close(fd);
	return 0;
}


