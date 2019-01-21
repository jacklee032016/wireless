#
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/30 16:59:12  lizhijie
# MAC HostAP
#
# Revision 1.1.1.1  2006/04/01 11:29:57  lizhijie
# hostap-all
#
# Revision 1.2  2005/04/04 06:25:48  lizhijie
# no message
#
# Revision 1.1.1.1  2005/03/30 10:39:42  lizhijie
# new WLAN for AP and driver
#
# $Revision: 1.1.1.1 $
# $Id: Makefile,v 1.1.1.1 2006/11/30 16:59:12 lizhijie Exp $
#

# TOP_DIR:=$(shell pwd)

# export TOP_DIR

include $(TOP_DIR)/Makefile.pre

DIR_WPA_SUPPLICANT=.
DIR_HOSTAP=.

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

# define HOSTAPD_DUMP_STATE to include SIGUSR1 handler for dumping state to
# a file (undefine it, if you want to save in binary size)
CFLAGS += -DHOSTAPD_DUMP_STATE

# Include directories for CVS version
CFLAGS += -I. -I$(DIR_HOSTAP) -I../utils -I$(DIR_WPA_SUPPLICANT)

# Uncomment following line and set the path to your kernel tree include
# directory if your C library does not include all header files.
# CFLAGS += -DUSE_KERNEL_HEADERS -I/usr/src/linux/include


APD_BIN_DIR=$(BIN_DIR)/sbin

SRC= 	hostapd.c \
	eloop.c \
	ieee802_1x.c \
	eapol_sm.c \
	radius.c \
	md5.c \
	rc4.c \
	common.c \
	ieee802_11.c \
	config.c \
	ieee802_11_auth.c \
	accounting.c \
	sta_info.c \
	sha1.c \
	radius_client.c \
	wpa.c \
	aes_wrap.c \
	ctrl_iface.c \
	driver_conf.c


-include .config

ifdef CONFIG_IAPP
CFLAGS += -DCONFIG_IAPP
SRC += iapp.c
endif

ifdef CONFIG_RSN_PREAUTH
CFLAGS += -DCONFIG_RSN_PREAUTH
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_DRIVER_HOSTAP
CFLAGS += -DCONFIG_DRIVER_HOSTAP
SRC += driver.c
endif

ifdef CONFIG_DRIVER_WIRED
CFLAGS += -DCONFIG_DRIVER_WIRED
SRC += driver_wired.c
endif

ifdef CONFIG_DRIVER_MADWIFI
CFLAGS += -DCONFIG_DRIVER_MADWIFI
SRC += driver_madwifi.c
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_DRIVER_PRISM54
CFLAGS += -DCONFIG_DRIVER_PRISM54
SRC += driver_prism54.c
endif

ifdef CONFIG_DRIVER_BSD
CFLAGS += -DCONFIG_DRIVER_BSD
SRC += driver_bsd.c
CONFIG_L2_PACKET=y
CONFIG_DNET_PCAP=y
endif

ifdef CONFIG_DRIVER_TEST
CFLAGS += -DCONFIG_DRIVER_TEST
SRC += driver_test.c
endif

ifdef CONFIG_L2_PACKET
SRC += $(DIR_WPA_SUPPLICANT)/l2_packet.c
endif

ifdef CONFIG_DNET_PCAP
CFLAGS += -DUSE_DNET_PCAP
SRC +=-ldnet -lpcap
endif

ifdef CONFIG_EAP_MD5
CFLAGS += -DEAP_MD5
SRC += eap_md5.c
endif

ifdef CONFIG_EAP_TLS
CFLAGS += -DEAP_TLS
SRC += eap_tls.c
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_PEAP
CFLAGS += -DEAP_PEAP
SRC += eap_peap.c
TLS_FUNCS=y
CONFIG_EAP_TLV=y
CONFIG_EAP_MSCHAPV2=y
endif

ifdef CONFIG_EAP_TTLS
CFLAGS += -DEAP_TTLS
SRC += eap_ttls.c
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_MSCHAPV2
CFLAGS += -DEAP_MSCHAPv2
SRC += eap_mschapv2.c
MS_FUNCS=y
endif

ifdef CONFIG_EAP_GTC
CFLAGS += -DEAP_GTC
SRC += eap_gtc.c
endif

ifdef CONFIG_EAP_SIM
CFLAGS += -DEAP_SIM
OBJS += eap_sim.c $(DIR_WPA_SUPPLICANT)/eap_sim_common.c
# Example EAP-SIM interface for GSM authentication. This can be replaced with
# another file implementating the interface specified in eap_sim_db.h.
SRC += eap_sim_db.c
endif

ifdef CONFIG_EAP_TLV
CFLAGS += -DEAP_TLV
SRC += eap_tlv.c
endif

