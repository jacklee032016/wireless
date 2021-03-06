/*
* $Id: wlan_proto.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#ifndef	__WLAN_PROTO_H__
#define	__WLAN_PROTO_H__

/*
 * 802.11 protocol implementation definitions.
 */

enum ieee80211_state
{
	IEEE80211_S_INIT			= 0,	/* default state */
	IEEE80211_S_SCAN			= 1,	/* scanning */
	IEEE80211_S_AUTH			= 2,	/* try to authenticate */
	IEEE80211_S_ASSOC			= 3,	/* try to assoc */
	IEEE80211_S_RUN			= 4	/* associated */
};
#define	IEEE80211_S_MAX		(IEEE80211_S_RUN+1)

#define	IEEE80211_SEND_MGMT(_ic,_ni,_type,_arg) \
	((*(_ic)->ic_send_mgmt)(_ic, _ni, _type, _arg))

/*
 * Transmitted frames have the following information
 * held in the sk_buff control buffer.  This is used to
 * communicate various inter-procedural state that needs
 * to be associated with the frame for the duration of it's existence.
 */
struct ieee80211_cb
{/* info keep in skb->control buffer */
	struct ieee80211_node			*ni;
	struct ieee80211_key			*key;
	u_int8_t						flags;
	int							age;
};

extern	const char *ieee80211_mgt_subtype_name[];
extern	const char *ieee80211_phymode_name[];

struct ieee80211_node;
extern	int 		ieee80211_input(struct ieee80211com *, struct sk_buff *, struct ieee80211_node *, int, u_int32_t);
extern	void 	ieee80211_recv_mgmt(struct ieee80211com *, struct sk_buff *, struct ieee80211_node *, int, int, u_int32_t);
extern	int 		ieee80211_send_nulldata(struct ieee80211com *,	struct ieee80211_node *);
extern	int 		ieee80211_send_mgmt(struct ieee80211com *, struct ieee80211_node *,	int, int);
extern	int 		ieee80211_classify(struct ieee80211com *, struct sk_buff *, struct ieee80211_node *);
extern	struct sk_buff 	*ieee80211_encap(struct ieee80211com *, struct sk_buff *, struct ieee80211_node *);	// TODO: ieee80211_node ** ??
extern	void 	ieee80211_pwrsave(struct ieee80211com *, struct ieee80211_node *, struct sk_buff *);

extern	void 	ieee80211_reset_erp(struct ieee80211com *);
extern	void 	ieee80211_set_shortslottime(struct ieee80211com *, int onoff);
extern	int 		ieee80211_iserp_rateset(struct ieee80211com *, struct ieee80211_rateset *);
extern	void 	ieee80211_set11gbasicrates(struct ieee80211_rateset *,	enum ieee80211_phymode);

