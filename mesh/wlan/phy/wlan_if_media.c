/*
* $Id: wlan_if_media.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#include "_phy.h"

#ifndef ifr_media
#define	ifr_media	ifr_ifru.ifru_ivalue
#endif

#define	WLAN_IFMEDIA_DEBUG

#ifdef WLAN_IFMEDIA_DEBUG
int	wlan_ifmedia_debug = 0;
static	void __wlan_ifmedia_printword(int);
#endif


/* Find media entry matching a given ifm word.*
 */
struct wlan_ifmedia_entry *__wlan_ifmedia_match(struct wlan_ifmedia *ifm, int target, int mask)
{
	struct wlan_ifmedia_entry *match, *next;

	match = NULL;
	mask = ~mask;

	LIST_FOREACH(next, &ifm->wlan_ifm_list, wlan_ifm_list)
	{
		if ((next->wlan_ifm_media & mask) == (target & mask))
		{
#if defined(WLAN_IFMEDIA_DEBUG) || defined(DIAGNOSTIC)
			if (match)
			{
				WLAN_DEBUG_INFO("%s : multiple match for 0x%x/0x%x\n",__FUNCTION__, target, mask);
			}
#endif
			match = next;
		}
	}

	return match;
}

/* Initialize if_media struct for a specific interface instance. */
void wlan_ifmedia_init( struct wlan_ifmedia *ifm,	int dontcare_mask, wlan_ifm_change_cb_t change_callback, wlan_ifm_stat_cb_t status_callback)
{

	LIST_INIT(&ifm->wlan_ifm_list);
	ifm->wlan_ifm_cur = NULL;
	ifm->wlan_ifm_media = 0;
	ifm->wlan_ifm_mask = dontcare_mask;		/* IF don't-care bits */
	ifm->wlan_ifm_change = change_callback;
	ifm->wlan_ifm_status = status_callback;
}

void wlan_ifmedia_removeall(struct wlan_ifmedia *ifm)
{
	struct wlan_ifmedia_entry *entry;

	for (entry = LIST_FIRST(&ifm->wlan_ifm_list); entry;entry = LIST_FIRST(&ifm->wlan_ifm_list))
	{
		LIST_REMOVE(entry, wlan_ifm_list);
		kfree(entry);
	}
}

/*
 * Add a media configuration to the list of supported media
 * for a specific interface instance.
 */
void wlan_ifmedia_add(struct wlan_ifmedia *ifm, int mword, int data, void *aux)
{
	register struct wlan_ifmedia_entry *entry;

#ifdef WLAN_IFMEDIA_DEBUG
	if (wlan_ifmedia_debug)
	{
		if (ifm == NULL)
		{
			WLAN_DEBUG_INFO("%s : null ifm\n", __FUNCTION__);
			return;
		}
		WLAN_DEBUG_INFO("Adding entry for ");
		__wlan_ifmedia_printword(mword);
	}
#endif

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (entry == NULL)
		panic("ifmedia_add: can't malloc entry");

	entry->wlan_ifm_media = mword;
	entry->wlan_ifm_data = data;
	entry->wlan_ifm_aux = aux;

	LIST_INSERT_HEAD(&ifm->wlan_ifm_list, entry, wlan_ifm_list);
}

#if 0
/*
 * Add an array of media configurations to the list of
 * supported media for a specific interface instance.
 */
void wlan_ifmedia_list_add(struct wlan_ifmedia *ifm, struct wlan_ifmedia_entry *lp, int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		wlan_ifmedia_add(ifm, lp[i].ifm_media, lp[i].ifm_data, lp[i].ifm_aux);
	}	
}
#endif

/*
 * Set the default active media. 
 *
 * Called by device-specific code which is assumed to have already
 * selected the default media in hardware.  We do _not_ call the
 * media-change callback.
 */
void wlan_ifmedia_set(struct wlan_ifmedia *ifm, int target)

