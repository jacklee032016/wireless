#
# $Revision: 1.1.1.1 $
# $Author: lizhijie $
#

RATE_CONTROL := ONOE
# AMRR ONOE SAMPLE

include $(TOP_DIR)/Makefile.pre

K_CFLAGS+= -I$(TOP_DIR)/mesh/include
K_CFLAGS+= -I$(TOP_DIR)/wlan/include 
K_CFLAGS+= -I$(TOP_DIR)/phy/include 
K_CFLAGS+= -I$(TOP_DIR)/hw 

CODE_SUBDIRS = ctrl hw init mesh wlan rate/amrr rate/onoe rate/sample 

KERNEL_LIB:= phy.o
PHY_CTRL_SRC += \
	ctrl/phy_sysctl_static.c \
	ctrl/phy_sysctl_dynamic.c \
	ctrl/phy_ioctl.c \

PHY_WLAN_SRC += \
	wlan/phy_crypto.c \
	wlan/phy_beacon.c \
	
PHY_INIT_SRC += \
	init/phy_init_utils.c \
	init/phy_dev_init.c \
	init/phy_init_tasklet.c \
	
PHY_MESH_SRC += \
	mesh/phy_dev_utils.c \
	mesh/phy_dev_mesh.c \
	
PHY_HW_SRC += \
	hw/phy_isr.c \
	hw/phy_rx.c \
	hw/phy_tx.c \

ifeq ($(RATE_CONTROL),AMRR)
	PHY_RATE_SRC += \
		rate/amrr/phy_rate_amrr.c \
		rate/amrr/phy_rate_amrr_sysctl.c \
#		rate/amrr/phy_rate_amrr_init.c \

endif

ifeq ($(RATE_CONTROL),ONOE)
	PHY_RATE_SRC += \
		rate/onoe/phy_rate_onoe.c \
		rate/onoe/phy_rate_onoe_sysctl.c \
#		rate/onoe/phy_rate_onoe_init.c \

endif

ifeq ($(RATE_CONTROL),SAMPLE)
	PHY_RATE_SRC += \
		rate/sample/phy_rate_sample.c \
		rate/sample/phy_rate_sample_sysctl.c \
#		rate/sample/phy_rate_sample_init.c \

endif
	
KSRC = 	 \
	$(PHY_CTRL_SRC) \
	$(PHY_WLAN_SRC) \
	$(PHY_INIT_SRC) \
	$(PHY_MESH_SRC) \
	$(PHY_HW_SRC) 	\
	$(PHY_RATE_SRC) \
	radar.c 	\
	phy_init.c 
	
KODDOBJS += 

include $(TOP_DIR)/Makefile.post
