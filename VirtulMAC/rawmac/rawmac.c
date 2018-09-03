
/* This is RawMAC.  rawmac provides athraw compatibility. */

#include <linux/module.h>
#if __ARM__
#include <asm/uaccess.h>
#else
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include "vmac.h"
#include "vmac_ath.h"
#include "vnetif.h"

MODULE_LICENSE("GPL");

struct rawmac_inst_proc_data;

/* private instance data */
struct rawmac_instance
{
	vmac_mac_t 		*macinfo;
	vmac_mac_t 		*macinfo_real;
	vmac_phy_t 		*phyinfo;
	vmac_phy_t 		*phyinfo_fake;

	vnet_handle 		*netif;
	mac_rx_func_t 	netif_rx;
	void 			*netif_rx_priv;

	int id;
	int encap_type; // ARPHRD_IEEE80211*

	struct proc_dir_entry *procfs_dir;
	struct list_head procfs_data;

	rwlock_t lock;
};

#define RAWMAC_ENCAP_NONE 					0
#define RAWMAC_ENCAP_RADIOTAP 				1

/*
 * radiotap stuff pulled from madwifi
 */

/* XXX tcpdump/libpcap do not tolerate variable-length headers,
 * yet, so we pad every radiotap header to 64 bytes. Ugh.
 */
#define IEEE80211_RADIOTAP_HDRLEN				64

/* The radio capture header precedes the 802.11 header. */
struct ieee80211_radiotap_header
{
	u_int8_t	it_version;	/* Version 0. Only increases
					 * for drastic changes,
					 * introduction of compatible
					 * new fields does not count.
					 */
	u_int8_t	it_pad;
	u_int16_t       it_len;         /* length of the whole
					 * header in bytes, including
					 * it_version, it_pad,
					 * it_len, and data fields.
					 */
	u_int32_t       it_present;     /* A bitmap telling which
					 * fields are present. Set bit 31
					 * (0x80000000) to extend the
					 * bitmap by another 32 bits.
					 * Additional extensions are made
					 * by setting bit 31.
					 */
} __attribute__((__packed__));

/* Name                                 Data type       Units
 * ----                                 ---------       -----
 *
 * IEEE80211_RADIOTAP_TSFT              u_int64_t       microseconds
 *
 *      Value in microseconds of the MAC's 64-bit 802.11 Time
 *      Synchronization Function timer when the first bit of the
 *      MPDU arrived at the MAC. For received frames, only.
 *
 * IEEE80211_RADIOTAP_CHANNEL           2 x u_int16_t   MHz, bitmap
 *
 *      Tx/Rx frequency in MHz, followed by flags (see below).
 *
 * IEEE80211_RADIOTAP_FHSS              u_int16_t       see below
 *
 *      For frequency-hopping radios, the hop set (first byte)
 *      and pattern (second byte).
 *
 * IEEE80211_RADIOTAP_RATE              u_int8_t        500kb/s
 *
 *      Tx/Rx data rate
 *
 * IEEE80211_RADIOTAP_DBM_ANTSIGNAL     int8_t          decibels from
 *                                                      one milliwatt (dBm)
 *
 *      RF signal power at the antenna, decibel difference from
 *      one milliwatt.
 *
 * IEEE80211_RADIOTAP_DBM_ANTNOISE      int8_t          decibels from
 *                                                      one milliwatt (dBm)
 *
 *      RF noise power at the antenna, decibel difference from one
 *      milliwatt.
 *
 * IEEE80211_RADIOTAP_DB_ANTSIGNAL      u_int8_t        decibel (dB)
 *
 *      RF signal power at the antenna, decibel difference from an
 *      arbitrary, fixed reference.
 *
 * IEEE80211_RADIOTAP_DB_ANTNOISE       u_int8_t        decibel (dB)
 *
 *      RF noise power at the antenna, decibel difference from an
 *      arbitrary, fixed reference point.
 *
 * IEEE80211_RADIOTAP_LOCK_QUALITY      u_int16_t       unitless
 *
 *      Quality of Barker code lock. Unitless. Monotonically
 *      nondecreasing with "better" lock strength. Called "Signal
 *      Quality" in datasheets.  (Is there a standard way to measure
 *      this?)
 *
 * IEEE80211_RADIOTAP_TX_ATTENUATION    u_int16_t       unitless
 *
 *      Transmit power expressed as unitless distance from max
 *      power set at factory calibration.  0 is max power.
 *      Monotonically nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DB_TX_ATTENUATION u_int16_t       decibels (dB)
 *
 *      Transmit power expressed as decibel distance from max power
 *      set at factory calibration.  0 is max power.  Monotonically
 *      nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DBM_TX_POWER      int8_t          decibels from
 *                                                      one milliwatt (dBm)
 *
 *      Transmit power expressed as dBm (decibels from a 1 milliwatt
 *      reference). This is the absolute power level measured at
 *      the antenna port.
 *
 * IEEE80211_RADIOTAP_FLAGS             u_int8_t        bitmap
 *
 *      Properties of transmitted and received frames. See flags
 *      defined below.
 *
 * IEEE80211_RADIOTAP_ANTENNA           u_int8_t        antenna index
 *
 *      Unitless indication of the Rx/Tx antenna for this packet.
 *      The first antenna is antenna 0.
 *
 * IEEE80211_RADIOTAP_RX_FLAGS          u_int16_t       bitmap
 *
 *	Properties of received frames. See flags defined below.
 *
 * IEEE80211_RADIOTAP_TX_FLAGS          u_int16_t       bitmap
 *
 *	Properties of transmitted frames. See flags defined below.
 *
 * IEEE80211_RADIOTAP_RTS_RETRIES       u_int8_t        data
 *
 *	Number of rts retries a transmitted frame used.
 * 
 * IEEE80211_RADIOTAP_DATA_RETRIES      u_int8_t        data
 *
 *	Number of unicast retries a transmitted frame used.
 * 
 */
