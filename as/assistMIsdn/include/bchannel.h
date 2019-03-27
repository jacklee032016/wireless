/*
 * $Id: bchannel.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * Basic declarations, defines for Bchannel hardware
 */
#ifndef  __BCHANNEL_H__
#define __BCHANNEL_H__

#include "asISDN.h"
#if 0// def HAS_WORKQUEUE
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

#define MAX_BLOG_SPACE		256

#define BC_FLG_INIT			1
#define BC_FLG_ACTIV		2
#define BC_FLG_TX_BUSY		3
#define BC_FLG_NOFRAME		4
#define BC_FLG_HALF			5
#define BC_FLG_EMPTY		6
#define BC_FLG_ORIG			7
#define BC_FLG_DLEETX		8
#define BC_FLG_LASTDLE		9
#define BC_FLG_FIRST		10
#define BC_FLG_LASTDATA	11
#define BC_FLG_NMD_DATA	12
#define BC_FLG_FTI_RUN		13
#define BC_FLG_LL_OK		14
#define BC_FLG_LL_CONN		15
#define BC_FLG_TX_NEXT		16
#define BC_FLG_DTMFSEND	17

#define BC_FLG_WAIT_DEACTIVATE		18


typedef struct _AS_ISDN_RAWdevice	as_isdn_rawdev_t;
typedef struct _bchannel_t				 bchannel_t;


struct _bchannel_t
{
	int				channel;
	int				protocol;
	u_long			Flag;
	int				debug;
	/* use B channel only as raw device */
#if 0	
	as_isdn_stack_t		*st;
	as_isdn_instance_t	inst;
#endif

	/* use raw device only, lizhijie,2005.12.22 */
//	as_isdn_device_t	*dev;
	as_isdn_rawdev_t	*dev;

	int (*down_if)(as_isdn_if_t *hif, struct sk_buff *skb);
	int (*file_ioctl)(bchannel_t *bch, unsigned int cmd, unsigned long data);
	/* end of added */
	
	void				*hw;
	u_char			(*Read_Reg)(void *, int, u_char);
	void				(*Write_Reg)(void *, int, u_char, u_char);

	/* used for TX, because user space and BhH both can access TX, so when one is TX, another put his skb into this position */
//	struct sk_buff	*next_skb;	/* is used for our request just send out, so just one skb is needed */
	u_char			*tx_buf;
	int				tx_idx;
	int             		tx_len;
	u_char			*rx_buf;
	int				rx_idx;
	struct sk_buff_head	rqueue;	/* B-Channel receive Queue for received request(indication in ISDN term )*/
	u_char			*blog;
	u_char			*conmsg;
	struct timer_list	transbusy;
//	struct work_struct	work;
	struct tq_struct	work;
	void				(*hw_bh) (struct _bchannel_t *);
	u_long			event;
	int				maxdatasize;
	int				up_headerlen;
	int				err_crc;
	int				err_tx;
	int				err_rdo;
	int				err_inv;

	void				*private;		/* point ot hfc_t */
};

/* the user interface to handle B Channel in raw device */
struct _AS_ISDN_RAWdevice
{
	struct list_head		list;
	int					minor;
//	struct semaphore		io_sema;
	struct semaphore		in_sema;						/* READ/WRITE simutaneously, lizhjie,2006.1.1 */
	struct semaphore		out_sema;
	struct semaphore		ioctrl_sema;						/* added for IOCTL operation,2006.1.1 */
	int					open_mode;
	as_isdn_port_t		rport;
	as_isdn_port_t		wport;
	/* upper fields must be same as as_isdn_device_t */


	bchannel_t 			*bch;
	/* data for event ,lizhijie, 2005.12.22 , for DTMF detect */
	int					eventinidx;  							/* out index in event buf (circular) */
	int					eventoutidx;  						/* in index in event buf (circular) */
	unsigned int			eventbuf[AS_ISDN_MAX_EVENTSIZE];  	/* event circ. buffer */
	wait_queue_head_t 	eventProcQ; 							/* event wait queue */
};


extern int as_isdn_init_bch(bchannel_t *);
extern int as_isdn_free_bch(bchannel_t *);

static inline void bch_set_para(bchannel_t *bch, AS_ISDN_stPara_t *stp)
{
	if (stp)
	{
		bch->maxdatasize = stp->maxdatalen;
//		bch->up_headerlen = stp->up_headerlen;
		bch->up_headerlen = 16;//stp->up_headerlen;
//		printk(KERN_ERR"BCH up header length =%d\n", bch->up_headerlen);
	}
	else
	{
		bch->maxdatasize = 0;
		bch->up_headerlen = 0;
	}
}

static inline void bch_sched_event(bchannel_t *bch, int event)
{
#if 0
	test_and_set_bit(event, &bch->event);
	schedule_work(&bch->work);
#endif

	if(!test_and_set_bit(event, &bch->event) )
	{
		schedule_work(&bch->work);
	}	

}

as_isdn_rawdev_t *as_rawdev_get_by_minor(int minor);

extern	int 	as_isdn_register_bchannel(bchannel_t *bchan);
extern	void as_isdn_unregister_bchannel(bchannel_t *bchan);

#endif

