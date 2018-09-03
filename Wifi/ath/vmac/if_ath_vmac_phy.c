/*
* $Id: if_ath_vmac_phy.c,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#include "if_ath_vmac.h"

struct ath_cu_softmac_proc_data
{
	struct list_head list;
	struct ath_softc* sc;
	int id;

	char name[VMAC_NAME_SIZE];
	struct proc_dir_entry* parent;  
};


struct ath_vmac_proc_entry
{
	const char *procname;
	mode_t mode;
	int ctl_name;
};

static const struct ath_vmac_proc_entry athphy_proc_entries[] =
{
	{
		.ctl_name	= ATH_VMAC,
		.procname	= "softmac_enable",
		.mode	= 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_RAW80211,
		.procname	= "raw_mode",
		.mode	= 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_PHOCUS_SETTLETIME,
		.procname	= "phocus_settletime",
		.mode	= 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_PHY_NF_RAW,
		.procname	= "nf_raw",
		.mode	= 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_PHOCUS_ENABLE,
		.procname	= "phocus_enable",
		.mode     = 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_PHOCUS_STATE,
		.procname	= "phocus_state",
		.mode	= 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_WIFIVERSION,
		.procname	= "wifiversion",
		.mode	= 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_WIFITYPE,
		.procname	= "wifitype",
		.mode	= 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_WIFISUBTYPE,
		.procname	= "wifisubtype",
		.mode     = 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_WIFIFLAGS,
		.procname	= "wififlags",
		.mode	= 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_NOAUTOCALIBRATE,
		.procname	= "noautocalibrate",
		.mode	= 0644,
	},
	{ 
		.ctl_name	= ATH_VMAC_CALIBRATE_NOW,
		.procname	= "calibrate_now",
		.mode	= 0200,
	},
	{ 
		.ctl_name = ATH_VMAC_MAC,
		.procname = "mac_layer",
		.mode     = 0644,
	},
	{ 
		.ctl_name = ATH_VMAC_ALLOW_CRCERR,
		.procname = "allow_crcerr",
		.mode     = 0644,
	},
	/* terminator, don't remove */
	{ 
		.ctl_name = 0,
		.procname = 0,
		.mode = 0,
	},
};


static void ath_vmac_phy_detach(vmac_phy_handle nfh)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;

	if (sc->sc_vmac_mac == sc->sc_vmac_defaultmac)
		return;

	write_lock(&(sc->sc_vmac_mac_lock));
	vmac_free_mac(sc->sc_vmac_mac);
	sc->sc_vmac_mac = sc->sc_vmac_defaultmac;

	write_unlock(&(sc->sc_vmac_mac_lock));
}

static int ath_vmac_phy_attach(vmac_phy_handle nfh,vmac_mac_t* macinfo)
{
	struct ath_softc* sc = (struct ath_softc*)nfh;
	int result = 0;

	ath_vmac_phy_detach(nfh);

	/* lock the mac */
	write_lock(&(sc->sc_vmac_mac_lock));
	/* get private copy of new MAC */
	sc->sc_vmac_mac = vmac_get_mac(macinfo);

	/* unlock the mac */
	write_unlock(&(sc->sc_vmac_mac_lock));

	return result;
}

