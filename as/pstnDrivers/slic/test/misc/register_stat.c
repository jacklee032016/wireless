
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "as_dev_ctl.h"

#include "as_fxs.h"

void register_dump(int fd)
{
	int x;
	int res;
		struct wcfxs_regs regs;
		int numregs = NUM_REGS;
		memset(&regs, 0, sizeof(regs));
		
		res = ioctl(fd, WCFXS_GET_REGS, &regs);

		if (res) 
		{
			fprintf(stderr, "Unable to get registers on channel : %s\n", strerror(errno) );
		} 
		else 
		{
			for (x=60;x<NUM_REGS;x++) 
			{
				if (regs.direct[x])
					break;
			}
			if (x == NUM_REGS) 
				numregs = 60;
			printf("Direct registers: \n");

			for (x=0;x<numregs;x++) 
			{
				printf("%3d. %02x  ", x, regs.direct[x]);
				if ((x % 8) == 7)
					printf("\n");
			}

			if (numregs == NUM_REGS) 
			{
				printf("\n\nIndirect registers: \n");
				for (x=0;x<NUM_INDIRECT_REGS;x++) 
				{
					printf("%3d. %04x  ", x, regs.indirect[x]);
					if ((x % 6) == 5)
						printf("\n");
				}
			}
			printf("\n\n");
		}
}


void print_volt(int fd)
{
	int res;
	
		struct wcfxs_stats stats;
		res = ioctl(fd, WCFXS_GET_STATS, &stats);
		if (res) 
		{
			fprintf(stderr, "Unable to get stats on channel : %s\n", strerror(errno) );
		} 
		else 
		{
			printf("TIP: %7.4f Volts\n", (float)stats.tipvolt / 1000.0);
			printf("RING: %7.4f Volts\n", (float)stats.ringvolt / 1000.0);
			printf("VBAT: %7.4f Volts\n", (float)stats.batvolt / 1000.0);
		}
}

int main(int argc, char *argv[])
{
	int fd;

	if (argc < 2) 
	{
		fprintf(stderr, "Usage: file_test <astel device> \n");
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) 
	{
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	register_dump( fd);
//	res = ioctl(fd,  ZT_SETLAW, &law );

	print_volt(fd);
	close(fd);
	return 0;
}



