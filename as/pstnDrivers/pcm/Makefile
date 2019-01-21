#
# $Revision: 1.1.1.1 $
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
# rebuild
#
# Revision 1.3  2005/07/21 03:31:42  tangr
# kernel includes put in subdirs instead of topdir
#
# Revision 1.2  2005/07/07 07:38:07  lizhijie
# add 'make install' for release pckage
#
# Revision 1.1  2005/06/27 05:59:01  lizhijie
# no message
#
#

TOP_DIR:=$(shell pwd)

export TOP_DIR

SUBDIRS = driver #userlib test #test_channel 

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


clean:
	- for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) clean ) ; done
	- find . -name '*.d' -print -exec /bin/rm -f {} \;
	- find . -name '*.gz' -print -exec /bin/rm -f {} \;
	
include $(TOP_DIR)/Makefile.pre

install:
	$(MKDIR) ./releases/$(RELEASES_DIR)
	$(COPY) $(BIN_DIR)/*.o ./releases/$(RELEASES_DIR)
	tar cvzf $(ASSIST_RELEASES_NAME) -C releases  lib
	$(RM) releases/
	