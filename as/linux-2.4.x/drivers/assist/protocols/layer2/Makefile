# Makefile for the Assist ISDN Layer-2 drivers
# $Id: Makefile,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
#

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
	EXTRA_CFLAGS += -DISDN_MEMDEBUG
endif

EXTRA_CFLAGS +=  -I. -I../.. -I../../include 

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= 
				
# Multipart objects.

list-multi	:= kl2.o
mlayer2-objs	:= 	layer2.o tei.o 

# Ordering constraints: 

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_MISDN_L2)		+= $(list-multi)

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

$(list-multi): $(mlayer2-objs)
	$(LD) -r -o $@ $(mlayer2-objs)
	mv $(list-multi) ../../

