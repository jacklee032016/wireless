/*
* $Id: compat.h,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
 */
 
#ifndef _ATH_COMPAT_H_
#define _ATH_COMPAT_H_
/*
 * BSD/Linux compatibility shims.  These are used mainly to
 * minimize differences when importing necesary BSD code.
 */
#define	NBBY	8			/* number of bits/byte */

#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))  /* to any y */
#define	howmany(x, y)	(((x)+((y)-1))/(y))

/* Bit map related macros. */
#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

#define	__packed	__attribute__((__packed__))
#define	__printflike(_a,_b) \
	__attribute__ ((__format__ (__printf__, _a, _b)))

#ifndef ALIGNED_POINTER
/*
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define ALIGNED_POINTER(p,t)	1
#endif

#ifdef __KERNEL__
#include <asm/page.h>

#define	KASSERT(exp, msg) do {			\
	if (unlikely(!(exp))) {			\
		printk msg;			\
		BUG();				\
	}					\
} while (0)
#endif /* __KERNEL__ */

/*
 * NetBSD/FreeBSD defines for file version.
 */
#define	__FBSDID(_s)
#define	__KERNEL_RCSID(_n,_s)
#endif /* _ATH_COMPAT_H_ */
