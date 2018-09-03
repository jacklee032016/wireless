#
# $Id: Makefile_ath.inc,v 1.1.1.1 2006/11/30 16:59:50 lizhijie Exp $
#

K_CFLAGS +=  -Wall -O3 -DMODULE  -D__KERNEL__ -DLINUX -DEXPORT_SYMTAB -I$(KINCLUDES)/include  
K_CFLAGS +=  -DMODVERSIONS  -include $(KINCLUDES)/include/linux/modversions.h 

K_CFLAGS += -DAH_REGOPS_FUNC 
K_CFLAGS += -include $(ATHEROS_DIR)/include/compat.h 
K_CFLAGS += -I$(ATHEROS_DIR)/include  
K_CFLAGS += -I$(ATHEROS_DIR)/hal -I$(ATHEROS_DIR)/hal/linux 
K_CFLAGS += -I$(ATHEROS_DIR)/hal/public -I$(ATHEROS_DIR)/ath_hal 
K_CFLAGS += -I$(ATHEROS_DIR)/net80211 -I$(ATHEROS_DIR) 
