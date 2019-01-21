#
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
# rebuild
#
# Revision 1.7  2005/08/05 04:19:50  lizhijie
# add compatability for 80234/80235 chipset
#
# Revision 1.6  2005/07/21 03:31:42  tangr
# kernel includes put in subdirs instead of topdir
#
# Revision 1.5  2005/07/21 01:49:25  tangr
# fix kernel include path
#
# Revision 1.4  2005/07/07 07:35:29  lizhijie
# add 'make install' for release package
#
# Revision 1.3  2005/06/17 09:48:39  wangwei
# no message
#
# Revision 1.1  2005/05/26 05:10:04  lizhijie
# add zarlink 5023x driver into CVS
#
# $Revision: 1.1.1.1 $
# $Id: Makefile,v 1.1.1.1 2006/11/29 09:16:55 lizhijie Exp $
#

TOP_DIR:=$(shell pwd)

BIN_DIR:=$(TOP_DIR)/Linux.bin.arm
OBJ_DIR:=$(TOP_DIR)/Linux.obj.arm

export TOP_DIR BIN_DIR OBJ_DIR

ifeq ($(KERNEL_DIR),)
	KERNEL_DIR=$(TOP_DIR)/../../kernel/linux-2.4.x/include
endif

SUBDIRS = driver

#IXP_KERNEL_HOME:=/cygdrive/d/ixp_works/linux-2.4.x
IXP_KERNEL_HOME:=$(KERNEL_DIR)

RELEASES_DIR=lib/modules/2.4.24-uc0
BUILDTIME := $(shell TZ=UTC date -u "+%Y_%m_%d-%H_%M")
ASSIST_RELEASES_NAME=assist_driver_zarlink_arm_$(BUILDTIME).tar.gz 


CC=arm-linux-gcc
LD=arm-linux-ld
STRIP=arm-linux-strip
MKDIR=mkdir -p 
COPY = cp
RM=rm -rf 

#CFLAGS+= -I$(IXP_KERNEL_HOME)/include -I. -include $(IXP_KERNEL_HOME)/include/linux/modversions.h  
CFLAGS+= -I$(IXP_KERNEL_HOME) -I. -include $(IXP_KERNEL_HOME)/linux/modversions.h  
CFLAGS+= -mbig-endian  -D__ARM__=1  -DMODULE -D__KERNEL__ -DEXPORT_SYMTAB -DMODVERSIONS  
CFLAGS+= -O6 -Wall -Wstrict-prototypes -fomit-frame-pointer 
CFLAGS+= -c -DZARLINK_CHIPSET_50235=0 

all: $(BIN_DIR) $(OBJ_DIR) drivers $(BIN_DIR)/zl_test

drivers:$(BIN_DIR)/zl.o
#	- for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) all ) ; done

$(BIN_DIR)/zl.o:$(OBJ_DIR)/as_zl_init.o $(OBJ_DIR)/as_zl_zarlink.o $(OBJ_DIR)/as_zl_if.o
	$(LD) -r -EB $^ -o $@

$(OBJ_DIR)/as_zl_init.o:as_zl_init.c as_zarlink.h
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/as_zl_zarlink.o:as_zl_zarlink.c as_zarlink.h
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/as_zl_if.o:as_zl_if.c as_zarlink.h
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/zl_test:zl_test.c
	$(CC) -mbig-endian  $< -o $@


$(OBJ_DIR):
	-if [ -d $(OBJ_DIR) ] ; then : ; else $(MKDIR) $(OBJ_DIR); fi	

$(BIN_DIR):
	if [ ! -d $(KERNEL_DIR)/asm ]; then cd $(KERNEL_DIR); ln -s asm-arm asm; fi
	-if [ -d $(BIN_DIR) ] ; then : ; else $(MKDIR) $(BIN_DIR); fi


clean:
	rm -rf *.o *~
	rm -rf $(BIN_DIR) $(OBJ_DIR)
	- find . -name '*.gz' -print -exec /bin/rm -f {} \;
	

install:
	$(MKDIR) ./releases/$(RELEASES_DIR)
	$(COPY) $(BIN_DIR)/*.o ./releases/$(RELEASES_DIR)
	tar cvzf $(ASSIST_RELEASES_NAME) -C releases  lib
	$(RM) releases/
	
