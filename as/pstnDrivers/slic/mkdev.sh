#
# $Author: lizhijie $
# $Log: mkdev.sh,v $
# Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
# rebuild
#
# Revision 1.3  2005/07/07 02:43:23  wangwei
# no message
#
# Revision 1.2  2005/04/26 06:06:09  lizhijie
# *** empty log message ***
#
# Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
# new drivers for all devices
#
# Revision 1.3  2004/12/17 07:36:51  lizhijie
# add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
#
# Revision 1.2  2004/12/11 05:33:50  lizhijie
# add IXP422 suppport
#
# Revision 1.2  2004/12/02 05:55:57  lizhijie
# Commite after integrated test with Analog-PBX
#
# Revision 1.1.1.1  2004/11/29 01:47:46  lizhijie
# Driver for Tiger-AnalogPBX
#
# $Revision: 1.1.1.1 $
#
#! /bin/sh
# create all device files need by Assist Telephone card
MAJOR=197

rm -r -f /dev/astel
mkdir -p /dev/astel

# Global control device file
#mknod /dev/astel/span c $MAJOR 0
# Channel 1 device file
mknod /dev/astel/0 c $MAJOR 0
# Channel 2 device file
mknod /dev/astel/1 c $MAJOR 1
# Channel 3 device file
mknod /dev/astel/2 c $MAJOR 2
# Channel 4 device file
mknod /dev/astel/3 c $MAJOR 3
mknod /dev/astel/4 c $MAJOR 4
mknod /dev/astel/5 c $MAJOR 5
mknod /dev/astel/6 c $MAJOR 6
mknod /dev/astel/7 c $MAJOR 7
mknod /dev/astel/8 c $MAJOR 8
mknod /dev/astel/9 c $MAJOR 9
mknod /dev/astel/10 c $MAJOR 10
mknod /dev/astel/11 c $MAJOR 11