{
	struct wlan_ifmedia_entry *match;

	match = __wlan_ifmedia_match(ifm, target, ifm->wlan_ifm_mask);

	if (match == NULL)
	{
		WLAN_DEBUG_INFO("%s : no match for 0x%x/0x%x\n",__FUNCTION__, target, ~ifm->wlan_ifm_mask);
		panic("ifmedia_set");
	}
	ifm->wlan_ifm_cur = match;

#ifdef WLAN_IFMEDIA_DEBUG
	if ( wlan_ifmedia_debug)
	{
		WLAN_DEBUG_INFO("%s : target ",__FUNCTION__);
		__wlan_ifmedia_printword(target);
		WLAN_DEBUG_INFO("%s : setting to ",__FUNCTION__);
		__wlan_ifmedia_printword(ifm->wlan_ifm_cur->wlan_ifm_media);
	}
#endif
}

/*
 * Device-independent media ioctl support function.
 */
//int wlan_ifmedia_ioctl(MESH_DEVICE *dev,struct ifreq *ifr,struct wlan_ifmedia *ifm, u_long cmd)
int wlan_ifmedia_ioctl(MESH_DEVICE *dev,struct wlan_ifmediareq *ifr,struct wlan_ifmedia *ifm, u_long cmd)
{
	struct wlan_ifmedia_entry *match;
	struct wlan_ifmediareq *ifmr = (struct wlan_ifmediareq *) ifr;
	int error = 0, sticky;

	if (dev == NULL || ifr == NULL || ifm == NULL)
		return(EINVAL);

	switch (cmd) 
	{
		/* Set the current media. */
		case  SIOCSIFMEDIA:
		{
			struct wlan_ifmedia_entry *oldentry;
			int oldmedia;
			/* why? ,lizhijie */
#if 1			
			int newmedia = ifr->wlan_ifm_current;//ifr_media;
#else
			int newmedia = ifmr->wlan_ifr_media;
#endif

			match = __wlan_ifmedia_match(ifm, newmedia, ifm->wlan_ifm_mask);
			if (match == NULL)
			{
#ifdef WLAN_IFMEDIA_DEBUG
				if (wlan_ifmedia_debug)
				{
					WLAN_DEBUG_INFO("%s : no media found for 0x%x\n",__FUNCTION__, newmedia);
				}
#endif
				return (ENXIO);
			}

			/*
			 * If no change, we're done.
			 * XXX Automedia may invole software intervention.
			 *     Keep going in case the the connected media changed.
			 *     Similarly, if best match changed (kernel debugger?).
			 */
			if ((IFM_SUBTYPE(newmedia) != IFM_AUTO) && (newmedia == ifm->wlan_ifm_media) && (match == ifm->wlan_ifm_cur) )
				return 0;

			/*
			 * We found a match, now make the driver switch to it.
			 * Make sure to preserve our old media type in case the
			 * driver can't switch.
			 */
#ifdef WLAN_IFMEDIA_DEBUG
			if (wlan_ifmedia_debug)
			{
				WLAN_DEBUG_INFO("%s : switching %s to ",__FUNCTION__, dev->name);
				__wlan_ifmedia_printword(match->wlan_ifm_media);
			}
#endif
			oldentry = ifm->wlan_ifm_cur;
			oldmedia = ifm->wlan_ifm_media;
			ifm->wlan_ifm_cur = match;
			ifm->wlan_ifm_media = newmedia;
			error = (*ifm->wlan_ifm_change)(dev);	/* callback */
			if (error)
			{
				ifm->wlan_ifm_cur = oldentry;
				ifm->wlan_ifm_media = oldmedia;
			}
			break;
		}

		/*
		 * Get list of available media and current media on interface.
		 */
		case  SIOCGIFMEDIA: 
		{
			struct wlan_ifmedia_entry *ep;
			int *kptr, count;
			int usermax;	/* user requested max */

			kptr = NULL;		/* XXX gcc */

			ifmr->wlan_ifm_active = ifmr->wlan_ifm_current = ifm->wlan_ifm_cur ? ifm->wlan_ifm_cur->wlan_ifm_media : IFM_NONE;
			ifmr->wlan_ifm_mask = ifm->wlan_ifm_mask;
			ifmr->wlan_ifm_status = 0;
			(*ifm->wlan_ifm_status)(dev, ifmr);

			count = 0;
			usermax = 0;

			/*
			 * If there are more interfaces on the list, count
			 * them.  This allows the caller to set ifmr->ifm_count
			 * to 0 on the first call to know how much space to
			 * allocate.
			 */
			LIST_FOREACH(ep, &ifm->wlan_ifm_list, wlan_ifm_list)
				usermax++;

			/*
			 * Don't allow the user to ask for too many
			 * or a negative number.
			 */
			if (ifmr->wlan_ifm_count > usermax)
				ifmr->wlan_ifm_count = usermax;
			else if (ifmr->wlan_ifm_count < 0)
				return (EINVAL);

			if (ifmr->wlan_ifm_count != 0)
			{
				kptr = (int *)kmalloc(ifmr->wlan_ifm_count * sizeof(int), GFP_KERNEL);

				if (kptr == NULL)
					return (ENOMEM);
				/*
				 * Get the media words from the interface's list.
				 */
				ep = LIST_FIRST(&ifm->wlan_ifm_list);
				for (; ep != NULL && count < ifmr->wlan_ifm_count; ep = LIST_NEXT(ep, wlan_ifm_list), count++)
					kptr[count] = ep->wlan_ifm_media;

				if (ep != NULL)
					error = E2BIG;	/* oops! */
			}
			else
			{
				count = usermax;
			}

			/*
			 * We do the copyout on E2BIG, because that's
			 * just our way of telling userland that there
			 * are more.  This is the behavior I've observed
			 * under BSD/OS 3.0
			 */
			sticky = error;
			if ((error == 0 || error == E2BIG) && ifmr->wlan_ifm_count != 0)
			{
				error = copy_to_user((caddr_t)ifmr->wlan_ifm_ulist,	(caddr_t)kptr, ifmr->wlan_ifm_count * sizeof(int));
			}

			if (error == 0)
				error = sticky;

			if (ifmr->wlan_ifm_count != 0)
				kfree(kptr);

			ifmr->wlan_ifm_count = count;
			break;
		}

		default:
			return (EINVAL);
	}

	return (error);
}