enum ieee80211_radiotap_type
{
	IEEE80211_RADIOTAP_TSFT 					= 0,
	IEEE80211_RADIOTAP_FLAGS 				= 1,
	IEEE80211_RADIOTAP_RATE 					= 2,
	IEEE80211_RADIOTAP_CHANNEL 				= 3,
	IEEE80211_RADIOTAP_FHSS 					= 4,
	IEEE80211_RADIOTAP_DBM_ANTSIGNAL 		= 5,
	IEEE80211_RADIOTAP_DBM_ANTNOISE 		= 6,
	IEEE80211_RADIOTAP_LOCK_QUALITY 		= 7,
	IEEE80211_RADIOTAP_TX_ATTENUATION 		= 8,
	IEEE80211_RADIOTAP_DB_TX_ATTENUATION 	= 9,
	IEEE80211_RADIOTAP_DBM_TX_POWER 		= 10,
	IEEE80211_RADIOTAP_ANTENNA 				= 11,
	IEEE80211_RADIOTAP_DB_ANTSIGNAL 		= 12,
	IEEE80211_RADIOTAP_DB_ANTNOISE 			= 13,
	IEEE80211_RADIOTAP_RX_FLAGS 				= 14,
	IEEE80211_RADIOTAP_TX_FLAGS 				= 15,
	IEEE80211_RADIOTAP_RTS_RETRIES 			= 16,
	IEEE80211_RADIOTAP_DATA_RETRIES 		= 17,
	IEEE80211_RADIOTAP_EXT 					= 31
};

/* Radio capture format. */
#define ATH_RX_RADIOTAP_PRESENT (		\
	(1 << IEEE80211_RADIOTAP_FLAGS)		| \
	(1 << IEEE80211_RADIOTAP_RATE)		| \
	(1 << IEEE80211_RADIOTAP_CHANNEL)	| \
	(1 << IEEE80211_RADIOTAP_ANTENNA)	| \
	(1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL)	| \
	(1 << IEEE80211_RADIOTAP_RX_FLAGS)	| \
	0)

struct ath_rx_radiotap_header
{
	struct ieee80211_radiotap_header wr_ihdr;
	u_int8_t	wr_flags;		/* XXX for padding */
	u_int8_t	wr_rate;
	u_int16_t	wr_chan_freq;
	u_int16_t	wr_chan_flags;
	u_int8_t	wr_antenna;
	u_int8_t	wr_antsignal;
	u_int16_t       wr_rx_flags;
};

#define	IEEE80211_RADIOTAP_F_FCS       			0x10		/* frame includes FCS */
#define	IEEE80211_RADIOTAP_F_RX_BADFCS  		0x0001      	/* frame failed crc check */

#define 	NUM_RADIOTAP_ELEMENTS 				18

