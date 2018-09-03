#
# $Author: lizhijie $
# $Id: Makefile,v 1.1.1.1 2006/11/30 17:01:32 lizhijie Exp $
#

include $(TOP_DIR)/Makefile.pre

K_CFLAGS+= -DCONFIG_IEEE80211_SOFTMAC_DEBUG=1 
K_CFLAGS+= -DIEEE80211_DEBUG=1
K_CFLAGS+= -I$(TOP_DIR)/80211/include 

KERNEL_LIB:= ieee80211.o
	
KSRC = 	 \
	ieee80211_module.c \
	ieee80211_tx.c \
	ieee80211_rx.c \
	ieee80211_wx.c \
	ieee80211_geo.c
			
include $(TOP_DIR)/Makefile.post