static int ath_vmac_phy_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int result = 0;
	struct ath_cu_softmac_proc_data* procdata = data;

	if (procdata && procdata->sc)
	{
		struct ath_softc *sc = procdata->sc;
		char* dest = (page + off);
		int intval = 0;

		switch (procdata->id)
		{
			case ATH_VMAC:
				intval = sc->sc_vmac;
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_RAW80211:
				// Allow softmac to operate on the whole 802.11 frame
				if ((sc->sc_vmac_options & CU_SOFTMAC_ATH_RAW_MODE))
				{
					intval = 1;
				}
				else
				{
					intval = 0;
				}
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_PHOCUS_SETTLETIME:
				// allow phocus time to settle after steering
				intval =sc->sc_vmac_phocus_settletime;
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_PHY_NF_RAW:
				// get value for the PHY_NF register
				intval =OS_REG_READ(sc->sc_ah,39012);
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_PHOCUS_ENABLE:
				// enable use of phocus antenna
				intval =sc->sc_vmac_phocus_enable;
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_PHOCUS_STATE:
				// configuration of phocus antenna
				intval =sc->sc_vmac_phocus_state;
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_WIFIVERSION:
				// get the wifi version encap field
				intval =sc->sc_vmac_wifictl0 & 0x3;
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_WIFITYPE:
				// get the wifi type encap field
				intval =(sc->sc_vmac_wifictl0 >> 2) & 0x3;
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_WIFISUBTYPE:
				// get the wifi subtype encap field
				intval =(sc->sc_vmac_wifictl0 >> 4) & 0xF;
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_WIFIFLAGS:
				// get the wifi flags encap field
				intval =sc->sc_vmac_wifictl1;
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;
				break;

			case ATH_VMAC_NOAUTOCALIBRATE:
				intval =sc->sc_vmac_noautocalibrate;
				break;//?????? lizhijie
				result = snprintf(dest,count, "%d\n", intval);
				*eof = 1;

			case ATH_VMAC_MAC:
				if (sc->sc_vmac_mac == sc->sc_vmac_defaultmac)
					result = snprintf(dest, count, "0\n");
				else
					result = snprintf(dest, count, "%s\n", sc->sc_vmac_mac->name);
				*eof = 1;
				break;

			case ATH_VMAC_ALLOW_CRCERR:
				// Allow softmac to operate on the whole 802.11 frame
				if (sc->sc_vmac_options & CU_SOFTMAC_ATH_ALLOW_CRCERR)
				{
					intval = 1;
				}
				else
				{
					intval = 0;
				}
				result = snprintf(dest,count, "%d\n", intval);
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

static int ath_vmac_phy_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int result = 0;
	struct ath_cu_softmac_proc_data* procdata = data;
	vmac_mac_t *mac;

	if (procdata && procdata->sc)
	{
		struct ath_softc *sc = procdata->sc;
		struct ath_hal *ah = sc->sc_ah;
		static const int maxkdatalen = 256;
		char kdata[maxkdatalen];
		char* endp = 0;
		long val = 0;

		/** Drag the data over into kernel land	 */
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
			case ATH_VMAC:
				// allow "softmac" access when in monitor mode
				val = simple_strtol(kdata, &endp, 10);
				printk(KERN_DEBUG VMAC_MODULE_ATH_PHY":%d \n", val);
				trace;
				sc->sc_vmac = val;
				if (val)
				{// Make sure we receive ALL the packets from the hal...
					ath_hal_setrxfilter(ah,ATH_VMAC_HAL_RX_FILTER_ALLOWALL);
				}
				break;

			case ATH_VMAC_RAW80211:
				// Allow softmac to operate on the whole 802.11 frame.
				val = simple_strtol(kdata, &endp, 10);
				if (val)
				{
					sc->sc_vmac_options |= CU_SOFTMAC_ATH_RAW_MODE;
				}
				else
				{
					sc->sc_vmac_options &= ~CU_SOFTMAC_ATH_RAW_MODE;
				}
				break;

			case ATH_VMAC_PHOCUS_SETTLETIME:
				// delay to use for phocus antenna
				val = simple_strtol(kdata, &endp, 10);
				sc->sc_vmac_phocus_settletime = val;
				break;

			case ATH_VMAC_PHY_NF_RAW:
				// set a value for the PHY_NF register
				val = simple_strtol(kdata, &endp, 10);
				OS_REG_WRITE(sc->sc_ah,39012,val);
				break;

			case ATH_VMAC_PHOCUS_ENABLE:
				// enable the phocus antenna unit
				val = simple_strtol(kdata, &endp, 10);
				sc->sc_vmac_phocus_enable = val;
				break;

			case ATH_VMAC_PHOCUS_STATE:
				// configure the phocus antenna unit
				val = simple_strtol(kdata, &endp, 10);
				if (sc->sc_vmac_phocus_enable && (val != sc->sc_vmac_phocus_state))
				{
					sc->sc_vmac_phocus_state = val;
					vmac_ath_set_phocus_state(sc->sc_vmac_phocus_state, 	sc->sc_vmac_phocus_settletime);
				}
				break;

			case ATH_VMAC_WIFIVERSION:
			{
				u_int8_t wctl0 = sc->sc_vmac_wifictl0;
				// set the wifi version encap field
				val = simple_strtol(kdata, &endp, 10);
				wctl0 &= ~(0x3);
				wctl0 |= (val & 0x3);
				sc->sc_vmac_wifictl0 = wctl0;
				break;
			}	

			case ATH_VMAC_WIFITYPE:
			{
				u_int8_t wctl0 = sc->sc_vmac_wifictl0;
				// set the wifi type encap field
				val = simple_strtol(kdata, &endp, 10);
				wctl0 &= ~(0x3 << 2);
				wctl0 |= ((val & 0x3) << 2);
				sc->sc_vmac_wifictl0 = wctl0;
				break;
			}	

			case ATH_VMAC_WIFISUBTYPE:
			{
				u_int8_t wctl0 = sc->sc_vmac_wifictl0;
				val = simple_strtol(kdata, &endp, 10);
				// set the wifi subtype encap field
				wctl0 &= ~(0xF << 4);
				wctl0 |= ((val & 0xF) << 4);
				sc->sc_vmac_wifictl0 = wctl0;
				break;
			}	

			case ATH_VMAC_WIFIFLAGS:
				val = simple_strtol(kdata, &endp, 10);
				sc->sc_vmac_wifictl1 = (val & 0xFF);
				break;

			case ATH_VMAC_NOAUTOCALIBRATE:
				val = simple_strtol(kdata, &endp, 10);
				sc->sc_vmac_noautocalibrate = val;
				break;

			case ATH_VMAC_CALIBRATE_NOW:
				ath_calibrate_now(&(sc->sc_dev));
				break;

			case ATH_VMAC_MAC:
				if (result > VMAC_NAME_SIZE)
					result = VMAC_NAME_SIZE;
				kdata[result-1] = 0;

				/* detach any mac already attached */
				sc->sc_vmac_mac->detach_phy(sc->sc_vmac_mac->mac_private);
				ath_vmac_phy_detach(sc);

				/* try to attach a new mac */
#if VMAC_DEBUG_ATH
				printk(KERN_DEBUG VMAC_MODULE_ATH_PHY ":attach mac '%s'\n", kdata);
#endif
				mac = vmac_get_mac_by_name(kdata);
				if (mac)
				{
					ath_vmac_phy_attach(sc, mac);
					mac->attach_phy(mac->mac_private, sc->sc_vmac_phy);
					vmac_free_mac(mac);
					printk("%s: attached to %s\n", sc->sc_vmac_phy->name, kdata);
				}
				break;

			case ATH_VMAC_ALLOW_CRCERR:
				val = simple_strtol(kdata, &endp, 10);
				if (val)
				{
					sc->sc_vmac_options |= CU_SOFTMAC_ATH_ALLOW_CRCERR;
				}
				else
				{
					sc->sc_vmac_options &= ~CU_SOFTMAC_ATH_ALLOW_CRCERR;
				}
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



static int ath_vmac_phy_make_procfs(struct ath_softc *sc)
{
	int i, result = 0;
	struct proc_dir_entry *ent, *root;
	struct ath_cu_softmac_proc_data *proc_data;

	if (sc)
	{
		root = sc->sc_vmac_phy->proc;
		for (i=0; (athphy_proc_entries[i].procname && athphy_proc_entries[i].procname[0]); i++)
		{
			ent = create_proc_entry(athphy_proc_entries[i].procname, athphy_proc_entries[i].mode, root);
			ent->owner = THIS_MODULE;
			ent->read_proc = ath_vmac_phy_read_proc;
			ent->write_proc = ath_vmac_phy_write_proc;
			ent->data = kmalloc(sizeof(struct ath_cu_softmac_proc_data), GFP_ATOMIC);
			proc_data = ent->data;

			INIT_LIST_HEAD(&(proc_data->list));
			list_add_tail(&(proc_data->list), &(sc->sc_vmac_procfs_data));
			proc_data->sc = sc;
			proc_data->id = athphy_proc_entries[i].ctl_name;
			strncpy(proc_data->name, athphy_proc_entries[i].procname, VMAC_NAME_SIZE);
			proc_data->parent = root;
		}
	}

	return result;
}

static int ath_vmac_phy_delete_procfs(struct ath_softc *sc)
{
	int result = 0;
	if (sc)
	{
		struct list_head* tmp = 0;
		struct list_head* p = 0;
		struct ath_cu_softmac_proc_data* proc_entry_data = 0;
		
		/** Remove individual entries and delete their data	 */
		list_for_each_safe(p, tmp, &(sc->sc_vmac_procfs_data))
		{
			proc_entry_data = list_entry(p, struct ath_cu_softmac_proc_data, list);
			list_del(p);
			remove_proc_entry(proc_entry_data->name, proc_entry_data->parent);
			kfree(proc_entry_data);
			proc_entry_data = 0;
		}
	}

	return result;
}

static u_int64_t ath_vmac_phy_get_time(vmac_phy_handle nfh)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	struct ath_hal *ah = sc->sc_ah;

	return ath_hal_gettsf64(ah) - sc->sc_vmac_zerotime;
}

static void ath_vmac_phy_set_time(vmac_phy_handle nfh,u_int64_t curtime)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	struct ath_hal *ah = sc->sc_ah;
	u_int64_t mytsf = 0;
	mytsf = ath_hal_gettsf64(ah);
	sc->sc_vmac_zerotime = mytsf - curtime;
}

static void ath_vmac_phy_schedule_work_asap(vmac_phy_handle nfh)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	int needmark = 0;
	//printk(KERN_DEBUG "if_ath: scheduling worktq\n");
	ATH_SCHEDULE_TQUEUE(&sc->sc_vmac_worktq, &needmark);
	
	if (needmark) 
		mark_bh(IMMEDIATE_BH);
}

