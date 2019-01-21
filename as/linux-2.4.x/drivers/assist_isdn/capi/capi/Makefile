# $Id: Makefile,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
# Makefile for the AVM ISDN device drivers and CAPI subsystem.

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= kcapi.o capiutil.o capifs.o

#b1dma.o b1pcmcia.o b1.o 

# Multipart objects.

list-multi	:= kernelcapi.o kcapiutil.o 
kernelcapi-objs	:= kcapi.o
kcapiutil-objs	:= capiutil.o

# Ordering constraints: kernelcapi.o first

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_ISDN_CAPI)				+= kernelcapi.o kcapiutil.o 
obj-$(CONFIG_ASSIST_ISDN_CAPI_CAPI20)		+= capi.o 
obj-$(CONFIG_ASSIST_ISDN_CAPI_CAPIDRV)		+= capidrv.o
obj-$(CONFIG_ASSIST_ISDN_CAPI_CAPIFS)		+= capifs.o


# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

kernelcapi.o: $(kernelcapi-objs)
	$(LD) -r -o $@ $(kernelcapi-objs)
	mv $@ ../..

kcapiutil.o : $(kcapiutil-objs)
	$(LD) -r -o $@ $(kcapiutil-objs)
	mv $@ ../..
