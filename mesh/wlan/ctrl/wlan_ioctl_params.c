/*
* $Id: wlan_ioctl_params.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_wlan_ioctl.h"

static int __cipher2cap(int cipher)
{
	switch (cipher)
	{
		case IEEE80211_CIPHER_WEP:
			return IEEE80211_C_WEP;
		case IEEE80211_CIPHER_AES_OCB:
			return IEEE80211_C_AES;
		case IEEE80211_CIPHER_AES_CCM:
			return IEEE80211_C_AES_CCM;
		case IEEE80211_CIPHER_CKIP:
			return IEEE80211_C_CKIP;
		case IEEE80211_CIPHER_TKIP:
			return IEEE80211_C_TKIP;
	}
	return 0;
}

static int __cap2cipher(int flag)
{
	switch (flag)
	{
		case IEEE80211_C_WEP:
			return IEEE80211_CIPHER_WEP;
		case IEEE80211_C_AES:
			return IEEE80211_CIPHER_AES_OCB;
		case IEEE80211_C_AES_CCM:
			return IEEE80211_CIPHER_AES_CCM;
		case IEEE80211_C_CKIP:
			return IEEE80211_CIPHER_CKIP;
		case IEEE80211_C_TKIP:
			return IEEE80211_CIPHER_TKIP;
	}
	return -1;
}


//int ieee80211_ioctl_setparam(struct ieee80211com *ic, struct meshw_request_info *info,
//		   	 void *w, char *extra)

int ieee80211_ioctl_setparam(struct ieee80211com *ic, struct meshw_request_info *info,
		   	 void *w, char *extra)
{
	struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	int *i = (int *) extra;
	int param = i[0];		/* parameter id is 1st */
	int value = i[1];		/* NB: most values are TYPE_INT */
#if 0	
	struct ifreq ifr;
#else
	struct wlan_ifmediareq ifr;
//	struct meshw_req ifr;
#endif
	int retv = 0;
	int j, caps;
	const struct ieee80211_authenticator *auth;
#if WITH_WLAN_AUTHBACK_ACL
	const struct ieee80211_aclator *acl;
