#ifndef DSR_KMODULE_H
#define DSR_KMODULE_H

#include <linux/kernel.h>
#include <linux/module.h>

#ifdef __KERNEL__

#include <linux/netfilter_ipv4.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <net/checksum.h>
#include <net/ip.h>

#include "dsr_debug.h"
#include "dsr_header.h"
#include "dsr_route.h"
#include "dsr_input.h"
#include "dsr_output.h"
#include "dsr_queue.h"

nf_hookfn pre_route_handler;
nf_hookfn local_out_handler;
nf_hookfn gw_post_route_handler;
nf_hookfn gw_post_route_out_handler;
nf_hookfn post_route_handler;
nf_hookfn local_in_handler;
int dsr_read_proc(char * page, char ** start, off_t off, int count,
		  int * eof, void * data);

#endif /* __KERNEL__ */
#endif /* DSR_KMODULE_H */
