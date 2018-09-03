/*
* $Id: wlan_input_mgmt_deauth.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_input.h"
#include <linux/random.h>

void _ieee80211_auth_open(struct ieee80211com *ic, struct ieee80211_frame *wh,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp, u_int16_t seq,   u_int16_t status)
{
	switch (ic->ic_opmode)
	{
		case IEEE80211_M_IBSS:
			if (ic->ic_state != IEEE80211_S_RUN ||seq != IEEE80211_AUTH_OPEN_REQUEST)
			{
				ic->ic_stats.is_rx_bad_auth++;
				return;
			}
			ieee80211_new_state(ic, IEEE80211_S_AUTH, wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;

		case IEEE80211_M_AHDEMO:
			/* should not come here */
			break;

		case IEEE80211_M_HOSTAP:
			if (ic->ic_state != IEEE80211_S_RUN ||seq != IEEE80211_AUTH_OPEN_REQUEST)
			{
				ic->ic_stats.is_rx_bad_auth++;
				return;
			}
			/* always accept open authentication requests */
			if (ni == ic->ic_bss)
			{
				ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);
				if (ni == NULL)
					return;
			}
			else
				(void) ieee80211_ref_node(ni);

			IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH, "[%s] station authenticated (open)\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr));
			break;

		case IEEE80211_M_STA:
			if (ic->ic_state != IEEE80211_S_AUTH ||seq != IEEE80211_AUTH_OPEN_RESPONSE)
			{
				ic->ic_stats.is_rx_bad_auth++;
				return;
			}

			if (status != 0)
			{
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH, "[%s] open auth failed (reason %d)\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr), status);
				/* XXX can this happen? */
				if (ni != ic->ic_bss)
					ni->ni_fails++;
				
				ic->ic_stats.is_rx_auth_fail++;
				return;
			}
			
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
			
		case IEEE80211_M_MONITOR:
			break;
	}
}

static int __alloc_challenge(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ni->ni_challenge == NULL)
		MALLOC(ni->ni_challenge, u_int32_t*, IEEE80211_CHALLENGE_LEN, M_DEVBUF, M_NOWAIT);

	if (ni->ni_challenge == NULL)
	{
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH, "[%s] shared key challenge alloc failed\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr));
		/* XXX statistic */
	}
	
	return (ni->ni_challenge != NULL);
}

