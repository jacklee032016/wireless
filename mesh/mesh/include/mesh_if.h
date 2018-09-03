/*
* $Id: mesh_if.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/


/* Standard interface flags (netdevice->flags). */
#define	MESHF_UP					0x1		/* interface is up		*/
#define	MESHF_BROADCAST			0x2		/* broadcast address valid	*/
#define	MESHF_DEBUG				0x4		/* turn on debugging		*/
#define	MESHF_LOOPBACK			0x8		/* is a loopback net		*/
#define	MESHF_POINTOPOINT		0x10		/* interface is has p-p link	*/
#define	MESHF_NOTRAILERS			0x20		/* avoid use of trailers	*/
#define	MESHF_RUNNING				0x40		/* resources allocated		*/
#define	MESHF_NOARP				0x80		/* no ARP protocol		*/
#define	MESHF_PROMISC				0x100		/* receive all packets		*/
#define	MESHF_ALLMULTI			0x200		/* receive all multicast packets*/

#define	MESHF_MASTER				0x400		/* master of a load balancer 	*/
#define	MESHF_SLAVE				0x800		/* slave of a load balancer	*/

#define	MESHF_MULTICAST			0x1000		/* Supports multicast		*/

