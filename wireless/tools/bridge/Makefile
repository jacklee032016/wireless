#
#$Id: Makefile,v 1.1.1.1 2006/11/30 16:56:25 lizhijie Exp $
#

TOP_DIR:=$(shell pwd)
export TOP_DIR

include $(TOP_DIR)/Makefile.pre

KERNEL_HEADERS=-I$(KINCLUDES)

CFLAGS +=-Ilibbridge 
CODE_SUBDIRS = libbridge brctl  

SRC = 	\
		libbridge/libbridge_devif.c \
		libbridge/libbridge_if.c \
		libbridge/libbridge_init.c \
		libbridge/libbridge_misc.c \
		brctl/brctl_cmd.c \
		brctl/brctl_disp.c \
		brctl/brctl.c 
	
LOCAL_ALL=$(BIN_DIR)/sbin/brctl


include $(TOP_DIR)/Makefile.post

$(LOCAL_ALL):$(OBJS)
	$(CC) $(LIB_LDFLAGS) -o $@ $(OBJS) -L$(BIN_DIR)/lib  
	$(STRIP)  $@

LOCAL_CLEAN:=clear

$(LOCAL_CLEAN):
	
install:
#	$(MKDIR) ./releases/sbin
	$(COPY) ./atheros/script/*.sh $(BIN_DIR)/sbin 
	tar cvzf $(ASSIST_RELEASES_NAME) -C $(BIN_DIR) sbin lib 
#	$(RM) releases/

