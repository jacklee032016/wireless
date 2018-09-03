#
# $Revision: 1.1.1.1 $
# $Author: lizhijie $
#

include $(TOP_DIR)/Makefile.pre

K_CFLAGS+= -I$(TOP_DIR)/mesh/include
K_CFLAGS+= -I$(TOP_DIR)/wlan/include 
K_CFLAGS+= -I$(TOP_DIR)/phy/include 

CODE_SUBDIRS = crypt ctrl input node output phy protocol we

KERNEL_LIB:= mac.o


WLAN_CTRL_SRC += \
	ctrl/wlan_ioctl_params.c \
	ctrl/wlan_ioctl_event.c \
	ctrl/wlan_ioctl.c \
	ctrl/wlan_handlers.c \
	ctrl/wlan_handlers_private.c \
	we/wlan_sysctrl.c \

WLAN_CRYPTO_SRC += \
	crypt/wlan_crypto_none.c \
	crypt/wlan_crypto_init.c \
	crypt/wlan_crypto.c \

WLAN_PHY_SRC += \
	phy/wlan_if_media.c \
	phy/wlan_media.c \
	phy/wlan_phy_init.c \
	phy/wlan_vap.c \
	
WLAN_NODE_SRC += \
	node/wlan_node.c \
	node/wlan_node_mgmt.c \
	node/wlan_node_scan.c \
	node/wlan_node_init.c \

WLAN_PROTOCOL_SRC += \
	protocol/wlan_proto_acl.c \
	protocol/wlan_proto_authen.c \
	protocol/wlan_proto_state.c \
	protocol/wlan_proto_wme.c \
	protocol/wlan_proto_utils.c \

WLAN_INPUT_SRC += \
	input/wlan_input_mgmt.c \
	input/wlan_input_data.c \
	input/wlan_input_mgmt_utils.c \
	input/wlan_input_mgmt_deauth.c \
	input/wlan_input_utils.c \
	input/wlan_input.c \

WLAN_OUTPUT_SRC += \
	output/wlan_output_wpa.c \
	output/wlan_output_misc.c \
	output/wlan_output_crypto.c \
	output/wlan_output_beacon.c \
	output/wlan_output.c \
	output/wlan_output_utils.c \
	
KSRC = 	 \
	$(WLAN_CTRL_SRC) \
	$(WLAN_CRYPTO_SRC) \
	$(WLAN_PHY_SRC) \
	$(WLAN_NODE_SRC) \
	$(WLAN_PROTOCOL_SRC) \
	$(WLAN_INPUT_SRC) \
	$(WLAN_OUTPUT_SRC) \
	wlan_init.c 
	
KODDOBJS += 

include $(TOP_DIR)/Makefile.post