ifdef CONFIG_EAP
CFLAGS += -DEAP_AUTHENTICATOR
SRC += eap.c eap_identity.c
endif

ifdef TLS_FUNCS
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, and EAP_TTLS)
CFLAGS += -DEAP_TLS_FUNCS
SRC += eap_tls_common.c $(DIR_WPA_SUPPLICANT)/tls_openssl.c
LIBS += -lssl -lcrypto
LIBS_p += -lcrypto
else
SRC += $(DIR_WPA_SUPPLICANT)/tls_none.c
endif

ifdef CONFIG_PKCS12
CFLAGS += -DPKCS12_FUNCS
endif

ifdef MS_FUNCS
ifndef TLS_FUNCS
LIBS += -lcrypto
endif
SRC += $(DIR_WPA_SUPPLICANT)/ms_funcs.c $(DIR_WPA_SUPPLICANT)/crypto.c
endif

ifdef CONFIG_RADIUS_SERVER
CFLAGS += -DRADIUS_SERVER
SRC += radius_server.c
endif


#releasedir:
#	mkdir -p release/bin.$(TARGET_NAME)
#	mkdir -p release/lib.$(TARGET_NAME)



#clean:
#	- for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) clean ) ; done
#	- find . -name '*.d' -print -exec /bin/rm -f {} \;

LOCAL_ALL := verify_config $(APD_BIN_DIR)/hostapd $(APD_BIN_DIR)/hostapd_cli

include $(TOP_DIR)/Makefile.post

verify_config:
	@if [ ! -r .config ]; then \
		echo 'Building hostapd requires a configuration file'; \
		echo '(.config). See README for more instructions. You can'; \
		echo 'run "cp defconfig .config" to create an example'; \
		echo 'configuration.'; \
		exit 1; \
	fi

driver_conf.c: Makefile .config
	rm -f driver_conf.c
	echo '/* THIS FILE AUTOMATICALLY GENERATED, DO NOT EDIT! */' \
							> driver_conf.c
	echo '#include <stdlib.h>'			>> driver_conf.c
	echo '#include <stdio.h>'			>> driver_conf.c
	echo '#include <sys/types.h>'			>> driver_conf.c
	echo '#include <netinet/in.h>'			>> driver_conf.c
	echo '#include "hostapd.h"'			>> driver_conf.c
	echo '#include "driver.h"'			>> driver_conf.c
ifdef CONFIG_DRIVER_HOSTAP
	echo "void hostap_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_WIRED
	echo "void wired_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_MADWIFI
	echo "void madwifi_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_PRISM54
	echo "void prism54_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_BSD
	echo "void bsd_driver_register(void);"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_TEST
	echo "void test_driver_register(void);"		>> driver_conf.c
endif
	echo 'void register_drivers(void) {'		>> driver_conf.c
ifdef CONFIG_DRIVER_HOSTAP
	echo "hostap_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_WIRED
	echo "wired_driver_register();"			>> driver_conf.c
endif
ifdef CONFIG_DRIVER_MADWIFI
	echo "madwifi_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_PRISM54
	echo "prism54_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_BSD
	echo "bsd_driver_register();"			>> driver_conf.c
endif
ifdef CONFIG_DRIVER_TEST
	echo "test_driver_register();"			>> driver_conf.c
endif
	echo '}'					>> driver_conf.c




$(APD_BIN_DIR)/hostapd: $(OBJS)
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LIBS)
	
	
$(APD_BIN_DIR)/hostapd_cli: $(OBJ_DIR)/hostapd_cli.o $(OBJ_DIR)/hostapd_ctrl.o
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LDFLAGS) 

	
cleanall:
	find . -name 'obj.*' -print -exec /bin/rm -rf {} \;
	find . -name 'bin.*' -print -exec /bin/rm -rf {} \;
	find . -name '*.d' -print -exec /bin/rm -f {} \;
	find . -name '.link_host' -print -exec /bin/rm -f {} \;
	for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) cleanall ) ; done
	rm -rf release/bin.*/*
	rm -rf release/lib.*/*
	rm -rf release/include/*

veryclean: clean
	- find . -name 'obj.*' -print -exec /bin/rm -rf {} \;
	- find . -name 'bin.*' -print -exec /bin/rm -rf {} \;
	- find . -name '*.o' -print -exec /bin/rm -f {} \;
	- find . -name '*.a' -print -exec /bin/rm -f {} \;
	- find . -name 'core' -print -exec /bin/rm -f {} \;
	- find . -name '.link_host' -print -exec /bin/rm -f {} \;

install:
	- find releases -name \CVS -exec rm -r -f {} \;
	tar cvf $(ASSIST_RELEASES_NAME) -C releases etc usr var  
	mv $(ASSIST_RELEASES_NAME)  $(ASSIST_IXP_RELEASES_HOME)

-include $(OBJS:%.o=%.d)
	
