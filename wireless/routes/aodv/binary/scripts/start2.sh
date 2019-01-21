#!/bin/sh

#AODV Dev
# Tells AODV which interface use wish to use.

AODV_DEV="ath0"

LOCAL_SUBNET="paramLocalSubnet=10.97.1.2/255.255.255.0"

echo "1" > /proc/sys/net/ipv4/ip_forward
echo "0" > /proc/sys/net/ipv4/route/min_delay

insmod ./aodv.o paramDevName=$AODV_DEV $LOCAL_SUBNET 
 