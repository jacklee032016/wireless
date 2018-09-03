#
# $Id: Makefile,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
#

#
# Makefile for the 802.11 WLAN modules.
#
ifeq ($(obj),)
obj=	.
endif

DEPTH=	../..

MOD_AUTH	:= wlan_auth.o

MOD_INSTALL	:= \
		   $(MOD_AUTH)

obj-m		+= $(MOD_INSTALL)

wlan_auth-objs	:= ieee80211_dot1x.o

export-objs	:= ieee80211_dot1x.o 

list-multi	:= $(MOD_INSTALL)

include $(obj)/${DEPTH}/Makefile.inc

INCS+=	-I${HAL} -I${HAL}/${OS} -I$(obj)/${ATH_HAL} -I$(obj)/${WLAN} \
	-I${obj}/${DEPTH} \
	-I$(obj)/${WLAN_RADIUS} -I.

EXTRA_CFLAGS+=$(INCS) ${COPTS}

-include $(TOPDIR)/Rules.make

all:
	$(MAKE) -C $(KERNELPATH) SUBDIRS=$(shell pwd) MODVERDIR=$(shell pwd)/${SYMBOLSDIR}  modules

wlan_auth.o:	$(wlan_auth-objs)
	$(LD) $(LDOPTS) -o wlan_auth.$(KMODSUF) -r $(wlan_auth-objs)
	mv $@ ${SYMBOLSDIR}/


install:
	test -d ${DESTDIR}/${MODULEPATH} || mkdir -p ${DESTDIR}/${MODULEPATH}
	for i in ${MOD_INSTALL}; do \
		f=`basename $$i .o`; \
		${TOOLPREFIX}strip -S $$f.$(KMODSUF); \
		cp $$f.${KMODSUF} ${DESTDIR}/${MODULEPATH}; \
	done

clean:
	-rm -f *~ *.o *.ko *.mod.c
	-rm -f .version .*.o.flags .*.o.d .*.o.cmd .*.ko.cmd
	-rm -rf .tmp_versions
