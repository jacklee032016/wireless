#
# $Id: Makefile,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
#

#
# Makefile for the Atheros WLAN driver.
#
ifeq ($(obj),)
obj=	.
endif

DEPTH=	..

ifeq ($(strip ${BUS}),AHB)
BUSNAME=ahb
COPTS+= -DATH_AHB
else
BUSNAME=pci
COPTS+= -DATH_PCI
endif

COPTS+= -DAR_DEBUG 

obj-m		+= ath_${BUSNAME}.o
ath_${BUSNAME}-objs	:= \
		if/if_ath_net.o \
		if/if_ath_crypto.o \
		if/if_ath_beacon.o \
		if/if_ath_rx.o \
		if/if_ath_tx.o \
		if/if_ath_ioctrl.o \
		if/if_ath_sysctrl.o \
		if/if_ath_init.o \
		if/if_ath_isr.o \
		if/if_ath.o 

ifeq ($(VMAC),yes)
ath_${BUSNAME}-objs	+= \
		vmac/if_ath_vmac_mac.o \
		vmac/if_ath_vmac_phy.o \
		vmac/if_ath_vmac_rx.o \
		vmac/if_ath_vmac.o 
endif

		
ath_${BUSNAME}-objs	+= if_ath_${BUSNAME}.o radar.o

export-objs	:= if_ath_${BUSNAME}.o radar.o
list-multi	:= ath_${BUSNAME}.o

include $(obj)/${DEPTH}/Makefile.inc

INCS+=	-I${HAL} -I${HAL}/${OS} -I${obj}/${ATH_HAL} \
	-I${obj}/${WLAN} -I${obj}/${DEPTH} -I${ATH} 

EXTRA_CFLAGS+= ${INCS} ${COPTS}

-include $(TOPDIR)/Rules.make

all:
	$(MAKE) -C $(KERNELPATH) SUBDIRS=$(shell pwd) MODVERDIR=$(shell pwd)/${SYMBOLSDIR} modules

install:
	test -d ${DESTDIR}/${MODULEPATH} || mkdir -p ${DESTDIR}/${MODULEPATH}
	cp ath_${BUSNAME}.${KMODSUF} ${DESTDIR}/${MODULEPATH}

clean:
	-rm -f *~ *.o *.ko *.mod.c
	-rm -f .version .*.o.flags .*.o.d .*.o.cmd .*.ko.cmd
	-rm -rf .tmp_versions

	-rm -f if/*~ if/*.o if/*.ko if/*.mod.c
	-rm -f if/.version if/.*.o.flags if/.*.o.d if/.*.o.cmd if/.*.ko.cmd
	-rm -rf if/.tmp_versions

	-rm -f vmac/*~ vmac/*.o vmac/*.ko vmac/*.mod.c
	-rm -f vmac/.version vmac/.*.o.flags vmac/.*.o.d vmac/.*.o.cmd vmac/.*.ko.cmd
	-rm -rf vmac/.tmp_versions

ath_${BUSNAME}.o: ${ath_${BUSNAME}-objs}
	$(LD) $(LDOPTS) -o ath_${BUSNAME}.$(KMODSUF) -r $(ath_${BUSNAME}-objs)
	mv $@ ${SYMBOLSDIR}/
