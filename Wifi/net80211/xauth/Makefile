#
# $Id: Makefile,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
#

#
# Makefile for the 802.11 WLAN modules.
#
ifeq ($(obj),)
obj=	.
endif

DEPTH=	../..
#
# There are two authenticator mechanisms: an in-kernel implementation
# (wlan_auth+wlan_radius) and an external implementation (wlan_xauth) that
# requires a user process to manage the authentication process.  By default
# the external authenticator is used.  ieee80211_proto.c has a table of module
# names that defines the default module to auto-load for each authentication
# scheme; to get the in-kernel authenticator by default modify it to load
# wlan_auth instead of wlan_xauth or manually load wlan_auth prior to use.
# 
MOD_AUTH	:= wlan_xauth.o

MOD_INSTALL	:= \
		   $(MOD_AUTH)

obj-m		+= $(MOD_INSTALL)

wlan_xauth-objs	:= ieee80211_xauth.o


export-objs	:= ieee80211_xauth.o

list-multi	:= $(MOD_INSTALL)

include $(obj)/${DEPTH}/Makefile.inc

INCS+=	-I${HAL} -I${HAL}/${OS} -I$(obj)/${ATH_HAL} -I$(obj)/${WLAN} \
	-I${obj}/${DEPTH} 

EXTRA_CFLAGS+=$(INCS) ${COPTS}

-include $(TOPDIR)/Rules.make

all:
	$(MAKE) -C $(KERNELPATH) SUBDIRS=$(shell pwd) MODVERDIR=$(shell pwd)/${SYMBOLSDIR}  modules

wlan.o:	$(wlan-objs)
	$(LD) $(LDOPTS) -o wlan.$(KMODSUF) -r $(wlan-objs)
	mv $@ ${SYMBOLSDIR}/

wlan_wep.o:	$(wlan_wep-objs)
	$(LD) $(LDOPTS) -o wlan_wep.$(KMODSUF) -r $(wlan_wep-objs)
	mv $@ ${SYMBOLSDIR}/

wlan_tkip.o:	$(wlan_tkip-objs)
	$(LD) $(LDOPTS) -o wlan_tkip.$(KMODSUF) -r $(wlan_tkip-objs)
	mv $@ ${SYMBOLSDIR}/

wlan_ccmp.o:	$(wlan_ccmp-objs)
	$(LD) $(LDOPTS) -o wlan_ccmp.$(KMODSUF) -r $(wlan_ccmp-objs)
	mv $@ ${SYMBOLSDIR}/

wlan_xauth.o:	$(wlan_xauth-objs)
	$(LD) $(LDOPTS) -o wlan_xauth.$(KMODSUF) -r $(wlan_xauth-objs)
	mv $@ ${SYMBOLSDIR}/

wlan_auth.o:	$(wlan_auth-objs)
	$(LD) $(LDOPTS) -o wlan_auth.$(KMODSUF) -r $(wlan_auth-objs)
	mv $@ ${SYMBOLSDIR}/

wlan_radius.o:	$(wlan_radius-objs)
	$(LD) $(LDOPTS) -o wlan_radius.$(KMODSUF) -r $(wlan_radius-objs)
	mv $@ ${SYMBOLSDIR}/

wlan_acl.o:	$(wlan_acl-objs)
	$(LD) $(LDOPTS) -o wlan_acl.$(KMODSUF) -r $(wlan_acl-objs)
	mv $@ ${SYMBOLSDIR}/

tkip_test.o:	$(tkip_test-objs)
	$(LD) $(LDOPTS) -o tkip_test.$(KMODSUF) -r $(tkip_test-objs)
	mv $@ ${SYMBOLSDIR}/

ccmp_test.o:	$(ccmp_test-objs)
	$(LD) $(LDOPTS) -o ccmp_test.$(KMODSUF) -r $(ccmp_test-objs)
	mv $@ ${SYMBOLSDIR}/

wep_test.o:	$(wep_test-objs)
	$(LD) $(LDOPTS) -o wep_test.$(KMODSUF) -r $(wep_test-objs)
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
