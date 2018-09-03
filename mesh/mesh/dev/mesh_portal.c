/*
 * $Id: mesh_portal.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
/*
* create net_device for MESH Portal Interface
*/
#include <net/bridge/br_private.h>

#include "mesh.h"
#if __ARM_IXP__
#include <linux/random.h>
#endif
#include <linux/rtnetlink.h>			/* rtnlock */
#include <linux/etherdevice.h>		/* eth_tran_type */

#include "wlan_if_media.h"
#include <ieee80211_var.h>
#include <if_llc.h>
#include "if_ethersubr.h"


#define VMAC_MIN_MTU 			68
#define VMAC_MAX_MTU 			2290

enum
{
	VNETIF_RX_PACKET_OK 		= 0,
	VNETIF_RX_PACKET_BUSY 	= 1,
	VNETIF_RX_PACKET_ERROR 	= 2,
};

static int __portal_net_open(struct net_device *dev)
{
	int result = 0;
#if 0	
	MESH_PORTAL *portal;
	MESH_DEVICE *mesh;

	if(!dev)
		return -ENODEV;
	portal = (MESH_PORTAL *)dev->priv;
	mesh =  &portal->mesh;
	if(!portal || !mesh)
		return -ENODEV;
#endif	
	netif_start_queue(dev);
	
	return result;
}

static int __portal_net_stop(struct net_device *dev)
{
	int result = 0;

	netif_stop_queue(dev);

	return result;
}

static void __portal_net_tx_timeout(struct net_device *dev)
{
	MESH_WARN_INFO("net device of Portal timeout on %s!\n", dev->name );
}

static void __portal_net_mclist(struct net_device *dev)
{
	/* Nothing to do for multicast filters.  We always accept all frames.    */
	return;
}

static struct net_device_stats *__portal_net_stats(struct net_device *dev)
{
	struct net_device_stats *nds = 0;
	MESH_PORTAL *portal;

	if(!dev)
		return NULL;
	portal = (MESH_PORTAL *)dev->priv;
	if(!portal )
		return NULL;

	nds = &portal->netStats;
	return nds;
}

static int __portal_net_change_mtu(struct net_device *dev, int new_mtu)
{
	int error=0;

//	MESH_PORTAL *portal = (MESH_PORTAL *)dev->priv;
	if (!(VMAC_MIN_MTU <= new_mtu && new_mtu <= VMAC_MAX_MTU))
	{
		MESH_WARN_INFO(" - %s: Invalid MTU size, %u < MTU < %u\n", dev->name, VMAC_MIN_MTU, VMAC_MAX_MTU);
		return -EINVAL;
	}

	/* add lock for protect */
	dev->mtu = new_mtu;

        return error;
}


static int __portal_read_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data)
{
	MESH_DEVICE	*dev = (MESH_DEVICE *)data;
	MESH_PORTAL	*portal = (MESH_PORTAL *)dev->priv;
	int 				length = 0;
//	unsigned long  	remainder = 0, numerator = 0;
	
	if(!dev )
	{
		MESH_WARN_INFO("Read Procedure of PORTAL is not allocated\n");
		return 0;
	}
	
	length += sprintf(buffer+length, "\nPortal Interface Initialized\n\n" );
	length += sprintf(buffer+length, "\n\tMESH Device \t: %s\t\t%s", dev->name, swjtu_mesh_mac_sprintf(dev->dev_addr ) );
	length += sprintf(buffer+length, "\n\tNet Device \t: %s\t\t%s", portal->net.name , swjtu_mesh_mac_sprintf(portal->net.dev_addr));
	length += sprintf(buffer+length, "\n\n");

	*buffer_location = buffer + offset;
	length -= offset;
	if (length > buffer_length)
		length = buffer_length;
	else if (length < 0)
		length = 0;

	return length;

}

int swjtu_remove_portal(MESH_MGR *mgr, void *data)
{
	MESH_PORTAL *portal;
	int	error = 0;

	if(!mgr ||!mgr->portal )
		return -ENOENT;

	portal = mgr->portal;

	swjtu_mesh_unregister_port(&portal->mesh);
	MESH_DEBUG_INFO(MESH_DEBUG_PORTAL, "remove portal device\n");
	mgr->portal = NULL;
	
	rtnl_lock();
	error = unregister_netdevice(&(portal->net));
	rtnl_unlock();

	if(error)
	{
		MESH_WARN_INFO("Unregister net device of MESH Portal failed: result == %d\n", error);
//		return error;
	}
	
//	kfree(portal);

	return error;
}

