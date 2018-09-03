/*
* $Id: phy_rate_sample.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef	__PHY_RATE_SAMPLE_H__
#define	__PHY_RATE_SAMPLE_H__

/* per-device state */
struct sample_softc
{
	struct ath_ratectrl		arc;						/* base state */
	int					ath_smoothing_rate;		/* ewma percentage (out of 100) */
	int					ath_sample_rate;        	/* send a different bit-rate 1/X packets */

#ifdef CONFIG_SYSCTL
	struct ctl_table_header *sysctl_header;
	struct ctl_table		*sysctls;
#endif

};
#define ATH_SOFTC_SAMPLE(sc)    ((struct sample_softc *)sc->sc_rc)

struct rate_info
{
	int		rate;
	int		rix;
	int		rateCode;
	int		shortPreambleRateCode;
};


struct rate_stats
{	
	unsigned average_tx_time;
	int successive_failures;
	int tries;
	int total_packets;
	int packets_acked;
	unsigned perfect_tx_time; /* transmit time for 0 retries */
	int last_tx;
};


/* for now, we track performance for three different packet size buckets */
#define NUM_PACKET_SIZE_BINS 3
static int packet_size_bins[NUM_PACKET_SIZE_BINS] = {250, 1600, 3000};

/* per-node state */
struct sample_node
{
	int static_rate_ndx;
	int num_rates;

	struct rate_info rates[IEEE80211_RATE_MAXSIZE];
	
	struct rate_stats stats[NUM_PACKET_SIZE_BINS][IEEE80211_RATE_MAXSIZE];
	int last_sample_ndx[NUM_PACKET_SIZE_BINS];

	int current_sample_ndx[NUM_PACKET_SIZE_BINS];       
	int packets_sent[NUM_PACKET_SIZE_BINS];

	int current_rate[NUM_PACKET_SIZE_BINS];
	int packets_since_switch[NUM_PACKET_SIZE_BINS];
	unsigned jiffies_since_switch[NUM_PACKET_SIZE_BINS];

	int packets_since_sample[NUM_PACKET_SIZE_BINS];
	unsigned sample_tt[NUM_PACKET_SIZE_BINS];
};
#define	ATH_NODE_SAMPLE(an)	((struct sample_node *)&an[1])


#ifndef MIN
#define MIN(a,b)        ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b)        ((a) > (b) ? (a) : (b))
#endif

#define WIFI_CW_MIN 31
#define WIFI_CW_MAX 1023

struct ar5212_desc {
	/*
	 * tx_control_0
	 */
	u_int32_t	frame_len:12;
	u_int32_t	reserved_12_15:4;
	u_int32_t	xmit_power:6;
	u_int32_t	rts_cts_enable:1;
	u_int32_t	veol:1;
	u_int32_t	clear_dest_mask:1;
	u_int32_t	ant_mode_xmit:4;
	u_int32_t	inter_req:1;
	u_int32_t	encrypt_key_valid:1;
	u_int32_t	cts_enable:1;

	/*
	 * tx_control_1
	 */
	u_int32_t	buf_len:12;
	u_int32_t	more:1;
	u_int32_t	encrypt_key_index:7;
	u_int32_t	frame_type:4;
	u_int32_t	no_ack:1;
	u_int32_t	comp_proc:2;
	u_int32_t	comp_iv_len:2;
	u_int32_t	comp_icv_len:2;
	u_int32_t	reserved_31:1;

	/*
	 * tx_control_2
	 */
	u_int32_t	rts_duration:15;
	u_int32_t	duration_update_enable:1;
	u_int32_t	xmit_tries0:4;
	u_int32_t	xmit_tries1:4;
	u_int32_t	xmit_tries2:4;
	u_int32_t	xmit_tries3:4;

	/*
	 * tx_control_3
	 */
	u_int32_t	xmit_rate0:5;
	u_int32_t	xmit_rate1:5;
	u_int32_t	xmit_rate2:5;
	u_int32_t	xmit_rate3:5;
	u_int32_t	rts_cts_rate:5;
	u_int32_t	reserved_25_31:7;

	/*
	 * tx_status_0
	 */
	u_int32_t	frame_xmit_ok:1;
	u_int32_t	excessive_retries:1;
	u_int32_t	fifo_underrun:1;
	u_int32_t	filtered:1;
	u_int32_t	rts_fail_count:4;
	u_int32_t	data_fail_count:4;
	u_int32_t	virt_coll_count:4;
	u_int32_t	send_timestamp:16;

	/*
	 * tx_status_1
	 */
	u_int32_t	done:1;
	u_int32_t	seq_num:12;
	u_int32_t	ack_sig_strength:8;
	u_int32_t	final_ts_index:2;
	u_int32_t	comp_success:1;
	u_int32_t	xmit_antenna:1;
	u_int32_t	reserved_25_31_x:7;
} __packed;

static __inline int size_to_bin(int size) 
{
	int x = 0;
	for (x = 0; x < NUM_PACKET_SIZE_BINS; x++)
	{
		if (size <= packet_size_bins[x])
		{
			return x;
		}
	}
	return NUM_PACKET_SIZE_BINS-1;
}

static __inline int bin_to_size(int index)
{
	return packet_size_bins[index];
}

static __inline int rate_to_ndx(struct sample_node *sn, int rate)
{
	int x = 0;
	for (x = 0; x < sn->num_rates; x++)
	{
		if (sn->rates[x].rate == rate)
		{
			return x;
		}      
	}
	return -1;
}

extern	int ath_smoothing_rate;
extern	int ath_sample_rate;

extern	char *phyRateSampleVersion;
extern	char *phyRateSampleDevInfo;

#endif

