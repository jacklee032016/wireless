# /bin/sh
# scripts start all Drivers
# Li Zhijie, 2006.08.30
# $Id: load.sh,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $

echo "Load Drivers....."
insmod hal.o
insmod mesh.o
insmod mac.o
insmod phy.o
echo "Drivers Loaded"
