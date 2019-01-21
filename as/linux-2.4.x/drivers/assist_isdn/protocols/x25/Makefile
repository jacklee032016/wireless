# Makefile for the Assist ISDN X25 drivers
# $Id: Makefile,v 1.1.1.1 2006/11/29 08:55:14 lizhijie Exp $
#

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
	EXTRA_CFLAGS += -DISDN_MEMDEBUG
endif

EXTRA_CFLAGS +=  -I. -I../../capi/mcapi -I../../capi/capi -I../../mcore 

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= 
				
# Multipart objects.

list-multi	:= x25.o
mx25-objs	:= 	x25_dte.o x25_l3.o 

# Ordering constraints: 

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_MISDN_X25)		+= $(list-multi)

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

$(list-multi): $(mx25-objs)
	$(LD) -r -o $@ $(mx25-objs)
	mv $(list-multi) ../../

