/*
 * $Id: meshconfig_info.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "meshconfig.h"

/*
 * Get wireless informations & config from the device driver
 * We will call all the classical wireless ioctl on the driver through
 * the socket to know what is supported and to get the settings...
 */
static int __get_info(int devFd, char *ifname, struct meshwu_info *info)
{
	struct meshw_req		wrq;

	memset((char *) info, 0, sizeof(struct meshwu_info));

	/* Get basic information */
	if(meshwu_get_basic_config(devFd, ifname, &(info->b)) < 0)
	{
#if 0	
		/* If no wireless name : no wireless extensions */
		/* But let's check if the interface exists at all */
		struct ifreq ifr;
		strncpy(ifr.ifr_name, ifname, MESHNAMSIZ);
		
		if(ioctl(devFd, SIOCGIFFLAGS, &ifr) < 0)
			return(-ENODEV);
		else
#endif			
			return(-ENOTSUP);
	}

	/* Get ranges */
	if(meshwu_get_range_info(devFd, ifname, &(info->range)) >= 0)
		info->has_range = 1;

	/* Get AP address */
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_AP_MAC, &wrq) >= 0)
	{
		info->has_ap_addr = 1;
		memcpy(&(info->ap_addr), &(wrq.u.ap_addr), sizeof (sockaddr));
	}

	/* Get bit rate */
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RATE, &wrq) >= 0)
	{
		info->has_bitrate = 1;
		memcpy(&(info->bitrate), &(wrq.u.bitrate), sizeof(iwparam));
	}

	/* Get Power Management settings */
	wrq.u.power.flags = 0;
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_POWER_MGMT, &wrq) >= 0)
	{
		info->has_power = 1;
		memcpy(&(info->power), &(wrq.u.power), sizeof(iwparam));
	}

	/* Get stats */
	if(meshwu_get_stats(devFd, ifname, &(info->stats), &info->range, info->has_range) >= 0)
	{
		info->has_stats = 1;
	}

	/* Get NickName */
	wrq.u.essid.pointer = (caddr_t) info->nickname;
	wrq.u.essid.length = MESHW_ESSID_MAX_SIZE + 1;
	wrq.u.essid.flags = 0;
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_NICKNAME, &wrq) >= 0)
		if(wrq.u.data.length > 1)
			info->has_nickname = 1;

	if((info->has_range) && (info->range.we_version_compiled > 9))
	{
		/* Get Transmit Power */
		if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_TX_POW, &wrq) >= 0)
		{
			info->has_txpower = 1;
			memcpy(&(info->txpower), &(wrq.u.txpower), sizeof(iwparam));
		}
	}

	/* Get sensitivity */
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_SENS, &wrq) >= 0)
	{
		info->has_sens = 1;
		memcpy(&(info->sens), &(wrq.u.sens), sizeof(iwparam));
	}

	if((info->has_range) && (info->range.we_version_compiled > 10))
	{
		/* Get retry limit/lifetime */
		if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RETRY, &wrq) >= 0)
		{
			info->has_retry = 1;
			memcpy(&(info->retry), &(wrq.u.retry), sizeof(iwparam));
		}
	}

	/* Get RTS threshold */
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_RTS, &wrq) >= 0)
	{
		info->has_rts = 1;
		memcpy(&(info->rts), &(wrq.u.rts), sizeof(iwparam));
	}

	/* Get fragmentation threshold */
	if(meshwu_get_ext(devFd, ifname, MESHW_IO_R_FRAG_THR, &wrq) >= 0)
	{
		info->has_frag = 1;
		memcpy(&(info->frag), &(wrq.u.frag), sizeof(iwparam));
	}

	return(0);
}

