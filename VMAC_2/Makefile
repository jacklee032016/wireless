#
# $Id: Makefile,v 1.1.1.1 2006/11/30 17:01:32 lizhijie Exp $
#

TOP_DIR:=$(shell pwd)

ARCH=arm

export TOP_DIR ARCH 

SUBDIRS = \
	score		\
	vnetif		\
	macs/aloha	\
	macs/nullmac	\
	macs/mactime	\
	macs/rawmac 

	
#	cheesymac \
#	multimac \
#	remotemac \
#       formage \
#	fec \

#	remotemac/ktund \


drivers:
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
	tar cvzf $(RELEASES_NAME) -C releases  lib
	$(RM) releases/
	