/* XXX TODO: add statistics */
void _ieee80211_auth_shared(struct ieee80211com *ic, struct ieee80211_frame *wh,
    u_int8_t *frm, u_int8_t *efrm, struct ieee80211_node *ni, int rssi, u_int32_t rstamp, u_int16_t seq, u_int16_t status)
{
	u_int8_t *challenge;
	int allocbs, estatus;

	/*
	 * NB: this can happen as we allow pre-shared key
	 * authentication to be enabled w/o wep being turned
	 * on so that configuration of these can be done
	 * in any order.  It may be better to enforce the
	 * ordering in which case this check would just be
	 * for sanity/consistency.
	 */
	if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0)
	{
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH, ni->ni_macaddr, "shared key auth", "%s", " PRIVACY is disabled");
		estatus = IEEE80211_STATUS_ALG;
		goto bad;
	}
	
	/*
	 * Pre-shared key authentication is evil; accept
	 * it only if explicitly configured (it is supported
	 * mainly for compatibility with clients like OS X).
	 */
	if (ni->ni_authmode != IEEE80211_AUTH_AUTO && ni->ni_authmode != IEEE80211_AUTH_SHARED)
	{
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH, ni->ni_macaddr, "shared key auth", "bad sta auth mode %u", ni->ni_authmode);
		ic->ic_stats.is_rx_bad_auth++;	/* XXX maybe a unique error? */
		estatus = IEEE80211_STATUS_ALG;
		goto bad;
	}

	challenge = NULL;

	if (frm + 1 < efrm)
	{
		if ((frm[1] + 2) > (efrm - frm))
		{
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,  ni->ni_macaddr, "shared key auth", "ie %d/%d too long", frm[0], (frm[1] + 2) - (efrm - frm));
			ic->ic_stats.is_rx_bad_auth++;
			estatus = IEEE80211_STATUS_CHALLENGE;
			goto bad;
		}
		
		if (*frm == IEEE80211_ELEMID_CHALLENGE)
			challenge = frm;

		frm += frm[1] + 2;
	}
	
	switch (seq)
	{
		case IEEE80211_AUTH_SHARED_CHALLENGE:
		case IEEE80211_AUTH_SHARED_RESPONSE:
			if (challenge == NULL)
			{
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH, ni->ni_macaddr, "shared key auth", "%s", "no challenge");
				ic->ic_stats.is_rx_bad_auth++;
				estatus = IEEE80211_STATUS_CHALLENGE;
				goto bad;
			}
			if (challenge[1] != IEEE80211_CHALLENGE_LEN)
			{
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH, ni->ni_macaddr, "shared key auth", "bad challenge len %d", challenge[1]);

				ic->ic_stats.is_rx_bad_auth++;
				estatus = IEEE80211_STATUS_CHALLENGE;
				goto bad;
			}
		default:
			break;
	}

	switch (ic->ic_opmode)
	{
		case IEEE80211_M_MONITOR:
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_IBSS:
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH, ni->ni_macaddr, "shared key auth", "bad operating mode %u", ic->ic_opmode);
			return;
			
		case IEEE80211_M_HOSTAP:
			if (ic->ic_state != IEEE80211_S_RUN)
			{
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,  ni->ni_macaddr, "shared key auth", "bad state %u", ic->ic_state);
				estatus = IEEE80211_STATUS_OTHER;
				goto bad;
			}
			
			switch (seq)
			{
			case IEEE80211_AUTH_SHARED_REQUEST:
				if (ni == ic->ic_bss)
				{
					ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);
					if (ni == NULL)
					{
						/* NB: no way to return an error */
						return;
					}
					allocbs = 1;
				}
				else
				{
					(void) ieee80211_ref_node(ni);
					allocbs = 0;
				}

				ni->ni_rssi = rssi;
				ni->ni_rstamp = rstamp;
				if (!__alloc_challenge(ic, ni))
				{
					/* NB: don't return error so they rexmit */
					return;
				}

				get_random_bytes(ni->ni_challenge, IEEE80211_CHALLENGE_LEN);

				IEEE80211_DPRINTF(ic,IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH, "[%s] shared key %sauth request\n", swjtu_mesh_mac_sprintf(ni->ni_macaddr), allocbs ? "" : "re");
				break;
				
			case IEEE80211_AUTH_SHARED_RESPONSE:
				if (ni == ic->ic_bss)
				{
					IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH, ni->ni_macaddr, "shared key response", "%s", "unknown station");
					/* NB: don't send a response */
					return;
				}
				
				if (ni->ni_challenge == NULL)
				{
					IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
					    ni->ni_macaddr, "shared key response",
					    "%s", "no challenge recorded");
					ic->ic_stats.is_rx_bad_auth++;
					estatus = IEEE80211_STATUS_CHALLENGE;
					goto bad;
				}
				if (memcmp(ni->ni_challenge, &challenge[2],
				           challenge[1]) != 0) {
					IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
					    ni->ni_macaddr, "shared key response",
					    "%s", "challenge mismatch");
					ic->ic_stats.is_rx_auth_fail++;
					estatus = IEEE80211_STATUS_CHALLENGE;
					goto bad;
				}
				IEEE80211_DPRINTF(ic,
				    IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
				    "[%s] station authenticated (shared key)\n",
				    swjtu_mesh_mac_sprintf(ni->ni_macaddr));
				break;
			default:
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
				    ni->ni_macaddr, "shared key auth",
				    "bad seq %d", seq);
				ic->ic_stats.is_rx_bad_auth++;
				estatus = IEEE80211_STATUS_SEQUENCE;
				goto bad;
		}
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
		break;

	case IEEE80211_M_STA:
		if (ic->ic_state != IEEE80211_S_AUTH)
			return;
		switch (seq)
		{
		case IEEE80211_AUTH_SHARED_PASS:
			if (ni->ni_challenge != NULL)
			{
				kfree(ni->ni_challenge);
				ni->ni_challenge = NULL;
			}
			
			if (status != 0)
			{
				IEEE80211_DPRINTF(ic,IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH, "[%s] shared key auth failed (reason %d)\n", swjtu_mesh_mac_sprintf(ieee80211_getbssid(ic, wh)), status);
				/* XXX can this happen? */
				if (ni != ic->ic_bss)
					ni->ni_fails++;
				ic->ic_stats.is_rx_auth_fail++;
				return;
			}
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		case IEEE80211_AUTH_SHARED_CHALLENGE:
			if (!__alloc_challenge(ic, ni))
				return;
			/* XXX could optimize by passing recvd challenge */
			memcpy(ni->ni_challenge, &challenge[2], challenge[1]);
			IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
			break;
		default:
			IEEE80211_DISCARD(ic, IEEE80211_MSG_AUTH,
			    wh, "shared key auth", "bad seq %d", seq);
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		break;
	}
	return;
bad:
	/*
	 * Send an error response; but only when operating as an AP.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		/* XXX hack to workaround calling convention */
		IEEE80211_SEND_MGMT(ic, ni,
			IEEE80211_FC0_SUBTYPE_AUTH,
			(seq + 1) | (estatus<<16));
	}
}

