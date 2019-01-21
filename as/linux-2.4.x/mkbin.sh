#!/bin/sh
#rm vmlinux.gz
gzip -9 vmlinux
mkimage -A arm -T kernel -C gzip -n "Assist Linux" -a 0x00000000 -e 0x00008000 -d vmlinux.gz vmlinux.bin
gzip -d vmlinux.gz

#cp vmlinux.bin ~yclin/
#rm ../images/ramdisk.bin
#/home/arm/u-boot-1.1.1/tools/mkimage -A arm -T ramdisk -C gzip -a 0x400000 -d ../images/ramdisk.gz ../images/ramdisk.bin
#cp ../images/ramdisk.bin ~yclin/
