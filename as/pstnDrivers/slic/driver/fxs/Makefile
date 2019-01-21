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
# Revision 1.4  2004/12/11 05:35:51  lizhijie
# add Tiger320 debug info
#
# Revision 1.3  2004/11/26 12:33:31  lizhijie
# add multi-card support
#
# Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
# driver for assist telephone cards Tiger320-Si3210/3050
#
# $Revision: 1.1.1.1 $
#

include $(TOP_DIR)/Makefile.pre

KERNEL_LIB:=fxs.o
	
KSRC = 	as_fxs.c \
	as_fxs_init.c \
	as_fxs_device_init.c \
	as_fxs_isr.c \
	as_fxs_proslic.c \
	as_fxs_voicedaa.c \
	as_fxs_handler.c \
	as_fxs_dtmf.c 

ifeq ($(HARDWARE_DSP),yes)
KSRC+=as_fxs_tone.c
else
endif

ifeq ($(TIGER_DEBUG),yes)
KSRC+=as_fxs_tiger.c
else
endif

include $(TOP_DIR)/Makefile.post
