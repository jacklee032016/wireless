/*
 * $Id: capidev.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

struct capidev {
	struct capidev *next;
	struct file    *file;
	__u16		applid;
	__u16		errcode;
	unsigned int    minor;

	struct sk_buff_head recv_queue;
	wait_queue_head_t recv_wait;

	/* Statistic */
	unsigned long	nrecvctlpkt;
	unsigned long	nrecvdatapkt;
	unsigned long	nsentctlpkt;
	unsigned long	nsentdatapkt;
};