#endif

	switch (param)
	{
		/* case 1 : media is turbo */
		case IEEE80211_PARAM_TURBO:
			if (!ic->ic_media.wlan_ifm_cur)
				return -EINVAL;
			memset(&ifr, 0, sizeof(ifr));
#if 0			
			ifr.ifr_media = ic->ic_media.wlan_ifm_cur->wlan_ifm_media;
			if (value)
				ifr.ifr_media |= IFM_IEEE80211_TURBO;
			else
				ifr.ifr_media &= ~IFM_IEEE80211_TURBO;
#else
			ifr.wlan_ifm_current = ic->ic_media.wlan_ifm_cur->wlan_ifm_media;
			if (value)
				ifr.wlan_ifm_current |= IFM_IEEE80211_TURBO;
			else
				ifr.wlan_ifm_current &= ~IFM_IEEE80211_TURBO;
#endif
			retv = wlan_ifmedia_ioctl(ic->ic_dev, &ifr, &ic->ic_media, SIOCSIFMEDIA);
			break;

		/* case 2 : media mode */	
		case IEEE80211_PARAM_MODE:
			if (!ic->ic_media.wlan_ifm_cur)
				return -EINVAL;
			
			memset(&ifr, 0, sizeof(ifr));
#if 0			
			ifr.ifr_media = ic->ic_media.wlan_ifm_cur->wlan_ifm_media &~ IFM_MMASK;
			ifr.ifr_media |= IFM_MAKEMODE(value);
#else
			ifr.wlan_ifm_current = ic->ic_media.wlan_ifm_cur->wlan_ifm_media &~ IFM_MMASK;
			ifr.wlan_ifm_current |= IFM_MAKEMODE(value);
#endif
			retv = wlan_ifmedia_ioctl(ic->ic_dev, &ifr, &ic->ic_media, SIOCSIFMEDIA);
			break;

		/* case 3 : authen mode */			
		case IEEE80211_PARAM_AUTHMODE:
			switch (value)
			{
				case IEEE80211_AUTH_WPA:	/* WPA */
				case IEEE80211_AUTH_8021X:	/* 802.1x */
				case IEEE80211_AUTH_OPEN:	/* open */
				case IEEE80211_AUTH_SHARED:	/* shared-key */
				case IEEE80211_AUTH_AUTO:	/* auto */
					auth = ieee80211_authenticator_get(value);
					if (auth == NULL)
						return -EINVAL;
					break;
				default:
					return -EINVAL;
			}
			
			switch (value)
			{
				case IEEE80211_AUTH_WPA:	/* WPA w/ 802.1x */
					ic->ic_flags |= IEEE80211_F_PRIVACY;
					value = IEEE80211_AUTH_8021X;
					break;
				case IEEE80211_AUTH_OPEN:	/* open */
					ic->ic_flags &= ~(IEEE80211_F_WPA|IEEE80211_F_PRIVACY);
					break;
				case IEEE80211_AUTH_SHARED:	/* shared-key */
				case IEEE80211_AUTH_8021X:	/* 802.1x */
					ic->ic_flags &= ~IEEE80211_F_WPA;
					/* both require a key so mark the PRIVACY capability */
					ic->ic_flags |= IEEE80211_F_PRIVACY;
					break;
				case IEEE80211_AUTH_AUTO:	/* auto */
					ic->ic_flags &= ~IEEE80211_F_WPA;
					/* XXX PRIVACY handling? */
					/* XXX what's the right way to do this? */
					break;
			}
			/* NB: authenticator attach/detach happens on state change */
			ic->ic_bss->ni_authmode = value;
			/* XXX mixed/mode/usage? */
			ic->ic_auth = auth;
			retv = ENETRESET;
			break;

		/* case 4 : protect mode, eg. CTS/RTS */
		case IEEE80211_PARAM_PROTMODE:
			if (value > IEEE80211_PROT_RTSCTS)
				return -EINVAL;
			ic->ic_protmode = value;

			/* NB: if not operating in 11g this can wait */
			if (ic->ic_curmode == IEEE80211_MODE_11G ||ic->ic_curmode == IEEE80211_MODE_TURBO_G)
				retv = ENETRESET;
			break;
			
		/* case 5 : multicast cipher */	
		case IEEE80211_PARAM_MCASTCIPHER:
			/* XXX s/w implementations */
			if ((ic->ic_caps & __cipher2cap(value)) == 0)
				return -EINVAL;
			rsn->rsn_mcastcipher = value;
			if (ic->ic_flags & IEEE80211_F_WPA)
				retv = ENETRESET;
			break;
			
		/* case 6 : multicast key length */	
		case IEEE80211_PARAM_MCASTKEYLEN:
			if (!(0 < value && value < IEEE80211_KEYBUF_SIZE))
				return -EINVAL;
			/* XXX no way to verify driver capability */
			rsn->rsn_mcastkeylen = value;
			if (ic->ic_flags & IEEE80211_F_WPA)
				retv = ENETRESET;
			break;

		/* case 7 : unicast ciphers */	
		case IEEE80211_PARAM_UCASTCIPHERS:
			/*
			 * Convert cipher set to equivalent capabilities.
			 * NB: this logic intentionally ignores unknown and
			 * unsupported ciphers so folks can specify 0xff or
			 * similar and get all available ciphers.
			 */
			caps = 0;
			for (j = 1; j < 32; j++)	/* NB: skip WEP */
				if (value & (1<<j))
					caps |= __cipher2cap(j);
			/* XXX s/w implementations */
			caps &= ic->ic_caps;	/* restrict to supported ciphers */
			/* XXX verify ciphers ok for unicast use? */
			/* XXX disallow if running as it'll have no effect */
			rsn->rsn_ucastcipherset = caps;
			if (ic->ic_flags & IEEE80211_F_WPA)
				retv = ENETRESET;
			break;

		/* case 8 : unicast cipher */	
		case IEEE80211_PARAM_UCASTCIPHER:
			if ((rsn->rsn_ucastcipherset & __cipher2cap(value)) == 0)
				return -EINVAL;
			rsn->rsn_ucastcipher = value;
			break;
		case IEEE80211_PARAM_UCASTKEYLEN:
			if (!(0 < value && value < IEEE80211_KEYBUF_SIZE))
				return -EINVAL;
			/* XXX no way to verify driver capability */
			rsn->rsn_ucastkeylen = value;
			break;
		case IEEE80211_PARAM_KEYMGTALGS:
			/* XXX check */
			rsn->rsn_keymgmtset = value;
			if (ic->ic_flags & IEEE80211_F_WPA)
				retv = ENETRESET;
			break;
		case IEEE80211_PARAM_RSNCAPS:
			/* XXX check */
			rsn->rsn_caps = value;
			if (ic->ic_flags & IEEE80211_F_WPA)
				retv = ENETRESET;
			break;
		case IEEE80211_PARAM_WPA:
			if (value > 3)
				return -EINVAL;
			/* XXX verify ciphers available */
			ic->ic_flags &= ~IEEE80211_F_WPA;
			switch (value) {
			case 1:
				ic->ic_flags |= IEEE80211_F_WPA1;
				break;
			case 2:
				ic->ic_flags |= IEEE80211_F_WPA2;
				break;
			case 3:
				ic->ic_flags |= IEEE80211_F_WPA1 | IEEE80211_F_WPA2;
				break;
			}
			retv = ENETRESET;		/* XXX? */
			break;
		case IEEE80211_PARAM_ROAMING:
			if (!(IEEE80211_ROAMING_DEVICE <= value && value <= IEEE80211_ROAMING_MANUAL))
				return -EINVAL;
			ic->ic_roaming = value;
			break;
		case IEEE80211_PARAM_PRIVACY:
			if (value)
			{
				/* XXX check for key state? */
				ic->ic_flags |= IEEE80211_F_PRIVACY;
			}
			else
				ic->ic_flags &= ~IEEE80211_F_PRIVACY;

			break;
		case IEEE80211_PARAM_DROPUNENCRYPTED:
			if (value)
				ic->ic_flags |= IEEE80211_F_DROPUNENC;
			else
				ic->ic_flags &= ~IEEE80211_F_DROPUNENC;
			break;
		case IEEE80211_PARAM_COUNTERMEASURES:
			if (value) 
			{
				if ((ic->ic_flags & IEEE80211_F_WPA) == 0)
					return -EINVAL;
				ic->ic_flags |= IEEE80211_F_COUNTERM;
			}
			else
				ic->ic_flags &= ~IEEE80211_F_COUNTERM;

			break;
		case IEEE80211_PARAM_DRIVER_CAPS:
			ic->ic_caps = value;		/* NB: for testing */
			break;

#if WITH_WLAN_AUTHBACK_ACL
		case IEEE80211_PARAM_MACCMD:
			acl = ic->ic_acl;
			switch (value)
			{
				case IEEE80211_MACCMD_POLICY_OPEN:
				case IEEE80211_MACCMD_POLICY_ALLOW:
				case IEEE80211_MACCMD_POLICY_DENY:
					if (acl == NULL)
					{
						acl = ieee80211_aclator_get("mac");
						if (acl == NULL || !acl->iac_attach(ic))
							return -EINVAL;
						ic->ic_acl = acl;
					}
					acl->iac_setpolicy(ic, value);
					break;
				case IEEE80211_MACCMD_FLUSH:
					if (acl != NULL)
						acl->iac_flush(ic);
					/* NB: silently ignore when not in use */
					break;
				case IEEE80211_MACCMD_DETACH:
					if (acl != NULL)
					{
						ic->ic_acl = NULL;
						acl->iac_detach(ic);
					}
					break;
			}
			break;
#endif

		case IEEE80211_PARAM_WME:
			if (ic->ic_opmode != IEEE80211_M_STA)
				return -EINVAL;
			if (value)
				ic->ic_flags |= IEEE80211_F_WME;
			else
				ic->ic_flags &= ~IEEE80211_F_WME;
			break;
		case IEEE80211_PARAM_HIDESSID:
			if (value)
				ic->ic_flags |= IEEE80211_F_HIDESSID;
			else
				ic->ic_flags &= ~IEEE80211_F_HIDESSID;
			retv = ENETRESET;
			break;
			
		case IEEE80211_PARAM_APBRIDGE:
			if (value == 0)
				ic->ic_flags |= IEEE80211_F_NOBRIDGE;
			else
				ic->ic_flags &= ~IEEE80211_F_NOBRIDGE;
			break;
		case IEEE80211_PARAM_INACT:
			ic->ic_inact_run = value / IEEE80211_INACT_WAIT;
			break;
		case IEEE80211_PARAM_INACT_AUTH:
			ic->ic_inact_auth = value / IEEE80211_INACT_WAIT;
			break;
		case IEEE80211_PARAM_INACT_INIT:
			ic->ic_inact_init = value / IEEE80211_INACT_WAIT;
			break;
		default:
			retv = EOPNOTSUPP;
			break;
	}

	if (retv == ENETRESET)
		retv = IS_UP_AUTO(ic) ? (*ic->ic_init)(ic->ic_dev) : 0;

	return -retv;
}

