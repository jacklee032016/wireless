/*
* $Id: wlan_proto_state.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_protocol.h"

#define	IEEE80211_RATE2MBS(r)	(((r) & IEEE80211_RATE_VAL) / 2)

/* state machine of IC */
int ieee80211_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	MESH_DEVICE *dev = ic->ic_dev;
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate;

	ostate = ic->ic_state;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_STATE, "%s: %s -> %s\n", __func__, ieee80211_state_name[ostate], ieee80211_state_name[nstate]);
	ic->ic_state = nstate;			/* New state : state transition */
	ni = ic->ic_bss;				/* NB: no reference held */
	
	switch (nstate)
	{
		case IEEE80211_S_INIT:
		{
			switch (ostate)
			{
				case IEEE80211_S_INIT:
					break;
					
				case IEEE80211_S_RUN:
					switch (ic->ic_opmode)
					{
#if WITH_SUPPORT_STA
						case IEEE80211_M_STA:
							IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DISASSOC, IEEE80211_REASON_ASSOC_LEAVE);
							ieee80211_sta_leave(ic, ni);
							break;
#endif							
						case IEEE80211_M_HOSTAP:
							nt = &ic->ic_sta;
							IEEE80211_NODE_LOCK(nt);
							TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
							{
								if (ni->ni_associd == 0)
									continue;
								IEEE80211_SEND_MGMT(ic, ni,
								    IEEE80211_FC0_SUBTYPE_DISASSOC,
								    IEEE80211_REASON_ASSOC_LEAVE);
							}
							IEEE80211_NODE_UNLOCK(nt);
							break;
						default:
							break;
					}
					goto reset;
					
				case IEEE80211_S_ASSOC:
					switch (ic->ic_opmode)
					{
#if WITH_SUPPORT_STA
					case IEEE80211_M_STA:
						IEEE80211_SEND_MGMT(ic, ni,
						    IEEE80211_FC0_SUBTYPE_DEAUTH,
						    IEEE80211_REASON_AUTH_LEAVE);
						break;
#endif						
					case IEEE80211_M_HOSTAP:
						nt = &ic->ic_sta;
						IEEE80211_NODE_LOCK(nt);
						TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
						{
							IEEE80211_SEND_MGMT(ic, ni,
							    IEEE80211_FC0_SUBTYPE_DEAUTH,
							    IEEE80211_REASON_AUTH_LEAVE);
						}
						IEEE80211_NODE_UNLOCK(nt);
						break;
					default:
						break;
					}
					goto reset;
					
				case IEEE80211_S_SCAN:
					ieee80211_cancel_scan(ic);
					goto reset;
					
				case IEEE80211_S_AUTH:
				reset:
					ic->ic_mgt_timer = 0;
					IF_DRAIN(&ic->ic_mgtq);	// TODO: b4 ic->ic_inact_timer = 0
					ieee80211_reset_bss(ic);
					break;
			}
			if (ic->ic_auth->ia_detach != NULL)
				ic->ic_auth->ia_detach(ic);
			break;
		}/*end of nstate is INIT */
		
		case IEEE80211_S_SCAN:
		{
			switch (ostate)
			{
				case IEEE80211_S_INIT:
					if ((ic->ic_opmode == IEEE80211_M_HOSTAP ||
					     ic->ic_opmode == IEEE80211_M_IBSS ||
					     ic->ic_opmode == IEEE80211_M_AHDEMO) &&
					    ic->ic_des_chan != IEEE80211_CHAN_ANYC)
					{
						/*
						 * AP operation and we already have a channel;
						 * bypass the scan and startup immediately.
						 */
						ieee80211_create_ibss(ic, ic->ic_des_chan);
					}
					else
					{
						ieee80211_begin_scan(ic, arg);
					}
					break;
				case IEEE80211_S_SCAN:
					/*
					 * Scan next. If doing an active scan and the
					 * channel is not marked passive-only then send
					 * a probe request.  Otherwise just listen for
					 * beacons on the channel.
					 */
					if ((ic->ic_flags & IEEE80211_F_ASCAN) && (ni->ni_chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0)
					{
						IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
					}
					break;
				case IEEE80211_S_RUN:
					/* beacon miss */
					IEEE80211_DPRINTF(ic, IEEE80211_MSG_STATE,
						"no recent beacons from %s; rescanning\n",
						swjtu_mesh_mac_sprintf(ic->ic_bss->ni_bssid));
					ieee80211_sta_leave(ic, ni);
					ic->ic_flags &= ~IEEE80211_F_SIBSS;	/* XXX */
					/* FALLTHRU */
				case IEEE80211_S_AUTH:
				case IEEE80211_S_ASSOC:
					/* timeout restart scan */
					ni = ieee80211_find_node(&ic->ic_scan, ic->ic_bss->ni_macaddr);

					if (ni != NULL)
					{
						ni->ni_fails++;
						ieee80211_unref_node(&ni);
					}
					ieee80211_begin_scan(ic, arg);
					break;
			}
			break;
		}/* end of new state is SCAN */
		
		case IEEE80211_S_AUTH:
		{
			switch (ostate)
			{
				case IEEE80211_S_INIT:
				case IEEE80211_S_SCAN:
					IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_AUTH, 1);
					break;
				case IEEE80211_S_AUTH:
				case IEEE80211_S_ASSOC:
					switch (arg)
					{
						case IEEE80211_FC0_SUBTYPE_AUTH:
							/* ??? */
							IEEE80211_SEND_MGMT(ic, ni,
							    IEEE80211_FC0_SUBTYPE_AUTH, 2);
							break;
						case IEEE80211_FC0_SUBTYPE_DEAUTH:
							/* ignore and retry scan on timeout */
							break;
					}
					break;
				case IEEE80211_S_RUN:
					switch (arg)
					{
						case IEEE80211_FC0_SUBTYPE_AUTH:
							IEEE80211_SEND_MGMT(ic, ni,
							    IEEE80211_FC0_SUBTYPE_AUTH, 2);
							ic->ic_state = ostate;	/* stay RUN */
							break;
						case IEEE80211_FC0_SUBTYPE_DEAUTH:
							/* try to reauth */
							IEEE80211_SEND_MGMT(ic, ni,
							    IEEE80211_FC0_SUBTYPE_AUTH, 1);
							ieee80211_sta_leave(ic, ni);
							break;
					}
					break;
				}
				break;
		}/* end of new state is AUTH */
		
		case IEEE80211_S_ASSOC:
		{
			switch (ostate)
			{
				case IEEE80211_S_INIT:
				case IEEE80211_S_SCAN:
				case IEEE80211_S_ASSOC:
					IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY, "%s: invalid transition\n", __func__);
					break;
				case IEEE80211_S_AUTH:
					IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 0);
					break;
				case IEEE80211_S_RUN:
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 1);
					ieee80211_sta_leave(ic, ni);
					break;
				}
				break;
		}
		
		case IEEE80211_S_RUN:
		{
			if (ic->ic_flags & IEEE80211_F_WPA)
			{/* XXX validate prerequisites */
			}
			
			switch (ostate)
			{
				case IEEE80211_S_INIT:
					if (ic->ic_opmode == IEEE80211_M_MONITOR)
						break;
					/* fall thru... */
				case IEEE80211_S_AUTH:
					IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,"%s: invalid transition\n", __func__);
					/* fall thru... */
				case IEEE80211_S_RUN:
					break;
				case IEEE80211_S_SCAN:		/* adhoc/hostap mode */
				case IEEE80211_S_ASSOC:		/* infra mode */
				{
					KASSERT(ni->ni_txrate < ni->ni_rates.rs_nrates, ("%s: bogus xmit rate %u setup\n", __func__, ni->ni_txrate));
#ifdef IEEE80211_DEBUG
					if (ieee80211_msg_debug(ic))
					{
#if WITH_SUPPORT_STA
						if (ic->ic_opmode == IEEE80211_M_STA)
							mdev_printf(dev, "associated ");
						else
#endif							
							mdev_printf(dev, "synchronized ");
						
						printf("with %s ssid ",  swjtu_mesh_mac_sprintf(ni->ni_bssid));
						ieee80211_print_essid(ic->ic_bss->ni_essid, ni->ni_esslen);
						printf(" channel %d start %uMb\n", ieee80211_chan2ieee(ic, ni->ni_chan), IEEE80211_RATE2MBS(ni->ni_rates.rs_rates[ni->ni_txrate]));
					}
#endif
					ic->ic_mgt_timer = 0;

#if WITH_SUPPORT_STA
					if (ic->ic_opmode == IEEE80211_M_STA)
						ieee80211_notify_node_join(ic, ni, arg == IEEE80211_FC0_SUBTYPE_ASSOC_RESP);
#endif


					//if_start(ifp);		/* XXX not authorized yet */
					(*dev->hard_start_xmit)(NULL, dev);

					break;
				}	
			}
			/*
			 * Start/stop the authenticator when operating as an
			 * AP.  We delay until here to allow configuration to
			 * happen out of order.
			 */
			if (ic->ic_opmode == IEEE80211_M_HOSTAP && /* XXX IBSS/AHDEMO */
			    ic->ic_auth->ia_attach != NULL)
			{
				/* XXX check failure */
				ic->ic_auth->ia_attach(ic);
			}
			else if (ic->ic_auth->ia_detach != NULL)
			{
				ic->ic_auth->ia_detach(ic);
			}

			/*
			 * When 802.1x is not in use mark the port authorized
			 * at this point so traffic can flow.
			 */
			if (ni->ni_authmode != IEEE80211_AUTH_8021X)
				ieee80211_node_authorize(ic, ni);
			/*
			 * Enable inactivity processing.
			 * XXX
			 */
			ic->ic_scan.nt_inact_timer = IEEE80211_INACT_WAIT;
			ic->ic_sta.nt_inact_timer = IEEE80211_INACT_WAIT;
			break;
		}	
	}
	return 0;
}

