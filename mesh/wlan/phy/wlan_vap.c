/*
* $Id: wlan_vap.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "_phy.h"

/* list of all ieee8011com instances */
SLIST_HEAD(ieee80211_list, ieee80211com);
static struct ieee80211_list ieee80211_list = SLIST_HEAD_INITIALIZER(ieee80211_list);

static u_int8_t ieee80211_vapmap[32];		// enough for 256
DECLARE_MUTEX(ieee80211_vap_mtx);

void _ieee80211_add_vap(struct ieee80211com *ic)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))

	u_int i;
	u_int8_t b;
	down(&ieee80211_vap_mtx);
	ic->ic_vap = 0;
	for (i = 0; i < N(ieee80211_vapmap) && ieee80211_vapmap[i] == 0xff; i++)
		ic->ic_vap += NBBY;
	if (i == N(ieee80211_vapmap))
		panic("vap table full");
	for (b = ieee80211_vapmap[i]; b & 1; b >>= 1)
		ic->ic_vap++;
	setbit(ieee80211_vapmap, ic->ic_vap);
	SLIST_INSERT_HEAD(&ieee80211_list, ic, ic_next);
	up(&ieee80211_vap_mtx);
#undef N
}

void _ieee80211_remove_vap(struct ieee80211com *ic)
{
	down(&ieee80211_vap_mtx);
	SLIST_REMOVE(&ieee80211_list, ic, ieee80211com, ic_next);
	KASSERT(ic->ic_vap < sizeof(ieee80211_vapmap)*NBBY,	("invalid vap id %d", ic->ic_vap));
	KASSERT(isset(ieee80211_vapmap, ic->ic_vap), ("vap id %d not allocated", ic->ic_vap));

	clrbit(ieee80211_vapmap, ic->ic_vap);
	up(&ieee80211_vap_mtx);
}

#if WITH_DOY_1X
/* Find an instance by it's mac address. */
struct ieee80211com *ieee80211_find_vap(const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic;

	/* XXX lock */
	SLIST_FOREACH(ic, &ieee80211_list, ic_next)
	{
		if (IEEE80211_ADDR_EQ(mac, ic->ic_myaddr))
			return ic;
	}	
	return NULL;
}
EXPORT_SYMBOL(ieee80211_find_vap);
#endif

struct ieee80211com *_ieee80211_find_instance(MESH_DEVICE *dev)
{
	struct ieee80211com *ic;

	/* XXX lock */
	/* XXX not right for multiple instances but works for now */
	SLIST_FOREACH(ic, &ieee80211_list, ic_next)
	{
		if (ic->ic_dev == dev)
		{
			return ic;
		}	
	}	
	return NULL;
}

