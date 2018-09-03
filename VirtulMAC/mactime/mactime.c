/*
 * softmac_mactime.c
 * VMAC functions for handling timed packet sending, e.g.
 * choreographed packet patterns or TDMA
 */
#include <linux/module.h>
#if __ARM__
#else
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include "vmac.h"
#include "mactime.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Neufeld");

/* The MACTime module presents sendpacket functions to be used by  the client MAC layer. */

/*
 * >0 -> time until slot arrives
 * <=0 -> -(time left in slot)
 */
int32_t vmac_tdma_slotstatus(mactime_state_t* mts,  u_int32_t* pcurslot)
{
	int32_t result = 0;

	if (mts && mts->phyinfo && mts->phyinfo->phy_get_time)
	{
		u_int32_t now = mts->phyinfo->phy_get_time(mts->phyinfo->phy_private);
		u_int32_t slotcount = (now/mts->tdma_slotlen);
		u_int32_t curslot = mts->tdma_slotcount;
		u_int64_t curslotstart = slotcount * mts->tdma_slotlen;
		u_int64_t curslotend = curslotstart + mts->tdma_slotlen;
		int32_t slotdistance = mts->tdma_myslot - curslot;

		if (pcurslot) 
			*pcurslot = slotcount;

		while (0 > slotdistance)
		{
			slotdistance += mts->tdma_slotcount;
		}

		if (0 == slotdistance)
		{ // We're in our slot -- return negative the distance to the end of the slot.
			result = -(curslotend - now);
		}
		else
		{
			// We're not in our slot -- return the distance to the end of the 
			// current slot + the time in the remaining slots
			result = (curslotend - now) + (slotdistance-1)*mts->tdma_slotlen; 
		}
	}
	else
	{
		printk(KERN_ALERT "MACTime tdma_slotstatus: null data\n");
	}

	return result;
}

int32_t vmac_tdma_time2nextslot(mactime_state_t* mts)
{
	int32_t result = -1;

	if (mts && mts->phyinfo && mts->phyinfo->phy_get_time)
	{
		u_int32_t now = mts->phyinfo->phy_get_time(mts->phyinfo->phy_private);
		u_int32_t slotcount = (now/mts->tdma_slotlen);
		u_int32_t curslot = slotcount % mts->tdma_slotcount;
		u_int64_t curslotstart = curslot * mts->tdma_slotlen;
		u_int64_t curslotend = curslotstart + mts->tdma_slotlen;
		int32_t slotdistance = mts->tdma_myslot - curslot;

		while (0 > slotdistance)
		{
			slotdistance += mts->tdma_slotcount;
		}

		// Return the distance to the end of the current slot + the time in the remaining slots
		result = (curslotend - now) + (slotdistance-1)*mts->tdma_slotlen; 
	}
	else
	{
		printk(KERN_ALERT "MACTime tdma_timetonextslot: null data\n");
	}

	return result;
}

/*
 * How long until we can transmit the packet? Return value >=0 means
 * time until we can transmit, <0 means that the packet is too big
 * to fit in the timeslot, i.e. we'll never be able to send it.
 */
int32_t vmac_tdma_xmitwhen(mactime_state_t* mts, struct sk_buff* packet)
{
	int32_t result = 0;
	if (mts && mts->phyinfo && mts->phyinfo->phy_get_duration &&
		mts->phyinfo->phy_get_txlatency)
	{
		vmac_phy_handle phyhandle = mts->phyinfo->phy_private;
		int32_t slotstat = vmac_tdma_slotstatus(mts,0);
		int32_t txlat = (mts->phyinfo->phy_get_txlatency)(phyhandle);
		int32_t pktlen = (mts->phyinfo->phy_get_duration)(phyhandle,packet);
		int32_t maxtxlen = mts->tdma_slotlen - 2*mts->tdma_guardtime;

		if (pktlen > maxtxlen)
		{ /* Not feasible to EVER transmit packet in this slot length */
			result = maxtxlen - pktlen;
		}
		else if (0 > slotstat)
		{
			/* We're in our slot. We can transmit now if we're at least
			* (guard time - lead time) into the slot and
			* there's at least (packet length + guard time + tx latency)
			* time left in the slot, otherwise we have to wait for
			* our next slot to begin.       */
			int32_t timefromstart = mts->tdma_slotlen + slotstat;
			int32_t timeleft = -slotstat;

			if ( (timefromstart >= (mts->tdma_guardtime - txlat)) &&
				(timeleft >= (pktlen + mts->tdma_guardtime + txlat)) )
			{
				result = 0;
			}
			else
			{
				/* Not enough time left, figure out how long we have to
				* wait to catch the next slot.	 */
				result = vmac__tdma_time2nextslot(mts) + mts->tdma_guardtime - txlat;
			}
		}

		else
		{    /* We aren't in our slot, wait until our next slot comes up */
			result = slotstat + mts->tdma_guardtime - txlat;
		}
	}

	return result;
}

static int __init __vmac_mactime_init(void)
{
	printk(KERN_ALERT "Loading VMAC MACTime module\n");
	return 0;
}

static void __exit __vmac_mactime_exit(void)
{
	printk(KERN_ALERT "Unloading VMAC MACTime module\n");
}

EXPORT_SYMBOL(vmac_tdma_slotstatus);
EXPORT_SYMBOL(vmac_tdma_time2nextslot);

module_init( __vmac_mactime_init);
module_exit( __vmac_mactime_exit);