#if 0
int	__portal_init_bridge(MESH_PORTAL *portal)
{
	int i = 0;
	char				bridgeName[MESHNAMSIZ];
	struct net_device	*dev;
	MESH_BRIDGE	*bridge, *header;
	int		res = 0;

	header = portal->mbridges;
	read_lock(&dev_base_lock);
	for (dev = dev_base; dev != NULL; dev = dev->next)
	{
		if (strncmp(dev->name, "lo", IFNAMSIZ) )
		{
			sprintf(bridgeName, "%s%d", MESH_BRIDGE_NAME, i);
			MESH_MALLOC(bridge, MESH_BRIDGE, sizeof(MESH_BRIDGE), MESH_M_NOWAIT|MESH_M_ZERO, bridgeName);
			if(!bridge)
				return -ENOMEM;

			if(!portal->mbridges) 
				portal->mbridges = bridge;
			else
			{
				bridge->next = portal->mbridges;
				portal->mbridges = bridge;
			}

			{
//				sprintf(bridgeName, "%s", dev->name);
				unsigned long arg[3] = { BRCTL_ADD_BRIDGE, (unsigned long) bridgeName };
				if( (res =br_ioctl_hook(arg) ) )
				{
					MESH_ERR_INFO("Add MESH BRIDGE for %s device is failed\n", dev->name);
					
					break;
				}
				bridge->master = dev;
//				dev_hold(dev);
			}	
		}
	}
	
	read_unlock(&dev_base_lock);
	return res;
}
#endif


/*
 */
static struct sk_buff *__skbhdr_adjust_4_wifi(int hdrsize, struct sk_buff *skb)
{
	int need_headroom = hdrsize;
	int need_tailroom = 0;

	need_headroom += sizeof(struct llc);
	skb = skb_unshare(skb, GFP_ATOMIC);
	if (skb == NULL)
	{
		MESH_DEBUG_INFO(MESH_DEBUG_DATAOUT, "%s: cannot unshare for encapsulation\n", __func__);
	}
	else if (skb_tailroom(skb) < need_tailroom)
	{
		int headroom = skb_headroom(skb) < need_headroom ?need_headroom - skb_headroom(skb) : 0;

		if (pskb_expand_head(skb, headroom,
			need_tailroom - skb_tailroom(skb), GFP_ATOMIC))
		{
			dev_kfree_skb(skb);
			MESH_DEBUG_INFO(MESH_DEBUG_DATAOUT,  "%s: cannot expand storage (tail)\n", __func__);
			skb = NULL;
		}
	}
	else if (skb_headroom(skb) < need_headroom)
	{
		struct sk_buff *tmp = skb;
		skb = skb_realloc_headroom(skb, need_headroom);
		dev_kfree_skb(tmp);
		if (skb == NULL)
		{
			MESH_DEBUG_INFO(MESH_DEBUG_DATAOUT,  "%s: cannot expand storage (head)\n", __func__);
		}
	}
	return skb;
}

/* mesh_device(portal).start_xmit() : called by fwd SIRQ and send to net_device 
* parse 802.11 header and construct ether net header 
*/
static int _portal_mesh_dev_xmit(struct sk_buff *skb, MESH_DEVICE *mesh)
{
	struct ieee80211_frame 	wh;
	struct ether_header 		*eh;
	struct llc 				*llc;
	u_short 					ether_type = 0;
	MESH_PORTAL	*portal = mesh->mgr->portal;
	struct net_device	*net ;
	assert(portal);

	net = &portal->net;
	assert(net);
	
	// TODO: why not using linux llc.h ..., same for ieee80211_encap()
//	MESH_DEBUG_INFO(MESH_DEBUG_DATAIN, "skb addrss %p -- ", skb);
//	swjtu_mesh_dump_rawpkt( (caddr_t) skb->data, skb->len, MESH_DEBUG_DATAOUT, "Frame from Mesh to Portal");

	memcpy(&wh, skb->data, sizeof(struct ieee80211_frame));
	llc = (struct llc *) skb_pull(skb, sizeof(struct ieee80211_frame));
	
	if (skb->len >= sizeof(struct llc) &&
		llc->llc_dsap == LLC_SNAP_LSAP && llc->llc_ssap == LLC_SNAP_LSAP &&
		llc->llc_control == LLC_UI && llc->llc_snap.org_code[0] == 0 &&
		llc->llc_snap.org_code[1] == 0 && llc->llc_snap.org_code[2] == 0)
	{
		ether_type = llc->llc_un.type_snap.ether_type;
		skb_pull(skb, sizeof(struct llc));
		llc = NULL;

//		MESH_DEBUG_INFO(MESH_DEBUG_DATAOUT, "this is a LLC encapsulated frame\n");
	}
	
	eh = (struct ether_header *) skb_push(skb, sizeof(struct ether_header));
	switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK)
	{
		case IEEE80211_FC1_DIR_NODS:
			IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
			IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
			break;
		case IEEE80211_FC1_DIR_TODS:
			IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
			IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
			break;
		case IEEE80211_FC1_DIR_FROMDS:
			IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
			IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr3);
			break;
		case IEEE80211_FC1_DIR_DSTODS:
			/* not yet supported */
			MESH_WARN_INFO( "Data Frame of DS to DS not supported now\n");
			portal->netStats.rx_dropped++;
			dev_kfree_skb(skb);
			return -1; 
	}