#ifdef WLAN_IFMEDIA_DEBUG
struct wlan_ifmedia_description ifm_type_descriptions[] =  IFM_TYPE_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_ethernet_descriptions[] = IFM_SUBTYPE_ETHERNET_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_ethernet_option_descriptions[] = IFM_SUBTYPE_ETHERNET_OPTION_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_tokenring_descriptions[] =  IFM_SUBTYPE_TOKENRING_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_tokenring_option_descriptions[] =  IFM_SUBTYPE_TOKENRING_OPTION_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_fddi_descriptions[] = IFM_SUBTYPE_FDDI_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_fddi_option_descriptions[] = IFM_SUBTYPE_FDDI_OPTION_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_ieee80211_descriptions[] =  IFM_SUBTYPE_IEEE80211_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_ieee80211_option_descriptions[] =  IFM_SUBTYPE_IEEE80211_OPTION_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_ieee80211_mode_descriptions[] = IFM_SUBTYPE_IEEE80211_MODE_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_subtype_shared_descriptions[] = IFM_SUBTYPE_SHARED_DESCRIPTIONS;

struct wlan_ifmedia_description ifm_shared_option_descriptions[] =  IFM_SHARED_OPTION_DESCRIPTIONS;

struct wlan_ifmedia_type_to_subtype
{
	struct wlan_ifmedia_description *subtypes;
	struct wlan_ifmedia_description *options;
	struct wlan_ifmedia_description *modes;
};

