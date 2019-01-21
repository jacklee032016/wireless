#ifndef DSR_DEBUG_H
#define DSR_DEBUG_H

#include <linux/kernel.h>
#include <linux/module.h>

#ifdef __KERNEL__

/* dump_skb includes */
#include <net/ip.h>
#include <net/route.h>
#include <net/tcp.h>
#include <linux/netfilter_ipv4.h>

#define DSR_ASSERT(x)                                           \
do {                                                            \
	if (!(x)) {                                             \
		printk("DSR_ASSERT: %s:%s:%u\n",                \
		       __FUNCTION__, __FILE__, __LINE__);       \
        }                                                       \
} while(0)


//#define DSR_DEBUG

void dump_skb(int pf, struct sk_buff *skb);
void dump_skb_ptr(struct sk_buff *skb);

#endif /* __KERNEL__ */
#endif /* DSR_DEBUG_H */
