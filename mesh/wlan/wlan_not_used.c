/* Drain a queue of sk_buff's */
void skb_queue_drain(struct sk_buff_head *q)
{
	struct sk_buff *skb;
	unsigned long flags;
	spin_lock_irqsave(&q->lock, flags);
	while ((skb = __skb_dequeue(q)) != NULL)
		dev_kfree_skb(skb);
	spin_unlock_irqrestore(&q->lock, flags);
}

#if IEEE80211_VLAN_TAG_USED

/* Register a vlan group */
void ieee80211_vlan_register(struct ieee80211com *ic, struct vlan_group *grp)
{
	ic->ic_vlgrp = grp;
}
EXPORT_SYMBOL(ieee80211_vlan_register);

/* Kill (i.e. delete) a vlan identifier */
void ieee80211_vlan_kill_vid(struct ieee80211com *ic, unsigned short vid)
{
	if (ic->ic_vlgrp)
		ic->ic_vlgrp->vlan_devices[vid] = NULL;
}
EXPORT_SYMBOL(ieee80211_vlan_kill_vid);
#endif /* IEEE80211_VLAN_TAG_USED */


