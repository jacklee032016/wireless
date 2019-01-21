# 
# Configure this with the following environment variables:
#

CROSS_COMPILE=arm-linux-


LOGIN		:= /bin/login

PROGS     = parse

INSTMODE  = 0755
INSTOWNER = root
INSTGROUP = root

CC        = $(CROSS_COMPILE)gcc
INSTALL   = install

CFLAGS	 += -mbig-endian -I. -pipe  -Wall

CFLAGS	 += -fomit-frame-pointer
STRIP	  = $(CROSS_COMPILE)strip 

LDFLAGS = -mbig-endian -Wl,-static

all: $(PROGS)

parse.o:parse_kernel_cmdline.c
	$(CC) $(CFLAGS)  -c $^ -o $@

$(PROGS): parse.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
	$(STRIP) --remove-section=.comment --remove-section=.note $@
	chmod $(INSTMODE) $(PROGS)
#	$(INSTALL) -m $(INSTMODE) -o $(INSTOWNER) -g $(INSTGROUP) $(PROGS) $(INSTDIR)

.PHONY: clean
clean:
	rm -f $(PROGS) *.o $(CORE)
