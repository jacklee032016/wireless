#
# $Id: xscale-be-elf.inc,v 1.1.1.1 2006/11/30 17:01:51 lizhijie Exp $
#

#
# Compilation configuration for building big-endian XScale/arm-elf.
#
# NB: built with AH_REGOPS_FUNC to so that register accesses
#     can be done using the Linux readl/writel functions on
#     systems that need it.
#
# Reported to work on:
#	IXP425
#

#
# Built with GNU cross-devel tools:
#
# PREFIX=/pub/gnu
# BINUTILS=binutils-2.14
# GCC=gcc-3.3.2
# target=arm-elf
#
# ${BINUTILS}/configure --target=$target --prefix=${PREFIX}
# ${GCC}/configure --target=$target   --prefix=${PREFIX} \
#	--enable-languages=c   --with-gnu-as --with-gnu-ld \
#	--with-newlib   --with-gxx-include-dir=${PREFIX}/$target/include
#

TOOLPREFIX=arm-linux-

ifndef TOOLPREFIX
TOOLPREFIX=	/pub/gnu/bin/arm-elf-
endif
#
CC=	${TOOLPREFIX}gcc
LD=	${TOOLPREFIX}ld
STRIP=	${TOOLPREFIX}strip
OBJCOPY=${TOOLPREFIX}objcopy
NM=	${TOOLPREFIX}nm

LDOPTS=	-EB
COPTS+=	-DAH_BYTE_ORDER=AH_BIG_ENDIAN -DAH_REGOPS_FUNC
COPTS+= -mbig-endian -fno-strict-aliasing -fno-common -mapcs-32 \
	-mtune=xscale -malignment-traps -msoft-float -mfp=2 -D__ARM__=1  -DEXPORT_SYMTAB 
# -march=armv4 