/* Return the size of the 802.11 header for a management or data frame. */
static inline int ieee80211_hdrsize(const void *data)
{
	const struct ieee80211_frame *wh = data;
	int size = sizeof(struct ieee80211_frame);

	/* NB: we don't handle control frames */
	KASSERT((wh->i_fc[0]&IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL,	("%s: control frame", __func__));
	
	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
		size += IEEE80211_ADDR_LEN;
	if (IEEE80211_QOS_HAS_SEQ(wh))
		size += sizeof(u_int16_t);
	return size;
}

/* Return the size of the 802.11 header; handles any type of frame */
static inline int ieee80211_anyhdrsize(const void *data)
{
	const struct ieee80211_frame *wh = data;

	if ((wh->i_fc[0]&IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
	{
		switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK)
		{
			case IEEE80211_FC0_SUBTYPE_CTS:
			case IEEE80211_FC0_SUBTYPE_ACK:
				return sizeof(struct ieee80211_frame_ack);
		}
		return sizeof(struct ieee80211_frame_min);
	}
	else
		return ieee80211_hdrsize(data);
}

/*
 * Template for an in-kernel authenticator.  Authenticators
 * register with the protocol code and are typically loaded
 * as separate modules as needed.
 */
struct ieee80211_authenticator
{
	const char 	*ia_name;		/* printable name */
	
	int			(*ia_attach)(struct ieee80211com *);
	void			(*ia_detach)(struct ieee80211com *);
	
	void			(*ia_node_join)(struct ieee80211com *,	struct ieee80211_node *);
	void			(*ia_node_leave)(struct ieee80211com *,struct ieee80211_node *);
};

extern	void ieee80211_authenticator_register(int type, const struct ieee80211_authenticator *);
extern	void ieee80211_authenticator_unregister(int type);
extern	const struct ieee80211_authenticator *ieee80211_authenticator_get(int auth);

#if WITH_WLAN_AUTHBACK_ACL
struct eapolcom;
/*
 * Template for an in-kernel authenticator backend.  Backends
 * register with the protocol code and are typically loaded
 * as separate modules as needed.
 */
struct ieee80211_authenticator_backend
{
	const char 	*iab_name;		/* printable name */
	int			(*iab_attach)(struct eapolcom *);
	void			(*iab_detach)(struct eapolcom *);
};
extern	void ieee80211_authenticator_backend_register(const struct ieee80211_authenticator_backend *);
extern	void ieee80211_authenticator_backend_unregister(	const struct ieee80211_authenticator_backend *);
extern	const struct ieee80211_authenticator_backend *ieee80211_authenticator_backend_get(const char *name);

/*
 * Template for an MAC ACL policy module.  Such modules
 * register with the protocol code and are passed the sender's
 * address of each received frame for validation.
 */
struct ieee80211_aclator
{
	const char 		*iac_name;		/* printable name */
	int				(*iac_attach)(struct ieee80211com *);
	void				(*iac_detach)(struct ieee80211com *);
	
	int				(*iac_check)(struct ieee80211com *, const u_int8_t mac[IEEE80211_ADDR_LEN]);
	int				(*iac_add)(struct ieee80211com *, const u_int8_t mac[IEEE80211_ADDR_LEN]);
	int				(*iac_remove)(struct ieee80211com *, const u_int8_t mac[IEEE80211_ADDR_LEN]);
	int				(*iac_flush)(struct ieee80211com *);
	
	int				(*iac_setpolicy)(struct ieee80211com *, int);
	int				(*iac_getpolicy)(struct ieee80211com *);
};
extern	void ieee80211_aclator_register(const struct ieee80211_aclator *);
extern	void ieee80211_aclator_unregister(const struct ieee80211_aclator *);
extern	const struct ieee80211_aclator *ieee80211_aclator_get(const char *name);
#endif

/* flags for ieee80211_fix_rate() */
#define	IEEE80211_F_DOSORT		0x00000001	/* sort rate list : Do Sort */
#define	IEEE80211_F_DOFRATE		0x00000002	/* use fixed rate : Do Fix Rate */
#define	IEEE80211_F_DONEGO		0x00000004	/* calc negotiated rate : Do Nego */
#define	IEEE80211_F_DODEL			0x00000008	/* delete ignore rate  : Do Del */
extern	int ieee80211_fix_rate(struct ieee80211com *,	struct ieee80211_node *, int);

/*
 * WME/WMM support.
 */
struct wmeParams
{
	u_int8_t	wmep_acm;
	u_int8_t	wmep_aifsn;
	u_int8_t	wmep_logcwmin;		/* log2(cwmin) */
	u_int8_t	wmep_logcwmax;		/* log2(cwmax) */
	u_int8_t	wmep_txopLimit;
	u_int8_t	wmep_noackPolicy;	/* 0 (ack), 1 (no ack) */
};
#define	IEEE80211_TXOP_TO_US(_txop)	((_txop)<<5)
#define	IEEE80211_US_TO_TXOP(_us)	((_us)>>5)

struct chanAccParams
{
	u_int8_t	cap_info;		/* version of the current set */
	struct wmeParams cap_wmeParams[WME_NUM_AC];
};

struct ieee80211_wme_state
{
	u_int	wme_flags;
#define	WME_F_AGGRMODE	0x00000001	/* STATUS: WME agressive mode */
	u_int	wme_hipri_traffic;	/* VI/VO frames in beacon interval */
	u_int	wme_hipri_switch_thresh;/* agressive mode switch thresh */
	u_int	wme_hipri_switch_hysteresis;/* agressive mode switch hysteresis */

	struct wmeParams wme_params[4];		/* from assoc resp for each AC*/
	struct chanAccParams wme_wmeChanParams;	/* WME params applied to self */
	struct chanAccParams wme_wmeBssChanParams;/* WME params bcast to stations */
	struct chanAccParams wme_chanParams;	/* params applied to self */
	struct chanAccParams wme_bssChanParams;	/* params bcast to stations */

	int	(*wme_update)(struct ieee80211com *);
};

extern	void ieee80211_wme_initparams(struct ieee80211com *);
extern	void ieee80211_wme_updateparams(struct ieee80211com *);
extern	void ieee80211_wme_updateparams_locked(struct ieee80211com *);

#define	ieee80211_new_state(_ic, _nstate, _arg) \
	(((_ic)->ic_newstate)((_ic), (_nstate), (_arg)))
extern	void ieee80211_print_essid(const u_int8_t *, int);
extern	void ieee80211_dump_pkt(const u_int8_t *, int, int, int);

extern	const char *ieee80211_state_name[IEEE80211_S_MAX];
extern	const char *ieee80211_wme_acnames[];

/*
 * Beacon frames constructed by ieee80211_beacon_alloc
 * have the following structure filled in so drivers
 * can update the frame later w/ minimal overhead.
 */
struct ieee80211_beacon_offsets
{
	u_int16_t			*bo_caps;	/* capabilities */
	u_int8_t			*bo_tim;	/* start of atim/dtim */
	u_int8_t			*bo_wme;	/* start of WME parameters */
	u_int8_t			*bo_trailer;	/* start of fixed-size trailer */
	u_int16_t			bo_tim_len;	/* atim/dtim length in bytes */
	u_int16_t			bo_trailer_len;	/* trailer length in bytes */
};
extern	struct sk_buff *ieee80211_beacon_alloc(struct ieee80211com *, struct ieee80211_node *, struct ieee80211_beacon_offsets *);
extern	int ieee80211_beacon_update(struct ieee80211com *, struct ieee80211_node *, struct ieee80211_beacon_offsets *, struct sk_buff *, int broadcast);

/*
 * Notification methods called from the 802.11 state machine.
 * Note that while these are defined here, their implementation
 * is OS-specific.
 */
extern	void ieee80211_notify_node_join(struct ieee80211com *,	struct ieee80211_node *, int newassoc);
extern	void ieee80211_notify_node_leave(struct ieee80211com *, struct ieee80211_node *);
extern	void ieee80211_notify_scan_done(struct ieee80211com *);

/* added lizhijie */
extern	void ieee80211_auth_setup(void);

#endif

