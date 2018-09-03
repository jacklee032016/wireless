# /bin/sh

echo "Using 802.11 NIC as Access Point"	
iwconfig ath0 mode Master
iwconfig ath0 essid "atheros-ap"
iwconfig ath0 nickname "ap_assist"
	
iwpriv ath0 mode  3
iwconfig ath0 key off
	
ifconfig ath0 192.168.5.1 
