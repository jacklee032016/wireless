# $Id: Makefile,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
#

#
# Makefile for the Atheros HAL Module.
#
# NB: we can't support MODVERSIONS because it requires building
#     from source which may not be available
#
ifeq ($(obj),)
obj=	.
endif

DEPTH=	..

obj-m		+= ath_hal.o
ath_hal-objs	:= ah_osdep.o hal.o
export-objs	:= ah_osdep.o
list-multi	:= ath_hal.o

include $(obj)/${DEPTH}/Makefile.inc

UUDECODE?=uudecode

INCS+=	-I${HAL} -I${HAL}/${OS} -I${HAL}/public -I${obj}/${ATH_HAL} -I${obj}/${DEPTH}

EXTRA_CFLAGS+= ${INCS} ${COPTS}

-include $(TOPDIR)/Rules.make

all:	ah_osdep.c hal.o opt_ah.h
	$(MAKE) -C $(KERNELPATH) SUBDIRS=$(shell pwd) MODVERDIR=$(shell pwd)/${SYMBOLSDIR} modules

ah_osdep.c:
	cp ${HAL}/${OS}/ah_osdep.c ah_osdep.c

$(obj)/hal.o:
	${UUDECODE} ${HAL}/public/${TARGET}.hal.o.uu

opt_ah.h: ${HAL}/public/${TARGET}.opt_ah.h
	cp ${HAL}/public/${TARGET}.opt_ah.h opt_ah.h

install:
	test -d ${DESTDIR}/${MODULEPATH} || mkdir -p ${DESTDIR}/${MODULEPATH}
	$(STRIP) -S ath_hal.$(KMODSUF)
	cp ath_hal.${KMODSUF} ${DESTDIR}/${MODULEPATH}

clean:
	-rm -f *~ *.o *.ko *.mod.c
	-rm -f .version .*.o.flags .*.o.d .*.o.cmd .*.ko.cmd
	-rm -rf .tmp_versions
# remove the files we copied to this dir
	-rm -f ah_osdep.c opt_ah.h hal.o

ath_hal.o: ${ath_hal-objs}
	$(LD) $(LDOPTS) -o ath_hal.$(KMODSUF) -r $(ath_hal-objs)
	mv $@ ${SYMBOLSDIR}/