/* Print on the screen in a neat fashion all the info we have collected on a device. */
static void __display_info(struct meshwu_info *info,char *ifname)
{
	char		buffer[128];

	/* One token is more of less 5 characters, 14 tokens per line */
	int	tokens = 3;	/* For name */

	/* Display device name and wireless name (name of the protocol used) */
	printf("%-8.16s  %s  ", ifname, info->b.name);

	/* Display ESSID (extended network), if any */
	if(info->b.has_essid)
	{
		if(info->b.essid_on)
		{
			/* Does it have an ESSID index ? */
			if((info->b.essid_on & MESHW_ENCODE_INDEX) > 1)
				printf("ESSID:\"%s\" [%d]  ", info->b.essid, (info->b.essid_on & MESHW_ENCODE_INDEX));
			else
				printf("ESSID:\"%s\"  ", info->b.essid);
		}
		else
			printf("ESSID:off/any  ");
	}

	/* Display NickName (station name), if any */
	if(info->has_nickname)
		printf("Nickname:\"%s\"", info->nickname);

	/* Formatting */
	if(info->b.has_essid || info->has_nickname)
	{
		printf("\n          ");
		tokens = 0;
	}

	/* Display Network ID */
	if(info->b.has_nwid)
	{/* Note : should display proper number of digits according to info in range structure */
		if(info->b.nwid.disabled)
			printf("NWID:off/any  ");
		else
			printf("NWID:%X  ", info->b.nwid.value);
		tokens +=2;
	}

	/* Display the current mode of operation */
	if(info->b.has_mode)
	{
		printf("Mode:%s  ", meshwu_operation_mode[info->b.mode]);
		tokens +=3;
	}

	if(info->b.has_freq)
	{
		int		channel = -1;
#if __ARM_IXP__
		int		freq = info->b.freq;

		if(info->has_range )
			channel = meshwu_freq_to_channel(freq, &info->range);
//		meshwu_print_freq(buffer, sizeof(buffer), freq, channel, info->b.freq_flags);
		printf("Frequency : %d MHz (Channel : %d) ", freq , channel);
#else
		double		freq = info->b.freq;
		/* Some drivers insist of returning channel instead of frequency.
		* This fixes them up. Note that, driver should still return
		* frequency, because other tools depend on it. */
		if(info->has_range && (freq < KILO))
			channel = meshwu_channel_to_freq((int) freq, &freq, &info->range);
		meshwu_print_freq(buffer, sizeof(buffer), freq, -1, info->b.freq_flags);
		printf("%s  ", buffer);
#endif
		tokens +=4;
	}

	/* Display the address of the current Access Point */
	if(info->has_ap_addr)
	{/* A bit of clever formatting */
		if(tokens > 8)
		{
			printf("\n          ");
			tokens = 0;
		}
		tokens +=6;

		/* Oups ! No Access Point in Ad-Hoc mode */
		if((info->b.has_mode) && (info->b.mode == MESHW_MODE_ADHOC))
			printf("Cell:");
		else
			printf("Access Point:");
		printf(" %s   ", meshwu_sawap_ntop(&info->ap_addr, buffer));
	}

	/* Display the currently used/set bit-rate */
	if(info->has_bitrate)
	{/* A bit of clever formatting */
		if(tokens > 11)
		{
			printf("\n          ");
			tokens = 0;
		}
		tokens +=3;

		/* Display it */
		meshwu_print_bitrate(buffer, sizeof(buffer), info->bitrate.value);
		printf("Bit Rate%c%s   ", (info->bitrate.fixed ? '=' : ':'), buffer);
	}

	/* Display the Transmit Power */
	if(info->has_txpower)
	{/* A bit of clever formatting */
		if(tokens > 11)
		{
			printf("\n          ");
			tokens = 0;
		}
		tokens +=3;

		/* Display it */
		meshwu_print_txpower(buffer, sizeof(buffer), &info->txpower);
		printf("Tx-Power%c%s   ", (info->txpower.fixed ? '=' : ':'), buffer);
	}

	/* Display sensitivity */
	if(info->has_sens)
	{/* A bit of clever formatting */
		if(tokens > 10)
		{
			printf("\n          ");
			tokens = 0;
		}
		tokens +=4;

		/* Fixed ? */
		if(info->sens.fixed)
			printf("Sensitivity=");
		else
			printf("Sensitivity:");

		if(info->has_range)
			/* Display in dBm ? */
			if(info->sens.value < 0)
				printf("%d dBm  ", info->sens.value);
			else
				printf("%d/%d  ", info->sens.value, info->range.sensitivity);
		else
			printf("%d  ", info->sens.value);
	}

	printf("\n          ");
	tokens = 0;

	/* Display retry limit/lifetime information */
	if(info->has_retry)
	{
		printf("Retry");
		/* Disabled ? */
		if(info->retry.disabled)
			printf(":off");
		else
		{/* Let's check the value and its type */
			if(info->retry.flags & MESHW_RETRY_TYPE)
			{
				meshwu_print_retry_value(buffer, sizeof(buffer), info->retry.value, info->retry.flags,
					info->range.we_version_compiled);
				printf("%s", buffer);
			}

			/* Let's check if nothing (simply on) */
			if(info->retry.flags == MESHW_RETRY_ON)
				printf(":on");
		}
		printf("   ");

		tokens += 5;	/* Between 3 and 5, depend on flags */
	}

	/* Display the RTS threshold */
	if(info->has_rts)
	{/* Disabled ? */
		if(info->rts.disabled)
			printf("RTS thr:off   ");
		else
		{/* Fixed ? */
			if(info->rts.fixed)
				printf("RTS thr=");
			else
				printf("RTS thr:");

			printf("%d B   ", info->rts.value);
		}
		tokens += 3;
	}

	/* Display the fragmentation threshold */
	if(info->has_frag)
	{/* A bit of clever formatting */
		if(tokens > 10)
		{
			printf("\n          ");
			tokens = 0;
		}
		tokens +=4;

		/* Disabled ? */
		if(info->frag.disabled)
			printf("Fragment thr:off");
		else
		{ /* Fixed ? */
			if(info->frag.fixed)
				printf("Fragment thr=");
			else
				printf("Fragment thr:");
			printf("%d B   ", info->frag.value);
		}
	}

	/* Formating */
	if(tokens > 0)
		printf("\n          ");

	/* Display encryption information */
	/* Note : we display only the "current" key, use iwlist to list all keys */
	if(info->b.has_key)
	{
		printf("Encryption key:");
		if((info->b.key_flags & MESHW_ENCODE_DISABLED) || (info->b.key_size == 0))
			printf("off");
		else
		{/* Display the key */
			meshwu_print_key(buffer, sizeof(buffer), info->b.key, info->b.key_size, info->b.key_flags);
			printf("%s", buffer);

			/* Other info... */
			if((info->b.key_flags & MESHW_ENCODE_INDEX) > 1)
				printf(" [%d]", info->b.key_flags & MESHW_ENCODE_INDEX);
			if(info->b.key_flags & MESHW_ENCODE_RESTRICTED)
				printf("   Security mode:restricted");
			if(info->b.key_flags & MESHW_ENCODE_OPEN)
				printf("   Security mode:open");
		}
		printf("\n          ");
	}

	/* Display Power Management information */
	/* Note : we display only one parameter, period or timeout. If a device
	* (such as HiperLan) has both, the user need to use iwlist... */
	if(info->has_power)	/* I hope the device has power ;-) */
	{
		printf("Power Management");
		/* Disabled ? */
		if(info->power.disabled)
			printf(":off");
		else
		{/* Let's check the value and its type */
			if(info->power.flags & MESHW_POWER_TYPE)
			{
				meshwu_print_pm_value(buffer, sizeof(buffer), info->power.value, 
					info->power.flags,	info->range.we_version_compiled);
				printf("%s  ", buffer);
			}

			/* Let's check the mode */
			meshwu_print_pm_mode(buffer, sizeof(buffer), info->power.flags);
			printf("%s", buffer);

			/* Let's check if nothing (simply on) */
			if(info->power.flags == MESHW_POWER_ON)
				printf(":on");
		}
		printf("\n          ");
	}

	/* Display statistics */
	if(info->has_stats)
	{
		meshwu_print_stats(buffer, sizeof(buffer), &info->stats.qual, &info->range, info->has_range);
		printf("Link %s\n", buffer);

		if(info->range.we_version_compiled > 11)
		{
			printf("          Rx invalid nwid:%d  Rx invalid crypt:%d  Rx invalid frag:%d\n          Tx excessive retries:%d  Invalid misc:%d   Missed beacon:%d\n",
				info->stats.discard.nwid, info->stats.discard.code,info->stats.discard.fragment,
				info->stats.discard.retries, info->stats.discard.misc, info->stats.miss.beacon);
		}
		else
		{
			printf("          Rx invalid nwid:%d  invalid crypt:%d  invalid misc:%d\n",
				info->stats.discard.nwid, info->stats.discard.code, info->stats.discard.misc);
		}
	}

	printf("\n");
}

int config_print_info(int devFd, char *ifname, char *args[], int count)
{
	struct meshwu_info	info;
	int			rc;

	args = args; count = count;

	rc = __get_info(devFd, ifname, &info);
	switch(rc)
	{
		case 0:	/* Success, Display it ! */
			__display_info(&info, ifname);
			break;

		case -ENOTSUP:
			MESHU_ERR_INFO( "%-8.16s  no wireless extensions.\n\n", ifname);
			break;

		default:
			MESHU_ERR_INFO( "%-8.16s  %s\n\n", ifname, strerror(-rc));
	}
	return(rc);
}

