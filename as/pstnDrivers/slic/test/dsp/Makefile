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
# Revision 1.1  2004/12/20 03:22:42  lizhijie
# add test for FSK generate and detect for test into CVS
#
# $Revision: 1.1.1.1 $
#

include $(TOP_DIR)/Makefile.pre

KERNEL=no

CODE_SUBDIRS = 

DELETE_LIST:= dsp_gain_test dsp_fsk_test_detect

#dsp_fsk_test_file busy_tone_gen  


 

LDFLAGS+=-L$(BIN_DIR) -L$(TOP_DIR)/libs -lassist -lm -lpthread

LOCAL_LAST=program

$(LOCAL_LAST):$(OBJS) $(DELETE_LIST)


############ Program 

busy_tone_gen:busy_tone_gen.c  $(BIN_DIR)/libassist.a
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	$(STRIP)  $@
	$(MOVE) $@ $(BIN_DIR)

dsp_gain_test:dsp_gain_test.c  $(BIN_DIR)/libassist.a
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	$(STRIP)  $@
	$(MOVE) $@ $(BIN_DIR)

dsp_fsk_test_file:dsp_fsk_test_file.c  $(BIN_DIR)/libassist.a
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	$(STRIP)  $@
	$(MOVE) $@ $(BIN_DIR)

dsp_fsk_test_detect:dsp_fsk_test_detect.c  $(BIN_DIR)/libassist.a
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	$(STRIP)  $@
	$(MOVE) $@ $(BIN_DIR)


LOCAL_CLEAN:=clear

$(LOCAL_CLEAN):
	rm -f *.o *.~ *.*~
	for n in $(DELETE_LIST);do \
		if test -f "$$n"; then \
			$(RM) "$$n";\
	  else :; fi; \
	done

include $(TOP_DIR)/Makefile.post
