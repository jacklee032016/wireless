insmod wlan.o
insmod ath_hal.o
insmod ath_rate_onoe.o
insmod ath_pci.o

iwconfig ath0 mode Ad-Hoc
iwconfig ath0 essid "atheros-test"
iwconfig ath0 nickname "adhoc_assist"

# mode : auto
iwpriv ath0 mode  3
iwconfig ath0 key off


ifconfig ath0 10.97.1.1 netmask 255.255.255.0 broadcast 10.97.1.255

