# Makefile for the Assist ISDN Layer-3(UDSS1) drivers
# $Id: Makefile,v 1.1.1.1 2006/11/29 08:55:14 lizhijie Exp $
#

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
	EXTRA_CFLAGS += -DISDN_MEMDEBUG
endif

EXTRA_CFLAGS +=  -I. -I../../mcore 

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= 
				
# Multipart objects.

list-multi	:= kl3udss1.o
ml3udss1-objs	:= 	layer3.o l3_udss1.o 

# Ordering constraints: 

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_MISDN_L3UDSS1)		+= $(list-multi)

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

$(list-multi): $(ml3udss1-objs)
	$(LD) -r -o $@ $(ml3udss1-objs)
	mv $(list-multi) ../../
