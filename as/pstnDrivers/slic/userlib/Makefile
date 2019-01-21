#
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
# rebuild
#
# Revision 1.2  2005/04/26 06:06:10  lizhijie
# *** empty log message ***
#
# Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
# new drivers for all devices
#
# Revision 1.7  2005/01/06 05:40:07  fengshikui
# add busy tone detect by fengshikui 2005.1.6
#
# Revision 1.6  2004/12/20 03:18:15  lizhijie
# Add DSP library for FSK detect/generate, DTMF detect and other test code
#
# Revision 1.5  2004/12/09 07:45:13  lizhijie
# no message
#
# Revision 1.4  2004/11/29 08:25:04  eagle
# 2229 by chenchaoxin
#
# Revision 1.3  2004/11/25 07:15:18  lizhijie
# convert into DOS format
#
# Revision 1.2  2004/11/22 01:54:04  lizhijie
# add some user module into CVS
#
# Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
# driver for assist telephone cards Tiger320-Si3210/3050
#
# $Revision: 1.1.1.1 $
#

include $(TOP_DIR)/Makefile.pre

KERNEL=no

CODE_SUBDIRS = use control dtmf utils  channel tonegen fsk busydetect # law io ioctl file 

K_CFLAGS+=-Idsp/user

SRC = 	control/as_lib_tonezone.c \
	control/as_tonezone_data.c \
	control/as_lib_ring_param.c \
	use/as_lib_gsm.c \
	use/as_lib_ring.c \
	use/as_lib_tones.c \
	use/as_lib_law.c \
	use/as_lib_hard_dtmf.c \
	dtmf/as_lib_dtmf_detect.c \
	dtmf/as_lib_dtmf_generator.c \
	utils/as_fifo.c \
	utils/as_list.c \
	utils/as_thread.c \
	as_lib_utils.c \
	channel/as_lib_buf.c \
	tonegen/as_dsp_utils.c \
	tonegen/as_dsp_tone_gen.c \
	tonegen/as_dsp_fsk_generator.c \
	tonegen/as_dsp_dtmf_generator.c \
	tonegen/as_dsp_gain.c \
	fsk/as_dsp_fsk_detect.c \
	fsk/as_dsp_demodule.c \
	fsk/as_dsp_filtros.c \
	busydetect/as_busy_detect.c
	
#	utils/as_network.c \
#	utils/as_network.c \
#	utils/as_log.c \
	


LOCAL_ALL=header_file

$(LOCAL_ALL):
#	make -C dsp/user all

LOCAL_CLEAN:=clear

$(LOCAL_CLEAN):
	
TARGET_LIB:=libassist.a

include $(TOP_DIR)/Makefile.post
