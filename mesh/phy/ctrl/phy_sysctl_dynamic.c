/*
* $Id: phy_sysctl_dynamic.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "phy.h"

#if 0
#include "if_ethersubr.h"		/* for ETHER_IS_MULTICAST */
#include "if_media.h"
#include "if_llc.h"

#include <ieee80211_var.h>


#include "radar.h"

#include "if_athvar.h"
#include "ah_desc.h"
#include "ah_devid.h"			/* XXX to identify IBM cards */
#include "ah.h"
#endif

#ifdef CONFIG_SYSCTL
/* Dynamic (i.e. per-device) sysctls.  These are automatically mirrored in /proc/sys. */
enum
{
	ATH_SLOTTIME			= 1,
	ATH_ACKTIMEOUT		= 2,
	ATH_CTSTIMEOUT		= 3,
	ATH_SOFTLED			= 4,
	ATH_LEDPIN				= 5,
	ATH_COUNTRYCODE		= 6,
	ATH_REGDOMAIN			= 7,
	ATH_DEBUG				= 8,
	ATH_TXANTENNA			= 9,
	ATH_RXANTENNA			= 10,
	ATH_DIVERSITY			= 11,
	ATH_TXINTRPERIOD		= 12,
	ATH_TPSCALE     			= 13,
	ATH_TPC         			= 14,
	ATH_TXPOWLIMIT  		= 15,	
	ATH_VEOL        			= 16,
	ATH_BINTVAL			= 17,
	ATH_RAWDEV      		= 18,
	ATH_RAWDEV_TYPE 		= 19,
	ATH_RXFILTER   			= 20,
	ATH_RADARSIM   		= 21,
};


#define	CTL_AUTO	-2	/* cannot be CTL_ANY or CTL_NONE */

#define NUM_RADIOTAP_ELEMENTS 18

static int radiotap_elem_to_bytes[NUM_RADIOTAP_ELEMENTS] = 
{
	8, /* IEEE80211_RADIOTAP_TSFT */
	 1, /* IEEE80211_RADIOTAP_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RATE */
	 4, /* IEEE80211_RADIOTAP_CHANNEL */
	 2, /* IEEE80211_RADIOTAP_FHSS */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_LOCK_QUALITY */
	 2, /* IEEE80211_RADIOTAP_TX_ATTENUATION */
	 2, /* IEEE80211_RADIOTAP_DB_TX_ATTENUATION */
	 1, /* IEEE80211_RADIOTAP_DBM_TX_POWER */
	 1, /* IEEE80211_RADIOTAP_ANTENNA */
	 1, /* IEEE80211_RADIOTAP_DB_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DB_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_RX_FLAGS */
	 2, /* IEEE80211_RADIOTAP_TX_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RTS_RETRIES */
	 1, /* IEEE80211_RADIOTAP_DATA_RETRIES */
};


/*
 * the following rt_* functions deal with verifying that a valid
 * radiotap header is on a packet as well as functions to extracting
 * what information is included.
 * XXX maybe these should go in ieee_radiotap.c
 */
static int rt_el_present(struct ieee80211_radiotap_header *th, u_int32_t element)
{
	if (element > NUM_RADIOTAP_ELEMENTS)
		return 0;
	return th->it_present & (1 << element);
}

static int rt_check_header(struct ieee80211_radiotap_header *th, int len) 
{
	int bytes = 0;
	int x = 0;
	if (th->it_version != 0) 
		return 0;

	if (th->it_len < sizeof(struct ieee80211_radiotap_header))
		return 0;
	
	for (x = 0; x < NUM_RADIOTAP_ELEMENTS; x++)
	{
		if (rt_el_present(th, x))
		    bytes += radiotap_elem_to_bytes[x];
	}

	if (th->it_len < sizeof(struct ieee80211_radiotap_header) + bytes) 
		return 0;
	
	if (th->it_len > len)
		return 0;

	return 1;
}

