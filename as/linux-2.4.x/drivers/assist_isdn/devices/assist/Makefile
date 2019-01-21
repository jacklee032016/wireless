# Makefile for the Assist ISDN HFC-4S PCI card driver
# $Id: Makefile,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
#

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
	EXTRA_CFLAGS += -DISDN_MEMDEBUG
endif

EXTRA_CFLAGS +=  -I. -I../../ -I../../mcore -I../../dsp/dsp 

EXTRA_CFLAGS += -DWITH_HW=1 -DCONFIG_HFCMULTI_PCIMEM=0    

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= 
				
# Multipart objects.

#list-multi	:= assist_hfcm.o
#assist-objs	:= 	assist_hfc_multi.o

list-multi	:= hfc.o
assist-objs	:= 	ashfc_bh.o \
				ashfc_isr.o \
				ashfc_hw_init.o \
				ashfc_hw_utils.o \
				ashfc_signal.o \
				ashfc.o \
				ashfc_init_utils.o \
				ashfc_init.o 

# Ordering constraints: 

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_MISDN_PCI_CARD)		+= $(list-multi)

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

#assist_hfcm.o: $(assist-objs)
$(list-multi): $(assist-objs)
	$(LD) -r -o $@ $(assist-objs)
	mv $(list-multi) ../..
