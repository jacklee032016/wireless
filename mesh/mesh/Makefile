#
# $Revision: 1.1.1.1 $
# $Author: lizhijie $
#

include $(TOP_DIR)/Makefile.pre

K_CFLAGS+= -I$(TOP_DIR)/mesh/include -I$(TOP_DIR)/wlan/include  
K_CFLAGS+= -I$(KINCLUDES)/../  

KERNEL:=yes

CODE_SUBDIRS = mgr dev ioctl fwdtables utils

KERNEL_LIB:=mesh.o

MESH_IOCTL_SRC += \
	ioctl/meshw_ioctl_core.c \
	ioctl/meshw_ioctl_event.c \
	ioctl/meshw_ioctl_spy.c \
	ioctl/meshw_ioctl_proc.c \

MESH_DEV_SRC += \
	dev/mesh_dev_sche.c \
	dev/mesh_portal.c \
	dev/mesh_dev_tx.c \
	dev/mesh_dev_rx.c \

MESH_MGR_SRC += \
	mgr/mesh_mgr_ioctl.c \
	mgr/mesh_mgr_mgmt.c \

MESH_FWD_SRC += \
	fwdtables/mesh_fwd_init.c \
	fwdtables/mesh_fwd_db.c \
	fwdtables/mesh_fwd_input.c \
	fwdtables/mesh_fwd_forward.c \

KSRC = 	 \
	$(MESH_FWD_SRC) \
	$(MESH_IOCTL_SRC) \
	$(MESH_DEV_SRC) \
	$(MESH_MGR_SRC) \
	utils/mesh_utils.c \
	mesh_file_ops.c \
	mesh_sysctrl.c \
	mesh_mgr_init.c 
	
KODDOBJS += 

include $(TOP_DIR)/Makefile.post
