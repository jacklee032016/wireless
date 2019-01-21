# Makefile for the Assist ISDN Layer-3(User INS) drivers
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

list-multi	:= ins.o
ml3udss1-objs	:= 	\
					l3_uins_init.o \
					l3_uins_if.o \
					l3_ins_global_handler.o \
					l3_ins_process_mgr.o \
					l3_ins_down_states.o \
					l3_ins_data_states.o \
					l3_ie.o \
					l3_fsm.o \
					l3_msg.o \
					l3_utils.o

# Ordering constraints: 

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_MISDN_L3UINS)		+= $(list-multi)

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

$(list-multi): $(ml3udss1-objs)
	$(LD) -r -o $@ $(ml3udss1-objs)
	mv $(list-multi) ../../