static int radiotap_elem_to_bytes[NUM_RADIOTAP_ELEMENTS] = 
{
	 8, /* IEEE80211_RADIOTAP_TSFT */
	 1, /* IEEE80211_RADIOTAP_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RATE */
	 4, /* IEEE80211_RADIOTAP_CHANNEL */
	 2, /* IEEE80211_RADIOTAP_FHSS */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_LOCK_QUALITY */
	 2, /* IEEE80211_RADIOTAP_TX_ATTENUATION */
	 2, /* IEEE80211_RADIOTAP_DB_TX_ATTENUATION */
	 1, /* IEEE80211_RADIOTAP_DBM_TX_POWER */
	 1, /* IEEE80211_RADIOTAP_ANTENNA */
	 1, /* IEEE80211_RADIOTAP_DB_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DB_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_RX_FLAGS */
	 2, /* IEEE80211_RADIOTAP_TX_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RTS_RETRIES */
	 1, /* IEEE80211_RADIOTAP_DATA_RETRIES */
};


/*
 * the following rt_* functions deal with verifying that a valid
 * radiotap header is on a packet as well as functions to extracting
 * what information is included.
 * XXX maybe these should go in ieee_radiotap.c
 */
static int rt_el_present(struct ieee80211_radiotap_header *th, u_int32_t element)
{
	if (element > NUM_RADIOTAP_ELEMENTS)
		return 0;
	return th->it_present & (1 << element);
}

static int rt_check_header(struct ieee80211_radiotap_header *th, int len) 
{
	int bytes = 0;
	int x = 0;
	if (th->it_version != 0) 
		return 0;

	if (th->it_len < sizeof(struct ieee80211_radiotap_header))
		return 0;
	
	for (x = 0; x < NUM_RADIOTAP_ELEMENTS; x++)
	{
		if (rt_el_present(th, x))
		    bytes += radiotap_elem_to_bytes[x];
	}

	if (th->it_len < sizeof(struct ieee80211_radiotap_header) + bytes) 
		return 0;
	
	if (th->it_len > len)
		return 0;

	return 1;
}

static u_int8_t *rt_el_offset(struct ieee80211_radiotap_header *th, u_int32_t element)
{
	unsigned int x = 0;
	u_int8_t *offset = ((u_int8_t *) th) + sizeof(struct ieee80211_radiotap_header);
	for (x = 0; x < NUM_RADIOTAP_ELEMENTS && x < element; x++)
	{
		if (rt_el_present(th, x))
			offset += radiotap_elem_to_bytes[x];
	}

	return offset;
}

#ifndef ARPHRD_IEEE80211_RADIOTAP
#define ARPHRD_IEEE80211_RADIOTAP 803 /* IEEE 802.11 + radiotap header */
#endif /* ARPHRD_IEEE80211_RADIOTAP */

/*
 * end radiotap stuff pulled from madwifi
 */

static int rawmac_next_id;
static const char *rawmac_name = "rawmac";

/**
 * Every VMAC MAC or PHY layer provides a vmac_layer_t interface
 */
static vmac_layer_t the_rawmac;

struct rawmac_inst_proc_data
{
	struct list_head list;
	struct rawmac_instance *inst;
	int id;
	char name[VMAC_NAME_SIZE];
	struct proc_dir_entry *parent;
};

enum
{
	RAWMAC_INST_PROC_MAC,
	RAWMAC_INST_PROC_TYPE
};

struct rawmac_inst_proc_entry
{
	const char *name;
	mode_t mode;
	int id;
};

static const struct rawmac_inst_proc_entry rawmac_inst_proc_entries[] =
{
	{
		"mac",
		0644,
		RAWMAC_INST_PROC_MAC
	},

	{
		"type",
		0644,
		RAWMAC_INST_PROC_TYPE
	},

	/* terminator, don't remove */
	{
		0,
		0,
		-1
	},
};

static void rawmac_encapsulate_mac(void *me, char *mac_instance_name);