int ieee80211_ioctl_getparam(struct ieee80211com *ic, struct meshw_request_info *info, void *w, char *extra)
{
	struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	struct wlan_ifmediareq imr;
	int *param = (int *) extra;
	u_int m;

	switch (param[0])
	{
		case IEEE80211_PARAM_TURBO:
			(*ic->ic_media.wlan_ifm_status)(ic->ic_dev, &imr);
			param[0] = (imr.wlan_ifm_active & IFM_IEEE80211_TURBO) != 0;
			break;
			
		case IEEE80211_PARAM_MODE:
			(*ic->ic_media.wlan_ifm_status)(ic->ic_dev, &imr);

			switch (IFM_MODE(imr.wlan_ifm_active))
			{
				case IFM_IEEE80211_11A:
					param[0] = 1;
					break;
				case IFM_IEEE80211_11B:
					param[0] = 2;
					break;
				case IFM_IEEE80211_11G:
					param[0] = 3;
					break;
				case IFM_IEEE80211_FH:
					param[0] = 4;
					break;
				case IFM_AUTO:
					param[0] = 0;
					break;
				default:
					return -EINVAL;
			}
			break;
		case IEEE80211_PARAM_AUTHMODE:
			if (ic->ic_flags & IEEE80211_F_WPA)
				param[0] = IEEE80211_AUTH_WPA;
			else
				param[0] = ic->ic_bss->ni_authmode;
			break;
		case IEEE80211_PARAM_PROTMODE:
			param[0] = ic->ic_protmode;
			break;
		case IEEE80211_PARAM_MCASTCIPHER:
			param[0] = rsn->rsn_mcastcipher;
			break;
		case IEEE80211_PARAM_MCASTKEYLEN:
			param[0] = rsn->rsn_mcastkeylen;
			break;
		case IEEE80211_PARAM_UCASTCIPHERS:
			param[0] = 0;
			for (m = 0x1; m != 0; m <<= 1)
				if (rsn->rsn_ucastcipherset & m)
					param[0] |= 1<<__cap2cipher(m);
			break;
		case IEEE80211_PARAM_UCASTCIPHER:
			param[0] = rsn->rsn_ucastcipher;
			break;
		case IEEE80211_PARAM_UCASTKEYLEN:
			param[0] = rsn->rsn_ucastkeylen;
			break;
		case IEEE80211_PARAM_KEYMGTALGS:
			param[0] = rsn->rsn_keymgmtset;
			break;
		case IEEE80211_PARAM_RSNCAPS:
			param[0] = rsn->rsn_caps;
			break;
		case IEEE80211_PARAM_WPA:
			switch (ic->ic_flags & IEEE80211_F_WPA)
			{
			case IEEE80211_F_WPA1:
				param[0] = 1;
				break;
			case IEEE80211_F_WPA2:
				param[0] = 2;
				break;
			case IEEE80211_F_WPA1 | IEEE80211_F_WPA2:
				param[0] = 3;
				break;
			default:
				param[0] = 0;
				break;
			}
			break;
		case IEEE80211_PARAM_ROAMING:
			param[0] = ic->ic_roaming;
			break;
		case IEEE80211_PARAM_PRIVACY:
			param[0] = (ic->ic_flags & IEEE80211_F_PRIVACY) != 0;
			break;
		case IEEE80211_PARAM_DROPUNENCRYPTED:
			param[0] = (ic->ic_flags & IEEE80211_F_DROPUNENC) != 0;
			break;
		case IEEE80211_PARAM_COUNTERMEASURES:
			param[0] = (ic->ic_flags & IEEE80211_F_COUNTERM) != 0;
			break;
		case IEEE80211_PARAM_DRIVER_CAPS:
			param[0] = ic->ic_caps;
			break;
		case IEEE80211_PARAM_WME:
			param[0] = (ic->ic_flags & IEEE80211_F_WME) != 0;
			break;
		case IEEE80211_PARAM_HIDESSID:
			param[0] = (ic->ic_flags & IEEE80211_F_HIDESSID) != 0;
			break;
		case IEEE80211_PARAM_APBRIDGE:
			param[0] = (ic->ic_flags & IEEE80211_F_NOBRIDGE) == 0;
			break;
		case IEEE80211_PARAM_INACT:
			param[0] = ic->ic_inact_run * IEEE80211_INACT_WAIT;
			break;
		case IEEE80211_PARAM_INACT_AUTH:
			param[0] = ic->ic_inact_auth * IEEE80211_INACT_WAIT;
			break;
		case IEEE80211_PARAM_INACT_INIT:
			param[0] = ic->ic_inact_init * IEEE80211_INACT_WAIT;
			break;
		default:
			return -EOPNOTSUPP;
	}
	return 0;
}

