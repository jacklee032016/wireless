/*
* $Id: phy_rate_onoe.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#ifndef __PHY_RATE_ONOE_H__
#define __PHY_RATE_ONOE_H__

/* per-device state */
struct onoe_softc
{
	struct ath_ratectrl 	arc;	/* base state */
	struct timer_list 		timer;	/* periodic timer */
};

/* per-node state */
struct onoe_node
{
	u_int		on_tx_ok;		/* tx ok pkt */
	u_int		on_tx_err;		/* tx !ok pkt */
	u_int		on_tx_retr;		/* tx retry count */
	int			on_tx_upper;	/* tx upper rate req cnt */
	u_int8_t		on_tx_rix0;		/* series 0 rate index */
	u_int8_t		on_tx_try0;		/* series 0 try count */
	u_int8_t		on_tx_rate0;	/* series 0 h/w rate */
	u_int8_t		on_tx_rate1;	/* series 1 h/w rate */
	u_int8_t		on_tx_rate2;	/* series 2 h/w rate */
	u_int8_t		on_tx_rate3;	/* series 3 h/w rate */
	u_int8_t		on_tx_rate0sp;	/* series 0 short preamble h/w rate */
	u_int8_t		on_tx_rate1sp;	/* series 1 short preamble h/w rate */
	u_int8_t		on_tx_rate2sp;	/* series 2 short preamble h/w rate */
	u_int8_t		on_tx_rate3sp;	/* series 3 short preamble h/w rate */
};

#define	ATH_NODE_ONOE(an)			((struct onoe_node *)&an[1])

extern	int ath_rateinterval ;
extern	int ath_rate_raise;
extern	int ath_rate_raise_threshold ;

#endif

