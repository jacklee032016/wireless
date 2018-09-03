# $Author: lizhijie $
# Mesh network
#

include $(TOP_DIR)/Makefile.pre

K_CFLAGS+= -I$(TOP_DIR)/mesh/include

CODE_SUBDIRS = . 

KERNEL_LIB:=hal.o
	
KSRC = 	 \
	hw_sysctrl.c \
	hw_init.c 
	

ifeq ($(ARCH),arm)
	HAL_BINARY =xscale-be-elf.hal.o.uu
else
	HAL_BINARY =i386-elf.hal.o.uu
endif

KODDOBJS += hal_raw$(ARCH).o

hal_raw$(ARCH).o:
	uudecode $(HAL_BINARY) -o $@  

include $(TOP_DIR)/Makefile.post
