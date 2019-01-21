# Makefile for the Assist ISDN HFC-4S PCI card driver
# $Id: Makefile,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
#

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
	EXTRA_CFLAGS += -DISDN_MEMDEBUG
endif

EXTRA_CFLAGS +=  -I. -I../../ -I../../mcore -I../../dsp/dsp 

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= 
				
# Multipart objects.

list-multi	:= hfc_m.o
assist-objs	:= 	hfc_multi.o

# Ordering constraints: 

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_MISDN_HFCMULTI)		+= hfc_m.o

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

hfc_m.o: $(assist-objs)
	$(LD) -r -o $@ $(assist-objs)
	mv $@ ../..
