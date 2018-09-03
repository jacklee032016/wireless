#
# $Id: Makefile,v 1.1.1.1 2006/11/30 16:59:50 lizhijie Exp $
#

KERNEL=yes

include $(TOP_DIR)/Makefile.pre

include $(ATHEROS_DIR)/Makefile_ath.inc

# K_CFLAGS += -DAH_REGOPS_FUNC 
# K_CFLAGS += -include $(ATHEROS_DIR)/include/compat.h -I $(ATHEROS_DIR)/include 
# K_CFLAGS += -I$(ATHEROS_DIR)/hal -I$(ATHEROS_DIR)/hal/linux 
# K_CFLAGS += -I$(ATHEROS_DIR)/hal/public -I$(ATHEROS_DIR)ath_hal -I$(ATHEROS_DIR) 

ifeq ($(ARCH),arm)
K_CFLAGS =  -D__KERNEL__ -I$(KERNELPATH)/include 
K_CFLAGS += -Wall -Wstrict-prototypes -Wno-trigraphs -Os 
K_CFLAGS += -fno-strict-aliasing -fno-common -Uarm -fno-common 
K_CFLAGS += -pipe -mapcs-32 -D__LINUX_ARM_ARCH__=5 -mcpu=xscale -mtune=xscale 
K_CFLAGS += -mshort-load-bytes -msoft-float -Uarm 
K_CFLAGS += -DMODULE -include $(ATHEROS_DIR)/include/compat.h -I$(ATHEROS_DIR)/include 
K_CFLAGS += -I$(ATHEROS_DIR)/hal -I$(ATHEROS_DIR)/hal/linux 
K_CFLAGS += -I$(ATHEROS_DIR)/hal/public -I$(ATHEROS_DIR)/ath_hal -I$(ATHEROS_DIR) 
K_CFLAGS += -DAH_BYTE_ORDER=AH_BIG_ENDIAN -DAH_REGOPS_FUNC 
K_CFLAGS += -march=armv4 -mbig-endian -fno-strict-aliasing -fno-common -mapcs-32 
K_CFLAGS += -mtune=xscale  -mshort-load-bytes -msoft-float -mfp=2 
K_CFLAGS += -nostdinc -iwithprefix include -DKBUILD_BASENAME=ah_osdep  
K_CFLAGS += -DEXPORT_SYMTAB 
endif

KERNEL_LIB:=$(MODULES_DIR)/ath_hal.o

$(MODULES_DIR)/ath_hal-objs	:= \
		$(OBJ_DIR)/ah_osdep.o \
		$(OBJ_DIR)/hal.o

UUDECODE?=uudecode
	
KSRC = 	ah_osdep.c 

ah_osdep.c:opt_ah.h
	cp $(ATHEROS_DIR)/hal/linux/ah_osdep.c ah_osdep.c
	cp $(ATHEROS_DIR)/hal/linux/ah_osdep.h ah_osdep.h

opt_ah.h: $(ATHEROS_DIR)/hal/public/${TARGET}.opt_ah.h
	cp $(ATHEROS_DIR)/hal/public/${TARGET}.opt_ah.h opt_ah.h
	
$(OBJ_DIR)/hal.o:
	${UUDECODE} $(ATHEROS_DIR)/hal/public/${TARGET}.hal.o.uu -o $@

KOBJS += $(OBJ_DIR)/hal.o
	
include $(TOP_DIR)/Makefile.post
