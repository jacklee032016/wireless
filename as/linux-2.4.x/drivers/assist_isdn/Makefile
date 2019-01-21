# $Id: Makefile,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
# Makefile for the kernel Assist ISDN subsystem and device drivers.

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= 

# Multipart objects.

list-multi	:= 
isdn-objs	:= 

# Optional parts of multipart objects.

# Ordering constraints: isdn.o first, rest doesn't matter

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN)			+= isdn.o
obj-$(CONFIG_ISDN_PPP_BSDCOMP)		+= isdn_bsdcomp.o

# Object files in subdirectories

mod-subdirs				:= mcore misc capi/capi capi/mcapi dsp/dsp dsp/dtmf \
		protocols/layer1 protocols/layer2 protocols/l3udss1 protocols/x25 \
		devices/assist   
subdir-$(CONFIG_ASSIST_ISDN_CAPI)		+= capi/capi
subdir-$(CONFIG_ASSIST_MISDN_CAPI)		+= capi/mcapi
subdir-$(CONFIG_ASSIST_MISDN_CORE)		+= mcore
subdir-$(CONFIG_ASSIST_MISDN_DSP_DTMF)		+= dsp/dtmf
subdir-$(CONFIG_ASSIST_MISDN_DSP_AUDIO)		+= dsp/dsp
subdir-$(CONFIG_ASSIST_MISDN_L1)		+= protocols/layer1
subdir-$(CONFIG_ASSIST_MISDN_L2)		+= protocols/layer2
subdir-$(CONFIG_ASSIST_MISDN_L3UDSS1)	+= protocols/l3udss1
subdir-$(CONFIG_ASSIST_MISDN_L3UINS)	+= protocols/uins
subdir-$(CONFIG_ASSIST_MISDN_X25)		+= protocols/x25
subdir-$(CONFIG_ASSIST_MISDN_PCI_CARD)	+= devices/assist
subdir-$(CONFIG_ASSIST_I4L_CAPI_LAYER)	+= misc
subdir-$(CONFIG_ASSIST_MISDN_L3_FAX)	+= misc

obj-y += $(addsuffix /vmlinux-obj.o, $(subdir-y))

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.
