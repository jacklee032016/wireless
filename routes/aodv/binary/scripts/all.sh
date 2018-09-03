# /bin/sh

echo "Wireless-MAC test Projects"

echo "MAC Drivers"
insmod wlan.o
insmod ath_hal.o
insmod ath_rate_onoe.o
insmod ath_pci.o

# 配置无线网卡的使用方式和基本信息，在参与AODV路由的节点上必须配置相同
iwconfig ath0 mode Ad-Hoc
iwconfig ath0 essid "mesh-test"
iwconfig ath0 nickname "adhoc_mesh"

# mode : auto，即速度为自动选择；关闭认证功能
iwpriv ath0 mode  3
iwconfig ath0 key off

# 配置网卡的地址，请明确定义网卡的掩码，否则会影响核心路由表的操作
# 不同节点的网卡地址应该在同一个网段内
ifconfig ath0 10.97.1.1 netmask 255.255.255.0 broadcast 10.97.1.255


# 本节点的报文转发即路由器功能；核心路由表的更新延迟为0,保证AODV路由能够及时更新、反映到核心路由表
echo "1" > /proc/sys/net/ipv4/ip_forward
echo "0" > /proc/sys/net/ipv4/route/min_delay

echo "AODV Route Protocol"
# 参数一
# AODV协议使用的网络设备名称，也可以是有线网卡；需要支持多个网络设备
AODV_DEV="ath0"

# 参数二
# 指定网卡的IP地址及其掩码，不同的节点上必须使用不同的IP地址
# 这个地址可以不是AODV协议使用的网卡的地址，但是它必须是唯一的
# 程序使用这个地址创建一个本地使用的AODV路由项，这个路由项用于维护本节点的Destination Sequence
# 并且将这个地址作为本节点发出RREQ的ID，所以它必须唯一
LOCAL_SUBNET="paramLocalSubnet=10.97.1.1/255.255.255.0"

# 参数三
# 可选配置的参数，配置本节点是否支持默认网关的功能
# 在包含有线连接的Mesh Portal节点上需要配置此功能。0:关闭此功能
# 一般都打开此功能，以方便调试
AODV_GAYEWAY="paramGateway=1"

insmod ./aodv.o paramDevName=$AODV_DEV $LOCAL_SUBNET $AODV_GATEWAY
 
