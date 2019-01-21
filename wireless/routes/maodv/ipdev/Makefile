#
# $Id: Makefile,v 1.1.1.1 2006/11/30 17:00:19 lizhijie Exp $
#
KERNEL=yes

# These are the options you wish to compile in
# Your choices are:
#  -DMESSAGES       = Support for printing kernel messages to the console
#  -DTRACE          = Support for trace messages for debuggin purposes
#  -DAODV_GATEWAY   = Support for gatewaying to outside networks
#  -DAODV_SIGNAL    = Support for monitoring the signal strength of neighbors
#  -DAODV_MULTICAST = Support for multicasting

K_CFLAGS +=   -DMESSAGES -DAODV_GATEWAY -DAODV_SIGNAL 

include $(TOP_DIR)/Makefile.pre

CODE_SUBDIRS = . 

KERNEL_LIB:=$(MODULES_DIR)/ip_route.o

KSRC = 	\
		ip_packet_queue.c \
		ip_netfilter.c \
		ip_dev.c \
		ip_dev_init.c
		
		
include $(TOP_DIR)/Makefile.post

K_CFLAGS += -Wall -O3 -DMODULE -D__KERNEL__ -DLINUX  -DMESSAGES -DAODV_GATEWAY -DAODV_SIGNAL \
	-I$(KINCLUDES)/include -I../include  
	                               