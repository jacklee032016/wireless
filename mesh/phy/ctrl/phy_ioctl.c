/*
* $Id: phy_ioctl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/
 
#include "phy.h"

//#include "if_ethersubr.h"		/* for ETHER_IS_MULTICAST */
//#include "if_media.h"
//#include "if_llc.h"

/*
 * Bounce functions to get to the 802.11 code. These are
 * necessary for now because wireless extensions operations
 * are done on the underlying device and not the 802.11 instance.
 * This will change when there can be multiple 802.11 instances
 * associated with a device and we must have a net_device for
 * each so we can manipulate them individually.
 */
#define	PHY_CHAR_BOUNCE(name)						\
static int	phy_ioctl_##name(MESH_DEVICE *dev, struct meshw_request_info *info,	\
	     	 char *erq, char *extra)				\
{									\
	struct ath_softc *sc = dev->priv;				\
	return ieee80211_ioctl_##name(&sc->sc_ic, info, erq, extra);	\
}

#define	PHY_POINT_BOUNCE(name)						\
static int	phy_ioctl_##name(MESH_DEVICE *dev, struct meshw_request_info *info,	\
	     	 struct meshw_point *erq, char *extra)			\
{									\
	struct ath_softc *sc = dev->priv;				\
	return ieee80211_ioctl_##name(&sc->sc_ic, info, erq, extra);	\
}

#define	PHY_PARAM_BOUNCE(name)						\
static int	phy_ioctl_##name(MESH_DEVICE *dev, struct meshw_request_info *info,	\
	     	 struct meshw_param *erq, char *extra)			\
{									\
	struct ath_softc *sc = dev->priv;				\
	return ieee80211_ioctl_##name(&sc->sc_ic, info, erq, extra);	\
}

#define	PHY_SOCKADDR_BOUNCE(name)					\
static int	phy_ioctl_##name(MESH_DEVICE *dev, struct meshw_request_info *info,	\
	     	 struct sockaddr *erq, char *extra)			\
{									\
	struct ath_softc *sc = dev->priv;				\
	return ieee80211_ioctl_##name(&sc->sc_ic, info, erq, extra);	\
}

#define	PHY_FREQ_BOUNCE(name)						\
static int	phy_ioctl_##name(MESH_DEVICE *dev, struct meshw_request_info *info,	\
	     	 struct meshw_freq *erq, char *extra)			\
{									\
	struct ath_softc *sc = dev->priv;				\
	return ieee80211_ioctl_##name(&sc->sc_ic, info, erq, extra);	\
}

#define	PHY_U32_BOUNCE(name)						\
static int	phy_ioctl_##name(MESH_DEVICE *dev, struct meshw_request_info *info,	\
	     	 __u32 *erq, char *extra)				\
{									\
	struct ath_softc *sc = dev->priv;				\
	return ieee80211_ioctl_##name(&sc->sc_ic, info, erq, extra);	\
}

#define	PHY_VOID_BOUNCE(name)						\
static int	phy_ioctl_##name(MESH_DEVICE *dev, struct meshw_request_info *info,	\
	     	 void *erq, char *extra)				\
{									\
	struct ath_softc *sc = dev->priv;				\
	return ieee80211_ioctl_##name(&sc->sc_ic, info, erq, extra);	\
}

