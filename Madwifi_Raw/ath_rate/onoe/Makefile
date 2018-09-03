#
# $Id: Makefile,v 1.1.1.1 2006/11/30 16:59:50 lizhijie Exp $
#
KERNEL=yes

include $(TOP_DIR)/Makefile.pre

include $(ATHEROS_DIR)/Makefile_ath.inc

K_CFLAGS += -I$(ATHEROS_DIR)/ath 

KERNEL_LIB:=$(MODULES_DIR)/ath_rate_onoe.o

KSRC = 	onoe.c 

include $(TOP_DIR)/Makefile.post                      
	                               