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
# Revision 1.2  2004/12/14 12:48:50  lizhijie
# support building header files in the architecture platform
#
# Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
# driver for assist telephone cards Tiger320-Si3210/3050
#
# $Revision: 1.1.1.1 $
#

all: myspan  myfxs  # mytest #$(SUB_DIRS)
#	make -C $<

myspan:
	make -C span all

myfxs:
	make -C fxs all

mytest:
	make -C test all

clean:
	rm -f core *.o
	make -C span clean
	make -C fxs clean
