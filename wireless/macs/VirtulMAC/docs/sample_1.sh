modprobe ath_pci

ifconfig ath0 up
iwconfig ath0 mode monitor
iwconfig ath0 channel 6

insmod softmac_cheesymac
insmod softmac_nullmac
insmod softmac_multimac
insmod softmac_remotemac
insmod softmac_rawmac

echo athphy > /proc/softmac/create_instance
echo 1 > /proc/softmac/insts/athphy0/softmac_enable
