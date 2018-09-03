# /bin/sh
# scripts which build all Device file
# Li Zhijie, 2006.08.29
# $Id: mkdev.sh,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $

MAJOR=199  

echo "make device file for SWJTU Mesh Device"

DEV_HOME=/dev/mesh

if [ -d $DEV_HOME ]; then
	rm -rf $DEV_HOME
fi

mkdir -p $DEV_HOME

echo "make Mgr device for all Mesh Device"
mknod $DEV_HOME/0 	c 	$MAJOR 0

echo "make Mesh Device For Different MESH DEVICE"
mknod $DEV_HOME/1 	c 	$MAJOR 1
mknod $DEV_HOME/2 	c 	$MAJOR 2