static int rawmac_inst_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data) 
{
	int result = 0;
	struct rawmac_inst_proc_data* procdata = data;
	
	if (procdata && procdata->inst)
	{
		struct rawmac_instance* inst = procdata->inst;
		char* dest = (page + off);
		int intval = 0;

		switch (procdata->id)
		{
			case RAWMAC_INST_PROC_TYPE:
				read_lock(&(inst->lock));
				switch (inst->encap_type)
				{
					case ARPHRD_ETHER:
						intval = 0;
						break;
					/*
					case ARPHRD_IEEE80211_PRISM:
						intval = 1;
						break;
					*/
					case ARPHRD_IEEE80211_RADIOTAP:
						intval = 2;
						break;
				}
				read_unlock(&(inst->lock));
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case RAWMAC_INST_PROC_MAC:
				read_lock(&(inst->lock));
				if (inst->macinfo_real)
					result = snprintf(dest,count, "%s\n", inst->macinfo_real->name);
				read_unlock(&(inst->lock));
				*eof = 1;
				break;

			default:
				result = 0;
				*eof = 1;
				break;
		}
	}

	return result;
}

static int rawmac_inst_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int result = 0;
	struct rawmac_inst_proc_data* procdata = data;
	unsigned long flags;

	if (procdata && procdata->inst)
	{
		struct rawmac_instance* inst = procdata->inst;
		static const int maxkdatalen = 256;
		char kdata[maxkdatalen];
		char* endp = 0;
		long intval = 0;

		/* Drag the data over into kernel land	 */
		if (maxkdatalen <= count)
		{
			copy_from_user(kdata,buffer,(maxkdatalen-1));
			kdata[maxkdatalen-1] = 0;
			result = (maxkdatalen-1);
		}
		else
		{
			copy_from_user(kdata,buffer,count);
			result = count;
		}

		switch (procdata->id)
		{
			case RAWMAC_INST_PROC_TYPE:
			{
					struct net_device *dev;
					intval = simple_strtol(kdata, &endp, 10);
					write_lock_irqsave(&(inst->lock), flags);
					switch (intval)
					{
						case 0:
							inst->encap_type = ARPHRD_ETHER;
							break;
							/*
						case 1:
							inst->encap_type = ARPHRD_IEEE80211_PRISM;
							break;
							*/
						case 2:
							inst->encap_type = ARPHRD_IEEE80211_RADIOTAP;
							break;
					}

					dev = vmac_get_netdev(inst->netif);
					dev->type = inst->encap_type;
					write_unlock_irqrestore(&(inst->lock), flags);
					break;
			}

			case RAWMAC_INST_PROC_MAC:
				if (result > VMAC_NAME_SIZE)
					result = VMAC_NAME_SIZE;
				kdata[result-1] = 0;
				rawmac_encapsulate_mac(inst, kdata);
				break;

			default:
				break;
		}
	}
	else
	{
		result = count;
	}
	return result;
}

static int rawmac_make_procfs_entries(struct rawmac_instance *inst)
{
	int i, result = 0;
	struct proc_dir_entry *ent;
	struct rawmac_inst_proc_data *proc_data;

	if (inst)
	{
		inst->procfs_dir = inst->macinfo->proc;

		for (i=0; (rawmac_inst_proc_entries[i].name && rawmac_inst_proc_entries[i].name[0]); i++)
		{
			ent = create_proc_entry(rawmac_inst_proc_entries[i].name,   rawmac_inst_proc_entries[i].mode, inst->procfs_dir);
			ent->owner = THIS_MODULE;
			ent->read_proc = rawmac_inst_read_proc;
			ent->write_proc = rawmac_inst_write_proc;
			ent->data = kmalloc(sizeof(struct rawmac_inst_proc_data), GFP_ATOMIC);
			proc_data = ent->data;

			INIT_LIST_HEAD(&(proc_data->list));
			list_add_tail(&(proc_data->list), &(inst->procfs_data));
			proc_data->inst = inst;
			proc_data->id = rawmac_inst_proc_entries[i].id;
			strncpy(proc_data->name, rawmac_inst_proc_entries[i].name, VMAC_NAME_SIZE);
			proc_data->parent = inst->procfs_dir;
		}
	}

	return result;
}

static int rawmac_delete_procfs_entries(struct rawmac_instance *inst)
{
	int result = 0;
	if (inst)
	{
		struct list_head* tmp = 0;
		struct list_head* p = 0;
		struct rawmac_inst_proc_data* proc_entry_data = 0;

		/* Remove individual entries and delete their data	 */
		list_for_each_safe(p, tmp, &(inst->procfs_data))
		{
			proc_entry_data = list_entry(p, struct rawmac_inst_proc_data, list);
			list_del(p);
			remove_proc_entry(proc_entry_data->name, proc_entry_data->parent);
			kfree(proc_entry_data);
			proc_entry_data = 0;
		}
	}

	return result;
}


