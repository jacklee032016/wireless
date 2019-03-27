# Makefile for the Assist ISDN DSP drivers
# $Id: Makefile,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
#

ifdef CONFIG_ASSIST_MISDN_MEMDEBUG
	EXTRA_CFLAGS += -DISDN_MEMDEBUG
endif

EXTRA_CFLAGS +=  -I. -I../../include 

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= dsp_core.o 
				
# Multipart objects.

list-multi	:= kdsp.o
mdsp-objs	:= 	dsp_core.o \
				dsp_cmx.o \
				dsp_tones.o \
				dsp_dtmf.o \
				dsp_audio.o \
				dsp_blowfish.o

# Ordering constraints: 

# Each configuration option enables a list of files.

obj-$(CONFIG_ASSIST_MISDN_DSP_AUDIO)		+= $(list-multi)

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

$(list-multi): $(mdsp-objs)
	$(LD) -r -o $@ $(mdsp-objs)
	mv $(list-multi) ../..
