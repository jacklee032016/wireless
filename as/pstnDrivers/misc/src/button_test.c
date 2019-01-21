#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_misc_dev.h"

AS_BUTTON_COMMAND button_state =
{
	type			:	AS_BUTTON_STATE,
	value		:	0	
};

int main(int argc, char *argv[])
{
	int fd;
	char str[128];
	int res = 0;

	sprintf(str, "/proc/%s/%s",  AS_MISC_PROC_DIR_NAME , AS_MISC_PROC_BUTTON);
	fd = open(str, O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", str, strerror(errno));
		exit(1);
	}

//#if TEST_ENABLE
	res = write(fd, &button_state, sizeof(AS_BEEP_COMMAND) );
//#endif

//#if TEST_DISBALE
	//res = write(fd, &beep_disable, sizeof(AS_BEEP_COMMAND) );
//#endif

	if(res < 0)
	{
		fprintf(stderr, "Write %s failed : %s\n", str, strerror(errno));
	}
	close(fd);
	return 0;
}

