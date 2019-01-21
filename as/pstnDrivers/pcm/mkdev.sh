#
# $Revision: 1.1.1.1 $
# $Author: lizhijie $
# $Log: mkdev.sh,v $
# Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
# rebuild
#
# Revision 1.1  2005/06/27 05:59:01  lizhijie
# no message
#
#
# Driver for Tiger-AnalogPBX

#
#! /bin/sh

# create all device files need by Assist Telephone card
MAJOR=197

rm -r -f /dev/astel
mkdir -p /dev/astel

# Global control device file
# mknod /dev/astel/span c $MAJOR 0 ：删除这个设备文件，2005.06.24

# Channel 0 device file
mknod /dev/astel/0 c $MAJOR 0

# Channel 1 device file
mknod /dev/astel/1 c $MAJOR 1

# Channel 2 device file
mknod /dev/astel/2 c $MAJOR 2

# Channel 3 device file
mknod /dev/astel/3 c $MAJOR 3

# Channel 4 device file
mknod /dev/astel/4 c $MAJOR 4

mknod /dev/astel/5 c $MAJOR 5
mknod /dev/astel/6 c $MAJOR 6
mknod /dev/astel/7 c $MAJOR 7
mknod /dev/astel/8 c $MAJOR 8
mknod /dev/astel/9 c $MAJOR 9
mknod /dev/astel/10 c $MAJOR 10
mknod /dev/astel/11 c $MAJOR 11
