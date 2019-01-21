# /bin/sh

echo "Wireless-MAC test Projects"

echo "MAC Drivers"
insmod route_core.o paramLocalSubnet=10.97.1.5/255.255.255.0
insmod wlan.o
insmod ath_hal.o
insmod ath_rate_onoe.o
insmod ath_pci.o

# 配置无线网卡的使用方式和基本信息，在参与AODV路由的节点上必须配置相同
iwconfig ath0 mode Ad-Hoc
iwconfig ath0 essid "mesh-1"
iwconfig ath0 nickname "adhoc_1"

# mode : auto，即速度为自动选择；关闭认证功能
iwpriv ath0 mode  3
iwconfig ath0 key off

# 配置网卡的地址，请明确定义网卡的掩码，否则会影响核心路由表的操作
# 不同节点的网卡地址应该在同一个网段内
ifconfig ath0 10.97.1.5 netmask 255.255.255.0 broadcast 10.97.1.255


# 本节点的报文转发即路由器功能；核心路由表的更新延迟为0,保证AODV路由能够及时更新、反映到核心路由表
echo "1" > /proc/sys/net/ipv4/ip_forward
echo "0" > /proc/sys/net/ipv4/route/min_delay