/* attach to a phy layer */
static int rawmac_mac_attach(void *me, vmac_phy_t *phyinfo)
{
	struct rawmac_instance *inst = me;
	int ret = -1;
	unsigned long flags;

	write_lock_irqsave(&(inst->lock), flags);
	if (!inst->phyinfo)
	{
		inst->phyinfo = vmac_get_phy(phyinfo);
		ret = 0;
	}

	write_unlock_irqrestore(&(inst->lock), flags);
	return ret;
}

/* detach from phy layer */
static int rawmac_mac_detach(void *me)
{
	struct rawmac_instance *inst = me;
	unsigned long flags;

	write_lock_irqsave(&(inst->lock), flags);
	if (inst->phyinfo)
	{
		vmac_free_phy(inst->phyinfo);
		inst->phyinfo = 0;
	}

	write_unlock_irqrestore(&(inst->lock), flags);
	return 0;
}

static int rawmac_mac_set_netif_rx_func(void *me, mac_rx_func_t rxfunc, void *rxpriv) 
{
	struct rawmac_instance *inst = me;
	unsigned long flags;

	write_lock_irqsave(&(inst->lock), flags);
	inst->netif_rx = rxfunc;
	inst->netif_rx_priv = rxpriv;

	if (!inst->netif_rx && inst->netif)
		rawmac_mac_set_netif_rx_func(inst, vmac_vnetif_rx, inst->netif);

	write_unlock_irqrestore(&(inst->lock), flags);
	return 0;
}

static int rawmac_mac_packet_tx(void *me, struct sk_buff *skb, int intop)
{
	struct rawmac_instance *inst = me;

	BUG_ON(intop);
	read_lock(&(inst->lock));

	/* transmit a packet. first do radiotap header processing     */
	if (inst->encap_type == ARPHRD_IEEE80211_RADIOTAP)
	{
		struct ieee80211_radiotap_header *th = (struct ieee80211_radiotap_header *) skb->data;
		u_int8_t arg8 = 0;

		if (rt_check_header(th, skb->len))
		{
			if (rt_el_present(th, IEEE80211_RADIOTAP_RATE))
			{
				if (inst->phyinfo)
				{
					arg8 = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_RATE));
					vmac_ath_set_tx_bitrate(inst->phyinfo->phy_private, skb, arg8);
				}
			}
			skb_pull(skb, th->it_len);
		}
	}

	/* send the packet to the encapsulated MAC for processing
	* when finished, the MAC will call rawmac_fakephy_sendpacket     */
	if (inst->macinfo_real)
	{
		inst->macinfo_real->mac_tx(inst->macinfo_real->mac_private, skb, intop);
	}

	read_unlock(&(inst->lock));
	return VMAC_MAC_NOTIFY_OK;
}

/* called by mac layer as netif_rx */
static int rawmac_macdone(void *me, struct sk_buff *skb)
{
	struct rawmac_instance *inst = me;

	read_lock(&(inst->lock));
	if (!inst->netif) 
		goto out;

	if (inst->encap_type == ARPHRD_IEEE80211_RADIOTAP)
	{/* encapsulate with radiotap header */
		struct ath_rx_radiotap_header *th;
		if (skb_headroom(skb) < sizeof(struct ath_rx_radiotap_header) &&
			pskb_expand_head(skb,  sizeof(struct ath_rx_radiotap_header), 0, GFP_ATOMIC))
		{
			printk("%s: couldn't pskb_expand_head\n", __func__);
			printk("%s: XXX leak!\n", __func__);
			goto out;
		}

		th = (struct ath_rx_radiotap_header *)skb_push(skb, sizeof(struct ath_rx_radiotap_header));
		memset(th, 0, sizeof(struct ath_rx_radiotap_header));
		th->wr_ihdr.it_version = 0;
		th->wr_ihdr.it_len = sizeof(struct ath_rx_radiotap_header);
		th->wr_ihdr.it_present = ATH_RX_RADIOTAP_PRESENT;
		th->wr_flags = IEEE80211_RADIOTAP_F_FCS;
		th->wr_rate = vmac_ath_get_rx_bitrate(inst->phyinfo->phy_private, skb);
		th->wr_chan_freq 	= ieee80211_ieee2mhz( vmac_ath_get_rx_channel(inst->phyinfo->phy_private, skb) );

		//th->wr_chan_flags = ic->ic_ibss_chan->ic_flags;
		th->wr_antenna =  vmac_ath_get_rx_antenna(inst->phyinfo->phy_private, skb);
		th->wr_antsignal = vmac_ath_get_rx_rssi(inst->phyinfo->phy_private, skb);
		th->wr_rx_flags = 0;
		if (vmac_ath_has_rx_crc_error(inst->phyinfo->phy_private, skb))
			th->wr_rx_flags |= IEEE80211_RADIOTAP_F_RX_BADFCS;
	}

	/* send it up the stack */
	inst->netif_rx(inst->netif_rx_priv, skb);
	
out:
	read_unlock(&(inst->lock));
	return 0;
}

