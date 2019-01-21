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
# Revision 1.3  2004/12/14 12:47:34  lizhijie
# build the tones header in architecture platform
#
# Revision 1.2  2004/12/11 05:37:35  lizhijie
# add Tiger320 debug info
#
# Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
# driver for assist telephone cards Tiger320-Si3210/3050
#
# $Revision: 1.1.1.1 $
#

include $(TOP_DIR)/Makefile.pre

KERNEL=no


DELETE_LIST:= as_gen_dtmf_$(ARCH) \
	as_gen_tones_$(ARCH) \
	as_gen_hardtones \
	as_gen_hard_dtmf \
	as_dtmf_tones.h \
	as_dtmf_hard_tones.h \
	as_zone_tones.h \
	as_zone_hard_tones.h


SRC = 	#as_tonezone_data.c 
	

############ Program 1
as_dtmf_tones.h:as_gen_dtmf
#	./as_gen_dtmf
	
as_gen_dtmf_$(ARCH):as_gen_dtmf_tones.o 
	$(CC) $(CFLAGS) -c as_gen_dtmf_tones.c -o as_gen_dtmf_tones.o
	$(CC) $(LDFLAGS) -o $@ $< -lm
	$(STRIP)  $@
	$(COPY) $@ $(BIN_DIR)

############ Program 2
as_zone_tones.h:as_gen_tones
#	./as_gen_tones
	
as_gen_tones_$(ARCH): as_gen_tones.o
	$(CC) $(CFLAGS) -c as_gen_tones.c -o as_gen_tones.o
	$(CC) -o $@ as_gen_tones.o $(LDFLAGS) -lm
	$(STRIP)  $@
	$(COPY) $@ $(BIN_DIR)


LDFLAGS+=-L$(BIN_DIR) -lassist

LOCAL_LAST=program

$(LOCAL_LAST):$(OBJS) as_gen_tones_$(ARCH) as_gen_dtmf_$(ARCH) #as_zone_tones.h as_dtmf_tones.h


LOCAL_CLEAN:=clear

$(LOCAL_CLEAN):
	rm -f *.o *.~ *.*~
	for n in $(DELETE_LIST);do \
		if test -f "$$n"; then \
			$(RM) "$$n";\
	  else :; fi; \
	done

include $(TOP_DIR)/Makefile.post
