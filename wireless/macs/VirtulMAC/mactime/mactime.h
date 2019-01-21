/*
 * softmac_mactime.h
 * VMAC functions for handling timed packet sending, e.g. TDMA
 */

/*
 * A client of the MACTime module is required to keep some state
 * around, i.e. the following structure, and hand it over to 
 * the MACTIME routine when making a request.
 */
typedef struct
{
	vmac_phy_t* 		phyinfo;
	u_int32_t 		tdma_slotlen;
	u_int32_t 		tdma_slotcount;
	u_int32_t 		tdma_myslot;
	u_int32_t 		tdma_guardtime;
} mactime_state_t;

/*
 * Indicates the current tdma slot status. Return values have following
 * semantics:
 *
 * >0 -> the time until the next slot arrives
 * <=0 -> -(time left in the current slot)
 */
int32_t vmac_tdma_slotstatus(mactime_state_t* mts, u_int32_t* pcurslot);

/* How long until the next transmission slot begins?  */
int32_t vmac__tdma_time2nextslot(mactime_state_t* mts);

/*
 * How long until we can transmit the packet? Return value >=0 means
 * time until we can transmit, <0 means that the packet is too big
 * to fit in the timeslot, i.e. we'll never be able to send it.
 */
int32_t vmac_tdma_xmitwhen(mactime_state_t* mts, struct sk_buff* packet);