//	swjtu_mesh_dump_rawpkt( (caddr_t) skb->data, skb->len);
	
	/*
	 *  TODO: we should ensure alignment on specific arch like amd64, alpha 
	 *  and ia64 at least. BSD uses ALIGNED_POINTER macro for this.
	 *  If anyone gets problems on one of the mentioned archs, modify compat.h
	 */
#if 0//def ALIGNED_POINTER
	if (!ALIGNED_POINTER(skb->data + sizeof(*eh), u_int32_t))
	{
		struct sk_buff *n;

		/* XXX does this always work? */
		n = skb_copy(skb, GFP_ATOMIC);
		dev_kfree_skb(skb);
		if (n == NULL)
			return -ENOMEM;//NULL;
		skb = n;
		eh = (struct ether_header *) skb->data;
	}
#endif /* ALIGNED_POINTER */

	if (llc != NULL)
		eh->ether_type = htons(skb->len - sizeof(*eh));
	else
		eh->ether_type = ether_type;

//	skb->pkt_type = PACKET_HOST;
	skb->dev = net;
	skb->protocol = eth_type_trans(skb, net);

	MESH_DEBUG_INFO(MESH_DEBUG_DATAIN, "Data Frame are send to Layer 3 Stack -- :");
	swjtu_mesh_dump_rawpkt( (caddr_t) skb->data, skb->len, MESH_DEBUG_DATAOUT, "Frame send into net device");

	portal->netStats.rx_packets++;
	portal->netStats.rx_bytes += skb->len;

	return netif_rx(skb );
}

/* Function handed over as the "hard_start" element in the network device structure. 
* rx frame from IP stack and send into mesh device meshif_rx
*/
static int _portal_net_dev_xmit(struct sk_buff  *skb, struct net_device *dev)
{
	int txresult = 0;
	MESH_PORTAL *portal;
	MESH_DEVICE *mesh;
	struct ether_header eh;
	struct llc *llc;
	struct ieee80211_frame *wh;
	int hdrsize, datalen;//, addqos;

//	MESH_DEBUG_INFO(MESH_DEBUG_DATAOUT, "\n\nframe send by Layer 3 Stack (Portal)\n" );
//	swjtu_mesh_dump_rawpkt((caddr_t) skb->data, skb->len, MESH_DEBUG_DATAIN, "Frame send from net into mesh portal");
	if(!dev)
		return -ENODEV;
	portal = (MESH_PORTAL *)dev->priv;
	KASSERT( portal != NULL, ("Portal is null!") );
	mesh =  &portal->mesh;
	KASSERT((mesh!=NULL), ("mesh is null!") );

	if(!portal || !mesh)
	{
		return -ENODEV;
	}

	if( (mesh->flags&MESHF_RUNNING)!= MESHF_RUNNING)
	{
		/* skb must be freed by up layer drivers ,lzj */
//		dev_kfree_skb(skb);
		MESH_DEBUG_INFO(MESH_DEBUG_DATAOUT, "frame is dropped because of mesh is not running now\n" );
		return -1;
	}
	
	KASSERT(skb->len >= sizeof(eh), ("no ethernet header!"));
	memcpy(&eh, skb->data, sizeof(struct ether_header));
	skb_pull(skb, sizeof(struct ether_header));

	hdrsize = sizeof(struct ieee80211_frame);
	
//	if (ic->ic_flags & IEEE80211_F_DATAPAD)
	hdrsize = roundup(hdrsize, sizeof(u_int32_t));

	skb = __skbhdr_adjust_4_wifi(hdrsize, skb);
	if (skb == NULL)
	{
		goto bad;
	}
	
	llc = (struct llc *) skb_push(skb, sizeof(struct llc));
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh.ether_type;
	datalen = skb->len;		/* NB: w/o 802.11 header */

	wh = (struct ieee80211_frame *)skb_push(skb, hdrsize);

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *)wh->i_dur = 0;

