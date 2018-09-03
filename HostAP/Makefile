# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/30 16:59:12  lizhijie
# MAC HostAP
#
# Revision 1.1.1.1  2006/04/01 11:29:57  lizhijie
# hostap-all
#
# Revision 1.4  2005/07/06 07:33:47  lizhijie
# add some descriptions about atheros
#
# $Id: Makefile,v 1.1.1.1 2006/11/30 16:59:12 lizhijie Exp $
#

TOP_DIR:=$(shell pwd)
export TOP_DIR

# SUBDIRS += aodv/src

SUBDIRS += atheros

# SUBDIRS += bridge 

SUBDIRS += tools/src

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

