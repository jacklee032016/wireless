# /bin/sh

echo "Wireless-MAC test Projects"

echo "MAC Drivers"
insmod route_core.o paramLocalSubnet=10.97.1.5/255.255.255.0
insmod wlan.o
insmod ath_hal.o
insmod ath_rate_onoe.o
insmod ath_pci.o

# ��������������ʹ�÷�ʽ�ͻ�����Ϣ���ڲ���AODV·�ɵĽڵ��ϱ���������ͬ
iwconfig ath0 mode Ad-Hoc
iwconfig ath0 essid "mesh-1"
iwconfig ath0 nickname "adhoc_1"

# mode : auto�����ٶ�Ϊ�Զ�ѡ�񣻹ر���֤����
iwpriv ath0 mode  3
iwconfig ath0 key off

# ���������ĵ�ַ������ȷ�������������룬�����Ӱ�����·�ɱ�Ĳ���
# ��ͬ�ڵ��������ַӦ����ͬһ��������
ifconfig ath0 10.97.1.5 netmask 255.255.255.0 broadcast 10.97.1.255


# ���ڵ�ı���ת����·�������ܣ�����·�ɱ�ĸ����ӳ�Ϊ0,��֤AODV·���ܹ���ʱ���¡���ӳ������·�ɱ�
echo "1" > /proc/sys/net/ipv4/ip_forward
echo "0" > /proc/sys/net/ipv4/route/min_delay
