# /bin/sh
# scripts stop all Drivers
# Li Zhijie, 2006.08.30
# $Id: unload.sh,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $

echo "remove all drivers of MESH sub-system"
rmmod phy
rmmod mac
rmmod mesh
rmmod hal
