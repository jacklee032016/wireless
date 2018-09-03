# /bin/sh
# scripts start all Drivers
# Li Zhijie, 2006.08.30
# $Id: startup.sh,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $

echo "Start WLAN Module of MESH....."
MESHCONFIG=./meshconfig
START=./mgrconfig
$MESHCONFIG 1 mode Ad-Hoc
$MESHCONFIG 1 essid "mesh-1"
$MESHCONFIG 1 nick "adhoc-1"
$MESHCONFIG 1 key off

ifconfig eth0 down
./mgrconfig 0 portal enable eth0 

ifconfig eth0 0.0.0.0
ifconfig eth0 up

ifconfig mesh up

ifconfig mbr 10.97.1.1 

$START 0 start Yes
echo "WLAN Module Started"