/* allocate extra space for ath_vmac_header */
static struct sk_buff* ath_vmac_phy_alloc_skb(vmac_phy_handle nfh,int datalen)
{
	struct sk_buff* newskb = 0;
	struct ath_softc *sc = (struct ath_softc*) nfh;

	/*
	* Allocate an sk_buff of the requested size with the kind of alignment
	* we'd like as well as with enough headroom for the extra couple of bytes
	* we have to pop onto the head of the packet in order to let it traverse
	* the stack without being altered by the Atheros hardware.
	*/
	// XXX
	// verify the sizing/aligning junk...
	// check for "raw 802.11" mode
	newskb = ath_alloc_skb(datalen+sizeof(struct ath_vmac_header), sc->sc_cachelsz);
	skb_pull(newskb,sizeof(struct ath_vmac_header));

	return newskb;
}

static void ath_vmac_phy_free_skb(vmac_phy_handle nfh,struct sk_buff* skb)
{
	//struct ath_softc *sc = (struct ath_softc*) nfh;
	//struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	if (in_irq())
	{
		dev_kfree_skb_irq(skb);
	}
	else
	{
		dev_kfree_skb(skb);
	}
}

/* Transmit a softmac frame. On failure we leave the skbuff alone
 * so that the MAC layer can decide what it wants to do with the packet.  */
