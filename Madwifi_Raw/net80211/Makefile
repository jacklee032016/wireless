#
# $Id: Makefile,v 1.1.1.1 2006/11/30 16:59:51 lizhijie Exp $
#
KERNEL=yes

include $(TOP_DIR)/Makefile.pre

include $(ATHEROS_DIR)/Makefile_ath.inc

KERNEL_LIB:=$(MODULES_DIR)/wlan.o

KSRC = 	if_media.c \
		rc4.c \
		ieee80211.c \
		ieee80211_crypto.c \
		ieee80211_input.c \
		ieee80211_node.c \
		ieee80211_output.c \
		ieee80211_proto.c \
		ieee80211_wireless.c \
		ieee80211_linux.c \
		ieee80211_crypto_none.c 

include $(TOP_DIR)/Makefile.post                      
	                               