/* must be in the same order as IFM_TYPE_DESCRIPTIONS */
struct wlan_ifmedia_type_to_subtype ifmedia_types_to_subtypes[] =
{
	{
		  &ifm_subtype_ethernet_descriptions[0],
		  &ifm_subtype_ethernet_option_descriptions[0],
		  NULL,
	},
	{
		  &ifm_subtype_tokenring_descriptions[0],
		  &ifm_subtype_tokenring_option_descriptions[0],
		  NULL,
	},
	{
		  &ifm_subtype_fddi_descriptions[0],
		  &ifm_subtype_fddi_option_descriptions[0],
		  NULL,
	},
	{
		  &ifm_subtype_ieee80211_descriptions[0],
		  &ifm_subtype_ieee80211_option_descriptions[0],
		  &ifm_subtype_ieee80211_mode_descriptions[0]
	},
};

/* print a media word. */
static void __wlan_ifmedia_printword(int ifmw)
{
	struct wlan_ifmedia_description *desc;
	struct wlan_ifmedia_type_to_subtype *ttos;
	int seen_option = 0;

	/* Find the top-level interface type. */
	for (desc = ifm_type_descriptions, ttos = ifmedia_types_to_subtypes;   desc->wlan_ifmt_string != NULL; desc++, ttos++)
	{
		if (IFM_TYPE(ifmw) == desc->wlan_ifmt_word)
			break;
	}
	
	if (desc->wlan_ifmt_string == NULL)
	{
		WLAN_DEBUG_INFO("<unknown type>\n");
		return;
	}

	printk(desc->wlan_ifmt_string);

	/* Any mode. */
	for (desc = ttos->modes; desc && desc->wlan_ifmt_string != NULL; desc++)
	{
		if (IFM_MODE(ifmw) == desc->wlan_ifmt_word)
		{
			if (desc->wlan_ifmt_string != NULL)
				WLAN_DEBUG_INFO(" mode %s", desc->wlan_ifmt_string);
			break;
		}
	}
	
	/*
	 * Check for the shared subtype descriptions first, then the
	 * type-specific ones.
	 */
	for (desc = ifm_subtype_shared_descriptions;desc->wlan_ifmt_string != NULL; desc++)
	{
		if (IFM_SUBTYPE(ifmw) == desc->wlan_ifmt_word)
			goto got_subtype;
	}
	
	for (desc = ttos->subtypes; desc->wlan_ifmt_string != NULL; desc++)
	{
		if (IFM_SUBTYPE(ifmw) == desc->wlan_ifmt_word)
			break;
	}
	
	if (desc->wlan_ifmt_string == NULL)
	{
		WLAN_DEBUG_INFO(" <unknown subtype>\n");
		return;
	}

 got_subtype:
	WLAN_DEBUG_INFO(" %s", desc->wlan_ifmt_string);

	/*
	 * Look for shared options.
	 */
	for (desc = ifm_shared_option_descriptions; desc->wlan_ifmt_string != NULL; desc++)
	{
		if (ifmw & desc->wlan_ifmt_word)
		{
			if (seen_option == 0)
				WLAN_DEBUG_INFO(" <");
			WLAN_DEBUG_INFO("%s%s", seen_option++ ? "," : "", desc->wlan_ifmt_string);
		}
	}

	/* Look for subtype-specific options.*/
	for (desc = ttos->options; desc->wlan_ifmt_string != NULL; desc++)
	{
		if (ifmw & desc->wlan_ifmt_word)
		{
			if (seen_option == 0)
				WLAN_DEBUG_INFO(" <");
			WLAN_DEBUG_INFO("%s%s", seen_option++ ? "," : "", desc->wlan_ifmt_string); 
		}
	}
	WLAN_DEBUG_INFO("%s\n", seen_option ? ">" : "");
}
#endif /* WLAN_IFMEDIA_DEBUG */

EXPORT_SYMBOL(wlan_ifmedia_ioctl);

