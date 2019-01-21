#
# $Id: Makefile,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
#

#
# Makefile for the Atheros Rate Control Support.
#
ifeq ($(obj),)
obj=	.
endif

DEPTH=	../..

obj-m		+= ath_rate_amrr.o
ath_rate_amrr-objs	:= amrr.o
export-objs	:= amrr.o
list-multi	:= ath_rate_onoe.o

include $(obj)/${DEPTH}/Makefile.inc

INCS+=	-I${HAL} -I${HAL}/${OS} -I${obj}/${ATH} -I${obj}/${WLAN} \
	-I${obj}/${DEPTH}

EXTRA_CFLAGS+= ${INCS} ${COPTS}

-include $(TOPDIR)/Rules.make

all:
	$(MAKE) -C $(KERNELPATH) SUBDIRS=$(shell pwd) modules

install:
	test -d ${DESTDIR}/${MODULEPATH} || mkdir -p ${DESTDIR}/${MODULEPATH}
	${TOOLPREFIX}strip -S ath_rate_amrr.$(KMODSUF)
	cp ath_rate_amrr.${KMODSUF} ${DESTDIR}/${MODULEPATH}

clean:
	-rm -f *~ *.o *.ko *.mod.c
	-rm -f .version .*.o.flags .*.o.d .*.o.cmd .*.ko.cmd

ath_rate_amrr.o: ${ath_rate_amrr-objs}
	$(LD) $(LDOPTS) -o ath_rate_amrr.$(KMODSUF) -r $(ath_rate_amrr-objs)
	mv $@ ${SYMBOLSDIR}/
