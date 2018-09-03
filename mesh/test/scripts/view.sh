# /bin/sh
# scripts for view running status of mesh device
# Li Zhijie, 2006.10.08
# $Id: view.sh,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $

MESHCONFIG=./meshconfig
MGRCONFIG=./mgrconfig

echo "Status information of MESH Subsystem"
echo ""
echo "View bridge status info....."
$MGRCONFIG 0 showbridge
echo "MAC of mesh device in bridge must be the same as wifi device"
echo ""

echo "View MESH MGR info....."
cat /proc/mesh/meshMgr
echo "MAC of portal device must be the same as Bridge ID"
echo ""

echo "View MESH forward table....."
cat /proc/mesh/mac_fwd
echo "MAC of portal device must be the same as Bridge ID"
echo ""

echo "View Portal Device info....."
cat /proc/mesh/portal
echo "MAC of MESH device must be the same as Bridge ID"
echo "MAC of net device must be the same as WIFI"
echo ""

echo "View WIFI associated stations....."
cat /proc/sys/mesh/dev/wifi/mac/associated_sta
echo "Peer or Stations associated must be in this list"
echo ""

