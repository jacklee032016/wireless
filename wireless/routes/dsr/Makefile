#Makefile for the dsr kernel module
CCarm=arm-linux-gcc

MODCFLAGS := -mbig-endian -O3 -Wall -DCONFIG_KERNELD -DMODULE -D__KERNEL__ -DLINUX

HPATHarm := /works/isdn/kernel/linux-2.4.x/include

COMPILEARM := $(CCarm) $(MODCFLAGS) -I$(HPATHarm)

LINKARM := arm-linux-ld  -EB -r 

COMMONOBJ:= dsr_route.o dsr_debug.o dsr_input.o dsr_output.o dsr_queue.o
LINKOBJ := dsr-kmodule.o $(COMMONOBJ)
LINKOBJDBG := dsr-kdbg.o $(COMMONOBJ)

SRC := $(wildcard *.c)
ARMOBJ := $(SRC:.c=.o)

TARGET := dsr.o dsrdbg.o 


all:            $(TARGET)

clean:
		rm -f *.o *.d

###arm build
%.d:        %.c
		$(CCarm) -I$(HPATHarm) -MM $< 2> /dev/null \
		| sed "s/\.o/\.o/" > $@

include $(ARMOBJ:.o=.d)

%.o:        %.c
		$(COMPILEARM) -c $< -o $@

dsr.o:       $(LINKOBJ:.o=.o)
		$(LINKARM) -o dsr.o $(LINKOBJ:.o=.o)

dsrdbg.o:    $(LINKOBJDBG:.o=.o)
		$(LINKARM) -o dsrdbg.o $(LINKOBJDBG:.o=.o)

