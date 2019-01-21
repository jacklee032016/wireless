#
# $Id: Makefile,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
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

KERNEL_LIB:=$(MODULES_DIR)/aodv.o

KSRC = 	\
		aodv_dev.c \
		aodv_task.c \
		aodv_route.c \
		aodv_neigh.c \
		aodv_timer_queue.c \
		aodv_pdu_reply.c \
		aodv_pdu_request.c \
		packet_queue.c \
		aodv_netfilter.c \
		aodv_procs.c \
		aodv_utils.c \
		aodv_lib.c \
		aodv_init.c 
		
include $(TOP_DIR)/Makefile.post

K_CFLAGS += -Wall -O3 -DMODULE -D__KERNEL__ -DLINUX  -DMESSAGES -DAODV_GATEWAY -DAODV_SIGNAL -I$(KINCLUDES)/include  
	                               