#
# $Id: Makefile,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
#

#
# Makefile for the HAL-based Atheros driver.
#
DEPTH=	.


include Makefile.inc

# NB: the order is important here
DIRS=	\
	${WLAN} \
	${ATH_HAL} ${ATH_RATE} \
	${ATH}

#	${WLAN_CRYPTO} ${WLAN_ACL} ${WLAN_XAUTH} \

# ${WLAN_DOT1X} ${WLAN_RADIUS} 

all: configcheck
	mkdir -p ${SYMBOLSDIR}
	for i in ${DIRS}; do \
		(cd $$i; make) || exit 1; \
	done

install:
	for i in ${DIRS}; do \
		(cd $$i; make install) || exit 1; \
	done
	@if [ -z $(DESTDIR) ]; then \
	    /sbin/depmod -ae ; \
	elif [ -f $(SYSTEMMAP) ]; then \
	    /sbin/depmod -ae -b $(DESTDIR) -F $(SYSTEMMAP) $(KERNELRELEASE) ; \
	else \
	    echo "Don't forget to run \"depmod -ae\" on the target system."; \
	fi

FILES=	ath_hal COPYRIGHT ath include Makefile Makefile.inc \
	README release.h share net80211 ath_rate
HAL_TAG=ATH_HAL_20030802

release:
	DATE=`date +'%Y%m%d'`; TAG="MADWIFI_$${DATE}"; DIR="madwifi-$${DATE}"; \
	cvs tag -F $${TAG} ${FILES}; \
	rm -rf $${DIR}; mkdir $${DIR}; \
	cvs export -d $${DIR} -r $${TAG} linux; \
	(cd $${DIR}; cvs export -r ${HAL_TAG} hal); \
	tar zcf $${DIR}.tgz --exclude=CVS --exclude=hal/freebsd $${DIR}; \
	rm -rf $${DIR}

clean:
	for i in ${DIRS}; do \
		(cd $$i; make clean); \
	done
	rm -rf ${SYMBOLSDIR}

info:
	@echo "The following settings will be used for compilation:"
	@echo "TARGET       : $(TARGET)"
	@echo "OS           : $(OS)"
	@echo "BUS          : $(BUS)"
	@if [ ! -z "$(TOOLPATH)" ]; then \
	    @echo "TOOLPATH     : $(TOOLPATH)"; \
	fi	
	@echo "KERNELRELEASE: $(KERNELRELEASE)"
	@echo "KERNELPATH   : $(KERNELPATH)"
	@echo "KERNELCONF   : $(KERNELCONF)"
	@echo "MODULEPATH   : $(MODULEPATH)"
	@echo "KMODSUF      : $(KMODSUF)"
	@if [ ! -z "$(DESTDIR)" ]; then \
	    echo "DESTDIR      : $(DESTDIR)"; \
	    echo "SYSTEMMAP    : $(SYSTEMMAP)"; \
	fi
	@echo "WIRELESSEXT  : $(WIRELESSEXT)"
	@echo "VMAC         : $(VMAC)"
	@echo "VMAC ROOT    : $(VMAC_ROOT)"

dotconfig:
	@#
	@# check if kernel configuration file is available
	@#	
	@if [ ! -f $(KERNELCONF) ]; then \
	    echo "You need to configure your kernel."; \
	    exit 1; \
	fi

# new targets should be inserted ABOVE this line in order to avoid
# problems with the included kernel configuration file below.
include $(KERNELCONF)

configcheck: dotconfig
	@echo -n "Checking if all requirements are met... "
	
	@# check version of kernel and wireless.h
	@echo $(KERNELRELEASE) | grep -q -i '^[2-9]\.[4-9]\.' || { \
	    echo "FAILED"; \
	    echo "Only kernel versions 2.4.x and above are supported."; \
	    echo "You have $(KERNELRELEASE)."; \
	    exit 1; \
	}
	
	@if [ "$(WIRELESSEXT)" -lt 14 ]; then \
	    echo "FAILED"; \
	    echo "Wireless extensions v14 or better is needed."; \
	    echo "You have v$(WIRELESSEXT)."; \
	    exit 1; \
	fi
	
	@# check kernel configuration
	@if [ -z "$(CONFIG_SYSCTL)" ]; then \
	    echo "FAILED"; \
	    echo "Please enable sysctl support."; \
	    exit 1; \
	fi
	@if [ -z "$(CONFIG_NET_RADIO)" ]; then \
	    echo "FAILED"; \
	    echo "Please enable wireless extensions."; \
	    exit 1; \
	fi
	@if [ -z "$(CONFIG_CRYPTO)" ]; then \
	    echo "FAILED"; \
	    echo "Please enable crypto API."; \
	    exit 1; \
	fi
	
	@# some other sanity checks
	@if [ ! -d $(ATH_RATE) ]; then \
	    echo "FAILED"; \
	    echo "Selected rate control $(ATH_RATE) not available."; \
	    exit 1; \
	fi
		
	@echo "ok."
