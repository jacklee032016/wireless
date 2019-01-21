TEST_DIR:=$(shell pwd)
ASTEL_HOME=$(TEST_DIR)/..


TESTS= file_test ctl_test fxs_stat io_test event_test test_tone  test_dtmf \
		gsmplay as_ring
APP_CFLAGS+=-I$(ASTEL_HOME)/include -I$(ASTEL_HOME)/include/ -I$(ASTEL_HOME)/include/driver
APP_CFLAGS+=-I.
all:$(TESTS) 

file_test: as_tel_file_test.c
	gcc $(APP_CFLAGS) $< -o $@

ctl_test: as_dev_test_ctl.c
	gcc $(APP_CFLAGS) $< -o $@

fxs_stat: as_fxs_stat.c
	gcc $(APP_CFLAGS) $< -o $@

io_test: io_test.c
	gcc $(APP_CFLAGS) $< -o $@

event_test: event_test.c
	gcc $(APP_CFLAGS) $< -o $@

test_tone: test_play_tones.c
	gcc $(APP_CFLAGS) $< -o $@

as_ring: test_ring.c
	gcc $(APP_CFLAGS) $< -o $@

test_dtmf:test_dtmf.c
	gcc $(APP_CFLAGS) $< -o $@

gsmplay: test_gsmplay.c  #../user/libastdm.a
#	gcc $(APP_CFLAGS) $< -o $@ -L../libs -lgsm #-L../user -lastdm 


clean:
	rm -f $(TESTS)
	rm -f core
