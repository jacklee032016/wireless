# Makefile for the Assist ISDN DSP drivers
#$Id: Makefile,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
#

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
	EXTRA_CFLAGS += -DISDN_MEMDEBUG
endif

EXTRA_CFLAGS +=  -I. -I../capi -I../../include  

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:=  
				
# Multipart objects.

list-multi	:= k2capi.o
mcapi-objs	:= 	capi.o \
				capi_enc.o \
				contr.o \
				listen.o \
				appl.o \
				plci.o \
				app_plci.o \
				ncci.o \
				asn1.o \
				asn1_address.o \
				asn1_aoc.o \
				asn1_basic_service.o \
				asn1_comp.o \
				asn1_diversion.o \
				asn1_enc.o \
				asn1_generic.o \
				supp_serv.o 

# Ordering constraints: 

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_MISDN_CAPI)		+= $(list-multi)

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

$(list-multi): $(mcapi-objs)
	$(LD) -r -o $@ $(mcapi-objs)
	mv $(list-multi) ../..

