# $Id: Makefile,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
# Makefile for the Assist ISDN Core drivers

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
	EXTRA_CFLAGS += -DISDN_MEMDEBUG
endif

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= core.o helper.o debug.o fsm.o dchannel.o bchannel.o l3helper.o memdbg.o 
				
# Multipart objects.

list-multi	:= isdn.o
mcore-objs	:= core.o \
				stack.o \
				udevice.o \
				helper.o \
				fsm.o \
				dchannel.o \
				bchannel.o \
				l3helper.o 
				
# remove this file from objects. lizhijie, 2005.12.15 				
#				debug.o \

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
mcore-objs += memdbg.o
endif

# Ordering constraints: kernelcapi.o first

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_MISDN_CORE)+= $(list-multi)

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

$(list-multi): $(mcore-objs)
	$(LD) -r -o $@ $(mcore-objs)
	mv $(list-multi)  ../
