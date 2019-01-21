#ifndef DSR_HEADER_H
#define DSR_HEADER_H

#include <asm/byteorder.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include "dsr.h"

struct dsr_hdr {
  __u8  nexthdr;
  __u8  reserved;
  __u16 length;
  /* options start here */
};

struct dsr_opt_hdr {
  __u8  type;
  __u8  len;
};

struct dsr_rt_req_opt {
  __u8  type;
  __u8  len;
  __u16 ident;
  __u32 taddr;
  __u32 addr[MAX_ROUTE_LEN];
};

struct dsr_rt_reply_opt {
  __u8  type;
  __u8  len;
#if defined(__LITTLE_ENDIAN_BITFIELD)
  __u8  reserved:7, lasthopx:1;
#elif defined (__BIG_ENDIAN_BITFIELD)
  __u8  lasthopx:1, reserved:7;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
  __u16 ident;
  __u32 addr[MAX_ROUTE_LEN];
} __attribute__ ((packed));

struct dsr_rt_err_opt {
  __u8  type;
  __u8  len;
  __u8  errtype;
#if defined(__LITTLE_ENDIAN_BITFIELD)
  __u8  salvage:4, reserved:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
  __u8  reserved:4, salvage:4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
  __u32 errsaddr;
  __u32 errdaddr;
  __u32 typeinfo; /* need to change for future route errors */
};

struct dsr_ack_req_opt {
  __u8  type;
  __u8  len;
  __u16 ident;
  __u32 saddr;
};

struct dsr_ack_opt {
  __u8  type;
  __u8  len;
  __u16 ident;
  __u32 saddr;
  __u32 daddr;
};

struct dsr_src_rt_opt {
  __u8  type;
  __u8  len;
#if defined(__LITTLE_ENDIAN_BITFIELD)
  __u16 segs_left:6, salvage:4, reserved:4, lasthopx:1, firsthopx:1; 
#elif defined (__BIG_ENDIAN_BITFIELD)
  __u16 firsthopx:1, lasthopx:1, reserved:4, salvage:4, segs_left:6;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
  __u32 addr[MAX_ROUTE_LEN];
};  

struct dsr_pad1_opt {
  __u8 type;
};

struct dsr_padn_opt {
  __u8  type;
  __u8  len;
  /* zero filled data */
};

/* route cache structures */
struct rt_entry {
  time_t        time;  /* in seconds */
  unsigned char segs_left;
  __u32         addr[MAX_ROUTE_LEN];
  __u32         dst;
};

struct id_fifo {
  __u32         saddr;
  time_t        time;  /* in seconds */
  __u16         id[REQ_TABLE_IDS];
  int           head;
  int           tail;
  int           len;
};

struct rt_req_entry {
  __u32             taddr;
  unsigned long     time;   /* in jiffies */
  unsigned long     timeout;
  int               n_req;
  /* timer_list is 20 bytes */
  struct timer_list timer;
};

/* the following structs has to be smaller than 48 bytes */
struct rxmt_info {
  __u16                   ident;
  unsigned int            n_rxmt;
  struct dsr_src_rt_opt * srcrt;
  /* timer_list is 20 bytes */
  struct timer_list       timer;
};

struct jitter_send {
  /* timer_list is 20 bytes */
  struct timer_list timer;
};

struct send_info {
  __u32 fwdaddr;
  int (*output)(struct sk_buff *);
  /* timer_list is 20 bytes */
  struct timer_list timer;
};
#endif /* DSR_HEADER_H */
