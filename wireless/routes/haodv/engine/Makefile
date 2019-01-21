#
# $Id: Makefile,v 1.1.1.1 2006/11/30 16:58:41 lizhijie Exp $
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

CODE_SUBDIRS = route iw utils aodv protocol 

KERNEL_LIB:=$(MODULES_DIR)/route_core.o

KSRC = 	\
		protocol/route_dev_rx.c \
		protocol/route_dev_tx.c \
		protocol/route_node_tx.c \
		route/route_route.c \
		route/route_neigh.c \
		route/route_timers.c \
		route/route_procs.c \
		route/route_engine.c \
		route/route_init.c \
		route/route_task.c \
		route/mesh_sysctrl.c \
		iw/route_iw_utils.c \
		utils/route_lib.c \
		utils/kroute.c 
		
		
include $(TOP_DIR)/Makefile.post

K_CFLAGS += -Wall -O3 -DMODULE -D__KERNEL__ -DLINUX  -DMESSAGES -DAODV_GATEWAY -DAODV_SIGNAL \
	-I$(KINCLUDES)/include -I../include -I./route  
	                               