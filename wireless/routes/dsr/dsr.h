#ifndef DSR_H
#define DSR_H

/* option types */
#define PAD1        0
#define PADN        1
#define ROUTE_REQ   2
#define ROUTE_REPLY 3
#define ROUTE_ERROR 4
#define ACK_REQ     5
#define ACK         6
#define SRC_ROUTE   7

/* ietf draft definitions */
#define BCAST_JITTER         20  /* milliseconds default 10 */
#define MAX_ROUTE_LEN        15  /* nodes default 15 */
#define ROUTE_CACHE_TIMEOUT  300 /* seconds */

#define SEND_BUFFER_TIMEOUT  30  /* seconds */
#define REQ_TABLE_SIZE       12  /* nodes default 64 */
#define REQ_TABLE_IDS        16
#define MAX_REQ_RXMT         16
#define MAX_REQ_PERIOD       1   /* seconds default 10 */
#define REQ_PERIOD           250 /* milliseconds default 500 */

#define DSR_RXMT_BUFFER_SIZE 300 /* packets default 50 */
#define DSR_MAXRXTSHIFT      0   /* default twice */

/* own definitions */
#define ROUTE_CACHE_SIZE 20      /* entries */
#define SEND_BUFFER_SIZE 50      /* packets */
#define NO_NEXT_HEADER   59      /* ipv6 no next header */
#define DSR_PROTOCOL     168     /* protocol number */
#define ACK_TIMEOUT_MS   300     /* milliseconds 300-500 is good */
#define ACK_TIMEOUT_JF   ((ACK_TIMEOUT_MS * HZ) / 1000)

#define DSR_SUBNET(x)     ((x & NETMASK) == NETWORK)
#define NOT_DSR_SUBNET(x) ((x & NETMASK) != NETWORK)

#endif /* DSR_H */
