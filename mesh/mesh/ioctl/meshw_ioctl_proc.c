/*
* $Id: meshw_ioctl_proc.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include 		"_meshw_ioctl.h"

#ifdef CONFIG_PROC_FS
/* Print one entry (line) of /proc/net/wireless */
static inline int sprintf_meshw_stats(char *buffer, MESH_DEVICE *dev)
{
	/* Get stats from the driver */
	struct meshw_statistics *stats;
	int size;

	stats = get_wireless_stats(dev);
	if (stats != (struct meshw_statistics *) NULL)
	{
		size = sprintf(buffer,
			       "%6s: %04x  %3d%c  %3d%c  %3d%c  %6d %6d %6d %6d %6d   %6d\n",
			       dev->name,
			       stats->status,
			       stats->qual.qual,
			       stats->qual.updated & 1 ? '.' : ' ',
			       ((__u8) stats->qual.level),
			       stats->qual.updated & 2 ? '.' : ' ',
			       ((__u8) stats->qual.noise),
			       stats->qual.updated & 4 ? '.' : ' ',
			       stats->discard.nwid,
			       stats->discard.code,
			       stats->discard.fragment,
			       stats->discard.retries,
			       stats->discard.misc,
			       stats->miss.beacon);
		stats->qual.updated = 0;
	}
	else
		size = 0;

	return size;
}

/* Print info for /proc/net/wireless (print all entries) */
int meshw_get_info(char * buffer, char **start, off_t offset,
			  int length)
{
	int		len = 0;
	off_t		begin = 0;
	off_t		pos = 0;
	int		size;
	
	struct MESH_DEVICE *dev;

	size = sprintf(buffer,
		       "Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE\n"
		       " face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | \n" );
	
	pos += size;
	len += size;

#if 0
	read_lock(&dev_base_lock);
	for (dev = dev_base; dev != NULL; dev = dev->next)
	{
		size = sprintf_meshw_stats(buffer + len, dev);
		len += size;
		pos = begin + len;

		if (pos < offset)
		{
			len = 0;
			begin = pos;
		}
		if (pos > offset + length)
			break;
	}
	read_unlock(&dev_base_lock);
#endif

	*start = buffer + (offset - begin);	/* Start of wanted data */
	len -= (offset - begin);		/* Start slop */
	if (len > length)
		len = length;			/* Ending slop */
	if (len < 0)
		len = 0;

	return len;
}
#endif	/* CONFIG_PROC_FS */


