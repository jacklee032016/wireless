# $Id: Makefile,v 1.1.1.1 2006/11/30 16:46:13 lizhijie Exp $
#
# Copyright (c) 2002-2004 Sam Leffler, Errno Consulting
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
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
