#$Id: Makefile,v 1.1.1.1 2006/11/29 09:16:54 lizhijie Exp $

DRIVERS_DIR=$(shell pwd)
DRIVERS_BIN=$(DRIVERS_DIR)/bin
export DRIVERS_DIR DRIVERS_BIN

ARCH=arm
ifneq ($(ARCH),arm)
	ARCH=i686
endif

SUBDIRS += slic 
SUBDIRS += pcm
SUBDIRS += zarlink/src
SUBDIRS += misc/src


all: 
	for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) $@ ) ; done

clean: 
	rm -rf $(DRIVERS_BIN)
	for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) $@ ) ; done

install:
	mkdir -p $(DRIVERS_BIN)/arm
	mkdir -p $(DRIVERS_BIN)/i686
	for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) $@ ) ; done
