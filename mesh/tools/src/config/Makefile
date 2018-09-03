# $Id: Makefile,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
# $Author: lizhijie $
#

include $(TOP_DIR)/Makefile.pre

KERNEL=no

CODE_SUBDIRS = .

C_CFLAGS+= -I$(TOP_DIR)/mesh/include -I$(TOP_DIR)/tools/include -I$(TOP_DIR)/tools/lib -DMESHWU_WITHOUT_LIBM 

C_FLAGS += $(C_CFLAGS) 

SRC += \
	meshconfig_info.c	\
	meshconfig_cmds.c	\
	meshconfig_main.c 

LDFLAGS+=-L$(BIN_DIR)/lib  
LDFLAGS+= -lmeshw

LOCAL_LAST=meshconfig

$(LOCAL_LAST):$(OBJS) 

include $(TOP_DIR)/Makefile.post

meshconfig:$(OBJS)
	$(CC) -o $@ $(C_FLAGS) $^ $(LDFLAGS) 
	$(STRIP) $@
	$(MOVE) $@ $(BIN_DIR)/bin   
