/*
* $Id: athctrl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
/*
 * Simple Atheros-specific tool to inspect and set atheros specific values
 * athctrl [-i interface] [-d distance]
 * (default interface is ath0).  
 */
#include <sys/types.h>
#include <sys/file.h>

#include <getopt.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>


static int
setsysctrl(const char *dev, const char *control , u_long value)
{
	char buffer[256];
	FILE * fd;

	snprintf(buffer, sizeof(buffer),"/proc/sys/dev/%s/%s",dev,control);
	fd = fopen(buffer, "w");
	if (fd != NULL) {
		fprintf(fd,"%i",value);
	}
	return 0;
}

static void usage(void)
{
    fprintf(stderr,
        "Atheros driver control\n"
        "Copyright (c) 2002-2004 Gunter Burchardt, Local-Web AG\n"
        "\n"
        "usage: athctrl [-i interface] [-d distance]\n"
        "\n"
        "options:\n"
        "   -h   show this usage\n"
		"   -i   interface (default interface is ath0)\n"
        "   -d   specify the maximum distance of a sta or the distance\n"
		"        of the master\n");

    exit(1);
}

int
main(int argc, char *argv[])
{
	char device[5];
	int distance = -1;
	int c;

	strncpy(device, "ath0", sizeof (device));

	for (;;) {
        c = getopt(argc, argv, "d:i:h");
        if (c < 0)
            break;
        switch (c) {
        case 'h':
            usage();
            break;
        case 'd':
			distance = atoi(optarg);
            break;
        case 'i':
			strncpy(device, optarg, sizeof (device));
            break;

        default:
            usage();
            break;
        }
    }

	if(distance >= 0){
        int slottime = 9+(distance/300)+((distance%300)?1:0);
		int acktimeout = slottime*2+3;
		int ctstimeout = slottime*2+3;
		
		printf("Setting distance on interface %s to %i meters\n", device, distance);
		setsysctrl(device, "slottime", slottime);
		setsysctrl(device, "acktimeout", acktimeout);
		setsysctrl(device, "ctstimeout", ctstimeout);
	}else{
		usage();
	}
}
