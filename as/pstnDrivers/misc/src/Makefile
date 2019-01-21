#
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
# rebuild
#
# Revision 1.10  2005/11/30 07:23:44  wangwei
# 增加烧录93LC66程序，可以通过条件WITH_93LC66=no选择编译
#
# Revision 1.9  2005/09/07 10:02:32  wangwei
# no message
#
# Revision 1.8  2005/07/21 03:31:42  tangr
# kernel includes put in subdirs instead of topdir
#
# Revision 1.7  2005/07/21 01:49:25  tangr
# fix kernel include path
#
# Revision 1.6  2005/07/07 07:37:01  lizhijie
# add 'make install' for release package
#
# Revision 1.5  2005/06/17 09:49:15  wangwei
# no message
#
# Revision 1.3  2005/06/15 02:57:36  wangwei
# no message
#
# Revision 1.2  2005/06/07 09:27:25  lizhijie
# build enable/disable/update 3 programs
#
# Revision 1.1  2005/06/07 09:16:21  lizhijie
# add into CVS
#
# $Revision: 1.1.1.1 $
# $Id: Makefile,v 1.1.1.1 2006/11/29 09:16:54 lizhijie Exp $
#

TOP_DIR:=$(shell pwd)

BIN_DIR:=$(TOP_DIR)/Linux.bin.arm
OBJ_DIR:=$(TOP_DIR)/Linux.obj.arm

export TOP_DIR BIN_DIR OBJ_DIR

SUBDIRS = driver

ifeq ($(KERNEL_DIR),)
	KERNEL_DIR=$(TOP_DIR)/../../kernel/linux-2.4.x/include
endif

#IXP_KERNEL_HOME:=/cygdrive/d/ixp_works/linux-2.4.x
IXP_KERNEL_HOME:=$(KERNEL_DIR)

RELEASES_DIR=lib/modules/2.4.24-uc0
BUILDTIME := $(shell TZ=UTC date -u "+%Y_%m_%d-%H_%M")
ASSIST_RELEASES_NAME=assist_driver_misc_arm_$(BUILDTIME).tar.gz 


CC=arm-linux-gcc
LD=arm-linux-ld
STRIP=arm-linux-strip
MKDIR=mkdir -p 
COPY = cp
RM=rm -rf 

CFLAGS+= -I$(IXP_KERNEL_HOME) -I. -include $(IXP_KERNEL_HOME)/linux/modversions.h  
CFLAGS+= -mbig-endian  -D__ARM__=1  -DMODULE -D__KERNEL__ -DEXPORT_SYMTAB -DMODVERSIONS  
CFLAGS+= -O6 -Wall -Wstrict-prototypes -fomit-frame-pointer 
CFLAGS+= -I$(TOP_DIR)/../include -c

ifeq ($(WITH_93LC66),no)
	CFLAGS+= -DHARDWARE_93LC66=0
else
	CFLAGS+= -DHARDWARE_93LC66=1
endif

ALL_MOD += $(BIN_DIR) 
ALL_MOD += $(OBJ_DIR) 
ALL_MOD += drivers  
ALL_MOD += $(BIN_DIR)/beep 
ALL_MOD += $(BIN_DIR)/watchdog 
ALL_MOD += $(BIN_DIR)/button 

OBJ_FILE += $(OBJ_DIR)/as_misc_init.o 
OBJ_FILE += $(OBJ_DIR)/as_misc_watchdog.o 
OBJ_FILE += $(OBJ_DIR)/as_misc_beep.o 
OBJ_FILE += $(OBJ_DIR)/as_misc_button.o 

ifeq ($(WITH_93LC66),no)
else
ALL_MOD += $(BIN_DIR)/93lc66
OBJ_FILE += $(OBJ_DIR)/as_misc_93lc66.o
endif

all: $(ALL_MOD)

drivers:$(BIN_DIR)/as_misc.o

$(BIN_DIR)/as_misc.o:$(OBJ_FILE)
	$(LD) -r -EB $^ -o $@

$(OBJ_DIR)/as_misc_init.o:as_misc_init.c ../include/as_misc_dev.h as_misc_version.h
	$(CC) $(CFLAGS) $< -o $@


$(OBJ_DIR)/as_misc_watchdog.o:as_misc_watchdog.c ../include/as_misc_dev.h as_misc_version.h
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/as_misc_beep.o:as_misc_beep.c ../include/as_misc_dev.h as_misc_version.h
	$(CC) $(CFLAGS) $< -o $@
	
$(OBJ_DIR)/as_misc_button.o:as_misc_button.c ../include/as_misc_dev.h as_misc_version.h
	$(CC) $(CFLAGS) $< -o $@
	
ifeq ($(WITH_93LC66),no)
else
$(OBJ_DIR)/as_misc_93lc66.o:as_misc_93lc66.c ../include/as_misc_dev.h as_misc_version.h
	$(CC) $(CFLAGS) $< -o $@
endif

$(BIN_DIR)/button:$(BIN_DIR)/button_state button_test.c

$(BIN_DIR)/button_state:button_test.c
	$(CC) -mbig-endian -DTEST_ENABLE=1 -I$(TOP_DIR)/../include $< -o $@
	$(STRIP) $@
$(BIN_DIR)/beep:$(BIN_DIR)/beep_enable $(BIN_DIR)/beep_disable beep_test.c


$(BIN_DIR)/beep_enable:beep_test.c
	$(CC) -mbig-endian -DTEST_ENABLE=1 -I$(TOP_DIR)/../include $< -o $@
	$(STRIP) $@
	
$(BIN_DIR)/beep_disable:beep_test.c
	$(CC) -mbig-endian -DTEST_DISBALE=1 -I$(TOP_DIR)/../include $< -o $@
	$(STRIP) $@


$(BIN_DIR)/watchdog:$(BIN_DIR)/watchdog_enable $(BIN_DIR)/watchdog_disable $(BIN_DIR)/watchdog_update watchdog_test.c

$(BIN_DIR)/watchdog_enable:watchdog_test.c
	$(CC) -mbig-endian -DTEST_ENABLE=1 -I$(TOP_DIR)/../include $< -o $@
	$(STRIP) $@
	
$(BIN_DIR)/watchdog_disable:watchdog_test.c
	$(CC) -mbig-endian -DTEST_DISBALE=1 -I$(TOP_DIR)/../include $< -o $@
	$(STRIP) $@

$(BIN_DIR)/watchdog_update:watchdog_test.c
	$(CC) -mbig-endian -DTEST_UPDATE=1 -I$(TOP_DIR)/../include $< -o $@
	$(STRIP) $@
ifeq ($(WITH_93LC66),no)
else
$(BIN_DIR)/93lc66:$(BIN_DIR)/m93lc66_write $(BIN_DIR)/m93lc66_read 93lc66_test.c
$(BIN_DIR)/m93lc66_write:93lc66_test.c
	$(CC) -mbig-endian -DTEST_ENABLE=1 -I$(TOP_DIR)/../include $< -o $@
	$(STRIP) $@
$(BIN_DIR)/m93lc66_read:93lc66_test.c
	$(CC) -mbig-endian -DTEST_ENABLE=1 -I$(TOP_DIR)/../include $< -o $@
	$(STRIP) $@
endif

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