PHY_CHAR_BOUNCE(giwname)
PHY_POINT_BOUNCE(siwencode)
PHY_POINT_BOUNCE(giwencode)
PHY_PARAM_BOUNCE(siwrate)
PHY_PARAM_BOUNCE(giwrate)
PHY_PARAM_BOUNCE(siwsens)
PHY_PARAM_BOUNCE(giwsens)
PHY_PARAM_BOUNCE(siwrts)
PHY_PARAM_BOUNCE(giwrts)
PHY_PARAM_BOUNCE(siwfrag)
PHY_PARAM_BOUNCE(giwfrag)
PHY_SOCKADDR_BOUNCE(siwap)
PHY_SOCKADDR_BOUNCE(giwap)
PHY_POINT_BOUNCE(siwnickn)
PHY_POINT_BOUNCE(giwnickn)
PHY_FREQ_BOUNCE(siwfreq)
PHY_FREQ_BOUNCE(giwfreq)
PHY_POINT_BOUNCE(siwessid)
PHY_POINT_BOUNCE(giwessid)
PHY_POINT_BOUNCE(giwrange)
PHY_U32_BOUNCE(siwmode)
PHY_U32_BOUNCE(giwmode)
PHY_PARAM_BOUNCE(siwpower)
PHY_PARAM_BOUNCE(giwpower)
PHY_PARAM_BOUNCE(siwretry)
PHY_PARAM_BOUNCE(giwretry)
PHY_PARAM_BOUNCE(siwtxpow)
PHY_PARAM_BOUNCE(giwtxpow)
PHY_POINT_BOUNCE(iwaplist)
#ifdef MESHW_IO_R_SCAN
PHY_POINT_BOUNCE(siwscan)
PHY_POINT_BOUNCE(giwscan)
#endif
PHY_VOID_BOUNCE(setparam)
PHY_VOID_BOUNCE(getparam)
PHY_VOID_BOUNCE(setkey)
PHY_VOID_BOUNCE(delkey)
PHY_VOID_BOUNCE(setmlme)
PHY_VOID_BOUNCE(setoptie)
PHY_VOID_BOUNCE(getoptie)
PHY_VOID_BOUNCE(addmac)
PHY_VOID_BOUNCE(delmac)
PHY_VOID_BOUNCE(chanlist)

#ifdef HAS_VMAC

static int phy_ioctl_getreg(MESH_DEVICE *dev, struct meshw_request_info *info, void *wrqu, char *extra)
{
	struct ath_softc *sc = dev->priv;
	uint32_t reg = *((uint32_t *) extra);
	uint32_t *res = (uint32_t *) extra;

  //if (reg < 0x4000 || reg > 0x9000) {
  //  return -EINVAL;
  //}
	*res = OS_REG_READ(sc->sc_ah, reg);
  	return 0;
}

static int phy_ioctl_setreg(MESH_DEVICE *dev,  struct meshw_request_info *info, void *wrqu, char *extra) 
{
	struct ath_softc *sc = dev->priv;
	int *i = (int *) extra;
	uint32_t reg = *i;
	uint32_t val = *(i + 1);

	//if (reg < 0x4000 || reg > 0x9000) { 
	//  return -EINVAL;
	//}

	OS_REG_WRITE(sc->sc_ah, reg, val);
	return  0;
}
#endif

