#
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
# rebuild
#
# Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
# new drivers for all devices
#
# Revision 1.4  2004/12/14 12:48:50  lizhijie
# support building header files in the architecture platform
#
# Revision 1.3  2004/11/22 02:05:52  lizhijie
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

CODE_SUBDIRS = dsp utils law io ioctl file echo

K_CFLAGS+=-Idsp/user

KSRC = 	as_tel_init.c \
	file/as_dev_file_rw.c \
	file/as_dev_file_open.c \
	file/as_dev_file_poll.c \
	io/as_dev_io_receive.c \
	io/as_dev_io_transmit.c \
	ioctl/as_dev_file_ctl.c \
	ioctl/as_dev_ctl_ch_and_pseudo.c \
	ioctl/as_span_ctl.c \
	law/as_tel_law.c \
	utils/as_dev_utils.c \
	utils/as_dev_chan_utils.c \
	utils/as_dev_event_queue.c \
	dsp/as_dsp_zone.c \
	dsp/as_dsp_dsp.c \
	echo/as_echo_cancel.c 	
	

#ifeq ($(HARDWARE_DSP),yes)
#KSRC += dsp/as_tel_zone_si.c \
#	dsp/as_tel_dsp_si.c 
#else
#KSRC += dsp/as_tel_zone.c \
#	dsp/as_tel_dsp.c 
#endif

KERNEL_LIB:=span.o

LOCAL_ALL=header_file

$(LOCAL_ALL):
	make -C dsp/user ARCH=$(ARCH) all

LOCAL_CLEAN:=clear

$(LOCAL_CLEAN):
	make -C dsp/user  ARCH=$(ARCH) clean
	

include $(TOP_DIR)/Makefile.post
