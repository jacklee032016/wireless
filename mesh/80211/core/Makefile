#
# $Revision: 1.1.1.1 $
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/30 17:00:40  lizhijie
# MAC MESH
#
# Revision 1.1  2006/08/05 15:18:59  lizhijie
# add into repository
#
#

include $(TOP_DIR)/Makefile.pre

K_CFLAGS+= -I$(TOP_DIR)/80211/modules/dot1x  

CODE_SUBDIRS = node input

KERNEL_LIB:=wifi.o
	
KSRC = 	 \
		if_media.c \
		rc4.c \
		ieee80211.c \
		ieee80211_crypto.c \
		node/ieee80211_node.c \
		node/ieee80211_node_mgmt.c \
		node/ieee80211_node_scan.c \
		ieee80211_output.c \
		ieee80211_proto.c \
		ieee80211_wireless.c \
		ieee80211_linux.c \
		ieee80211_crypto_none.c \
		input/ieee80211_input.c \
		input/ieee80211_input_data.c \
		input/ieee80211_input_mgmt.c \
		input/ieee80211_input_mgmt_utils.c \
		input/ieee80211_input_utils.c 

ifeq ($(MESH),yes)
	KSRC += 	\
		mesh/ieee80211_mesh_mgmt.c \
		mesh/mesh_mac_dev.c \
		mesh/mesh_init.c 
endif
		
KODDOBJS += 

include $(TOP_DIR)/Makefile.post
