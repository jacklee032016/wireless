# $Id: readme.txt,v 1.4 2007/01/25 10:35:43 lizhijie Exp $
		README.txt
                         Li Zhijie, 2006.12.08


2007.01.25
        remove Profilic from kernel and compiled as modules(pl2303.o)

2007.01.18
        add kmod, eg. request_modules
        add crypt API support, and HMAC(must), MD5, AES algorithms

2006.12.08
	����net-->IP Tunnel+GRE over IP֧��(���������豸gre0(ipip.c)��tunl0(ip_gre.c))
	����net-->QoS(ȫ��֧��)�����������豸��teql0(sec_teql.c)��
	net-->net_filter-->GRE+PPTP
	��MPPEֱ�����ӵ�kernel��

2005.01.25
	add conditional compile for MAC address config
		cmdline/parse_kernel_cmdline.c
		kernel/printk.c
		drivers/net/ixp425_eth.c
		
.config  
	Configuration file for IXP422 Linux 2.4.24 kernel

config_12_30_2004
	���ӶԷ������֧��

config_12_29_2004
	Support ext3 file system
	redefine the map of flash
	
config.10.20
	IDE-CF file 

	