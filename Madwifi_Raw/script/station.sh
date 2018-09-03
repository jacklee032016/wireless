# /bin/sh

echo "Using 802.11 NIC as mobile station"	
ifconfig ath0 up
udhcpc -i ath0