static int ath_vmac_phy_sendpacket_keeponfail(vmac_phy_handle nfh, int max_packets_inflight, struct sk_buff* skb)
{
	//struct net_device *dev = ic->ic_dev;
	struct ath_softc *sc = (struct ath_softc*) nfh;
	struct net_device* dev = &(sc->sc_dev);
	//struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_node *ni = NULL;
	struct ath_buf *bf = NULL;
	//struct ieee80211_frame *wh;
	//struct ieee80211_cb *cb;
	struct ieee80211com *ic = &sc->sc_ic;
	int error = VMAC_PHY_SENDPACKET_OK;

	trace;
	
	atomic_inc(&(sc->sc_vmac_tx_packets_inflight));
	if (!(sc->sc_vmac_options & CU_SOFTMAC_ATH_RAW_MODE))
	{
		skb = vmac_ath_encapsulate(sc,skb);
	}

	if ((max_packets_inflight > 0) && (max_packets_inflight < atomic_read(&(sc->sc_vmac_tx_packets_inflight))))
	{    // Too many pending packets -- bail out...
		error = VMAC_PHY_SENDPACKET_ERR_TOOMANYPENDING;
		goto bad;
	}

	if ((dev->flags & IFF_RUNNING) == 0 || sc->sc_invalid)
	{
		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: discard, invalid %d flags %x\n",   __func__, sc->sc_invalid, dev->flags);
		sc->sc_stats.ast_tx_invalid++;
		error = VMAC_PHY_SENDPACKET_ERR_NETDOWN;
		goto bad;
	}

	if (sc->sc_vmac_phocus_enable)
	{
		u_int16_t state = (u_int16_t)skb->cb[ATH_VMAC_CB_ANTENNA];
		if (state != sc->sc_vmac_phocus_state)
		{
			vmac_ath_set_phocus_state(state, sc->sc_vmac_phocus_settletime);
		}
	}

	/* Grab a TX buffer and associated resources.   */
	ATH_TXBUF_LOCK_BH(sc);
	bf = STAILQ_FIRST(&sc->sc_txbuf);
	if (bf != NULL)
		STAILQ_REMOVE_HEAD(&sc->sc_txbuf, bf_list);

	if (STAILQ_EMPTY(&sc->sc_txbuf))
	{
		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: stop queue\n", __func__);
		sc->sc_stats.ast_tx_qstop++;
		netif_stop_queue(dev);
	}

	ATH_TXBUF_UNLOCK_BH(sc);
	if (bf == NULL)
	{
		printk(KERN_DEBUG "ath_cusdr_sendpacket: discard, no xmit buf\n");
		//sc->sc_stats.ast_tx_nobufmgt++;
		error = VMAC_PHY_SENDPACKET_ERR_NOBUFFERS;
		goto bad;
	}

	/* NB: the referenced node pointer is in the
	* control block of the sk_buff.  This is
	* placed there by ieee80211_mgmt_output because
	* we need to hold the reference with the frame.
	*/
	//cb = (struct ieee80211_cb *)skb->cb;
	//ni = cb->ni;
	ni = ic->ic_bss;
	//error = ath_tx_start(dev, ni, bf, skb);

	error = ath_tx_raw_start(dev, ni, bf, skb);
	if (error == 0)
	{
		//sc->sc_stats.ast_tx_mgmt++;
		return 0;
	}
	
  /* fall thru... */
 bad:
	//if (ni && ni != ic->ic_bss)
	//ieee80211_free_node(ic, ni);
	// Decrement inflight packet count on failure
	atomic_dec(&(sc->sc_vmac_tx_packets_inflight));
	if (!(sc->sc_vmac_options & CU_SOFTMAC_ATH_RAW_MODE))
	{
		skb = vmac_ath_decapsulate(sc,skb);
	}

	if (bf != NULL)
	{
		ATH_TXBUF_LOCK_BH(sc);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK_BH(sc);
	}

	//if (skb){
	//dev_kfree_skb(skb);
	//}

	return error;
}

