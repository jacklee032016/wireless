#
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
# rebuild
#
# Revision 1.7  2005/07/21 03:31:42  tangr
# kernel includes put in subdirs instead of topdir
#
# Revision 1.6  2005/07/07 07:38:38  lizhijie
# add 'make install' for release SLIC package
#
# Revision 1.5  2005/06/17 09:48:50  wangwei
# no message
#
# Revision 1.2  2005/04/26 06:06:09  lizhijie
# *** empty log message ***
#
# Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
# new drivers for all devices
#
# Revision 1.3  2005/01/06 05:35:49  fengshikui
# add busy tone detect by fengshikui 2005.1.6
#
# Revision 1.2  2004/11/25 07:10:59  lizhijie
# convert format into DOS format
#
# Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
# driver for assist telephone cards Tiger320-Si3210/3050
#
# $Revision: 1.1.1.1 $
#

TOP_DIR:=$(shell pwd)

export TOP_DIR

SUBDIRS = userlib driver test

drivers:
	if [ ! -d $(KERNEL_DIR)/asm ]; then cd $(KERNEL_DIR); ln -s asm-arm asm; fi
	- for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) all ) ; done

all: drivers

releasedir:
	mkdir -p release/bin.$(TARGET_NAME)
	mkdir -p release/lib.$(TARGET_NAME)


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

include $(TOP_DIR)/Makefile.pre

install:
	$(MKDIR) ./releases/$(RELEASES_DIR)
	$(COPY) $(BIN_DIR)/*.o ./releases/$(RELEASES_DIR)
	tar cvzf $(ASSIST_RELEASES_NAME) -C releases  lib
	$(RM) releases/
	

#	install -m 644 Linux.bin.i686/libassist.a   /usr/lib
#	install -m 644 include/assist_lib.h   /usr/include
#	-if [ -z $(DRIVERS_BIN) ]; then : ; else  cp -f Linux.bin.i686/span.o $(DRIVERS_BIN)/i686; cp -f Linux.bin.i686/fxs.o $(DRIVERS_BIN)/i686; fi

clean:
	- for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) clean ) ; done
	- find . -name '*.d' -print -exec /bin/rm -f {} \;
	- find . -name '*.gz' -print -exec /bin/rm -f {} \;
