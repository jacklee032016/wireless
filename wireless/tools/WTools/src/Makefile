#$Id: Makefile,v 1.1.1.1 2006/11/30 17:01:03 lizhijie Exp $

# TOP_DIR:=$(shell pwd)/..

include $(TOP_DIR)/Makefile.pre

# name of all files to be compiled (don't include header files)
SRC += iwlib.c


# Version of the Wireless Tools
WT_VERSION := $(shell sed -ne "/WT_VERSION/{s:\([^0-9]*\)::;p;q;}" < iwlib.h )

# Version of Wireless Extensions.
WE_VERSION := $(shell sed -ne "/WE_VERSION/{s:\([^0-9]*\)::;p;q;}" < iwlib.h )

# Always use local header for wireless extensions
WEXT_HEADER = wireless.$(WE_VERSION).h

WT_BIN_DIR=$(BIN_DIR)/sbin
	
CFLAGS=-Os -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wshadow \
	-Wpointer-arith -Wcast-qual -Winline -I. 
DEPFLAGS=-MMD
XCFLAGS=$(CFLAGS) $(DEPFLAGS) $(WARN) $(HEADERS) $(WELIB_FLAG)
PICFLAG=-fPIC

INCLUDE_DIR= -I.  

C_CFLAGS+= 
		 
		 
		 
IWLIB := libiw
TARGET_LIB = $(BIN_DIR)/lib/$(IWLIB)

LOCAL_ALL=wireless.h \
	$(WT_BIN_DIR)/iwconfig \
	$(WT_BIN_DIR)/iwlist \
	$(WT_BIN_DIR)/iwpriv \
	$(WT_BIN_DIR)/iwspy \
	$(WT_BIN_DIR)/iwgetid \
	$(WT_BIN_DIR)/iwevent \
	$(WT_BIN_DIR)/ifrename \
	$(WT_BIN_DIR)/macaddr \
	install

include $(TOP_DIR)/Makefile.post

$(WT_BIN_DIR)/iwconfig: $(OBJ_DIR)/iwconfig.o 
	$(CC) $(LIB_LDFLAGS) -o $@ $< $(LDFLAGS) -lm
	
$(WT_BIN_DIR)/iwlist: $(OBJ_DIR)/iwlist.o 
	$(CC) $(LIB_LDFLAGS) -o $@ $< $(LDFLAGS) -lm

$(WT_BIN_DIR)/iwpriv: $(OBJ_DIR)/iwpriv.o
	$(CC) $(LIB_LDFLAGS) -o $@ $< $(LDFLAGS) -lm

$(WT_BIN_DIR)/iwspy: $(OBJ_DIR)/iwspy.o
	$(CC) $(LIB_LDFLAGS) -o $@ $< $(LDFLAGS) -lm

$(WT_BIN_DIR)/iwgetid: $(OBJ_DIR)/iwgetid.o
	$(CC) $(LIB_LDFLAGS) -o $@ $< $(LDFLAGS) -lm

$(WT_BIN_DIR)/iwevent: $(OBJ_DIR)/iwevent.o 
	$(CC) $(LIB_LDFLAGS) -o $@ $< $(LDFLAGS) -lm

$(WT_BIN_DIR)/ifrename: $(OBJ_DIR)/ifrename.o
	$(CC) $(LIB_LDFLAGS) -o $@ $< $(LDFLAGS) -lm

$(WT_BIN_DIR)/macaddr: $(OBJ_DIR)/macaddr.o
	$(CC) $(LIB_LDFLAGS) -o $@ $< $(LDFLAGS) -lm 

wireless.h:
	cp $(WEXT_HEADER) wireless.h

install::
	cp iwlib.h $(INSTALL_INC)
	cp wireless.h $(INSTALL_INC)
	install -m 644 iwlib.h $(INSTALL_INC)
	install -m 644 wireless.h $(INSTALL_INC)

LIB_LDFLAGS += -lm 

LDFLAGS += $(LIB_LDFLAGS) -L/bin/$(ARCH) -L$(BIN_DIR)/lib -liw # -lpthread

