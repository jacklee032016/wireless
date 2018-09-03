/*
* $Id: amrr.h,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#ifndef _DEV_ATH_RATE_AMRR_H
#define _DEV_ATH_RATE_AMRR_H

/* per-device state */
struct amrr_softc {
	struct ath_ratectrl arc;	/* base state */
	struct timer_list timer;	/* periodic timer */
};

/* per-node state */
struct amrr_node {
  	/* AMRR statistics for this node */
  	u_int           amn_tx_try0_cnt;
  	u_int           amn_tx_try1_cnt;
  	u_int           amn_tx_try2_cnt;
  	u_int           amn_tx_try3_cnt;
  	u_int           amn_tx_failure_cnt; 
        /* AMRR algorithm state for this node */
  	u_int           amn_success_threshold;
  	u_int           amn_success;
  	u_int           amn_recovery;
	/* rate index et al. */
	u_int8_t	amn_tx_rix0;	/* series 0 rate index */
	u_int8_t	amn_tx_rate0;	/* series 0 h/w rate */
	u_int8_t	amn_tx_rate1;	/* series 1 h/w rate */
	u_int8_t	amn_tx_rate2;	/* series 2 h/w rate */
	u_int8_t	amn_tx_rate3;	/* series 3 h/w rate */
	u_int8_t	amn_tx_rate0sp;	/* series 0 short preamble h/w rate */
	u_int8_t	amn_tx_rate1sp;	/* series 1 short preamble h/w rate */
	u_int8_t	amn_tx_rate2sp;	/* series 2 short preamble h/w rate */
	u_int8_t	amn_tx_rate3sp;	/* series 3 short preamble h/w rate */
	u_int8_t	amn_tx_try0;	/* series 0 try count */
  	u_int           amn_tx_try1;    /* series 1 try count */
  	u_int           amn_tx_try2;    /* series 2 try count */
  	u_int           amn_tx_try3;    /* series 3 try count */
};
#define	ATH_NODE_AMRR(an)	((struct amrr_node *)&an[1])
#endif /* _DEV_ATH_RATE_AMRR_H */
