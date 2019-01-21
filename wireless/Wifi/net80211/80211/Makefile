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
#
# Crypto test modules.  These are used only to test/validate the
# software crypto implementations.
#
MOD_CRYPTO_TEST	:= tkip_test.o ccmp_test.o wep_test.o
#
# Modules to be built+installed (leave out the crypto test modules).
#
MOD_INSTALL	:= wlan.o \
		   $(MOD_AUTH)

obj-m		+= $(MOD_INSTALL)

wlan-objs	:= if_media.o rc4.o \
		   ieee80211.o ieee80211_crypto.o \
		   ieee80211_node.o ieee80211_output.o ieee80211_proto.o \
		   ieee80211_wireless.o ieee80211_linux.o \
		   ieee80211_crypto_none.o \
		   input/ieee80211_input.o input/ieee80211_input_data.o \
		   input/ieee80211_input_mgmt.o input/ieee80211_input_mgmt_utils.o \
		   input/ieee80211_input_utils.o 


export-objs	:= if_media.o rc4.o \
		   ieee80211.o ieee80211_crypto.o \
		   ieee80211_node.o ieee80211_output.o ieee80211_proto.o \
		   ieee80211_wireless.o ieee80211_linux.o \
		   ieee80211_crypto_none.o 

		   
list-multi	:= $(MOD_INSTALL)

include $(obj)/${DEPTH}/Makefile.inc

INCS+=	-I${HAL} -I${HAL}/${OS} -I$(obj)/${ATH_HAL} -I$(obj)/${WLAN} \
	-I${obj}/${DEPTH} \
	-I$(obj)/${WLAN_DOT1X}  
	
ifeq ($(MESH),yes)
	wlan-objs += 	\
			mesh/ieee80211_mesh_mgmt.o \
			mesh/mesh_mac_dev.o \
			mesh/mesh_init.o 

#	export-objs +=	mesh/mesh_init.o

	INCS += -I$(WLAN_MESH) 
endif

EXTRA_CFLAGS+=$(INCS) ${COPTS}

-include $(TOPDIR)/Rules.make

all:
	$(MAKE) -C $(KERNELPATH) SUBDIRS=$(shell pwd) MODVERDIR=$(shell pwd)/${SYMBOLSDIR}  modules

wlan.o:	$(wlan-objs)
	$(LD) $(LDOPTS) -o wlan.$(KMODSUF) -r $(wlan-objs)
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

	-rm -f input/*~ input/*.o input/*.ko input/*.mod.c
	-rm -f input/.version input/.*.o.flags input/.*.o.d input/.*.o.cmd input/.*.ko.cmd
	-rm -rf input/.tmp_versions

	-rm -f mesh/*~ mesh/*.o mesh/*.ko mesh/*.mod.c
	-rm -f mesh/.version mesh/.*.o.flags mesh/.*.o.d mesh/.*.o.cmd mesh/.*.ko.cmd
	-rm -rf mesh/.tmp_versions
