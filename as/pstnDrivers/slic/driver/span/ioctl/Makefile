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
# Revision 1.1.1.5  2004/12/31 08:48:46  fengshikui
# no message
#
# Revision 1.1.1.4  2004/12/31 08:38:04  fengshikui
# no message
#
# Revision 1.1.1.3  2004/12/31 08:23:51  fengshikui
# ÐÞ¸Ä°æ
#
# Revision 1.1.1.2  2004/12/31 08:00:09  fengshikui
# ÐÞ¸Ä°æ
#
# Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
# driver for assist telephone cards Tiger320-Si3210/3050
#
# $Revision: 1.1.1.1 $
#

all: ioctl.o
	cp ioctl.o ../
	
ioctl_objs =	as_dev_file_ctl.o  as_dev_ctl_ch_and_pseudo.o


ioctl.o: $(ioctl_objs)
	$(LD) -r -o $@ $(ioctl_objs)

	
as_dev_file_ctl.o:as_dev_file_ctl.c
	$(HOSTCC) $(KFLAGS) -o $@ -c $<

as_dev_ctl_ch_and_pseudo.o:as_dev_ctl_ch_and_pseudo.c
	$(HOSTCC) $(KFLAGS) -o $@ -c $<


CFLAGS += -I$(TOP_DIR)/include


clean:
	rm -f *.o 