/* Structures to export the Wireless Handlers */
static const meshw_handler phy_handlers[] =
{
	(meshw_handler) NULL,				/* MESHW_IO_W_COMMIT */
	(meshw_handler) phy_ioctl_giwname,			/* MESHW_IO_R_NAME */
	(meshw_handler) NULL,				/* MESHW_IO_W_NWID */
	(meshw_handler) NULL,				/* MESHW_IO_R_NWID */
	(meshw_handler) phy_ioctl_siwfreq,			/* MESHW_IO_W_FREQ */
	(meshw_handler) phy_ioctl_giwfreq,			/* MESHW_IO_R_FREQ */
	(meshw_handler) phy_ioctl_siwmode,			/* MESHW_IO_W_MODE */
	(meshw_handler) phy_ioctl_giwmode,			/* MESHW_IO_R_MODE */
	(meshw_handler) phy_ioctl_siwsens,			/* MESHW_IO_W_SENS */
	(meshw_handler) phy_ioctl_giwsens,			/* MESHW_IO_R_SENS */
	(meshw_handler) NULL /* not used */,		/* MESHW_IO_W_RANGE */
	(meshw_handler) phy_ioctl_giwrange,		/* MESHW_IO_R_RANGE */
	(meshw_handler) NULL /* not used */,		/* MESHW_IO_W_PRIV */
	(meshw_handler) NULL /* kernel code */,		/* MESHW_IO_R_PRIV */
	(meshw_handler) NULL /* not used */,		/* MESHW_IO_W_STATS */
	(meshw_handler) NULL /* kernel code */,		/* MESHW_IO_R_STATS */
	(meshw_handler) NULL,				/* MESHW_IO_W_SPY */
	(meshw_handler) NULL,				/* MESHW_IO_R_SPY */
	(meshw_handler) NULL,				/* -- hole -- */
	(meshw_handler) NULL,				/* -- hole -- */
	(meshw_handler) phy_ioctl_siwap,			/* MESHW_IO_W_AP_MAC */
	(meshw_handler) phy_ioctl_giwap,			/* MESHW_IO_R_AP_MAC */
	(meshw_handler) NULL,				/* -- hole -- */
	(meshw_handler) phy_ioctl_iwaplist,		/* MESHW_IO_R_AP_LIST */
#ifdef MESHW_IO_R_SCAN
	(meshw_handler) phy_ioctl_siwscan,			/* MESHW_IO_W_SCAN */
	(meshw_handler) phy_ioctl_giwscan,			/* MESHW_IO_R_SCAN */
#else
	(meshw_handler) NULL,				/* MESHW_IO_W_SCAN */
	(meshw_handler) NULL,				/* MESHW_IO_R_SCAN */
#endif /* MESHW_IO_R_SCAN */
	(meshw_handler) phy_ioctl_siwessid,		/* MESHW_IO_W_ESSID */
	(meshw_handler) phy_ioctl_giwessid,		/* MESHW_IO_R_ESSID */
	(meshw_handler) phy_ioctl_siwnickn,		/* MESHW_IO_W_NICKNAME */
	(meshw_handler) phy_ioctl_giwnickn,		/* MESHW_IO_R_NICKNAME */
	(meshw_handler) NULL,				/* -- hole -- */
	(meshw_handler) NULL,				/* -- hole -- */
	(meshw_handler) phy_ioctl_siwrate,			/* MESHW_IO_W_RATE */
	(meshw_handler) phy_ioctl_giwrate,			/* MESHW_IO_R_RATE */
	(meshw_handler) phy_ioctl_siwrts,			/* MESHW_IO_W_RTS */
	(meshw_handler) phy_ioctl_giwrts,			/* MESHW_IO_R_RTS */
	(meshw_handler) phy_ioctl_siwfrag,			/* MESHW_IO_W_FRAG_THR */
	(meshw_handler) phy_ioctl_giwfrag,			/* MESHW_IO_R_FRAG_THR */
	(meshw_handler) phy_ioctl_siwtxpow,		/* MESHW_IO_W_TX_POW */
	(meshw_handler) phy_ioctl_giwtxpow,		/* MESHW_IO_R_TX_POW */
	(meshw_handler) phy_ioctl_siwretry,		/* MESHW_IO_W_RETRY */
	(meshw_handler) phy_ioctl_giwretry,		/* MESHW_IO_R_RETRY */
	(meshw_handler) phy_ioctl_siwencode,		/* MESHW_IO_W_ENCODE */
	(meshw_handler) phy_ioctl_giwencode,		/* MESHW_IO_R_ENCODE */
	(meshw_handler) phy_ioctl_siwpower,		/* MESHW_IO_W_POWER_MGMT */
	(meshw_handler) phy_ioctl_giwpower,		/* MESHW_IO_R_POWER_MGMT */
};

