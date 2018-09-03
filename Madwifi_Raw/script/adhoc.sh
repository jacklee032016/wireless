# /bin/sh

echo "Using 802.11 NIC as Ad Hoc Peer"	
iwconfig ath0 mode Ad-Hoc
iwconfig ath0 essid "atheros-adhoc"
iwconfig ath0 nickname "adhoc_assist"

# auto-negotiation for 802.11 a/b/g
iwpriv ath0 mode  3
iwconfig ath0 key off
	
ifconfig ath0 192.168.5.1 
