# /bin/sh
# Start WLAN and Ad Hoc Router(AODV)
#  Li Zhijie, 2005.04.20

insmod  wlan
insmod 	ath_hal
insmod	ath_rate_onoe
insmod 	ath_pci

iwconfig ath0 mode Ad-Hoc
iwconfig ath0 essid "atheros-test"
iwconfig ath0 nickname "adhoc_assist"

# mode : auto
iwpriv ath0 mode  3
iwconfig ath0 key off

ifconfig ath0 192.168.5.1 