static int rawmac_mac_packet_rx(void *me, struct sk_buff *skb, int intop)
{
	struct rawmac_instance *inst = me;

	read_lock(&(inst->lock));
	if (!inst->macinfo_real)
	{
		if (inst->phyinfo)
			inst->phyinfo->phy_free_skb(inst->phyinfo->phy_private, skb);
		else
			kfree_skb(skb);

		read_unlock(&(inst->lock));
		return VMAC_MAC_NOTIFY_OK;
	}

	/* receive a packet from the network call the encapsulated MAC to do its thing
	* when finished, the MAC will call rawmac_macdone     */
	inst->macinfo_real->rx_packet(inst->macinfo_real->mac_private, skb, intop);

	read_unlock(&(inst->lock));
	return VMAC_MAC_NOTIFY_OK;
} 

static int rawmac_netif_txhelper(vnet_handle nif, void* priv, struct sk_buff *skb)
{
	struct rawmac_instance *inst = priv;
	if (inst)
	{
		int ret = rawmac_mac_packet_tx(inst, skb, 0);
		if (ret != MAC_TX_OK)
			dev_kfree_skb(skb);
	}
	return 0;
}

/* create a network interface and attach to it */
static int rawmac_create_and_attach_netif(void *me)
{
	struct rawmac_instance *inst = me;
	vmac_mac_t *macinfo = inst->macinfo;
	int ret = 0;
	struct net_device *checknet = 0;

	checknet = dev_get_by_name(macinfo->name);
	if (checknet)
	{
		//printk("%s: attaching to %s\n", __func__, macinfo->name);
		inst->netif = vmac_get_vnetif(checknet);
		dev_put(checknet);
		vmac_vnetif_set_tx_callback(inst->netif, rawmac_netif_txhelper, (void *)inst);
	}
	else
	{
		//printk("%s: creating %s\n", __func__, macinfo->name);
		inst->netif = vmac_create_vnetif(macinfo->name, 0, rawmac_netif_txhelper, inst);
	}

	if (inst->netif)
	{
		rawmac_mac_set_netif_rx_func(inst, vmac_vnetif_rx, inst->netif);
	}
	else
	{
		printk("%s: error unable to attach to netif!\n", __func__);
		ret = -1;
	}

	return ret;
}

static int rawmac_fakephy_sendpacket(void *me, int max_inflight, struct sk_buff *skb)
{
	struct rawmac_instance *inst = (struct rawmac_instance *)me;
	vmac_phy_t *phy;
	int txresult = 0;

	read_lock(&(inst->lock));
	phy = inst->phyinfo;
	if (phy)
		txresult = phy->phy_sendpacket(phy->phy_private, max_inflight, skb); 

	read_unlock(&(inst->lock));
	return txresult;
}

static struct sk_buff* rawmac_fakephy_alloc_skb(void *me, int len)
{
	struct rawmac_instance *inst = me;
	vmac_phy_t *phy;
	struct sk_buff *skb = 0;

	read_lock(&(inst->lock));
	phy = inst->phyinfo;
	if (phy)
		skb = phy->phy_alloc_skb(phy->phy_private, len);

	read_unlock(&(inst->lock));
	return skb;
}

