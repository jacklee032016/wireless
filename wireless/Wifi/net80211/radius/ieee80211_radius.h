/*
* $Id: ieee80211_radius.h,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#ifndef _NET80211_RADIUS_H_
#define _NET80211_RADIUS_H_

/*
 * IEEE 802.1x radius client support.
 *
 * Radius client support for the 802.1x authenticator.
 * This could be a user process that communicates with
 * the authenticator through messages; but for now its
 * in the kernel where it's easy to share state with the
 * authenticator state machine.  We structure this as a
 * module so it is not always resident.  Also we instantiate
 * this support only as needed so the cost of having it
 * is only incurred when in use (e.g. not for stations).
 */

struct eapol_radius_key {
	u_int8_t	rk_data[64];	/* decoded key length+data */
#define	rk_len	rk_data[0]		/* decoded key length */
	u_int16_t	rk_salt;	/* salt from server */
} __attribute__((__packed__));

struct eapol_auth_radius_node {
	struct eapol_auth_node	ern_base;
	LIST_ENTRY(eapol_auth_radius_node) ern_next;	/* reply list */
	struct sk_buff		*ern_msg;	/* supplicant reply */
	u_int8_t		*ern_radmsg;	/* radius server message */
	u_int16_t		ern_radmsglen;	/* length of server message */
	u_int8_t		ern_onreply;	/* on reply pending list */
	u_int8_t		ern_reqauth[16];/* request authenticator */
	u_int8_t		ern_state[255];
	u_int8_t		ern_statelen;
	u_int8_t		ern_class[255];
	u_int8_t		ern_classlen;
	struct eapol_radius_key	ern_txkey;
	struct eapol_radius_key	ern_rxkey;
};
#define	EAPOL_RADIUSNODE(_x)	((struct eapol_auth_radius_node *)(_x))

#define	ern_ic		ern_base.ean_ic
#define	ern_node	ern_base.ean_node

#define	RAD_MAXMSG	4096			/* max message size */

struct crypto_tfm;

struct radiuscom {
	struct eapolcom	*rc_ec;			/* back reference */
	struct socket	*rc_sock;		/* open socket */
	pid_t		rc_pid;			/* pid of client thread */
	struct sockaddr_in rc_server;		/* server's address */
	struct sockaddr_in rc_local;		/* local address */
	u_int8_t	*rc_secret;		/* shared secret */
	u_int		rc_secretlen;		/* length of shared secret */
	u_int8_t	rc_buf[RAD_MAXMSG];	/* recv thread msg buffer */
	ATH_LIST_HEAD(, eapol_auth_radius_node) rc_replies;
	/* saved copies of eapolcom methods */
	struct eapol_auth_node *(*rc_node_alloc)(struct eapolcom *);
	void		(*rc_node_free)(struct eapol_auth_node *);
	void		(*rc_node_reset)(struct eapol_auth_node *);
	/* callbacks used by the main .1x code */
	void		(*rc_identity_input)(struct eapol_auth_node *,
				struct sk_buff *);
	void		(*rc_txreq)(struct eapol_auth_node *);
	void		(*rc_sendsrvr)(struct eapol_auth_node *);
	void		(*rc_txkey)(struct eapol_auth_node *);
};
#endif /* _NET80211_RADIUS_H_ */
