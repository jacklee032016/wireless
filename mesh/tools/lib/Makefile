# $Id: Makefile,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
# $Author: lizhijie $
#

include $(TOP_DIR)/Makefile.pre

KERNEL=no

CODE_SUBDIRS = bridge

C_CFLAGS+= -I$(TOP_DIR)/mesh/include -I$(TOP_DIR)/tools/include  -DMESHWU_WITHOUT_LIBM 

C_FLAGS += $(C_CFLAGS)


SRC += \
	meshwu_lib.c \
	meshwu_lib_utils.c \
	meshwu_lib_tools.c \
	meshwu_lib_80211.c \
	bridge/mbridge_devif.c \
	bridge/mbridge_if.c \
	mesh_lib.c 
	
TARGET_LIB:=libmeshw
# TARGET_LIB_SHARED:=$(TARGET_LIB).so

LOCAL_ALL:=

include $(TOP_DIR)/Makefile.post

