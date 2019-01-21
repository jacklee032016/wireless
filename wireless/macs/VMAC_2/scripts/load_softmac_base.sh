#!/bin/sh

insmod ath_hal.o
insmod wlan.o
insmod ath_rate_onoe.o

insmod vm_core.o
insmod vnetif.o
insmod ath_pci.o

ifconfig ath0 up
iwconfig ath0 mode monitor
iwconfig ath0 channel 6

insmod aloha.o
# modprobe softmac_nullmac
# modprobe softmac_multimac
# modprobe softmac_remotemac
# modprobe softmac_rawmac

echo athphy > /proc/vmac/create
echo ALOHA > /proc/vmac/create 

# echo 1 > /proc/vmac/insts/athphy0/softmac_enable

ifconfig aloha1 10.97.1.1
ping 10.97.1.2