/* Transmit a softmac frame.  On failure we reclaim the skbuff.  */
static int ath_vmac_phy_sendpacket(vmac_phy_handle nfh,int max_packets_inflight, struct sk_buff* skb)
{
	int result;
	trace;
	result = ath_vmac_phy_sendpacket_keeponfail(nfh,max_packets_inflight,skb);
	if (0 != result)
	{
		dev_kfree_skb(skb);
		skb = 0;
	}

	return result;
}

static u_int32_t ath_vmac_phy_get_duration(vmac_phy_handle nfh,struct sk_buff* skb)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	struct net_device* dev = &(sc->sc_dev);

	return ath_cu_softmac_packetduration(dev,skb);
}

static u_int32_t ath_vmac_phy_get_txlatency(vmac_phy_handle nfh)
{
	struct ath_softc *sc = (struct ath_softc*) nfh;
	return sc->sc_vmac_txlatency;
}


// The following functions are some atheros-specific things.
void vmac_ath_set_phyinfo_functions(vmac_phy_t* phyinfo)
{
	// XXX lock?
	phyinfo->attach_mac = ath_vmac_phy_attach;
	phyinfo->detach_mac = ath_vmac_phy_detach;
	phyinfo->phy_get_time = ath_vmac_phy_get_time;
	phyinfo->phy_set_time = ath_vmac_phy_set_time;
	phyinfo->phy_schedule_work_asap = ath_vmac_phy_schedule_work_asap;
	phyinfo->phy_alloc_skb = ath_vmac_phy_alloc_skb;
	phyinfo->phy_free_skb = ath_vmac_phy_free_skb;
	phyinfo->phy_sendpacket = ath_vmac_phy_sendpacket;
	phyinfo->phy_sendpacket_keepskbonfail = ath_vmac_phy_sendpacket_keeponfail;
	phyinfo->phy_get_duration = ath_vmac_phy_get_duration;
	phyinfo->phy_get_txlatency = ath_vmac_phy_get_txlatency;
}

