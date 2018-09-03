/*
* $Id: athchans.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
/*
 * athchans [-i interface] chan ...
 * (default interface is ath0).
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>

#include <stdio.h>
#include <ctype.h>
#include <getopt.h>

#include "_ieee80211.h"
#include "ieee80211.h"
#include "ieee80211_crypto.h"
#include "ieee80211_ioctl.h"

static	int s = -1;
const char *progname;

static void
checksocket()
{
	if (s < 0 ? (s = socket(AF_INET, SOCK_DGRAM, 0)) == -1 : 0)
		perror("socket(SOCK_DRAGM)");
}

static int
set80211priv(const char *dev, int op, void *data, int len, int show_err)
{
	struct meshwreq iwr;

	checksocket();

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, dev, IFNAMSIZ);
	if (len < IFNAMSIZ) {
		/*
		 * Argument data fits inline; put it there.
		 */
		memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(s, op, &iwr) < 0) {
		if (show_err) {
			static const char *opnames[] = {
				"ioctl[IEEE80211_IOCTL_SETPARAM]",
				"ioctl[IEEE80211_IOCTL_GETPARAM]",
				"ioctl[IEEE80211_IOCTL_SETKEY]",
				"ioctl[IEEE80211_IOCTL_GETKEY]",
				NULL,
				"ioctl[IEEE80211_IOCTL_DELKEY]",
				NULL,
				"ioctl[IEEE80211_IOCTL_MLME]",
				"ioctl[IEEE80211_IOCTL_SETOPTIE]",
				"ioctl[IEEE80211_IOCTL_GETOPTIE]",
				"ioctl[IEEE80211_IOCTL_ADDMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_DELMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_CHANLIST]",
			};
			if (IEEE80211_IOCTL_SETPARAM <= op &&
			    op <= IEEE80211_IOCTL_CHANLIST)
				perror(opnames[op - MESHW_IO_FIRSTPRIV]);
			else
				perror("ioctl[unknown???]");
		}
		return -1;
	}
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-i device] channels...\n", progname);
	exit(-1);
}

#define	MAXCHAN	(sizeof(struct ieee80211req_chanlist) * NBBY)
int
main(int argc, char *argv[])
{
	const char *ifname = "ath0";
	struct ieee80211req_chanlist chanlist;
	const char *cp;
	int c;

	progname = argv[0];
	while ((c = getopt(argc, argv, "i:")) != -1)
		switch (c) {
		case 'i':
			ifname = optarg;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();

	memset(&chanlist, 0, sizeof(chanlist));
	for (; argc > 0; argc--, argv++) {
		int first, last, f;

		switch (sscanf(argv[0], "%u-%u", &first, &last)) {
		case 1:
			if (first > MAXCHAN)
				errx(-1, "%s: channel %u out of range, max %u",
					progname, first, MAXCHAN);
			setbit(chanlist.ic_channels, first);
			break;
		case 2:
			if (first > MAXCHAN)
				errx(-1, "%s: channel %u out of range, max %u",
					progname, first, MAXCHAN);
			if (last > MAXCHAN)
				errx(-1, "%s: channel %u out of range, max %u",
					progname, last, MAXCHAN);
			if (first > last)
				errx(-1, "%s: void channel range, %u > %u",
					progname, first, last);
			for (f = first; f <= last; f++)
				setbit(chanlist.ic_channels, f);
			break;
		}
	}
	return set80211priv(ifname, IEEE80211_IOCTL_CHANLIST,
		&chanlist, sizeof(chanlist), 1);
}
