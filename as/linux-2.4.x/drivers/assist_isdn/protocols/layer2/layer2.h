/*
 * $Id: layer2.h,v 1.1.1.1 2006/11/29 08:55:14 lizhijie Exp $
 * Layer 2 defines
 */

#include <linux/asISDNif.h>
#include <linux/skbuff.h>
#include "fsm.h"
#ifdef AS_ISDN_MEMDEBUG
#include "memdbg.h"
#endif

#define MAX_WINDOW	8	/* window size for congest control */

/* represent a independent TEI procedure, so Ri is defined for this TEI procedure and TEI value is keep in layer2_t */
typedef struct _teimgr
{
	int				ri;
	struct FsmInst	tei_m;
	struct FsmTimer	t202;
	int				T202, N202;
	int				debug;
	struct _layer2	*l2;
} teimgr_t;

typedef struct _laddr
{
	u_char	A;
	u_char	B;
} laddr_t;

typedef struct _layer2
{
	struct list_head	list;
	/* SAPI+TEI=DLCI(Connection Identifier) */
	int				sapi;	/* SAPI for this layer2 entity */
	int				tei;		/* TEI for this layer2 entity */
	laddr_t			addr;
	u_int			maxlen;
	teimgr_t			*tm;
	u_long			flag;
	
	/* Variable for Send, Ack, Receive */
	u_int			vs, va, vr;
	int				rc;
	u_int			window;		/* traffic control window size */
	u_int			sow;
	int				entity;
	struct FsmInst	l2m;
	struct FsmTimer	t200, t203;
	int				T200, N200, T203;
	int				debug;
	AS_ISDNinstance_t	inst;
	AS_ISDNif_t		*cloneif;
	int				next_id;
	u_int			down_id;

	/* skb point array for awaiting ACK : used for traffic control */
	struct sk_buff		*windowar[MAX_WINDOW]; 	
	struct sk_buff_head	i_queue;
	struct sk_buff_head	ui_queue;
	struct sk_buff_head	down_queue;
	struct sk_buff_head	tmp_queue;
	spinlock_t			lock;
} layer2_t;

/* l2 status_info */
typedef struct _status_info_l2
{
	int	len;
	int	typ;
	int	protocol;
	int	state;
	int	sapi;
	int	tei;
	laddr_t addr;
	u_int	maxlen;
	u_long	flag;
	u_int	vs;
	u_int	va;
	u_int	vr;
	int	rc;
	u_int	window;
	u_int	sow;
	int	T200;
	int	N200;
	int	T203;
	int	len_i_queue;
	int	len_ui_queue;
	int	len_d_queue;
	int	debug;
	int	tei_state;
	int	tei_ri;
	int	T202;
	int	N202;
	int	tei_debug;
} status_info_l2_t;

/* from AS_ISDN_l2.c */
extern int tei_l2(layer2_t *l2, struct sk_buff *skb);

/* from tei.c */
extern int l2_tei(teimgr_t *tm, struct sk_buff *skb);
extern int create_teimgr(layer2_t *l2);
extern void release_tei(teimgr_t *tm);
extern int TEIInit(void);
extern void TEIFree(void);

/* TEI value range defination, refer to specs. vol.1. P.180 */
#define GROUP_TEI	127		/* 127 is broadcast TEI */
/* SAPI */
#define TEI_SAPI		63		/* SAPI for management entity */
#define CTRL_SAPI	0		/* SAPI for call control procedure */

/* frame type , refer to specs, vol.1, page 185, table 3.5 */
#define RR		0x01	/* receive ready */
#define RNR		0x05	/* receive not ready */
#define REJ		0x09	/* reject */
#define SABME	0x6f	/* Set Asynchronous Balanced Mode Extended: SABM when modulus is 128  */
#define SABM		0x2f	/* SABME when modulus is 8 */
#define DM		0x0f	/* Disconnected Mode */
#define UI		0x03	/* Unnumbered Info */
#define DISC		0x43	/* Disconnect */
#define UA		0x63	/* unnumbered ack */
#define FRMR		0x87	/* Frame Reject */
#define XID		0xaf	/* Exchage Identification */

#define CMD		0
#define RSP		1

#define LC_FLUSH_WAIT 	1

#define FLG_LAPB		0
#define FLG_LAPD		1
#define FLG_ORIG		2
#define FLG_MOD128		3	/* for layer2, I frame sequence number modulus */
#define FLG_PEND_REL	4
#define FLG_L3_INIT		5
#define FLG_T200_RUN	6
#define FLG_ACK_PEND	7
#define FLG_REJEXC		8
#define FLG_OWN_BUSY	9
#define FLG_PEER_BUSY	10
#define FLG_DCHAN_BUSY	11
#define FLG_L1_ACTIV	12
#define FLG_ESTAB_PEND	13
#define FLG_PTP			14
#define FLG_FIXED_TEI	15
#define FLG_L2BLOCK		16
#define FLG_L1_BUSY		17
#define FLG_LAPD_NET	18