#if 0	
	/* STA mode */
	wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
	IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_bssid);
	IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
	IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
#endif

	/* Ad Hoc Mode */
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);	/* next hop address */
	IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
//	IEEE80211_ADDR_COPY(wh->i_addr3, ic->ic_bss->ni_bssid);
	IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost );
#if 0
	/* AP mode */
	wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
	IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
	IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
	IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);

#endif

	skb->dev = (struct net_device *)mesh;
/*
	MESH_DEBUG_INFO(MESH_DEBUG_DATAOUT, "frame send into MESH --:" );
	swjtu_mesh_dump_rawpkt((caddr_t) skb->data, skb->len, MESH_DEBUG_DATAIN, "Frame send before into mesh");
*/

	portal->netStats.tx_packets++;
	portal->netStats.tx_bytes += skb->len;

	return swjtu_meshif_rx(skb);
bad:
	if (skb != NULL)
	{
		portal->netStats.tx_dropped++;
		dev_kfree_skb(skb);
		skb = NULL;
		txresult = 0;
	}
	return txresult;
}


static int __portal_startup(MESH_DEVICE *dev, int isStartup)
{
#define	NET_IS_UP(_dev) \
	(((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))

	int	error = 0;
	MESH_PORTAL *portal = (MESH_PORTAL *)dev->priv;
	struct net_device *net = &portal->net;


	if(isStartup )
	{
		MESH_DEBUG_INFO(MESH_DEBUG_PORTAL, "Portal Device is startup\n" );
		if(!net->br_port ||!net->br_port->br )
		{
			MESH_WARN_INFO("net device %s is not a Bridge Port\n", net->name);
			return -ENODEV;
		}
		
		memcpy(dev->dev_addr, net->br_port->br->bridge_id.addr, ETH_ALEN);
		
		if(!NET_IS_UP( net) )
		{
			net->flags |= IFF_RUNNING|IFF_UP;
			dev->flags |= MESHF_RUNNING|MESHF_UP;
		}

		netif_carrier_on(net);
		MESH_DEBUG_INFO(MESH_DEBUG_PORTAL, "Portal Device is startup\n\n\n" );
		return error;
	}
	else
	{/* stop device */
		netif_carrier_off(net);
		
		memcpy(dev->dev_addr, 0, ETH_ALEN);
		
		if(!NET_IS_UP( net) )
		{
			net->flags &= ~IFF_RUNNING;
			net->flags &= ~IFF_UP;
			dev->flags &= ~MESHF_RUNNING;
			dev->flags &= ~MESHF_UP;
		}
		MESH_DEBUG_INFO(MESH_DEBUG_PORTAL, "Portal Device is stop\n" );
		return error;
	}

	return 0;
}

/*   get mac address from bridge and set into portal->mesh; 
*     make portal->net as RUNNING state to fwd packet
*     enable all PHY and MAC layer drivers 
*/
static int _portal_mesh_ioctl(MESH_DEVICE *dev, unsigned long data, int cmd)
{
	int	error = 0;
	MESH_PORTAL *portal = (MESH_PORTAL *)dev->priv;
	struct net_device *net ;
	if(!portal )
	{
		MESH_WARN_INFO("MESH device %s is not a portal device\n", dev->name);
		return -ENODEV;
	}

	net = &portal->net;
	KASSERT(net!=NULL, ("MESH NET Device") );

	switch(cmd)
	{
		case SWJTU_PHY_STARTUP:
		{
			error = __portal_startup(dev, 1);
			break;
		}	
		case SWJTU_PHY_STOP:
		{
			error = __portal_startup(dev, 0 );
			break;
		}	
		default:
			MESH_WARN_INFO("IOCTL Command %x is not implemented in Portal\n", cmd);
			break;
	}

	return error;
}

static MESH_DEVICE_STATS *__portal_mesh_stats(MESH_DEVICE *mesh)
{
	MESH_DEVICE_STATS 	*stats = 0;
	MESH_PORTAL 		*portal;

	if(!mesh)
		return NULL;
	portal = (MESH_PORTAL *)mesh->priv;
	if(!portal )
		return NULL;

	stats = &portal->meshStats;
	return stats;
}


/*  */
int swjtu_create_portal(MESH_MGR *mgr, void *data)
{
	MESH_PORTAL	*portal;
	MESH_DEVICE	*mesh;
	struct net_device	*meshNet;//, *masterDev;
//	unsigned char		randommacaddr[ETH_ALEN];
	int	error = 0;

	if(mgr->portal)
		return -EEXIST;
	
	MESH_MALLOC( portal, (MESH_PORTAL), sizeof(MESH_PORTAL), MESH_M_NOWAIT|MESH_M_ZERO, "MESH PORTAL DEVICE");
	if(!portal)
		return -ENOMEM;
	
	mesh = &portal->mesh;
	meshNet = &(portal->net);

#if 0
		if (!macaddr)
		{
			macaddr = randommacaddr;
			*((u_int16_t *)macaddr) = htons(0x00FF);
			get_random_bytes(macaddr + 2, 4);
		}

		newinst->txFunc = txfunc;
		newinst->txFuncPriv = txfunc_priv;
		write_unlock(&(newinst->devLock));
#endif

	ether_setup(meshNet);
	strncpy(meshNet->name, SWJTU_MESH_DEV_NAME, MESHNAMSIZ);
	
	/* after wifi device added, mgr->id.meshAddr has been set as wifi's MAC address */
	memcpy(meshNet->dev_addr,mgr->id.meshAddr, ETH_ALEN );

	meshNet->open = __portal_net_open;
	meshNet->stop = __portal_net_stop;
	meshNet->hard_start_xmit = _portal_net_dev_xmit;
	meshNet->tx_timeout = __portal_net_tx_timeout;
	meshNet->watchdog_timeo = 5 * HZ;			/* XXX */
	meshNet->set_multicast_list = __portal_net_mclist;
	meshNet->get_stats = __portal_net_stats;
	meshNet->change_mtu = __portal_net_change_mtu;


	rtnl_lock();
	error = register_netdevice( meshNet);
	rtnl_unlock();

	if(error)
	{
		kfree(portal);
		MESH_WARN_INFO("unable to register netdev of portal interface!\n");
		return error;
	}

#if WITH_QOS
#else
	skb_queue_head_init(&mesh->txPktQueue );
#endif

	strncpy(mesh->name, SWJTU_MESH_NAME_PORTAL, MESHNAMSIZ);
	mesh->procEntry.read = __portal_read_proc;
	sprintf(mesh->procEntry.name, "%s", mesh->name );
	
	mesh->hard_start_xmit = _portal_mesh_dev_xmit;
	mesh->do_ioctl = _portal_mesh_ioctl;
	mesh->get_stats = __portal_mesh_stats;
	
	mesh->priv = (void *)portal;
	mesh->type = MESH_TYPE_PORTAL;

	memcpy(mesh->dev_addr, mgr->id.meshAddr, ETH_ALEN );

	error = swjtu_mesh_register_port(mesh);
	if(error )
	{
		rtnl_lock();
		unregister_netdevice(meshNet);
		rtnl_unlock();
		return error;
	}

	meshNet->priv = portal;
	mesh->priv = portal;
	
	mgr->portal = portal;
	portal->mgr = mgr;
	
	return 0;
}

/* is called by forward tasklet when output this packet for portal interface which is get from mgr->portal 
* in order to optimzed performance, call the net_device->netif_rx directly, so bypass mesh device when send to portal
*/
int portal_netif_rx(void *handler, struct sk_buff* packet)
{
	int result = VNETIF_RX_PACKET_OK;
	MESH_PORTAL *portal = (MESH_PORTAL *)handler;
	struct net_device  *dev = &portal->net;

	packet->dev = dev;
	packet->mac.raw = packet->data;

		//packet->nh.raw = packet->data + sizeof(struct ether_header);
	packet->protocol = eth_type_trans(packet,dev);

	/* decap the frame from WLAN to ethernet */	
//	if (inst->devOpen)
		{
			MESH_DEBUG_INFO(MESH_DEBUG_DATAIN, "netif of mesh : receiving packet!\n");
			netif_rx(packet);
		}
#if 0
	else
	{
		dev_kfree_skb_any(packet);
		result = VNETIF_RX_PACKET_ERROR;
	}
#endif

	return result;
}


