/* 
 * $Id: dchannel.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * Basic declarations for dchannel HW
 */

#include <linux/asISDNif.h>
#if 0// def 0 HAS_WORKQUEUE
#include <linux/workqueue.h>
#else
#include <linux/tqueue.h>
#endif
#include <linux/smp.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/skbuff.h>
#ifdef AS_ISDN_MEMDEBUG
#include "memdbg.h"
#endif

#define MAX_DFRAME_LEN_L1	300
#define MAX_MON_FRAME		32
#define MAX_DLOG_SPACE		2048

#define FLG_TWO_DCHAN		4
#define FLG_TX_BUSY		5
#define FLG_TX_NEXT		6
#define FLG_L1_DBUSY		7
#define FLG_DBUSY_TIMER 	8
#define FLG_LOCK_ATOMIC 	9
#define FLG_ARCOFI_TIMER	10
#define FLG_ARCOFI_ERROR	11
#define FLG_HW_L1_UINT		12
#define FLG_HW_INIT		13

typedef struct _dchannel_t
{
	int			channel;
	AS_ISDNinstance_t		inst;
	u_long			DFlags;
	u_int			type;
	u_int			ph_state;
	u_char			(*read_reg) (void *, u_char);
	void			(*write_reg) (void *, u_char, u_char);
	void			(*read_fifo) (void *, u_char *, int);
	void			(*write_fifo) (void *, u_char *, int);
	char			*dlog;
	int			debug;
	struct sk_buff		*rx_skb;
	/* used for TX, because user space and BhH both can access TX, so when one is TX, another put his skb into this position */
	struct sk_buff		*next_skb;	/* is used for our request just send out, so just one skb is needed */
	u_char			*tx_buf;
	int			tx_idx;
	int             	tx_len;
	int			up_headerlen;
	int			err_crc;
	int			err_tx;
	int			err_rx;
	void			*hw;
	struct timer_list	dbusytimer;
	u_long			event;
	struct sk_buff_head	rqueue; /* D-channel receive queue */
//	struct work_struct	work;
	struct tq_struct	work;
	void			(*hw_bh) (struct _dchannel_t *);
} dchannel_t;

#define MON0_RX	1
#define MON1_RX	2
#define MON0_TX	4
#define MON1_TX	8

extern int as_isdn_init_dch(dchannel_t *);
extern int as_isdn_free_dch(dchannel_t *);

static inline void dch_set_para(dchannel_t *dch, AS_ISDN_stPara_t *stp)
{
	if (stp)
	{
		dch->up_headerlen = stp->up_headerlen;
//		printk(KERN_ERR"DCH up header length =%d\n", dch->up_headerlen);
	}
	else
		dch->up_headerlen = 0;
}

static inline void dchannel_sched_event(dchannel_t *dch, int event)
{
	if(!test_and_set_bit(event, &dch->event) )
	{
		schedule_work(&dch->work);
	}	
}