static const meshw_handler phy_priv_handlers[] =
{
	(meshw_handler) phy_ioctl_setparam,		/* SIOCWFIRSTPRIV+0 */
	(meshw_handler) phy_ioctl_getparam,		/* SIOCWFIRSTPRIV+1 */
	(meshw_handler) phy_ioctl_setkey,			/* SIOCWFIRSTPRIV+2 */
	(meshw_handler) NULL,				/* SIOCWFIRSTPRIV+3 */
	(meshw_handler) phy_ioctl_delkey,			/* SIOCWFIRSTPRIV+4 */
	(meshw_handler) NULL,				/* SIOCWFIRSTPRIV+5 */
	(meshw_handler) phy_ioctl_setmlme,			/* SIOCWFIRSTPRIV+6 */
	(meshw_handler) NULL,				/* SIOCWFIRSTPRIV+7 */
	(meshw_handler) phy_ioctl_setoptie,		/* SIOCWFIRSTPRIV+8 */
	(meshw_handler) phy_ioctl_getoptie,		/* SIOCWFIRSTPRIV+9 */
	(meshw_handler) phy_ioctl_addmac,			/* SIOCWFIRSTPRIV+10 */
	(meshw_handler) NULL,				/* SIOCWFIRSTPRIV+11 */
	(meshw_handler) phy_ioctl_delmac,			/* SIOCWFIRSTPRIV+12 */
	(meshw_handler) NULL,				/* SIOCWFIRSTPRIV+13 */
	(meshw_handler) phy_ioctl_chanlist,		/* SIOCWFIRSTPRIV+14 */
#ifdef HAS_VMAC
	(meshw_handler) NULL,
	(meshw_handler) phy_ioctl_setreg, 
	(meshw_handler) phy_ioctl_getreg, 
#endif
};

struct meshw_handler_def phy_meshw_handler_def =
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
	.standard				= (meshw_handler *) phy_handlers,
	.num_standard		= N(phy_handlers),
	.private				= (meshw_handler *) phy_priv_handlers,
	.num_private			= N(phy_priv_handlers),
#undef N
};

/*
 * Diagnostic interface to the HAL.  This is used by various
 * tools to do things like retrieve register contents for
 * debugging.  The mechanism is intentionally opaque so that
 * it can change frequently w/o concern for compatiblity.
 */
int _phy_ioctl_diag(struct ath_softc *sc, struct ath_diag *ad)
{
	struct ath_hal *ah = sc->sc_ah;
	u_int id = ad->ad_id & ATH_DIAG_ID;
	void *indata = NULL;
	void *outdata = NULL;
	u_int32_t insize = ad->ad_in_size;
	u_int32_t outsize = ad->ad_out_size;
	int error = 0;

	if (ad->ad_id & ATH_DIAG_IN)
	{
		/* Copy in data. */
		indata = kmalloc(insize, GFP_KERNEL);
		if (indata == NULL)
		{
			error = -ENOMEM;
			goto bad;
		}
		if (copy_from_user(indata, ad->ad_in_data, insize))
		{
			error = -EFAULT;
			goto bad;
		}
	}
	
	if (ad->ad_id & ATH_DIAG_DYN)
	{
		/*
		 * Allocate a buffer for the results (otherwise the HAL
		 * returns a pointer to a buffer where we can read the
		 * results).  Note that we depend on the HAL leaving this
		 * pointer for us to use below in reclaiming the buffer;
		 * may want to be more defensive.
		 */
		outdata = kmalloc(outsize, GFP_KERNEL);
		if (outdata == NULL)
		{
			error = -ENOMEM;
			goto bad;
		}
	}
	
	if (ath_hal_getdiagstate(ah, id, indata, insize, &outdata, &outsize))
	{
		if (outsize < ad->ad_out_size)
			ad->ad_out_size = outsize;
		if (outdata && copy_to_user(ad->ad_out_data, outdata, ad->ad_out_size))
			error = -EFAULT;
	}
	else
	{
		error = -EINVAL;
	}
bad:
	if ((ad->ad_id & ATH_DIAG_IN) && indata != NULL)
		kfree(indata);
	if ((ad->ad_id & ATH_DIAG_DYN) && outdata != NULL)
		kfree(outdata);
	return error;
}