static void rawmac_encapsulate_mac(void *me, char *mac_instance_name)
{
	unsigned long flags;
	struct rawmac_instance *inst = me;

	write_lock_irqsave(&(inst->lock), flags);

	if (inst->macinfo_real)
	{
		printk("%s: detaching mac %s\n", inst->macinfo->name, inst->macinfo_real->name);
		inst->macinfo_real->detach_phy(inst->macinfo_real->mac_private);
		inst->macinfo_real->set_rx_func(inst->macinfo_real->mac_private, 0, 0);
		vmac_free_mac(inst->macinfo_real);
		inst->macinfo_real = 0;
	}

	if (mac_instance_name)
	{
		inst->macinfo_real = vmac_get_mac_by_name(mac_instance_name);
		if (inst->macinfo_real)
		{
			inst->macinfo_real->set_rx_func(inst->macinfo_real->mac_private, rawmac_macdone, me);

			inst->macinfo_real->attach_phy(inst->macinfo_real->mac_private, inst->phyinfo_fake);
			printk("%s: encapsulating mac %s\n", inst->macinfo->name, inst->macinfo_real->name);
		}
		else
		{
			printk("%s: error unable to encapsulate mac %s\n", __func__, mac_instance_name);
		}
	}

	write_unlock_irqrestore(&(inst->lock), flags);
}

/* create and return a new rawmac instance */
static void *rawmac_new_instance (void *layer_priv)
{
	struct rawmac_instance *inst;
	void *ret = 0;

	inst = kmalloc(sizeof(struct rawmac_instance), GFP_ATOMIC);
	if (inst)
	{
		memset(inst, 0, sizeof(struct rawmac_instance));

		inst->lock = RW_LOCK_UNLOCKED;
		inst->id = rawmac_next_id++;

		/* setup the macinfo structure */
		inst->macinfo = vmac_alloc_mac();
		inst->macinfo->mac_private = inst;
		inst->macinfo->layer = &the_rawmac;
		snprintf(inst->macinfo->name, VMAC_NAME_SIZE, "%s%d", the_rawmac.name, inst->id);

		/* override some macinfo functions */
		/* the rest remain pointed at the default "do nothing" functions */
		inst->macinfo->mac_tx = rawmac_mac_packet_tx;
		inst->macinfo->rx_packet = rawmac_mac_packet_rx;
		inst->macinfo->attach_phy = rawmac_mac_attach;
		inst->macinfo->detach_phy = rawmac_mac_detach;
		inst->macinfo->set_rx_func = rawmac_mac_set_netif_rx_func;

		/* */
		rawmac_create_and_attach_netif(inst);

		/* */
		inst->phyinfo_fake = vmac_alloc_phy();
		inst->phyinfo_fake->phy_private = inst;
		snprintf(inst->phyinfo_fake->name,  VMAC_NAME_SIZE,  "%s_fake_phy", inst->macinfo->name);

		inst->phyinfo_fake->phy_sendpacket = rawmac_fakephy_sendpacket;
		inst->phyinfo_fake->phy_alloc_skb = rawmac_fakephy_alloc_skb;

		/* register with softmac */
		vmac_register_mac(inst->macinfo);

		/* */
		INIT_LIST_HEAD(&inst->procfs_data);
		rawmac_make_procfs_entries(inst);

		/* */
		rawmac_encapsulate_mac(inst, 0);

		/* we've registered with softmac, decrement the ref count */
		vmac_free_mac(inst->macinfo);

		ret = inst->macinfo;
	}

	return ret;
}

/* called by softmac_core when a rawmac vmac_mac_t
 * instance (inst->macinfo) is deallocated 
 */
static void rawmac_free_instance (void *layer_priv, void *info)
{
	vmac_mac_t *macinfo = info;
	struct rawmac_instance *inst = macinfo->mac_private;

	/* detach and free phyinfo, phyinfo_fake, macinfo_real */
	rawmac_mac_detach(inst);
	rawmac_encapsulate_mac(inst, 0);
	inst->phyinfo_fake->phy_private = 0;
	vmac_free_phy(inst->phyinfo_fake);

	rawmac_delete_procfs_entries(inst);
	kfree(inst);
}

static int __init __vmac_rawmac_init(void)
{
	/* register the rawmac layer with softmac */
	strncpy(the_rawmac.name, rawmac_name, VMAC_NAME_SIZE);
	the_rawmac.new_instance = rawmac_new_instance;
	the_rawmac.destroy_instance = rawmac_free_instance;
	vmac_layer_register(&the_rawmac);

	return 0;
}

static void __exit __vmac_rawmac_exit(void)
{
	/* tell softmac we're leaving */
	vmac_layer_unregister((vmac_layer_t *)&the_rawmac);
}

module_init(__vmac_rawmac_init);
module_exit(__vmac_rawmac_exit);

