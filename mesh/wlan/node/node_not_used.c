

void ieee80211_dump_node(struct ieee80211_node_table *nt, struct ieee80211_node *ni)
{
	printf("0x%p: mac %s refcnt %d\n", ni,
		swjtu_mesh_mac_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni));
	printf("\tscangen %u authmode %u flags 0x%x\n",
		ni->ni_scangen, ni->ni_authmode, ni->ni_flags);
	printf("\tassocid 0x%x txpower %u vlan %u\n",
		ni->ni_associd, ni->ni_txpower, ni->ni_vlan);
	printf("\ttxseq %u rxseq %u fragno %u rxfragstamp %u\n",
		ni->ni_txseqs[0],
		ni->ni_rxseqs[0] >> IEEE80211_SEQ_SEQ_SHIFT,
		ni->ni_rxseqs[0] & IEEE80211_SEQ_FRAG_MASK,
		ni->ni_rxfragstamp);
	printf("\trstamp %u rssi %u intval %u capinfo 0x%x\n",
		ni->ni_rstamp, ni->ni_rssi, ni->ni_intval, ni->ni_capinfo);
	printf("\tbssid %s essid \"%.*s\" channel %u:0x%x\n",
		swjtu_mesh_mac_sprintf(ni->ni_bssid),
		ni->ni_esslen, ni->ni_essid,
		ni->ni_chan->ic_freq, ni->ni_chan->ic_flags);
	printf("\tfails %u inact %u txrate %u\n",
		ni->ni_fails, ni->ni_inact, ni->ni_txrate);
}

void ieee80211_dump_nodes(struct ieee80211_node_table *nt)
{
	ieee80211_iterate_nodes(nt,	(ieee80211_iter_func *) ieee80211_dump_node, nt);
}

