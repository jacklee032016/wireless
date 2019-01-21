# Makefile for the upper layer driver of PCM-Aanlog PBX
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
# rebuild
#
# Revision 1.1  2005/06/27 06:01:01  lizhijie
# no message
#
# Revision 1.2  2005/06/24 04:07:27  lizhijie
# *** empty log message ***
#
# Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
# new drivers for all devices
#
# Revision 1.1.1.1  2004/11/29 01:47:47  lizhijie
# Driver for Tiger-AnalogPBX
#
# $Revision: 1.1.1.1 $
#

include $(TOP_DIR)/Makefile.pre

CODE_SUBDIRS = utils io ioctl file law #echo dsp 

K_CFLAGS+=-Idsp/user

KSRC = 	as_tel_init.c \
	file/as_dev_file_rw.c \
	file/as_dev_file_open.c \
	io/as_dev_io_receive.c \
	io/as_dev_io_transmit.c \
	ioctl/as_dev_file_ctl.c \
	utils/as_dev_utils.c \
	utils/as_dev_chan_utils.c \
 	law/as_tel_gain.c 


KERNEL_LIB:=span.o


LOCAL_CLEAN:=clear


$(LOCAL_CLEAN):
	

include $(TOP_DIR)/Makefile.post
