#
# $Id: Makefile,v 1.1.1.1 2006/11/30 16:59:50 lizhijie Exp $
#

ATHEROS_DIR:=$(shell pwd)
TOP_DIR:=$(shell pwd)
export TOP_DIR

ARCH=arm

ifeq ($(ARCH),arm)
	TARGET=xscale-be-elf
	KERNELPATH=/works/isdn/kernel/linux-2.4.x
else
	TARGET=i386-elf
	KERNELPATH=/usr/src/linux-2.4
endif

export ATHEROS_DIR TARGET ARCH KERNELPATH 


ifeq ($(ATH_RATE),)
ATH_RATE=	ath_rate/onoe
endif

ATH_HAL= ath_hal

WLAN=	net80211

ATH=	ath


SUBDIRS += $(ATH_HAL)
SUBDIRS += $(ATH_RATE)
SUBDIRS += $(WLAN)
SUBDIRS += $(ATH)

SUBDIRS += tools 

include $(TOP_DIR)/Makefile.pre

all: 
	for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) $@ ) ; done

clean: 
	for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) $@ ) ; done

install:
#	$(MKDIR) ./releases/sbin
	$(COPY) ./atheros/script/*.sh $(BIN_DIR)/sbin 
	tar cvzf $(ASSIST_RELEASES_NAME) -C $(BIN_DIR) sbin lib 
#	$(RM) releases/
