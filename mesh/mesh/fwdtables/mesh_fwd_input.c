/*
 * $Id: mesh_fwd_input.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "_fwd.h"
#include "wlan_if_media.h"
#include <ieee80211_var.h>

#if 0
unsigned char bridge_ula[6] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };

/* send to portal device */
static void __pass_frame_up(MESH_FWD_TABLE *table, struct sk_buff *skb)
{
	MESH_DEVICE *indev, *portal;

	table->mib.rxPackets++;
	table->mib.rxBytes += skb->len;

	indev = (MESH_DEVICE *)skb->dev;
//	skb->dev = &br->dev;
	portal = &table->mgr->portal->mesh;
	if(indev == portal )
	{
		MESH_ERR_INFO("FWD loop detect on portal device\n");
		kfree_skb(skb);

		return;
	}
	
	skb->dev = &table->mgr->portal->net;

#if 0	
	skb->pkt_type = PACKET_HOST;
	skb_push(skb, ETH_HLEN);
//	skb->protocol = eth_type_trans(skb, &br->dev);
	skb->protocol = eth_type_trans(skb, &table->mgr->portal->net);

//	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_IN, skb, indev, NULL,
	br_pass_frame_up_finish(skb);
#endif
	portal->hard_start_xmit(skb, portal);
}
#endif

int mesh_fwd_handle_frame_finish(struct sk_buff *skb)
{
	MESH_FWD_TABLE *table;
	MESH_DEVICE	*dev;
	MESH_FWD_ITEM 	*destItem;
	unsigned char *destMac, *sourceMac;
	struct ieee80211_frame *wh;
	int passedup;
	wh = (struct ieee80211_frame *)skb->data;

//	destMac = skb->mac.ethernet->h_dest;
#if 0
	destMac = wh->i_addr3;// address 3 used as destination address 
#else
	destMac = wh->i_addr1;
#endif
	sourceMac = wh->i_addr2;

	dev = (MESH_DEVICE *)skb->dev;
	if(dev == NULL)
	{
		MESH_ERR_INFO("No MESH DEVICE is assigned to skb, So drop this frame\n");
		kfree_skb(skb);
		return -ENOENT;
	}

	table = dev->fwdTable;
	assert(table);
	
	MESH_READ_LOCK(&table->lock);

	passedup = 0;

	/* no promisc mode is needed */
/*
	if ( dev->flags & MESHF_PROMISC)
	{
		struct sk_buff *skb2;

		skb2 = skb_clone(skb, GFP_ATOMIC);
		if (skb2 != NULL)
		{
			passedup = 1;
			__pass_frame_up(table, skb2);
		}
	}
*/

	if(MESH_IS_MULTICAST(destMac) )
	{
		mesh_fwd_flood_forward(table, skb, !passedup);

		passedup = 1;/* flood include up to portal interface */
#if 0		
		if (!passedup)
			__pass_frame_up(table, skb);
#endif

		goto out;
	}

	destItem = mesh_fwd_db_get(table, destMac);
#if 0
	if (destItem != NULL && destItem->isLocal)
	{/* if dest is local device and not passed up until now */
		if (!passedup)
			__pass_frame_up(table, skb);
		else
			kfree_skb(skb);

		mesh_fwd_db_put(destItem);
		goto out;
	}
#endif

	if (destItem != NULL)
	{/* if dest is not local device, forward to unicast MAC address */
		IEEE80211_ADDR_COPY(wh->i_addr1, destItem->addr.addr );	/* next hop address */

		fwd_forward(destItem->dest, skb);
		mesh_fwd_db_put(destItem);
		goto out;
	}

	/* flood */
	mesh_fwd_flood_forward(table, skb, 0);

out:
	MESH_READ_UNLOCK(&table->lock);
	return 0;

#if 0
err:
	MESH_READ_UNLOCK(&table->lock);
err_nolock:
	kfree_skb(skb);
	return 0;
#endif	
}

/* entry point of frame fwd */
int mesh_fwd_handle_frame(struct sk_buff *skb, MESH_DEVICE *dev, MESH_PACKET *packet)
{
	MESH_FWD_TABLE	*table;
	unsigned char *destMac, *sourceMac;
	struct ieee80211_frame *wh;
	wh = (struct ieee80211_frame *)skb->data;
#if 0
	MESH_DEVICE *dev;
	dev = (MESH_DEVICE *)skb->dev;
#endif	

#if 0
	destMac = wh->i_addr3;// address 3 used as destination address when MESH frame
#else
	destMac = wh->i_addr1;// address 1 used as destination address whem normal WIFI frame
#endif
	sourceMac = wh->i_addr2;

/*
	MESH_DEBUG_INFO(MESH_DEBUG_FWD, "fwd frame with sources %s, ", swjtu_mesh_mac_sprintf(sourceMac) );
	MESH_DEBUG_INFO(MESH_DEBUG_FWD, "dest %s\n", swjtu_mesh_mac_sprintf(destMac) );
	swjtu_mesh_dump_rawpkt( (caddr_t) skb->data, skb->len, MESH_DEBUG_DATAOUT, "forward frame");
*/

	if(!dev)
	{
		MESH_ERR_INFO("No MESH DEVICE is assigned to skb, So drop this frame\n");
		kfree_skb(skb);
		return -ENOENT;
	}
	
	table = dev->fwdTable;
	assert(table);
	
	MESH_READ_LOCK(&table->lock);

	if (!(dev->flags & MESHF_UP) )// ||p->state == BR_STATE_DISABLED)
		goto err;

	/* source address is multicast address : ERROR frame? */
	if(MESH_IS_MULTICAST(sourceMac))  //skb->mac.ethernet->h_source) )	
		goto err;

//	if(dev->state == BR_STATE_LEARNING ||dev->state == BR_STATE_FORWARDING)
	swjtu_mesh_fwd_db_insert(table, dev, sourceMac, 0, 0);

#if 0
	if(table->stp_enabled && !memcmp(destMac, bridge_ula, 5) && !(destMac[5] & 0xF0))
		goto handle_special_frame;
#endif

//	if (dev->state == BR_STATE_FORWARDING)
	{
#if 0	
		if (br_should_route_hook && br_should_route_hook(&skb))
		{
			MESH_READ_UNLOCK(&table->lock);
			return -1;
		}
#endif

//		NF_HOOK(PF_BRIDGE, NF_BR_PRE_ROUTING, skb, skb->dev, NULL,
		mesh_fwd_handle_frame_finish(skb);
		MESH_READ_UNLOCK(&table->lock);
		return 0;
	}

err:
	MESH_READ_UNLOCK(&table->lock);
//err_nolock:
	kfree_skb(skb);
	return 0;

#if 0
handle_special_frame:

	if (!destMac[5])
	{/* should be 0xF0 for STP */
//		NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_IN, skb, skb->dev,NULL,
//		br_stp_handle_bpdu(skb);
		MESH_READ_UNLOCK(&table->lock);
		return 0;
	}

	MESH_READ_UNLOCK(&table->lock);
	kfree_skb(skb);
	return 0;
#endif

}