static u_int8_t *rt_el_offset(struct ieee80211_radiotap_header *th, u_int32_t element)
{
	unsigned int x = 0;
	u_int8_t *offset = ((u_int8_t *) th) + sizeof(struct ieee80211_radiotap_header);
	for (x = 0; x < NUM_RADIOTAP_ELEMENTS && x < element; x++)
	{
		if (rt_el_present(th, x))
			offset += radiotap_elem_to_bytes[x];
	}

	return offset;
}


static int ATH_SYSCTL_DECL(ath_sysctl_halparam, ctl, write, filp, buffer, lenp, ppos)
{
	struct ath_softc *sc = ctl->extra1;
	struct ath_hal *ah = sc->sc_ah;
	u_int val;
	int ret;

	ctl->data = &val;
	ctl->maxlen = sizeof(val);
	if (write)
	{/* write */
		ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos);
		if (ret == 0)
		{
			switch (ctl->ctl_name)
			{
				case ATH_SLOTTIME:
					if (!ath_hal_setslottime(ah, val))
						ret = -EINVAL;
					break;
				case ATH_ACKTIMEOUT:
					if (!ath_hal_setacktimeout(ah, val))
						ret = -EINVAL;
					break;
				case ATH_CTSTIMEOUT:
					if (!ath_hal_setctstimeout(ah, val))
						ret = -EINVAL;
					break;
#if 0
				case ATH_SOFTLED:
					if (val != sc->sc_softled)
					{
						if (val)
							ath_hal_gpioCfgOutput(ah, sc->sc_ledpin);
						ath_hal_gpioset(ah, sc->sc_ledpin,!val);
						sc->sc_softled = val;
					}
					break;
				case ATH_LEDPIN:
					sc->sc_ledpin = val;
					break;
#endif

				case ATH_DEBUG:
					sc->sc_debug = val;
					break;
				case ATH_TXANTENNA:
					/* XXX validate? */
					sc->sc_txantenna = val;
					break;
				case ATH_RXANTENNA:
					/* XXX validate? */
					ath_setdefantenna(sc, val);
					break;
				case ATH_DIVERSITY:
					/* XXX validate? */
					if (!sc->sc_hasdiversity)
						return -EINVAL;
					sc->sc_diversity = val;
					ath_hal_setdiversity(ah, val);
					break;
				case ATH_TXINTRPERIOD:
					sc->sc_txintrperiod = val;
					break;
	                        case ATH_TPSCALE:
	                                /* XXX validate? */
					// TODO: error handling
	                                ath_hal_settpscale(ah, val);
	                                break;
	                        case ATH_TPC:
	                                /* XXX validate? */
	                                if (!sc->sc_hastpc)
	                                        return -EINVAL;
	                                ath_hal_settpc(ah, val);
	                                break;
	                        case ATH_TXPOWLIMIT:
	                                /* XXX validate? */
	                                ath_hal_settxpowlimit(ah, val);
	                                break;
	                        case ATH_BINTVAL:
					if ((sc->sc_ic).ic_opmode != IEEE80211_M_HOSTAP &&
	    					(sc->sc_ic).ic_opmode != IEEE80211_M_IBSS)
	                    		    return -EINVAL;
					
	            			if (IEEE80211_BINTVAL_MIN <= val && val <= IEEE80211_BINTVAL_MAX)
					{
						(sc->sc_ic).ic_lintval = val;
						ret = -ENETRESET;              /* requires restart */
	            			}
					else
	                    		    ret = -EINVAL;

					break;
#if 0									
				case ATH_RAWDEV:
					if (val && !sc->sc_rawdev_enabled)
					{
						ath_rawdev_attach(sc);
					}
					else if (!val && sc->sc_rawdev_enabled)
					{
						ath_rawdev_detach(sc);
					}
					
					_phy_reset(&sc->sc_dev);
					break;
				case ATH_RAWDEV_TYPE:
					switch (val)
					{
						case 0: 
							sc->sc_rawdev.type = ARPHRD_IEEE80211;
							break;
						case 1: 
							sc->sc_rawdev.type = ARPHRD_IEEE80211_PRISM;
							break;
						case 2: 
							sc->sc_rawdev.type = ARPHRD_IEEE80211_RADIOTAP;
							break;
						default:
							return -EINVAL;
					}
					_phy_reset(&sc->sc_dev);
					break;
#endif					
				case ATH_RXFILTER:
					sc->sc_rxfilter = val;
#ifdef HAS_VMAC
					_phy_reset(&sc->sc_dev);
#else
#define 	HAL_RX_FILTER_ALLOWALL  0x3BF		/* should check the newly code from CVS, lzj, 2006.04.01 */

					ath_hal_setrxfilter(ah,HAL_RX_FILTER_ALLOWALL);
#endif
					break;
				case ATH_RADARSIM:
					ATH_SCHEDULE_TQUEUE (&sc->sc_radartq, &needmark);
					break;
				default:
					return -EINVAL;
			}
		}
	}
	else
	{/* read */
		switch (ctl->ctl_name)
		{
			case ATH_SLOTTIME:
				val = ath_hal_getslottime(ah);
				break;
			case ATH_ACKTIMEOUT:
				val = ath_hal_getacktimeout(ah);
				break;
			case ATH_CTSTIMEOUT:
				val = ath_hal_getctstimeout(ah);
				break;
			case ATH_SOFTLED:
				val = sc->sc_softled;
				break;
			case ATH_LEDPIN:
				val = sc->sc_ledpin;
				break;
			case ATH_COUNTRYCODE:
				ath_hal_getcountrycode(ah, &val);
				break;
			case ATH_REGDOMAIN:
				ath_hal_getregdomain(ah, &val);
				break;
			case ATH_DEBUG:
				val = sc->sc_debug;
				break;
			case ATH_TXANTENNA:
				val = sc->sc_txantenna;
				break;
			case ATH_RXANTENNA:
				val = ath_hal_getdefantenna(ah);
				break;
			case ATH_DIVERSITY:
				val = sc->sc_diversity;
				break;
			case ATH_TXINTRPERIOD:
				val = sc->sc_txintrperiod;
				break;
			case ATH_TPSCALE:
				ath_hal_gettpscale(ah, &val);
				break;
			case ATH_TPC:
				val = ath_hal_gettpc(ah);
				break;
			case ATH_TXPOWLIMIT:
				ath_hal_gettxpowlimit(ah, &val);
				break;
			case ATH_VEOL:
				val = sc->sc_hasveol;
	 			break;
			case ATH_BINTVAL:
				val = (sc->sc_ic).ic_lintval;
	 			break;
#if 0				
			case ATH_RAWDEV:
				val = sc->sc_rawdev_enabled;
	 			break;
			case ATH_RAWDEV_TYPE:
				switch (sc->sc_rawdev.type)
				{
					case ARPHRD_IEEE80211:
						val = 0;
						break;
					case ARPHRD_IEEE80211_PRISM:
						val = 1;
						break;
					case ARPHRD_IEEE80211_RADIOTAP:
						val = 2;
						break;
					default: 
						val = 0;
				}
	 			break;
#endif

			case ATH_RXFILTER:
#ifdef HAS_VMAC
			  val = ath_hal_getrxfilter(ah);
#else
				val = sc->sc_rxfilter;
#endif
				break;
			case ATH_RADARSIM:
				val = 0;
				break;
			default:
				return -EINVAL;
		}
		
		ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos);
	}/* end of read */
	return ret;
}

