#
# $Id: Makefile,v 1.1.1.1 2006/11/30 16:56:48 lizhijie Exp $
# testcvs

TOP_DIR:=$(shell pwd)
export TOP_DIR

SUBDIRS += src

# SUBDIRS += atheros

#SUBDIRS += bridge 

# SUBDIRS += tools/src

# SUBDIRS += hostapd/src 
# SUBDIRS += hostap-utils/src

all: 
	for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) $@ ) ; done

clean: 
	for i in $(SUBDIRS) ; do ( cd $$i && $(MAKE) $@ ) ; done

include $(TOP_DIR)/Makefile.pre

install:
#	$(MKDIR) ./releases/sbin
	$(COPY) ./atheros/script/*.sh $(BIN_DIR)/sbin 
	tar cvzf $(ASSIST_RELEASES_NAME) -C $(BIN_DIR) sbin lib 
#	$(RM) releases/

