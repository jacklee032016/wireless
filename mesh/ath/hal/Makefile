# $Author: lizhijie $
# Mesh network
#

include $(TOP_DIR)/Makefile.pre

KERNEL_LIB:=hal.o
	
KSRC = 	 \
	ah_osdep.c 

KODDOBJS += hal_raw.o

hal_raw.o:
	uudecode xscale-be-elf.hal.o.uu -o $@  

include $(TOP_DIR)/Makefile.post


