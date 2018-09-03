# /bin/sh

echo "Wireless-MAC test Projects"

echo "MAC Drivers"
insmod wlan.o
insmod ath_hal.o
insmod ath_rate_onoe.o
insmod ath_pci.o

# ��������������ʹ�÷�ʽ�ͻ�����Ϣ���ڲ���AODV·�ɵĽڵ��ϱ���������ͬ
iwconfig ath0 mode Ad-Hoc
iwconfig ath0 essid "mesh-test"
iwconfig ath0 nickname "adhoc_mesh"

# mode : auto�����ٶ�Ϊ�Զ�ѡ�񣻹ر���֤����
iwpriv ath0 mode  3
iwconfig ath0 key off

# ���������ĵ�ַ������ȷ�������������룬�����Ӱ�����·�ɱ�Ĳ���
# ��ͬ�ڵ��������ַӦ����ͬһ��������
ifconfig ath0 10.97.1.1 netmask 255.255.255.0 broadcast 10.97.1.255


# ���ڵ�ı���ת����·�������ܣ�����·�ɱ�ĸ����ӳ�Ϊ0,��֤AODV·���ܹ���ʱ���¡���ӳ������·�ɱ�
echo "1" > /proc/sys/net/ipv4/ip_forward
echo "0" > /proc/sys/net/ipv4/route/min_delay

echo "AODV Route Protocol"
# ����һ
# AODVЭ��ʹ�õ������豸���ƣ�Ҳ������������������Ҫ֧�ֶ�������豸
AODV_DEV="ath0"

# ������
# ָ��������IP��ַ�������룬��ͬ�Ľڵ��ϱ���ʹ�ò�ͬ��IP��ַ
# �����ַ���Բ���AODVЭ��ʹ�õ������ĵ�ַ��������������Ψһ��
# ����ʹ�������ַ����һ������ʹ�õ�AODV·������·��������ά�����ڵ��Destination Sequence
# ���ҽ������ַ��Ϊ���ڵ㷢��RREQ��ID������������Ψһ
LOCAL_SUBNET="paramLocalSubnet=10.97.1.1/255.255.255.0"

# ������
# ��ѡ���õĲ��������ñ��ڵ��Ƿ�֧��Ĭ�����صĹ���
# �ڰ����������ӵ�Mesh Portal�ڵ�����Ҫ���ô˹��ܡ�0:�رմ˹���
# һ�㶼�򿪴˹��ܣ��Է������
AODV_GAYEWAY="paramGateway=1"

insmod ./aodv.o paramDevName=$AODV_DEV $LOCAL_SUBNET $AODV_GATEWAY
 
