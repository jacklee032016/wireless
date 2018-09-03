/*
* $Id: ieee80211_mesh.h,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
*/
#ifndef __IEEE80211_MESH_H__
#define __IEEE80211_MESH_H__

/* defination for Action Frame body */
/* 11e, p.39 */
#define	IEEE80211_ACTION_CATEGORY_QOS							0x01
#define	IEEE80211_ACTION_CATEGORY_DLP							0x02
#define	IEEE80211_ACTION_CATEGORY_BLOCK_ACK					0x03

/* 11s, p.27 */
#define	IEEE80211_ACTION_CATEGORY_RADIO_MGMT					0x04
#define	IEEE80211_ACTION_CATEGORY_MESH_MGMT					0x05

/* 11s, p.44 */
#define	IEEE80211_ACTION_ACTION_LOCAL_LINKSTATE_ANNOUNCE		0x00
#define	IEEE80211_ACTION_ACTION_PEER_LINK_DISCONNECT			0x01

#define	IEEE80211_ACTION_ACTION_ROUTE_REQUEST					0x02
#define	IEEE80211_ACTION_ACTION_ROUTE_REPLY					0x03
#define	IEEE80211_ACTION_ACTION_ROUTE_ERROR					0x04
#define	IEEE80211_ACTION_ACTION_ROUTE_REPLY_ACK				0x05

#define	IEEE80211_ACTION_ACTION_CONGEST_REQUEST				0x06
#define	IEEE80211_ACTION_ACTION_CONGEST_RESPONSE				0x07
#define	IEEE80211_ACTION_ACTION_CONGEST_ANNOUNCE				0x08

#define	IEEE80211_ACTION_ACTION_UNKNOWN						-1

struct ieee80211_action_header
{
	u_int8_t				category;
	u_int8_t				action;
} __packed;

#define	IEEE80211_IS_MESH_MGMT(_a)		((_a) & IEEE80211_ACTION_CATEGORY_MESH_MGMT )

#define		IEEE80211_MESH_MAX_QUEUE		1024


int ieee80211_mesh_send_mgmt(struct ieee80211com *ic, unsigned char action_type, int length, void *route_pdu_data, int isbroadcast);
void action_frame_rx(struct ieee80211com *ic, struct sk_buff *skb, struct ieee80211_node *ni,	int subtype, int rssi, u_int32_t rstamp);


#endif

