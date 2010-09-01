TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = SerialPort.cpp \
	SonyProtocol.cpp \
	VTRControl.cpp \
	HTTPVTRControl.cpp \
	DummyVTRControl.cpp

GENERAL_OBJECTS=$(GENERAL_DIR)/.objs/Threads.o \
	$(GENERAL_DIR)/.objs/Logging.o \
	$(GENERAL_DIR)/.objs/RecorderException.o \
	$(GENERAL_DIR)/.objs/Utilities.o \
	$(GENERAL_DIR)/.objs/Config.o \
	$(GENERAL_DIR)/.objs/Timing.o

HTTP_OBJECTS = $(HTTP_DIR)/.objs/JSONObject.o \
	$(HTTP_DIR)/.objs/HTTPServer.o
	
ifdef USE_DVS_DUMMY
	CAPTURE_OBJECT_DEPS = $(CAPTURE_DIR)/.c_objs/sv_dummy.o
else
	CAPTURE_OBJECT_DEPS =
endif


INCLUDES += $(GENERAL_INC) $(HTTP_INC) $(DATABASE_INC) $(DVS_INC)



.PHONY: all
all: $(OBJECTS) vtr_control


vtr_control: .objs/main.o $(GENERAL_OBJECTS) $(HTTP_OBJECTS) $(OBJECTS) $(CAPTURE_OBJECT_DEPS)
	$(COMPILE) .objs/main.o -L. $(GENERAL_OBJECTS) $(HTTP_OBJECTS) $(OBJECTS) $(SHTTPD_LIB) $(DVS_LIB) $(INGEX_COMMON_LIB) -lpthread -lrt -o $@
	
	

.PHONY: clean
clean: cmn-clean
	@rm -f vtr_control
	
	
include $(TOPLEVEL)/rules.mk


