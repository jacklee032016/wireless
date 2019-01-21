# IP Phone prototype
# created by Li zhijie 2004.08

USER_TOP_DIR:=$(shell pwd)
#ZAPTEL_HOME=$(TOP_DIR)/..

export USER_TOP_DIR #ZAPTEL_HOME

AS_TDM_LIB:=libastdm.a
EXEC:= $(AS_TDM_LIB) test

objs:=as_main.o as_init.o

include $(USER_TOP_DIR)/Makefile.include

export OBJDIR 



LIB_OBJS = $(OBJDIR)/as_init.o \
	$(OBJDIR)/as_device_stats.o $(OBJDIR)/as_device_monitor.o $(OBJDIR)/as_device.o\
	$(OBJDIR)/as_events.o  $(OBJDIR)/as_event_hook.o $(OBJDIR)/as_calling_agent.o\
	$(OBJDIR)/as_callee_monitor.o   $(OBJDIR)/as_callee_agent.o \
	$(OBJDIR)/as_data_sendout_agent.o \
	$(OBJDIR)/as_net_agent.o \
	$(OBJDIR)/as_tones.o  $(OBJDIR)/as_rings.o\
	$(OBJDIR)/as_gsm_io.o \
	$(OBJDIR)/as_utils.o $(OBJDIR)/as_thread.o \
	$(OBJDIR)/as_list.o $(OBJDIR)/as_fifo.o

EXEC_OBJS = $(OBJDIR)/as_main.o 

$(AS_TDM_LIB):$(LIB_OBJS)
	ar r $(AS_TDM_LIB) $(LIB_OBJS)
	ranlib $(AS_TDM_LIB)

test:	$(EXEC_OBJS) $(AS_TDM_LIB)
	$(CC) -o $@ $(EXEC_OBJS) $(LFLAGS)   $(LIBS)


install:   uninstall
#	$(MKDIR) $(PACKAGE_HOME)
#	$(CP) dhid genkeys $(PACKAGE_HOME)
#	$(CHOWN) $(OWNER) $(PACKAGE_HOME)dhid $(PACKAGE_HOME)genkeys
#	$(CHMOD) $(MODE)  $(PACKAGE_HOME)dhid $(PACKAGE_HOME)genkeys
#	touch $(PACKAHE_HOME)dhid.pid
#	$(CP) dhid.conf $(PACKAGE_HOME)

	
clear:
	$(RM) *.o *.*~ core
	
uninstall:	
	$(RM) $(PACKAGE_HOME)

clean:
	$(RM) *.o;\
	$(RM) $(EXEC);\
	$(RM) $(OBJDIR)/*.o;\
	for n in $(SUBDIRS);do \
	$(MAKE) -C "$$n" clear ;\
	done