void* ath_vmac_new_phy(void *layer_private)
{
	struct list_head *list = layer_private;
	struct list_head *p;
	struct ath_softc *sc;
	vmac_phy_t *phyinfo;

	list_for_each(p, list)
	{
		sc = list_entry(p, struct ath_softc, sc_vmac_phy_list);
		if (!sc->sc_vmac_phy)
		{
			/* default do nothing mac to avoid null checks */
			sc->sc_vmac_defaultmac = vmac_alloc_mac();
			sc->sc_vmac_mac = sc->sc_vmac_defaultmac;

			/* create a public interface and register it with softmac */
			sc->sc_vmac_phy = vmac_alloc_phy();
			phyinfo = sc->sc_vmac_phy;
			snprintf(phyinfo->name, VMAC_NAME_SIZE, VMAC_MODULE_ATH_PHY"%d", sc->sc_vmac_phy_id);
			phyinfo->layer = &the_athphy;
			phyinfo->phy_private = sc;
			vmac_ath_set_phyinfo_functions(phyinfo);
			vmac_register_phy(phyinfo);

			/* create softmac specific procfs entries */
			INIT_LIST_HEAD(&sc->sc_vmac_procfs_data);
			ath_vmac_phy_make_procfs(sc);

			/* an instance doesn't hold a reference to itself */
			vmac_free_phy(phyinfo);

			return phyinfo;
		}
	}
	return 0;
}


/* Called when a Atheros vmac_phy_t structure is being deallocated */
void ath_vmac_free_phy(void *layer_private, void *phy)
{
	vmac_phy_t *phyinfo = phy;
	struct ath_softc *sc = phyinfo->phy_private;

	/* delete softmac specific procfs entries */
	ath_vmac_phy_delete_procfs(sc);

	ath_vmac_phy_detach(sc);
	vmac_free_phy(sc->sc_vmac_phy);
	sc->sc_vmac_phy = 0;
	vmac_free_mac(sc->sc_vmac_defaultmac);
	sc->sc_vmac_defaultmac = 0;
	sc->sc_vmac_mac = 0;
}

EXPORT_SYMBOL(vmac_ath_set_phyinfo_functions);

