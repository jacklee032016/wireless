#!/bin/sh

rm ramdisk.bin
mkimage -A arm -T ramdisk -C gzip -n "Assist RamDisk" -a 0x400000 -d ramdisk.gz ramdisk.bin
