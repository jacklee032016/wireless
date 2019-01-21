# Makefile for the driver of PCM-Aanlog PBX
# $Author: lizhijie $
# $Log: Makefile,v $
# Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
# rebuild
#
# Revision 1.1  2005/06/27 06:00:14  lizhijie
# no message
#
# Revision 1.2  2005/05/26 05:15:43  lizhijie
# add zarlink 5023x driver into CVS
#
# Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
# new drivers for all devices
#
# Revision 1.2  2004/12/11 08:41:34  lizhijie
# remove odd item for make clean
#
# Revision 1.1.1.1  2004/11/29 01:47:46  lizhijie
# Driver for Tiger-AnalogPBX
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
#	make -C test clean
