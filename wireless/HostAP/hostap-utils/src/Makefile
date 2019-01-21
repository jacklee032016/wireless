#$Id: Makefile,v 1.1.1.1 2006/11/30 16:59:12 lizhijie Exp $

# TOP_DIR:=$(shell pwd)/..

include $(TOP_DIR)/Makefile.pre

# name of all files to be compiled (don't include header files)
SRC += util.c

WT_BIN_DIR=$(BIN_DIR)/sbin
	
INCLUDE_DIR= -I.  

C_CFLAGS+= 
		 
		 
		 

LOCAL_ALL= \
	$(WT_BIN_DIR)/prism2_srec \
	$(WT_BIN_DIR)/hostap_crypt_conf \
	$(WT_BIN_DIR)/hostap_diag \
	$(WT_BIN_DIR)/hostap_io_debug \
	$(WT_BIN_DIR)/hostap_rid \
	install


include $(TOP_DIR)/Makefile.post

$(WT_BIN_DIR)/prism2_srec: $(OBJ_DIR)/prism2_srec.o $(OBJ_DIR)/util.o 
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LDFLAGS) -lm
	
$(WT_BIN_DIR)/hostap_crypt_conf: $(OBJ_DIR)/hostap_crypt_conf.o $(OBJ_DIR)/util.o
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LDFLAGS) -lm
	
$(WT_BIN_DIR)/hostap_diag: $(OBJ_DIR)/hostap_diag.o $(OBJ_DIR)/util.o 
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LDFLAGS) -lm

$(WT_BIN_DIR)/hostap_io_debug: $(OBJ_DIR)/hostap_io_debug.o  $(OBJ_DIR)/util.o
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LDFLAGS) -lm

$(WT_BIN_DIR)/hostap_rid: $(OBJ_DIR)/hostap_rid.o $(OBJ_DIR)/util.o
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LDFLAGS) -lm


install::

LDFLAGS += -L/bin/$(ARCH) -L$(BIN_DIR) -lpthread