static const ctl_table ath_sysctl_template[] =
{
	{
		.ctl_name	= ATH_SLOTTIME,
		.procname	= "slottime",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_ACKTIMEOUT,
		.procname	= "acktimeout",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_CTSTIMEOUT,
		.procname	= "ctstimeout",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
#if 0	
	{ 
		.ctl_name	= ATH_SOFTLED,
		.procname	= "softled",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{
		.ctl_name	= ATH_LEDPIN,
		.procname	= "ledpin",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
#endif	
	{ 
		.ctl_name	= ATH_COUNTRYCODE,
		.procname	= "countrycode",
		.mode		= 0444,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_REGDOMAIN,
		.procname	= "regdomain",
		.mode		= 0444,
		.proc_handler	= ath_sysctl_halparam
	},
#ifdef AR_DEBUG
	{ 
		.ctl_name	= ATH_DEBUG,
		.procname	= "debug",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
#endif
	{ 
		.ctl_name	= ATH_TXANTENNA,
		.procname	= "txantenna",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_RXANTENNA,
		.procname	= "rxantenna",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_DIVERSITY,
		.procname	= "diversity",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_TXINTRPERIOD,
		.procname	= "txintrperiod",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},/* 12 */
	{ 
		.ctl_name	= ATH_TPSCALE,
		.procname	= "tpscale",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_TPC,
		.procname	= "tpc",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_TXPOWLIMIT,
		.procname	= "txpowlimit",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_VEOL,
		.procname	= "veol",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_BINTVAL,
		.procname	= "bintval",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
#if 0	
	{ 
		.ctl_name	= ATH_RAWDEV,
		.procname	= "rawdev",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_RAWDEV_TYPE,
		.procname	= "rawdev_type",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
#endif	
	{ 
		.ctl_name	= ATH_RXFILTER,
		.procname	= "rxfilter",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 
		.ctl_name	= ATH_RADARSIM,
		.procname	= "radarsim",
		.mode		= 0644,
		.proc_handler	= ath_sysctl_halparam
	},
	{ 0 }
};

void ath_dynamic_sysctl_register(struct ath_softc *sc)
{
	int i, space;

	space = 5*sizeof(struct ctl_table) + sizeof(ath_sysctl_template);
	sc->sc_sysctls = kmalloc(space, GFP_KERNEL);
	if (sc->sc_sysctls == NULL)
	{
		printk("%s: no memory for sysctl table!\n", __func__);
		return;
	}

	/* setup the table */
	memset(sc->sc_sysctls, 0, space);
	sc->sc_sysctls[0].ctl_name = CTL_DEV;
	sc->sc_sysctls[0].procname = "dev";
	sc->sc_sysctls[0].mode = 0555;
	sc->sc_sysctls[0].child = &sc->sc_sysctls[2];
	/* [1] is NULL terminator */
	sc->sc_sysctls[2].ctl_name = CTL_AUTO;
	sc->sc_sysctls[2].procname = sc->sc_dev.name;
	sc->sc_sysctls[2].mode = 0555;
	sc->sc_sysctls[2].child = &sc->sc_sysctls[4];
	/* [3] is NULL terminator */
	/* copy in pre-defined data */
	memcpy(&sc->sc_sysctls[4], ath_sysctl_template, sizeof(ath_sysctl_template));

	/* add in dynamic data references */
	for (i = 4; sc->sc_sysctls[i].ctl_name; i++)
		if (sc->sc_sysctls[i].extra1 == NULL)
			sc->sc_sysctls[i].extra1 = sc;

	/* and register everything */
	sc->sc_sysctl_header = register_sysctl_table(sc->sc_sysctls, 1);
	if (!sc->sc_sysctl_header)
	{
		printk("%s: failed to register sysctls!\n", sc->sc_dev.name);
		kfree(sc->sc_sysctls);
		sc->sc_sysctls = NULL;
	}

	/* initialize values */
#ifdef AR_DEBUG
	sc->sc_debug = ath_debug;
#endif
	sc->sc_txantenna = 0;		/* default to auto-selection */
	sc->sc_txintrperiod = ATH_TXINTR_PERIOD;
}

void ath_dynamic_sysctl_unregister(struct ath_softc *sc)
{
	if (sc->sc_sysctl_header)
	{
		unregister_sysctl_table(sc->sc_sysctl_header);
		sc->sc_sysctl_header = NULL;
	}
	if (sc->sc_sysctls)
	{
		kfree(sc->sc_sysctls);
		sc->sc_sysctls = NULL;
	}
}

#endif

