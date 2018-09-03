# /bin/sh

echo "Wireless-MAC test Projects"

echo "MAC Drivers"
insmod wlan.o
insmod ath_hal.o
insmod ath_rate_onoe.o
insmod ath_pci.o

iwconfig ath0 mode Ad-Hoc
iwconfig ath0 essid "mesh-1"
iwconfig ath0 nickname "adhoc_1"

# mode : auto
iwpriv ath0 mode  3
iwconfig ath0 key off


ifconfig ath0 10.97.1.5 netmask 255.255.255.0 broadcast 10.97.1.255

echo "AODV Route Protocol"
#AODV Dev
AODV_DEV="ath0"

LOCAL_SUBNET="paramLocalSubnet=10.97.1.5/255.255.255.0"

echo "1" > /proc/sys/net/ipv4/ip_forward
echo "0" > /proc/sys/net/ipv4/route/min_delay

insmod ./route_core.o $LOCAL_SUBNET 
insmod ./ip_route.o paramDevName=$AODV_DEV                                        
            
