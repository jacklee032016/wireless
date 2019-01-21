# Makefile for the Assist ISDN MISC drivers
#$Id: Makefile,v 1.1.1.1 2006/11/29 08:55:14 lizhijie Exp $
#

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
	EXTRA_CFLAGS += -DISDN_MEMDEBUG
endif

EXTRA_CFLAGS +=  -I. -I../capi/mcapi -I../capi/capi -I../mcore 

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= 
				
# Multipart objects.

list-multi	:= fax.o i4l.o
fax-objs	:= 	faxl3.o 
i4l-objs	:= 	i4l_mISDN.o 

# Ordering constraints: 

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_I4L_CAPI_LAYER)		+= i4l.o
obj-$(CONFIG_ASSIST_MISDN_L3_FAX)		+= fax.o

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

fax.o: $(fax-objs)
	$(LD) -r -o $@ $(fax-objs)
	mv fax.o ../

i4l.o: $(i4l-objs)
	$(LD) -r -o $@ $(i4l-objs)
	mv i4l.o ../
