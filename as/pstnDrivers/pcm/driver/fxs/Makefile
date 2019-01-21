# Makefile for the low layer driver of PCM-Aanlog PBX
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
# rebuild
#
# Revision 1.1  2005/06/27 06:00:44  lizhijie
# no message
#
# Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
# new drivers for all devices
#
# Revision 1.1.1.1  2004/11/29 01:47:46  lizhijie
# Driver for Tiger-AnalogPBX
#
# $Revision: 1.1.1.1 $
#

include $(TOP_DIR)/Makefile.pre

KERNEL_LIB:=pcm.o
	
KSRC = 	 \
	as_fxs_init.c \
	as_fxs_device_init.c \
	as_fxs_isr.c 

ifeq ($(DEBUG_TIGER),yes)
	KSRC += as_fxs_tiger.c
endif	

include $(TOP_DIR)/Makefile.post
